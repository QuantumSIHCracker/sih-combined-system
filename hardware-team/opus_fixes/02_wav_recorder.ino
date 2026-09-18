/* =====================================================================
 *  INMP441 + ESP32-S3  —  WAV RECORDER  (Rev 5.0)
 *  Streams clean 16-bit / 16 kHz mono PCM over serial.
 *  Capture it on your PC with capture_wav.py -> produces a real .wav
 *
 *  Core 3.x (driver/i2s_std.h). Run 01_mic_diagnostic.ino FIRST and
 *  copy the slot it chose into USE_RIGHT_SLOT below.
 *
 *  Bandwidth check:
 *    needed  = 16000 samples/s x 2 bytes = 32,000 B/s
 *    at 921600 baud (8N1, 10 bits/byte) = 92,160 B/s available
 *    utilisation = 34.7%  -> 65% headroom, no dropped frames.
 *
 *  Protocol on the wire:
 *    "<<<WAVSTART>>>"  + raw little-endian int16 PCM + "<<<WAVEND>>>"
 *  ===================================================================== */

#include <driver/i2s_std.h>
#include <driver/gpio.h>

#define PIN_WS    4
#define PIN_SCK   5
#define PIN_SD    7

#define SAMPLE_RATE      16000
#define READ_SAMPLES     512
#define SETTLE_MS        150
#define RECORD_SECONDS   10
#define SOFT_GAIN        4.0f

// Set to true if 01_mic_diagnostic.ino reported "Using RIGHT slot"
#define USE_RIGHT_SLOT   false

// Set true to apply the 80 Hz high-pass before writing (recommended —
// removes DC and the sub-20 Hz drift). Set false to record fully raw.
#define APPLY_HPF        true

#define MARK_START "<<<WAVSTART>>>"
#define MARK_END   "<<<WAVEND>>>"

i2s_chan_handle_t rx = NULL;

struct HPF {
  float a = 0.9690f, xPrev = 0, yPrev = 0;
  inline float step(float x) { float y = a*(yPrev + x - xPrev); xPrev=x; yPrev=y; return y; }
} hpf;

static inline int16_t toInt16(int32_t raw) {
  int32_t s24 = raw >> 8;                          // 24-bit left-justified
  float   v   = ((float)s24 / 256.0f) * SOFT_GAIN;
  if (v >  32767.0f) return  32767;                // saturate, never wrap
  if (v < -32768.0f) return -32768;
  return (int16_t)v;
}

bool i2sStart() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.dma_desc_num  = 8;
  chan_cfg.dma_frame_num = 512;
  chan_cfg.auto_clear    = true;
  if (i2s_new_channel(&chan_cfg, NULL, &rx) != ESP_OK) return false;

  i2s_std_config_t std_cfg = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                  I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = (gpio_num_t)PIN_SCK,
      .ws   = (gpio_num_t)PIN_WS,
      .dout = I2S_GPIO_UNUSED,
      .din  = (gpio_num_t)PIN_SD,
      .invert_flags = { .mclk_inv=false, .bclk_inv=false, .ws_inv=false },
    },
  };
#if SOC_I2S_SUPPORTS_APLL
  std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_APLL;
#endif
  std_cfg.slot_cfg.slot_mask = USE_RIGHT_SLOT ? I2S_STD_SLOT_RIGHT : I2S_STD_SLOT_LEFT;

  if (i2s_channel_init_std_mode(rx, &std_cfg) != ESP_OK) return false;
  gpio_set_pull_mode((gpio_num_t)PIN_SD, GPIO_PULLDOWN_ONLY);   // AFTER init
  if (i2s_channel_enable(rx) != ESP_OK) return false;

  delay(SETTLE_MS);
  static int32_t junk[READ_SAMPLES]; size_t br;
  for (int i = 0; i < 6; i++)
    i2s_channel_read(rx, junk, sizeof(junk), &br, 100/portTICK_PERIOD_MS);
  return true;
}

void recordOnce() {
  const uint32_t totalSamples = (uint32_t)SAMPLE_RATE * RECORD_SECONDS;
  static int32_t raw[READ_SAMPLES];
  static int16_t pcm[READ_SAMPLES];
  uint32_t written = 0, clipped = 0;

  Serial.print(MARK_START);

  uint32_t t0 = millis();
  while (written < totalSamples) {
    size_t br = 0;
    if (i2s_channel_read(rx, raw, sizeof(raw), &br, 200/portTICK_PERIOD_MS) != ESP_OK)
      continue;
    int n = br / sizeof(int32_t);
    if (written + n > totalSamples) n = totalSamples - written;

    for (int i = 0; i < n; i++) {
      int16_t s = toInt16(raw[i]);
#if APPLY_HPF
      float f = hpf.step((float)s);
      if (f >  32767.0f) f =  32767.0f;
      if (f < -32768.0f) f = -32768.0f;
      s = (int16_t)f;
#endif
      if (s == 32767 || s == -32768) clipped++;
      pcm[i] = s;
    }
    Serial.write((uint8_t*)pcm, n * sizeof(int16_t));   // no delay() anywhere
    written += n;
  }
  uint32_t el = millis() - t0;

  Serial.print(MARK_END);
  Serial.flush();
  delay(200);

  Serial.printf("\n[done] %u samples in %u ms -> %.0f Hz effective (expect %d)\n",
                written, el, 1000.0f*written/(float)el, SAMPLE_RATE);
  Serial.printf("[done] clipped %.3f %% of samples  [want < 0.10%%]\n",
                100.0f*clipped/(float)written);
}

void setup() {
  Serial.begin(921600);
  uint32_t t = millis(); while (!Serial && millis()-t < 3000) {}
  delay(300);

  if (!i2sStart()) { Serial.println("I2S init FAILED"); while (1) delay(1000); }

  Serial.println("\nINMP441 WAV RECORDER Rev 5.0");
  Serial.printf("16-bit / %d Hz mono / %d s / slot=%s / HPF=%s\n",
                SAMPLE_RATE, RECORD_SECONDS,
                USE_RIGHT_SLOT ? "RIGHT" : "LEFT",
                APPLY_HPF ? "on" : "off");
  Serial.println("Send 'r' to start a recording.");
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'r' || c == 'R') {
      Serial.printf("\nRecording %d seconds — speak now...\n", RECORD_SECONDS);
      delay(500);
      recordOnce();
      Serial.println("Send 'r' again for another take.");
    }
  }
}
