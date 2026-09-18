/* =====================================================================
 *  INMP441 + ESP32-S3  —  MIC DIAGNOSTIC  (Rev 5.0, clean rewrite)
 *  SIH 2026 — Hardware Team
 *
 *  For Arduino-ESP32 core 3.x (ESP-IDF 5.x) — uses driver/i2s_std.h
 *  If you are on core 2.x, use 01_mic_diagnostic_LEGACY.ino instead.
 *
 *  WHAT THIS FIXES vs the old sketch
 *  --------------------------------------------------------------
 *   1. No int16 wraparound  -> saturating conversion, never wraps
 *   2. MCLK explicitly unused -> never lands on GPIO 0 (BOOT pin)
 *   3. Pull-down applied AFTER pin config -> actually takes effect
 *   4. No delay() in the read loop -> 100% of samples captured
 *   5. APLL clock source -> exact 16.000 kHz, low jitter
 *   6. 100 ms settling + DMA flush -> no startup transient
 *   7. DC-removal high-pass filter -> real noise floor, not DC bias
 *   8. AUTO slot probe (LEFT vs RIGHT) -> solves the silent-mic quirk
 *   9. Band-energy check -> detects "sub-20 Hz garbage" fault directly
 *  ===================================================================== */

#include <driver/i2s_std.h>
#include <driver/gpio.h>
#include <math.h>

// ---------------------------------------------------------------- PINS
// All three are SAFE on ESP32-S3: not strapping pins, not USB (19/20),
// not flash/PSRAM (26-37). Do not move these to GPIO 0/3/45/46.
#define PIN_WS    4      // Word Select  / LRCLK   -> INMP441 WS
#define PIN_SCK   5      // Bit Clock    / BCLK    -> INMP441 SCK
#define PIN_SD    7      // Serial Data  (input)   <- INMP441 SD

#define SAMPLE_RATE      16000
#define READ_SAMPLES     512          // samples per i2s read
#define SETTLE_MS        150          // mic wake-up time after clock starts

// Gain applied AFTER 24-bit extraction, before int16 saturation.
// 1.0 = unity. Raise if speech is too quiet, lower if it clips.
#define SOFT_GAIN        4.0f

// ------------------------------------------------------- PASS CRITERIA
#define NOISE_FLOOR_MAX      300     // quiet-room RMS must be below this
#define SPEECH_RMS_MIN      1000     // talking must exceed this
#define AUDIOBAND_MIN_PCT   60.0f    // >=60% of energy must be >80 Hz
#define ZCR_SPEECH_MIN       200     // zero-crossings/sec during speech
#define CLIP_MAX_PCT         0.10f   // <0.1% of samples may hit the rail

i2s_chan_handle_t rx = NULL;
i2s_std_slot_mask_t activeSlot = I2S_STD_SLOT_LEFT;

// ==================================================== I2S SETUP / TEARDOWN
void i2sStop() {
  if (rx) {
    i2s_channel_disable(rx);
    i2s_del_channel(rx);
    rx = NULL;
  }
}

bool i2sStart(i2s_std_slot_mask_t slot) {
  i2sStop();

  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.dma_desc_num = 8;      // 8 x 512 = 4096 samples = 256 ms of slack
  chan_cfg.dma_frame_num = 512;   // old code had 4 x 256 = 64 ms only
  chan_cfg.auto_clear = true;

  if (i2s_new_channel(&chan_cfg, NULL, &rx) != ESP_OK) return false;

  i2s_std_config_t std_cfg = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                  I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,            // <-- FIX: never defaults to GPIO 0
      .bclk = (gpio_num_t)PIN_SCK,
      .ws   = (gpio_num_t)PIN_WS,
      .dout = I2S_GPIO_UNUSED,
      .din  = (gpio_num_t)PIN_SD,
      .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
    },
  };

  // FIX: exact 16.000 kHz via APLL (matters for MFCC / KWS alignment)
#if SOC_I2S_SUPPORTS_APLL
  std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_APLL;
#endif

  std_cfg.slot_cfg.slot_mask = slot;      // LEFT (L/R->GND) or RIGHT

  if (i2s_channel_init_std_mode(rx, &std_cfg) != ESP_OK) return false;

  // FIX: pull-down AFTER pin config, so it is not wiped by the GPIO matrix
  gpio_set_pull_mode((gpio_num_t)PIN_SD, GPIO_PULLDOWN_ONLY);

  if (i2s_channel_enable(rx) != ESP_OK) return false;

  delay(SETTLE_MS);                       // FIX: mic settling
  // flush the startup transient out of the DMA ring
  static int32_t junk[READ_SAMPLES];
  size_t br;
  for (int i = 0; i < 6; i++)
    i2s_channel_read(rx, junk, sizeof(junk), &br, 100 / portTICK_PERIOD_MS);

  return true;
}

// ================================================ SAMPLE CONVERSION (SAFE)
// INMP441 gives 24-bit data left-justified in a 32-bit slot.
// raw >> 8  => true signed 24-bit value.
// Then scale to int16 with SATURATION — this is the wraparound fix.
static inline int16_t toInt16(int32_t raw) {
  int32_t s24 = raw >> 8;                          // signed 24-bit
  float   v   = ((float)s24 / 256.0f) * SOFT_GAIN; // 24->16 bit + gain
  if (v >  32767.0f) return  32767;                // SATURATE, never wrap
  if (v < -32768.0f) return -32768;
  return (int16_t)v;
}

// ================================================= DC-REMOVAL HIGH-PASS
// One-pole HPF, ~80 Hz. Kills DC bias and the sub-20 Hz drift that made
// your last recording unlistenable. a = RC/(RC+dt) for fc=80Hz @16kHz.
struct HPF {
  float a = 0.9690f, xPrev = 0, yPrev = 0;
  inline float step(float x) {
    float y = a * (yPrev + x - xPrev);
    xPrev = x; yPrev = y;
    return y;
  }
  void reset() { xPrev = yPrev = 0; }
};
HPF hpf;

// ==================================================== MEASUREMENT STRUCT
struct Metrics {
  float rmsRaw;        // RMS before HPF (includes DC + sub-audio junk)
  float rmsAudio;      // RMS after HPF  (real audio band only)
  float audioPct;      // % of energy that is actually in the audio band
  float dcOffset;
  int   peak;
  float clipPct;
  float zcrPerSec;     // zero-crossing rate after HPF
  float actualRate;    // measured samples/sec — detects clock misconfig
};

Metrics measure(uint32_t durationMs) {
  Metrics m = {0,0,0,0,0,0,0,0};
  static int32_t buf[READ_SAMPLES];
  static int16_t pcm[READ_SAMPLES];

  double sumRawSq = 0, sumAudioSq = 0, sumDC = 0;
  uint32_t total = 0, clipped = 0, crossings = 0;
  float prevAudio = 0;
  hpf.reset();

  uint32_t t0 = millis();
  while (millis() - t0 < durationMs) {
    size_t br = 0;
    if (i2s_channel_read(rx, buf, sizeof(buf), &br, 200 / portTICK_PERIOD_MS) != ESP_OK)
      continue;
    int n = br / sizeof(int32_t);

    for (int i = 0; i < n; i++) {
      int16_t s = toInt16(buf[i]);
      pcm[i] = s;

      if (s == 32767 || s == -32768) clipped++;
      if (abs(s) > m.peak) m.peak = abs(s);

      sumDC    += s;
      sumRawSq += (double)s * s;

      float a = hpf.step((float)s);
      sumAudioSq += (double)a * a;
      if ((a >= 0 && prevAudio < 0) || (a < 0 && prevAudio >= 0)) crossings++;
      prevAudio = a;
    }
    total += n;
  }
  uint32_t elapsed = millis() - t0;

  if (total == 0) return m;
  m.rmsRaw    = sqrt(sumRawSq / total);
  m.rmsAudio  = sqrt(sumAudioSq / total);
  m.dcOffset  = sumDC / total;
  m.clipPct   = 100.0f * clipped / total;
  m.zcrPerSec = 1000.0f * crossings / (float)elapsed;
  m.actualRate= 1000.0f * total / (float)elapsed;
  m.audioPct  = (sumRawSq > 0) ? 100.0f * (float)(sumAudioSq / sumRawSq) : 0;
  if (m.audioPct > 100.0f) m.audioPct = 100.0f;
  return m;
}

void printMetrics(const char* label, const Metrics& m) {
  Serial.printf("\n--- %s ---\n", label);
  Serial.printf("  RMS (raw, incl. DC/drift) : %8.1f\n", m.rmsRaw);
  Serial.printf("  RMS (audio band >80Hz)    : %8.1f\n", m.rmsAudio);
  Serial.printf("  Energy in audio band      : %7.2f %%   [need >= %.0f%%]\n",
                m.audioPct, AUDIOBAND_MIN_PCT);
  Serial.printf("  DC offset                 : %8.1f\n", m.dcOffset);
  Serial.printf("  Peak                      : %8d\n", m.peak);
  Serial.printf("  Clipped samples           : %7.3f %%   [need <  %.2f%%]\n",
                m.clipPct, CLIP_MAX_PCT);
  Serial.printf("  Zero-crossing rate        : %8.0f /s\n", m.zcrPerSec);
  Serial.printf("  Measured sample rate      : %8.0f Hz  [expect %d]\n",
                m.actualRate, SAMPLE_RATE);
}

// ======================================================== SLOT AUTO-PROBE
// Solves the classic ESP32 quirk: with L/R tied to GND, the correct
// slot_mask is sometimes RIGHT, not LEFT. Tests both, keeps the live one.
void probeSlots() {
  Serial.println("\n[1/3] Probing I2S slot (LEFT vs RIGHT)...");
  Serial.println("      Make some noise now — talk, tap the mic.");

  i2sStart(I2S_STD_SLOT_LEFT);
  Metrics L = measure(2500);
  Serial.printf("      LEFT  : audio-band RMS = %.1f\n", L.rmsAudio);

  i2sStart(I2S_STD_SLOT_RIGHT);
  Metrics R = measure(2500);
  Serial.printf("      RIGHT : audio-band RMS = %.1f\n", R.rmsAudio);

  if (L.rmsAudio >= R.rmsAudio) {
    activeSlot = I2S_STD_SLOT_LEFT;
    Serial.println("      -> Using LEFT slot.");
  } else {
    activeSlot = I2S_STD_SLOT_RIGHT;
    Serial.println("      -> Using RIGHT slot  (L/R pin is tied to GND but the");
    Serial.println("         peripheral latches the right slot — known quirk).");
  }
  i2sStart(activeSlot);
}

// =============================================================== VERDICT
void verdict(const Metrics& quiet, const Metrics& loud) {
  Serial.println("\n================= VERDICT =================");
  bool ok = true;

  // Clock sanity
  float rateErr = 100.0f * fabs(loud.actualRate - SAMPLE_RATE) / SAMPLE_RATE;
  if (rateErr > 2.0f) {
    Serial.printf("FAIL  Sample rate off by %.1f%% — clock misconfigured.\n", rateErr);
    ok = false;
  } else Serial.printf("PASS  Sample rate within %.1f%% of 16 kHz.\n", rateErr);

  // The exact fault your last WAV had
  if (loud.audioPct < AUDIOBAND_MIN_PCT) {
    Serial.printf("FAIL  Only %.1f%% of energy is in the audio band.\n", loud.audioPct);
    Serial.println("      => Signal is sub-20Hz drift, NOT speech.");
    Serial.println("      => Check SCK/WS wiring, shorten jumpers, reseat SD.");
    ok = false;
  } else Serial.printf("PASS  %.1f%% of energy is real audio-band content.\n", loud.audioPct);

  if (loud.zcrPerSec < ZCR_SPEECH_MIN) {
    Serial.printf("FAIL  Zero-crossing rate %.0f/s is too low for speech.\n", loud.zcrPerSec);
    ok = false;
  } else Serial.printf("PASS  Zero-crossing rate %.0f/s looks like speech.\n", loud.zcrPerSec);

  if (quiet.rmsAudio > NOISE_FLOOR_MAX) {
    Serial.printf("FAIL  Noise floor %.0f > %d.\n", quiet.rmsAudio, NOISE_FLOOR_MAX);
    ok = false;
  } else Serial.printf("PASS  Noise floor %.0f.\n", quiet.rmsAudio);

  if (loud.rmsAudio < SPEECH_RMS_MIN) {
    Serial.printf("FAIL  Speech RMS %.0f < %d — raise SOFT_GAIN or move closer.\n",
                  loud.rmsAudio, SPEECH_RMS_MIN);
    ok = false;
  } else Serial.printf("PASS  Speech RMS %.0f.\n", loud.rmsAudio);

  if (loud.clipPct > CLIP_MAX_PCT) {
    Serial.printf("FAIL  %.3f%% clipped — lower SOFT_GAIN.\n", loud.clipPct);
    ok = false;
  } else Serial.printf("PASS  Clipping %.3f%%.\n", loud.clipPct);

  float snr = (quiet.rmsAudio > 1)
              ? 20.0f * log10(loud.rmsAudio / quiet.rmsAudio) : 99;
  Serial.printf("      SNR (speech vs quiet): %.1f dB  [>20 dB is good for KWS]\n", snr);

  Serial.println(ok ? "\n*** ALL CHECKS PASSED — mic is good for KWS. ***"
                    : "\n*** FAILED — fix above before building the KWS pipeline. ***");
  Serial.println("===========================================");
}

// ================================================================== MAIN
void setup() {
  Serial.begin(115200);
  uint32_t t = millis();
  while (!Serial && millis() - t < 3000) {}
  delay(300);

  Serial.println("\n\n============================================");
  Serial.println(" INMP441 DIAGNOSTIC  —  Rev 5.0");
  Serial.printf ( " CPU %d MHz | Free heap %u B | Flash %u MB\n",
                  ESP.getCpuFreqMHz(), ESP.getFreeHeap(),
                  ESP.getFlashChipSize()/(1024*1024));
  Serial.printf ( " Pins: WS=%d  SCK=%d  SD=%d\n", PIN_WS, PIN_SCK, PIN_SD);
  Serial.println("============================================");

  probeSlots();

  Serial.println("\n[2/3] QUIET test — stay silent for 3 seconds...");
  delay(700);
  Metrics quiet = measure(3000);
  printMetrics("QUIET (noise floor)", quiet);

  Serial.println("\n[3/3] SPEECH test — talk normally for 4 seconds NOW...");
  delay(400);
  Metrics loud = measure(4000);
  printMetrics("SPEECH", loud);

  verdict(quiet, loud);
  Serial.println("\nEntering live monitor (no delay, continuous capture)...\n");
}

// Live monitor — continuous, non-blocking, no dropped samples.
void loop() {
  static int32_t buf[READ_SAMPLES];
  static uint32_t lastPrint = 0;
  static double accSq = 0; static uint32_t accN = 0; static int accPeak = 0;

  size_t br = 0;
  if (i2s_channel_read(rx, buf, sizeof(buf), &br, 100 / portTICK_PERIOD_MS) != ESP_OK) return;
  int n = br / sizeof(int32_t);

  for (int i = 0; i < n; i++) {
    float a = hpf.step((float)toInt16(buf[i]));
    accSq += (double)a * a;
    if (fabs(a) > accPeak) accPeak = (int)fabs(a);
  }
  accN += n;

  if (millis() - lastPrint > 250 && accN > 0) {
    lastPrint = millis();
    float rms = sqrt(accSq / accN);
    const char* lvl = rms <  150 ? "silence"
                    : rms <  600 ? "ambient"
                    : rms < 4000 ? "SPEECH"
                                 : "LOUD";
    Serial.printf("RMS %6.0f | peak %6d | %-8s | heap %u\n",
                  rms, accPeak, lvl, ESP.getFreeHeap());
    accSq = 0; accN = 0; accPeak = 0;
  }
}
