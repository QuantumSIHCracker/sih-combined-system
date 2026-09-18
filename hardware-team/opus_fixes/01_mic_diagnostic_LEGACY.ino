/* =====================================================================
 *  INMP441 + ESP32-S3  —  MIC DIAGNOSTIC  (Rev 5.0 — LEGACY API)
 *
 *  Use this ONLY if you are on Arduino-ESP32 core 2.x (driver/i2s.h).
 *  On core 3.x use 01_mic_diagnostic.ino instead.
 *
 *  Check your core version: Arduino IDE -> Tools -> Board -> Boards Manager
 *  -> search "esp32". If it says 3.x, use the other file.
 *
 *  Same fixes as Rev 5.0: saturating conversion, explicit MCLK,
 *  pull-down after set_pin, APLL, settling delay, no delay() in loop,
 *  HPF, slot probe, band-energy fault detection.
 *  ===================================================================== */

#include <driver/i2s.h>
#include <driver/gpio.h>
#include <math.h>

#define PIN_WS    4
#define PIN_SCK   5
#define PIN_SD    7

#define SAMPLE_RATE    16000
#define READ_SAMPLES   512
#define SETTLE_MS      150
#define SOFT_GAIN      4.0f

#define NOISE_FLOOR_MAX     300
#define SPEECH_RMS_MIN     1000
#define AUDIOBAND_MIN_PCT  60.0f
#define ZCR_SPEECH_MIN      200
#define CLIP_MAX_PCT        0.10f

i2s_channel_fmt_t activeFmt = I2S_CHANNEL_FMT_ONLY_LEFT;

void i2sStop() {
  i2s_driver_uninstall(I2S_NUM_0);
}

bool i2sStart(i2s_channel_fmt_t fmt) {
  i2sStop();

  i2s_config_t cfg = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate          = SAMPLE_RATE,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format       = fmt,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count        = 8,          // was 4 — now 256 ms of slack
    .dma_buf_len          = 512,        // was 256
    .use_apll             = true,       // FIX: exact 16.000 kHz
    .tx_desc_auto_clear   = false,
    .fixed_mclk           = 0
  };

  // FIX: designated initializers + explicit MCLK (never defaults to GPIO 0)
  i2s_pin_config_t pins = {
    .mck_io_num   = I2S_PIN_NO_CHANGE,
    .bck_io_num   = PIN_SCK,
    .ws_io_num    = PIN_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num  = PIN_SD
  };

  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL) != ESP_OK) return false;
  if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) return false;

  // FIX: pull-down AFTER set_pin
  gpio_set_pull_mode((gpio_num_t)PIN_SD, GPIO_PULLDOWN_ONLY);

  delay(SETTLE_MS);                     // FIX: settling
  i2s_zero_dma_buffer(I2S_NUM_0);
  return true;
}

static inline int16_t toInt16(int32_t raw) {
  int32_t s24 = raw >> 8;
  float   v   = ((float)s24 / 256.0f) * SOFT_GAIN;
  if (v >  32767.0f) return  32767;     // SATURATE — the wraparound fix
  if (v < -32768.0f) return -32768;
  return (int16_t)v;
}

struct HPF {
  float a = 0.9690f, xPrev = 0, yPrev = 0;
  inline float step(float x) { float y = a*(yPrev + x - xPrev); xPrev=x; yPrev=y; return y; }
  void reset() { xPrev = yPrev = 0; }
};
HPF hpf;

struct Metrics {
  float rmsRaw, rmsAudio, audioPct, dcOffset;
  int   peak;
  float clipPct, zcrPerSec, actualRate;
};

Metrics measure(uint32_t durationMs) {
  Metrics m = {0,0,0,0,0,0,0,0};
  static int32_t buf[READ_SAMPLES];
  double sumRawSq=0, sumAudioSq=0, sumDC=0;
  uint32_t total=0, clipped=0, crossings=0;
  float prevAudio = 0;
  hpf.reset();

  uint32_t t0 = millis();
  while (millis() - t0 < durationMs) {
    size_t br = 0;
    if (i2s_read(I2S_NUM_0, buf, sizeof(buf), &br, 200/portTICK_PERIOD_MS) != ESP_OK) continue;
    int n = br / sizeof(int32_t);
    for (int i = 0; i < n; i++) {
      int16_t s = toInt16(buf[i]);
      if (s == 32767 || s == -32768) clipped++;
      if (abs(s) > m.peak) m.peak = abs(s);
      sumDC += s;
      sumRawSq += (double)s*s;
      float a = hpf.step((float)s);
      sumAudioSq += (double)a*a;
      if ((a>=0 && prevAudio<0) || (a<0 && prevAudio>=0)) crossings++;
      prevAudio = a;
    }
    total += n;
  }
  uint32_t el = millis() - t0;
  if (!total) return m;
  m.rmsRaw     = sqrt(sumRawSq/total);
  m.rmsAudio   = sqrt(sumAudioSq/total);
  m.dcOffset   = sumDC/total;
  m.clipPct    = 100.0f*clipped/total;
  m.zcrPerSec  = 1000.0f*crossings/(float)el;
  m.actualRate = 1000.0f*total/(float)el;
  m.audioPct   = sumRawSq>0 ? 100.0f*(float)(sumAudioSq/sumRawSq) : 0;
  if (m.audioPct > 100.0f) m.audioPct = 100.0f;
  return m;
}

void printMetrics(const char* label, const Metrics& m) {
  Serial.printf("\n--- %s ---\n", label);
  Serial.printf("  RMS raw  %8.1f | RMS audio %8.1f\n", m.rmsRaw, m.rmsAudio);
  Serial.printf("  Audio-band energy %6.2f %%  [need >= %.0f%%]\n", m.audioPct, AUDIOBAND_MIN_PCT);
  Serial.printf("  DC %7.1f | peak %6d | clip %.3f %%\n", m.dcOffset, m.peak, m.clipPct);
  Serial.printf("  ZCR %6.0f /s | rate %6.0f Hz\n", m.zcrPerSec, m.actualRate);
}

void probeSlots() {
  Serial.println("\n[1/3] Probing channel format — make noise now...");
  i2sStart(I2S_CHANNEL_FMT_ONLY_LEFT);
  Metrics L = measure(2500);
  Serial.printf("      ONLY_LEFT  : %.1f\n", L.rmsAudio);
  i2sStart(I2S_CHANNEL_FMT_ONLY_RIGHT);
  Metrics R = measure(2500);
  Serial.printf("      ONLY_RIGHT : %.1f\n", R.rmsAudio);
  activeFmt = (L.rmsAudio >= R.rmsAudio) ? I2S_CHANNEL_FMT_ONLY_LEFT
                                         : I2S_CHANNEL_FMT_ONLY_RIGHT;
  Serial.printf("      -> Using %s\n",
                activeFmt==I2S_CHANNEL_FMT_ONLY_LEFT ? "ONLY_LEFT" : "ONLY_RIGHT");
  i2sStart(activeFmt);
}

void verdict(const Metrics& q, const Metrics& l) {
  Serial.println("\n================= VERDICT =================");
  bool ok = true;
  float rateErr = 100.0f*fabs(l.actualRate-SAMPLE_RATE)/SAMPLE_RATE;
  if (rateErr > 2.0f) { Serial.printf("FAIL  rate off %.1f%%\n", rateErr); ok=false; }
  else Serial.printf("PASS  rate within %.1f%%\n", rateErr);

  if (l.audioPct < AUDIOBAND_MIN_PCT) {
    Serial.printf("FAIL  only %.1f%% audio-band -> sub-20Hz drift, not speech.\n", l.audioPct);
    Serial.println("      Check SCK/WS wiring, shorten jumpers, reseat SD.");
    ok=false;
  } else Serial.printf("PASS  %.1f%% audio-band energy\n", l.audioPct);

  if (l.zcrPerSec < ZCR_SPEECH_MIN) { Serial.printf("FAIL  ZCR %.0f/s too low\n", l.zcrPerSec); ok=false; }
  else Serial.printf("PASS  ZCR %.0f/s\n", l.zcrPerSec);

  if (q.rmsAudio > NOISE_FLOOR_MAX) { Serial.printf("FAIL  noise floor %.0f\n", q.rmsAudio); ok=false; }
  else Serial.printf("PASS  noise floor %.0f\n", q.rmsAudio);

  if (l.rmsAudio < SPEECH_RMS_MIN) { Serial.printf("FAIL  speech RMS %.0f — raise SOFT_GAIN\n", l.rmsAudio); ok=false; }
  else Serial.printf("PASS  speech RMS %.0f\n", l.rmsAudio);

  if (l.clipPct > CLIP_MAX_PCT) { Serial.printf("FAIL  clip %.3f%% — lower SOFT_GAIN\n", l.clipPct); ok=false; }
  else Serial.printf("PASS  clip %.3f%%\n", l.clipPct);

  float snr = q.rmsAudio > 1 ? 20.0f*log10(l.rmsAudio/q.rmsAudio) : 99;
  Serial.printf("      SNR %.1f dB  [>20 dB good for KWS]\n", snr);
  Serial.println(ok ? "\n*** ALL CHECKS PASSED ***" : "\n*** FAILED — fix before KWS ***");
  Serial.println("===========================================");
}

void setup() {
  Serial.begin(115200);
  uint32_t t = millis(); while (!Serial && millis()-t < 3000) {}
  delay(300);
  Serial.println("\n\n INMP441 DIAGNOSTIC Rev 5.0 (legacy API)");
  Serial.printf(" Pins WS=%d SCK=%d SD=%d\n", PIN_WS, PIN_SCK, PIN_SD);

  probeSlots();
  Serial.println("\n[2/3] QUIET — stay silent 3 s...");  delay(700);
  Metrics q = measure(3000); printMetrics("QUIET", q);
  Serial.println("\n[3/3] SPEECH — talk 4 s NOW...");     delay(400);
  Metrics l = measure(4000); printMetrics("SPEECH", l);
  verdict(q, l);
  Serial.println("\nLive monitor...\n");
}

void loop() {
  static int32_t buf[READ_SAMPLES];
  static uint32_t lastPrint = 0;
  static double accSq = 0; static uint32_t accN = 0; static int accPeak = 0;

  size_t br = 0;
  if (i2s_read(I2S_NUM_0, buf, sizeof(buf), &br, 100/portTICK_PERIOD_MS) != ESP_OK) return;
  int n = br/sizeof(int32_t);
  for (int i = 0; i < n; i++) {
    float a = hpf.step((float)toInt16(buf[i]));
    accSq += (double)a*a;
    if (fabs(a) > accPeak) accPeak = (int)fabs(a);
  }
  accN += n;

  if (millis()-lastPrint > 250 && accN) {
    lastPrint = millis();
    float rms = sqrt(accSq/accN);
    const char* lvl = rms<150?"silence":rms<600?"ambient":rms<4000?"SPEECH":"LOUD";
    Serial.printf("RMS %6.0f | peak %6d | %-8s\n", rms, accPeak, lvl);
    accSq=0; accN=0; accPeak=0;
  }
}
