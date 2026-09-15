# 🔌 Circuit Design & Component Test Guide
### SIH 2026 — ESP32-S3 Voice Assistant | Rev 1.0
### Hardware Team — QuantumSIHCracker

> **Test every component BEFORE writing any firmware.**
> A wiring mistake found now saves hours of debugging later.

---

## 📐 Schematic — Rev 1.0

![Circuit Schematic Rev 1.0](circuit_schematic_rev1.jpg)

---

## 🧰 Bill of Materials (BOM)

| # | Component | Specification | Qty | Purpose |
|---|---|---|---|---|
| 1 | ESP32-S3 Dev Module | Xtensa LX7 Dual Core, 8/16MB Flash, 8MB PSRAM | 1 | Main MCU |
| 2 | INMP441 MEMS Microphone | I2S, Omnidirectional, 3.3V, -26 dBFS sensitivity | 1 | Audio capture |
| 3 | WS2812B RGB LED | 5V tolerant, 3.3V data signal compatible | 1 | Status indicator |
| 4 | Capacitor, 100nF (0.1µF) | Ceramic, any package | 1 | INMP441 VDD decoupling |
| 5 | Resistor, 330Ω | 1/4W or 1/8W | 1 | WS2812B data line protection |
| 6 | Resistor, 10kΩ | 1/4W | 1 | Optional pull-up on BOOT btn |
| 7 | Breadboard | Full size (830 tie points) | 1 | Prototyping |
| 8 | Jumper wires | Male-to-Male, assorted colours | 10+ | Connections |
| 9 | USB-C cable | Data capable (not charge-only) | 1 | Programming + power |
| 10 | 5V USB power supply | ≥ 500mA | 1 | Board power (via USB) |

> ⚠️ **Critical**: Use a **data-capable** USB-C cable. Charge-only cables will not show up as a serial port on your PC.

---

## 🗺️ Complete Wiring Table

### Power Rail
| From | To | Wire Colour | Notes |
|---|---|---|---|
| ESP32-S3 `3V3` | INMP441 `VDD` | 🔴 Red | 3.3V only — never 5V |
| ESP32-S3 `3V3` | WS2812B `VCC` | 🔴 Red | 3.3V is sufficient for 1 LED |
| ESP32-S3 `GND` | INMP441 `GND` | ⚫ Black | Common ground |
| ESP32-S3 `GND` | INMP441 `L/R` | ⚫ Black | **Must be tied to GND** (selects left channel) |
| ESP32-S3 `GND` | WS2812B `GND` | ⚫ Black | Common ground |
| INMP441 `VDD` | Cap+ (100nF) | — | Place cap as close to VDD pin as possible |
| Cap– (100nF) | INMP441 `GND` | — | Decoupling capacitor |

### I2S Signal Lines (INMP441 → ESP32-S3)
| From | To | GPIO | Wire Colour | Function |
|---|---|---|---|---|
| ESP32-S3 `GPIO 4` | INMP441 `WS` | 4 | 🟡 Yellow | Word Select / LRCLK |
| ESP32-S3 `GPIO 5` | INMP441 `SCK` | 5 | 🟠 Orange | Bit Clock / BCLK |
| INMP441 `SD` | ESP32-S3 `GPIO 7` | 7 | 🔵 Blue | Serial Data (audio out) |

### LED Signal Line
| From | To | GPIO | Notes |
|---|---|---|---|
| ESP32-S3 `GPIO 48` | 330Ω resistor | 48 | Series resistor |
| 330Ω resistor other end | WS2812B `DIN` | — | Data in to LED |

---

## 🔍 Pin Identification Guide

### ESP32-S3 Dev Module Pinout
```
         USB-C
    ┌──────────┐
3V3 │  1    30 │ 3V3
GND │  2    29 │ GND
IO1 │  3    28 │ IO1
IO2 │  4    27 │ IO2
IO4 │  5    26 │ IO4   ← WS  (yellow wire → INMP441 WS)
IO6 │  6    25 │ IO5   ← SCK (orange wire → INMP441 SCK)
IO5 │  7    24 │ IO7   ← SD  (blue wire ← INMP441 SD)
IO6 │  8    23 │ IO8
IO7 │  9    20 │ IO3
IO1 │ 10    21 │ IO1
IO9 │ 11    22 │ IO17
3V3 │ 12    19 │ IO48  ← WS2812B DIN (green wire → 330Ω → DIN)
    └──────────┘
         USB-C
         
Note: BOOT button = GPIO 0 (built-in)
Note: GPIO 48 = built-in RGB LED on most ESP32-S3 DevKit boards
```

### INMP441 Breakout Pinout
```
    ┌──────┐
    │ VDD  │ ← 3.3V (red)
    │ GND  │ ← GND (black)
    │ SD   │ → GPIO 7 (blue)
    │ SCK  │ ← GPIO 5 (orange)
    │ WS   │ ← GPIO 4 (yellow)
    │ L/R  │ ← GND (black) ← MUST be tied to GND
    └──────┘
```

---

## ✅ Component Test Checklist

Work through this **before writing any firmware**. Mark each item when done.

---

### 🔋 Test 1 — Power & Continuity

**Equipment needed**: Multimeter

- [ ] Set multimeter to DC Voltage mode
- [ ] Connect ESP32-S3 to PC via USB-C
- [ ] Measure voltage between ESP32-S3 `3V3` pin and `GND` → should read **3.28V – 3.35V**
- [ ] Measure voltage at INMP441 `VDD` to `GND` → should read **3.28V – 3.35V**
- [ ] Set multimeter to Continuity/Beep mode
- [ ] Check INMP441 `L/R` pin → `GND` → should **beep** (connected)
- [ ] Check INMP441 `GND` → ESP32-S3 `GND` → should **beep**
- [ ] Check there is **NO continuity** between `3V3` and `GND` (no short circuit)

**Pass criteria**: 3.3V stable, all grounds connected, no short

---

### 💡 Test 2 — WS2812B RGB LED

**Method**: Upload the LED blink sketch in Arduino IDE

```cpp
// Test: WS2812B LED on GPIO 48
// Upload this to confirm LED works
#include <Adafruit_NeoPixel.h>

#define LED_PIN 48
#define NUM_LEDS 1

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
    strip.begin();
    strip.show();
}

void loop() {
    strip.setPixelColor(0, strip.Color(255, 0, 0));   // RED
    strip.show(); delay(500);
    strip.setPixelColor(0, strip.Color(0, 255, 0));   // GREEN
    strip.show(); delay(500);
    strip.setPixelColor(0, strip.Color(0, 0, 255));   // BLUE
    strip.show(); delay(500);
    strip.setPixelColor(0, strip.Color(0, 0, 0));     // OFF
    strip.show(); delay(500);
}
```

**Arduino IDE settings for this test:**
- Board: `ESP32S3 Dev Module`
- USB CDC On Boot: `Enabled`
- Upload Speed: `921600`

- [ ] LED cycles RED → GREEN → BLUE → OFF repeatedly
- [ ] All three colours are clearly distinct (not just white)
- [ ] No flickering or unstable colour

**Pass criteria**: All 3 colours visible, smooth cycling

---

### 🎤 Test 3 — INMP441 Microphone (I2S Read)

**Method**: Upload this I2S test sketch

```cpp
// Test: INMP441 I2S microphone read
// Prints raw audio samples to Serial Monitor
#include <driver/i2s.h>

#define I2S_WS   4
#define I2S_SCK  5
#define I2S_SD   7

void setup() {
    Serial.begin(115200);
    while (!Serial);
    Serial.println("INMP441 I2S Test Starting...");

    // Configure I2S
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 256,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD
    };

    // Set pull-down on SD pin to avoid floating
    gpio_set_pull_mode((gpio_num_t)I2S_SD, GPIO_PULLDOWN_ONLY);

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);

    Serial.println("I2S initialized. Reading samples...");
    Serial.println("SILENCE: values should be near 0");
    Serial.println("CLAP:    values should jump to +/- thousands");
}

void loop() {
    int32_t rawSamples[128];
    size_t bytesRead = 0;

    i2s_read(I2S_NUM_0, rawSamples, sizeof(rawSamples), &bytesRead, portMAX_DELAY);

    int samplesRead = bytesRead / sizeof(int32_t);
    int32_t peak = 0;

    for (int i = 0; i < samplesRead; i++) {
        int16_t sample = (int16_t)(rawSamples[i] >> 11); // scale to int16
        if (abs(sample) > abs(peak)) peak = sample;
    }

    Serial.printf("Peak amplitude: %6d  |  %s\n", peak,
        abs(peak) < 200   ? "SILENCE" :
        abs(peak) < 2000  ? "ambient noise" :
        abs(peak) < 10000 ? "SPEECH" : "LOUD / CLAP");

    delay(100);
}
```

**Expected Serial Monitor output:**
```
INMP441 I2S Test Starting...
I2S initialized. Reading samples...
SILENCE: values should be near 0
CLAP:    values should jump to +/- thousands

Peak amplitude:     42  |  SILENCE
Peak amplitude:     67  |  SILENCE
Peak amplitude:   2341  |  ambient noise   ← when talking
Peak amplitude:   8210  |  SPEECH          ← talking directly into mic
Peak amplitude:  18500  |  LOUD / CLAP     ← clap near mic
```

- [ ] Serial Monitor shows values near 0 in silence (< 200)
- [ ] Values jump when you talk (> 1000)
- [ ] Values jump sharply when you clap (> 10000)
- [ ] No value is constantly `0x7FFFFFFF` or `-32768` (floating line — check SD pull-down and wiring)
- [ ] No value is always exactly `0` (check VDD connection)

**Pass criteria**: Responsive to sound, noise floor < 200, speech > 1000

---

### 🔁 Test 4 — Serial Communication (921600 Baud)

**Method**: Upload and test at full project baud rate

```cpp
// Test: Serial at 921600 baud — confirms PC + cable can handle it
void setup() {
    Serial.begin(921600);
    delay(1000);
}

void loop() {
    Serial.println("BAUD_TEST_OK");
    delay(100);
}
```

- [ ] Open Serial Monitor at **921600** baud
- [ ] See `BAUD_TEST_OK` printing clearly every 100ms
- [ ] No garbled characters or question marks
- [ ] Confirmed on the actual cable you will use for the demo

**Pass criteria**: Clean output at 921600 baud on your demo cable

---

### 🌡️ Test 5 — System Stress (All Components Together)

**Method**: Run all three peripherals simultaneously for 5 minutes

```cpp
// Test: All components running together — check for heat, crashes, resets
#include <driver/i2s.h>
#include <Adafruit_NeoPixel.h>

#define I2S_WS 4
#define I2S_SCK 5
#define I2S_SD 7
#define LED_PIN 48

Adafruit_NeoPixel led(1, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
    Serial.begin(921600);
    led.begin(); led.show();

    gpio_set_pull_mode((gpio_num_t)I2S_SD, GPIO_PULLDOWN_ONLY);

    i2s_config_t cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4, .dma_buf_len = 256
    };
    i2s_pin_config_t pins = {I2S_SCK, I2S_WS, I2S_PIN_NO_CHANGE, I2S_SD};
    i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pins);

    Serial.println("Stress test running — check for resets...");
}

int ledColor = 0;
unsigned long lastLed = 0, lastPrint = 0;

void loop() {
    int32_t buf[256]; size_t br = 0;
    i2s_read(I2S_NUM_0, buf, sizeof(buf), &br, 10);

    int32_t peak = 0;
    for (int i = 0; i < (int)(br/4); i++)
        if (abs(buf[i] >> 11) > abs(peak)) peak = buf[i] >> 11;

    if (millis() - lastLed > 500) {
        lastLed = millis();
        ledColor = (ledColor + 1) % 3;
        led.setPixelColor(0, ledColor==0 ? led.Color(255,0,0) :
                             ledColor==1 ? led.Color(0,255,0) :
                                          led.Color(0,0,255));
        led.show();
    }

    if (millis() - lastPrint > 1000) {
        lastPrint = millis();
        Serial.printf("[%lus] Peak: %d | Free heap: %u bytes | Uptime OK\n",
                      millis()/1000, (int)peak, ESP.getFreeHeap());
    }
}
```

- [ ] Runs for **5 minutes** without spontaneous reset
- [ ] Free heap stays **stable** (not slowly decreasing — memory leak)
- [ ] LED cycling continues without freezing
- [ ] Audio peak responds to sound throughout
- [ ] ESP32-S3 chip does **not get hot to touch** (warm is fine, hot is not)

**Pass criteria**: 5-minute stable run, no resets, no heat issues

---

## 📊 Test Results Log

Fill this in as you complete each test. Keep this as your hardware validation record.

| Test | Date | Pass/Fail | Notes |
|---|---|---|---|
| Test 1 — Power & Continuity | | | |
| Test 2 — WS2812B LED | | | |
| Test 3 — INMP441 Microphone | | | |
| Test 4 — Serial @ 921600 | | | |
| Test 5 — Stress Test (5 min) | | | |

---

## ❌ Troubleshooting Guide

| Symptom | Most Likely Cause | Fix |
|---|---|---|
| Serial Monitor shows nothing | Wrong COM port or charge-only USB cable | Try different cable, check Device Manager |
| Peak always 0 | INMP441 VDD not connected, or no common GND | Check red and black wires |
| Peak always `32767` or `-32768` | SD pin floating | Add pull-down on GPIO 7, check SD wire |
| Peak never changes with sound | L/R pin not tied to GND | Connect INMP441 L/R → GND |
| LED is white instead of colours | Loose DIN wire | Reseat the wire on GPIO 48 |
| LED doesn't light at all | No GND or VCC, or wrong GPIO | Check wiring and confirm GPIO 48 |
| Upload fails | Wrong board settings or CDC not enabled | Set USB CDC On Boot = Enabled |
| Upload works but no serial output | Wrong baud rate in monitor | Set baud to 921600 |
| ESP32 resets randomly in stress test | Power supply can't handle load | Use powered USB hub or mains adapter |

---

## 📁 Files in This Folder

```
hardware-team/
├── HARDWARE_TEAM_PLAN.md       ← Full firmware build plan
├── CIRCUIT_DESIGN.md           ← This file (circuit + tests)
└── circuit_schematic_rev1.jpg  ← Schematic image Rev 1.0
```

> Once all 5 tests pass → move to `HARDWARE_TEAM_PLAN.md` Phase 1 firmware tasks.
