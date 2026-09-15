# 🔩 Hardware Team — Complete Working Plan
### Smart India Hackathon (SIH) 2026 | QuantumSIHCracker | Team: Hardware Firmware

> **Hardware Lead**: Arpit Kumar (WSL Ubuntu: `/home/arpit_ubuntu`)
> **Repository**: `https://github.com/QuantumSIHCracker/sih-hardware-firmware`
> **Branch Strategy**: `main` (protected) → `dev` → `feature/<name>`
> **All new code lives in the repo above — nothing else.**

---

## 📌 Your Mission

You are the **Hardware & Firmware Team**. You build and own:
1. Clean audio capture from the **INMP441 MEMS mic** over I2S at 16 kHz
2. On-device **Keyword Spotting (KWS)** — detect wake word "Ankit" locally, no cloud
3. Reliable audio streaming to the Server Team via USB Serial or Wi-Fi WebSocket
4. Visual status feedback via the onboard RGB LED

You are the **first link in the chain**. Clean audio in = good transcription out.

---

## 🏗️ Where You Fit

```
[INMP441 Mic] → [I2S DMA] → [Core 0: DC Filter + Gain] → [FIFO Ring Buffer (3s)]
                                                                     │
                                             [Acoustic Gate + TENet KWS INT8]
                                                                     │ wake word
                                                        [Core 1: Stream Manager]
                                                                     │
                         USB Serial @ 921,600 baud ── OR ── Wi-Fi WebSocket ws://ip:8080
                                                                     │
                                                         [SERVER TEAM TAKES OVER]
```

---

## 📐 Hardware Specification

### Board: ESP32-S3 Dev Module
- **CPU**: Xtensa LX7 Dual Core @ 240 MHz
- **Flash**: 8MB or 16MB
- **PSRAM**: 8MB OPI
- **RAM**: 512KB internal SRAM
- **Wi-Fi**: 2.4 GHz only (5 GHz NOT supported)

### Microphone: INMP441 MEMS I2S

| INMP441 Pin | ESP32-S3 GPIO | Function | Notes |
|---|---|---|---|
| VDD | 3.3V | Power | **Never connect to 5V** |
| GND | GND | Ground | Common ground rail |
| **L/R** | **GND** | Channel select | **Must be tied firmly to GND** |
| WS | GPIO 4 | Word Select (LRCLK) | I2S word select clock |
| SCK | GPIO 5 | Serial Clock (BCLK) | Continuous bit clock |
| SD | GPIO 7 | Serial Data | Audio output from mic |
| — | GPIO 48 | WS2812 RGB LED | Status indicator |
| — | GPIO 0 | BOOT button | Manual trigger fallback |
| — | GPIO 2 | Simple LED | Fallback status (no RGB) |

### LED Status Design
| LED State | Meaning |
|---|---|
| Constant RED | Idle — waiting for speech |
| Solid GREEN | Voice energy detected |
| 4× RED flash | Wake word "Ankit" triggered |
| Solid GREEN | Actively streaming audio to server |

---

## 🏛️ Firmware Architecture Design

Build your firmware with this structure:

### Core 0 — Audio Capture Task (highest priority)
```
Loop:
  i2s_read(512 samples at a time)
  → Apply 40Hz IIR DC high-pass filter on raw 32-bit sample
  → Scale to int16 (shift right by 11)
  → Write to True FIFO ring buffer (mutex-protected)
  → Calculate chunk peak energy
  → Run Acoustic Energy Gate
  → If voice energy: run TENet KWS inference
  → Update RGB LED based on state
```

### Core 1 — Stream Manager Task
```
Loop:
  → Check trigger queue
  → On trigger: send {"event":"start"} to server
  → Set read pointer to LOOKBACK position in ring buffer
  → Stream FIFO audio chunks until stop signal or timeout
  → Send telemetry JSON every 3 seconds
  → Handle "stop" signal from server (non-blocking)
```

### Ring Buffer Design
- **Size**: 3 seconds × 16,000 samples = 48,000 `int16_t` values = 96 KB
- **Write index**: advanced by Core 0 only
- **Read index**: advanced by Core 1 only
- **Mutex**: FreeRTOS `SemaphoreHandle_t` — always take before accessing either index
- **Lookback**: on trigger, set read pointer 500ms behind current write pointer

### Audio Packet Framing (The Protocol Contract)
Every audio chunk sent to the server MUST use this exact format:
```
[0xAA][0x55][len_hi][len_lo][...int16_t PCM bytes, little-endian...]
```
- `0xAA 0x55` = magic header for server re-sync
- `len = number of PCM bytes` (not samples), big-endian uint16
- Typical chunk: 512 samples → 1024 bytes
- This format is the **contract with the Server Team — never change it unilaterally**

### JSON Events Format
```cpp
// On wake word detection (before streaming starts):
Serial.println("{\"event\":\"start\"}");

// Telemetry (every 3 seconds):
Serial.printf("{\"event\":\"telemetry\",\"free_heap\":%u,\"cpu_percent\":%d,\"mic_peak\":%d,\"uptime_ms\":%u}\n",
              ESP.getFreeHeap(), cpuPercent, micPeak, millis());

// Listen for stop from server (non-blocking byte parser):
// Server sends: {"event":"stop"}\n
```

---

## ⚠️ Critical Design Requirements (Do Not Skip These)

These are **architecture-level requirements** — get them wrong and the system breaks:

### 1. True FIFO Ring Buffer (Not a Lookback)
Use **separate write and read indices** protected by a mutex. Core 0 writes, Core 1 reads. Every sample passes through exactly once.
> ❌ Do NOT use a single index + `delay()` to simulate a ring buffer — it causes sample duplication every chunk which makes Whisper output garbage.

### 2. DC High-Pass Filter BEFORE Gain Scaling
The INMP441 has a hardware DC bias. Apply the IIR filter on the raw **32-bit** I2S sample first, then shift:
```
// Correct order:
dcTracker += (rawSample32 - dcTracker) >> 6;   // 40Hz IIR
int32_t acSample = rawSample32 - dcTracker;    // remove DC
int16_t scaled = (int16_t)clamp(acSample >> 11, -32768, 32767); // gain
```
> ❌ Do NOT apply gain first then filter — the DC offset causes the signal to saturate/clip after scaling.

### 3. Non-Blocking Serial Stop Signal Reader
Read the stop signal byte-by-byte using `Serial.available()` inside the streaming loop:
```cpp
while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') { check_if_buffer_contains_stop(); }
    else { append_to_cmd_buffer(c); }
}
```
> ❌ Do NOT use `Serial.readStringUntil('\n')` — it blocks for 1000ms if no newline arrives, stalling audio.

### 4. Baud Rate Must Be 921,600
16kHz × 16-bit = 32,000 bytes/second of audio. The serial port must handle 3× headroom minimum.
> ❌ 115,200 baud = ~11.5KB/s capacity — causes buffer overflow and frame drops.

### 5. SD Pin Pull-Down
Configure an internal pull-down resistor on the I2S SD pin (GPIO 7):
```cpp
gpio_set_pull_mode((gpio_num_t)I2S_SD_PIN, GPIO_PULLDOWN_ONLY);
```
> ❌ A floating SD line reads `0xFFFFFFFF` during silence causing false triggers.

### 6. VAD Timeout Fallbacks
Implement a hard safety cutoff (e.g. 12s) for streaming even if no stop signal arrives. The server should send stop first, but the device must not stream forever if the connection drops.

### 7. Sustained Energy Gate
Require **2 consecutive** audio chunks above the energy threshold before triggering. Single-sample clicks and pops must not trigger the wake word pipeline.

---

## 📋 Task List (Build From Scratch)

### Phase 1 — Foundation (Week 1)
- [ ] Set up Arduino IDE with ESP32-S3 board support
- [ ] Wire INMP441 to ESP32-S3 per pinout table
- [ ] Write basic I2S read loop — confirm audio data in Serial Monitor
- [ ] Implement True FIFO ring buffer with FreeRTOS mutex
- [ ] Implement DC high-pass filter + gain scaling
- [ ] Confirm mic reads sensible values (silence < 200, speech 1500–10000)
- [ ] Push skeleton firmware to `feature/audio-capture` branch

### Phase 2 — Streaming & KWS (Week 2)
- [ ] Implement packet framing: `[0xAA][0x55][len_hi][len_lo][PCM...]`
- [ ] Implement USB Serial streaming at 921,600 baud
- [ ] Implement non-blocking serial stop signal reader
- [ ] Implement acoustic energy gate (sustained 2-chunk threshold)
- [ ] Integrate TENet KWS model (from ML Team as `kws_model_data.h`)
- [ ] Implement trigger queue + state machine (IDLE ↔ STREAMING)
- [ ] Implement RGB LED status (red/green/flash)
- [ ] Implement telemetry JSON (every 3s)
- [ ] End-to-end test: USB Serial → Server → Transcript
- [ ] Push to `feature/streaming-kws` branch

### Phase 3 — Wi-Fi & Polish (Week 3)
- [ ] Implement Wi-Fi WebSocket mode (`ws://ip:8080/stream`)
- [ ] Test Wi-Fi at 1m, 3m, 5m from router
- [ ] Tune energy threshold per demo room acoustics
- [ ] End-to-end Wi-Fi integration test with Server Team
- [ ] Final tag `v1.0` on `main`

---

## 🛠️ Development Environment Setup

### Arduino IDE Configuration (Tools menu)
| Setting | Value |
|---|---|
| Board | `ESP32S3 Dev Module` |
| USB CDC On Boot | `Enabled` |
| CPU Frequency | `240MHz (WiFi)` |
| Flash Size | `16MB (128Mb)` or `8MB (64Mb)` |
| Partition Scheme | `Huge APP (3MB No OTA/1MB SPIFFS)` |
| PSRAM | `OPI PSRAM` or `Disabled` (if boot issues) |
| Upload Speed | `921600` |
| Port | Your COM port |

### Install Board Support
In Arduino IDE → File → Preferences, add:
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```
Then Tools → Board → Boards Manager → search `esp32` by Espressif → install 2.0.14 or 3.x.

### Required Libraries (Tools → Manage Libraries)
| Library | Version | Purpose |
|---|---|---|
| `tflm_esp32` | 2.0.0 | TensorFlow Lite Micro for ESP32-S3 |
| `arduinoFFT` | 2.0.4 | FFT for MFCC feature extraction |
| `WebSockets` | 2.7.2 | Wi-Fi WebSocket client |
| `ArduinoJson` | 6.x / 7.x | JSON telemetry serialization |

> ⚠️ If you have multiple TFLite libraries installed, keep only `tflm_esp32` and remove others to avoid compilation conflicts.

---

## 🔗 Git Workflow

### Repository
```
https://github.com/QuantumSIHCracker/sih-hardware-firmware
```

### Initial Setup (run once)
```bash
git clone https://github.com/QuantumSIHCracker/sih-hardware-firmware.git
cd sih-hardware-firmware
git config user.name "Your Name"
git config user.email "your@email.com"
git checkout dev  # always start from dev
```

### Daily Workflow
```bash
git checkout dev && git pull origin dev
git checkout -b feature/<what-you-are-building>

# ... write code ...

git add .
git commit -m "feat(firmware): describe what you built"
git push origin feature/<name>

# Open Pull Request on GitHub: feature/<name> → dev
# After review → merge to dev
# After full integration test → dev → main
```

### Commit Message Format
```
feat(firmware): add FIFO ring buffer with FreeRTOS mutex
feat(kws): integrate TENet INT8 model for wake word detection
feat(transport): add Wi-Fi WebSocket streaming mode
fix(audio): correct DC filter order before gain scaling
fix(serial): replace blocking readStringUntil with byte parser
test(hardware): verify INMP441 at 1m 3m distance
docs(pinout): update GPIO table for ESP32-S3 DevKit v1.1
```

### Rules
- **NEVER push directly to `main`**
- Always compile and test before pushing
- If changing the audio packet format → open issue tagged `protocol-change` first

---

## 🤝 Integration Points

### → Server Team (what you deliver)
1. **Audio stream**: binary packets `[0xAA][0x55][len_hi][len_lo][PCM int16 LE 16kHz]`
2. **Start event**: `{"event":"start"}\n` when wake word fires
3. **Telemetry**: `{"event":"telemetry","free_heap":N,"cpu_percent":N,"mic_peak":N,"uptime_ms":N}\n` every 3s
4. **Stop listener**: receives `{"event":"stop"}\n` from server → stops streaming

### ← ML Team (what they deliver to you)
1. `model.tflite` — quantized INT8 TFLite KWS model (< 60KB)
2. Model spec: input shape `[1, 51, 1, 10]`, output `[1, 5]`, class index for wake word
3. Recommended score threshold for triggering

**Convert tflite to C header** (run once when ML delivers a new model):
```python
# Run: python3 tflite_to_header.py model.tflite
import sys
with open(sys.argv[1], "rb") as f:
    data = f.read()
with open("kws_model_data.h", "w") as f:
    f.write(f"const unsigned int g_kws_model_data_len = {len(data)};\n")
    f.write("const unsigned char g_kws_model_data[] = {\n  ")
    rows = [", ".join(f"0x{b:02x}" for b in data[i:i+12]) for i in range(0, len(data), 12)]
    f.write(",\n  ".join(rows))
    f.write("\n};\n")
print(f"Generated: kws_model_data.h ({len(data)} bytes)")
```

---

## 🤖 AI Prompt — Start Your Work

Copy this into your AI assistant when beginning any firmware task:

```
You are an expert embedded systems engineer specializing in:
- ESP32-S3 firmware (Arduino IDE, FreeRTOS, Xtensa LX7)
- I2S audio capture (INMP441 MEMS microphone)
- TensorFlow Lite Micro (TFLM) keyword spotting on microcontrollers
- Real-time audio DSP (IIR filters, ring buffers, energy detection)

PROJECT: Smart India Hackathon 2026 — Edge-to-Cloud Voice Assistant
HARDWARE: ESP32-S3 Dev Module + INMP441 MEMS mic (GPIO 4/5/7)
WAKE WORD: "Ankit" (detected on-device via TENet INT8 TFLite model)
TRANSPORT: USB Serial @ 921,600 baud (demo) OR Wi-Fi WebSocket ws://ip:8080/stream (deploy)

AUDIO PIPELINE TO BUILD:
- I2S DMA read at 16kHz (32-bit raw → int16 after DC filter + >>11 gain)
- True FIFO ring buffer (3s, Core 0 writes, Core 1 reads, FreeRTOS mutex)
- Acoustic energy gate (2 consecutive chunks > threshold → KWS inference)
- TENet INT8 TFLite inference for "Ankit" detection
- Packet framing: [0xAA][0x55][len_hi][len_lo][int16 PCM LE...]

CRITICAL DESIGN RULES:
1. True FIFO ring buffer — separate read/write indices, never lookback + delay
2. DC filter on raw 32-bit sample BEFORE >>11 gain scaling
3. Non-blocking serial reader (Serial.available() loop, never readStringUntil)
4. 921,600 baud serial — lower rates cause buffer overflow
5. Pull-down on GPIO 7 (I2S SD pin) — prevents floating line artifacts
6. Sustained energy gate — 2 chunks minimum before trigger

My current task: [DESCRIBE WHAT YOU WANT TO BUILD]

Provide production-quality, well-commented Arduino/C++ code.
```

---

## 📞 Communication Protocol

- **Daily standup**: `[HW] Done: X | Doing: Y | Blocked: Z`
- **Protocol changes**: Open GitHub issue tagged `protocol-change` BEFORE changing anything
- **Model update from ML**: They open an issue on your repo tagged `model-update`
- **Server integration sync**: Every 2 days — verify packet format and events are working
