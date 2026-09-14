# 🔩 Hardware Team — Complete Working Plan
### Smart India Hackathon (SIH) 2026 | Quantum-SIH-Cracker | Team: Hardware Firmware

> **Team Lead**: Arpit Kumar (working on WSL Ubuntu: `/home/arpit_ubuntu`)
> **Repository**: `https://github.com/Quantum-SIH-Cracker/sih-hardware-firmware`
> **Branch Strategy**: `main` (protected) → `dev` → `feature/<name>` branches

---

## 📌 Your Mission

You are the **Hardware & Firmware Team**. Your job is to:
1. Capture clean audio from the **INMP441 MEMS microphone** via I2S on **ESP32-S3**
2. Run **on-device Keyword Spotting (KWS)** to detect the wake word ("Ankit") locally on chip — NO cloud needed for detection
3. Stream the buffered audio to the Server Team over USB Serial or Wi-Fi WebSocket
4. Keep the device reliable, low-power, and demo-ready at all times

You are the **first link in the chain**. If audio is corrupted here, nothing downstream (ML or Server) can fix it.

---

## 🏗️ System Context — Where You Fit

```
[INMP441 Mic] → [I2S DMA] → [Core 0: DC Filter + Gain] → [FIFO Ring Buffer (3s)]
                                                                    │
                                              [Acoustic Gatekeeper + TENet KWS]
                                                                    │ (wake word detected)
                                                          [Core 1: Stream Manager]
                                                                    │
                          USB Serial 921600 baud ── OR ── Wi-Fi WebSocket ws://ip:8080/stream
                                                                    │
                                                          [SERVER TEAM TAKES OVER]
```

---

## 📐 Hardware Reference

### Board
- **ESP32-S3 Dev Module** (Xtensa LX7 Dual Core @ 240 MHz, 512KB SRAM, 16MB Flash, 8MB PSRAM)
- All production code must be Arduino IDE compatible (Arduino ESP32 Core 2.0.14 or 3.x)

### Microphone: INMP441 MEMS I2S

| INMP441 Pin | ESP32-S3 GPIO | Function |
|---|---|---|
| VDD | 3.3V | Power — **Never connect to 5V** |
| GND | GND | Common ground |
| **L/R** | **GND** | Channel Select — **Must be tied to GND** (selects Left I2S channel) |
| WS | GPIO 4 | Word Select (LRCLK) |
| SCK | GPIO 5 | Serial Clock (BCLK) |
| SD | GPIO 7 | Serial Data Output |
| (onboard) | GPIO 48 | WS2812 RGB LED Status |
| (onboard) | GPIO 0 | BOOT Button (manual trigger) |
| (onboard) | GPIO 2 | Simple status LED (fallback) |

> ⚠️ **CRITICAL**: The L/R pin must be firmly soldered/connected to GND. A floating L/R pin causes audio channel selection failure.
> ⚠️ **CRITICAL**: The firmware configures an internal pull-down on GPIO 7. A floating SD line reads `0xFFFFFFFF`.

### Circuit Diagram
Reference file: `/home/arpit_ubuntu/SIH_Voice_Assistant/circuit_diagram_A4.pdf`

---

## ⚙️ Current Firmware State (DO NOT BREAK THESE)

The firmware at `/home/arpit_ubuntu/SIH_Voice_Assistant_Handoff/esp32_sih_hard_ware_firmware.ino` is **fully working and battle-tested**. Critical implementations:

### 1. True FIFO Ring Buffer
- `ringBuffer[RING_BUFFER_SAMPLES]` with separate `ringWriteIdx` (Core 0) and `ringReadIdx` (Core 1)
- Protected by `ringMutex` (FreeRTOS semaphore)
- Size: `16000 samples/sec × 3 sec = 48,000 int16_t samples = 96KB`
- **NEVER** go back to `ringBufferGetLookback()` with `delay(20)` — that caused 192 sample duplications per 20ms

### 2. Pre-Scale DC High-Pass Filter (Core 0)
```cpp
dcTracker += (rawSample - dcTracker) >> 6;  // 40Hz cutoff IIR
int32_t acSample = rawSample - dcTracker;   // Remove DC offset
int32_t scaled = acSample >> 11;             // Gain scaling (INMP441 24-bit in bits [31:8])
```
- This MUST happen on the raw 32-bit I2S sample BEFORE the >>11 shift
- Normal speech: 1,500–10,000 counts | Silence: <150 counts

### 3. Sustained Energy Gate (Anti-false-trigger)
```cpp
if (peak >= SPEECH_ENERGY_THRESHOLD) {  // 850 default
    sustainedEnergyCount++;
    if (sustainedEnergyCount >= 2) { fireWakeWordTrigger(); }
} else {
    sustainedEnergyCount = 0;
}
```
- Requires 2 consecutive chunks above threshold to trigger (prevents clicks)

### 4. Non-Blocking Serial Command Reader
```cpp
while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
        // process rxCmdBuf
    } else { rxCmdBuf[rxCmdIdx++] = c; }
}
```
- **NEVER** use `Serial.readStringUntil('\n')` — it blocks for 1000ms timeout

### 5. Audio Packet Framing Protocol
```
[0xAA][0x55][len_hi][len_lo][...PCM int16_t bytes (little-endian)...]
```
- Magic bytes `0xAA 0x55` allow server to re-sync if bytes are lost
- `len_hi:len_lo` = number of bytes of PCM data (not samples)
- Chunk size: 512 samples = 1024 bytes per packet
- **This protocol is the contract with the Server Team. Do NOT change without coordinating.**

---

## 📋 Your Task List (Priority Order)

### Phase 1 — Immediate (Week 1)
- [ ] **Clone the repo** and verify existing firmware compiles and runs
- [ ] **Verify audio pipeline**: Use Serial Monitor @ 921600 baud to confirm telemetry JSON output
- [ ] **Test mic detection**: Speak — confirm mic_peak shows values 1500–10000 in telemetry
- [ ] **Test USB Serial streaming**: Run server.py and confirm transcription works end-to-end
- [ ] **LED verification**: Confirm RED (idle), GREEN (speech detected), 4x RED flash (wake word)
- [ ] **Push working firmware to `feature/firmware-baseline` branch**

### Phase 2 — Enhancement (Week 2)
- [ ] **Integrate upgraded KWS model from ML Team**
  - ML team will provide a new `.tflite` file
  - Convert to C header: `xxd -i model.tflite > kws_model_data.h` (or use Python script below)
  - Update `INPUT_TENSOR_SIZE` and class labels if changed
- [ ] **Tune Acoustic Gatekeeper thresholds** based on demo room acoustics:
  - `SPEECH_ENERGY_THRESHOLD`: increase if false triggers (default: 850)
  - `VOICE_BAND_ENERGY_THRESHOLD`: adjust per room noise floor
- [ ] **Test Wi-Fi WebSocket mode** (`ENABLE_NETWORK_STREAM = 1`)
  - Coordinate with Server Team for server IP and port
  - Test at various distances (1m, 3m, 5m from router)
- [ ] **Power optimization**: Measure and document current draw in idle vs streaming states

### Phase 3 — Integration & Demo Prep (Week 3)
- [ ] **End-to-end integration test** with Server Team — full pipeline on same LAN
- [ ] **Demo harness**: Build a reliable enclosure/mount for the ESP32-S3 + INMP441
- [ ] **Failsafe testing**: Verify 12s safety timeout, cooldown (3s between triggers), button trigger
- [ ] **Final firmware v1.0 tag** on `main` branch

---

## 🛠️ Development Environment Setup (This Machine — WSL Ubuntu)

### Arduino IDE Setup
1. Install Arduino IDE 2.x on Windows (works with WSL via USB passthrough)
2. Add ESP32 board manager URL:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Install **esp32 by Espressif Systems** (version 2.0.14 or 3.x)

### Required Libraries (install via Tools → Manage Libraries)
| Library | Version | Purpose |
|---|---|---|
| `tflm_esp32` | 2.0.0 | TensorFlow Lite Micro for ESP32-S3 |
| `arduinoFFT` | 2.0.4 | FFT for MFCC spectrogram |
| `WebSockets` | 2.7.2 | Wi-Fi WebSocket client (for network mode) |
| `ArduinoJson` | 6.x or 7.x | JSON telemetry serialization |

### Arduino IDE Board Settings (Tools menu)
| Setting | Value |
|---|---|
| Board | `ESP32S3 Dev Module` |
| USB CDC On Boot | `Enabled` |
| CPU Frequency | `240MHz (WiFi)` |
| Flash Size | `16MB (128Mb)` or `8MB (64Mb)` |
| Partition Scheme | `Huge APP (3MB No OTA/1MB SPIFFS)` |
| PSRAM | `OPI PSRAM` |
| Upload Speed | `921600` |
| Port | Your COM port (e.g. COM7) |

> ⚠️ **PSRAM Note**: Keep PSRAM `Disabled` in Board settings if you get bootloader halts. The firmware falls back to internal SRAM gracefully.

---

## 🔗 Git Workflow (Hardware Team)

### Repository
```
https://github.com/Quantum-SIH-Cracker/sih-hardware-firmware
```

### Initial Setup (run once on your machine)
```bash
git clone https://github.com/Quantum-SIH-Cracker/sih-hardware-firmware.git
cd sih-hardware-firmware
git config user.name "Your Name"
git config user.email "your-email@example.com"
```

### Daily Workflow
```bash
# Start new work
git checkout dev
git pull origin dev
git checkout -b feature/<your-feature-name>

# After making changes
git add .
git commit -m "feat(firmware): <describe what you changed>"
git push origin feature/<your-feature-name>

# Then open a Pull Request on GitHub:
#   feature/<name> → dev
# After review and test, dev → main (protected, requires passing tests)
```

### Commit Message Convention
```
feat(firmware): add Wi-Fi WebSocket reconnection logic
fix(audio): correct DC filter coefficient for 40Hz cutoff
test(integration): verify USB serial at 921600 baud
docs(hardware): update GPIO pinout table
```

### ⚠️ Rules
- **NEVER push directly to `main`**
- Always test firmware compiles before pushing
- Tag release versions: `git tag -a v1.0 -m "SIH Demo Build v1.0"`

---

## 🤝 Integration Points with Other Teams

### → Server Team (your outputs)
You send them:
1. **Audio packets** over USB Serial or Wi-Fi WebSocket using the framing protocol:
   ```
   [0xAA][0x55][len_hi][len_lo][...int16_t PCM samples (16kHz, mono, little-endian)...]
   ```
2. **JSON telemetry** every 3 seconds:
   ```json
   {"event":"telemetry","free_heap":194560,"cpu_percent":3,"mic_peak":2400,"uptime_ms":45000}
   ```
3. **Start event** when wake word detected:
   ```json
   {"event":"start"}
   ```
4. **Listen for stop signal** from server:
   - USB Serial: `{"event":"stop"}\n`
   - WebSocket: text message containing `"stop"`

### ← ML Team (their outputs you consume)
They give you:
1. **New `.tflite` file** with improved KWS model
2. **Model input specs**: tensor shape `[1, N_FRAMES, 1, N_MFCC_BINS]`, data type (INT8)
3. **Class labels** and which index is the wake word
4. **Recommended threshold** for the wake word score

**Conversion script** (to embed model in firmware):
```python
# Run on your machine to convert .tflite → C header
import sys

tflite_path = sys.argv[1]  # e.g. "new_model.tflite"
with open(tflite_path, "rb") as f:
    data = f.read()

with open("kws_model_data.h", "w") as f:
    f.write("// Auto-generated from: " + tflite_path + "\n")
    f.write(f"const unsigned int g_kws_model_data_len = {len(data)};\n")
    f.write("const unsigned char g_kws_model_data[] = {\n  ")
    hex_bytes = [f"0x{b:02x}" for b in data]
    f.write(",\n  ".join([", ".join(hex_bytes[i:i+12]) for i in range(0, len(hex_bytes), 12)]))
    f.write("\n};\n")
print(f"Generated kws_model_data.h ({len(data)} bytes)")
```

---

## 🤖 AI Prompt to Start Your Work

Copy this prompt into your AI assistant to begin:

```
You are an expert embedded systems engineer specializing in ESP32-S3 firmware development, FreeRTOS real-time systems, I2S audio capture, and TensorFlow Lite Micro (TFLM) for keyword spotting.

PROJECT CONTEXT:
We are building a Smart India Hackathon (SIH) voice assistant with an ESP32-S3 microcontroller and an INMP441 MEMS I2S microphone. The system captures audio at 16kHz, runs on-device keyword spotting using a TENet INT8 TFLite model to detect the wake word "Ankit", then streams buffered audio to a FastAPI server via USB Serial (921600 baud) or Wi-Fi WebSocket.

MY ROLE: Hardware & Firmware Team Lead

CURRENT FIRMWARE STATE:
- True FIFO ring buffer (3s, 48000 samples) with FreeRTOS mutex
- Dual-core partitioning: Core 0 (I2S DMA + DC filter) / Core 1 (stream manager)
- Pre-scale 40Hz IIR DC high-pass filter before gain scaling (>>11)
- Sustained energy gate (2 consecutive blocks > 850 threshold to trigger)
- Non-blocking serial command reader
- Binary packet framing: [0xAA, 0x55, len_hi, len_lo, ...PCM int16_t...]
- Dual transport: USB Serial @ 921600 baud / Wi-Fi WebSocket ws://ip:8080/stream
- WS2812 RGB LED status (GPIO 48): RED=idle, GREEN=voice, 4xRED flash=wake word

CRITICAL BUGS ALREADY SOLVED (don't regress):
1. Sample duplication (was reading 512 samples every 20ms, only 320 new → fixed with true FIFO)
2. DC clipping (was >>14 after bias → fixed with pre-scale filter then >>11)
3. 1000ms serial hang (was readStringUntil → fixed with non-blocking byte parser)
4. VAD hang (15s stream → fixed with 1.2s silence + 4s no-speech + 7s max cap)
5. Baud rate overflow (115200 → 921600)

My current task: [DESCRIBE WHAT YOU WANT TO DO]

Please help me implement/debug/optimize this while preserving all existing functionality.
```

---

## 📁 File Reference

| File | Location | Purpose |
|---|---|---|
| Main Firmware | `firmware/esp32_sih_hard_ware_firmware.ino` | Primary ESP32-S3 code |
| KWS Model Header | `firmware/kws_model_data.h` | Embedded TFLite model bytes |
| Circuit Diagram PDF | `docs/circuit_diagram_A4.pdf` | Printable A4 schematic |
| Circuit Diagram PNG | `docs/circuit_diagram_A4.png` | High-res wiring reference |

---

## 📞 Team Communication Protocol

- **Daily standup**: Post progress in team group with format: `[HW] Done: X | Doing: Y | Blocked: Z`
- **Integration sync**: Every 2 days with Server Team to verify protocol compliance
- **Critical issues**: Tag `@server-team` or `@ml-team` in GitHub issue immediately
- **Protocol changes**: MUST be agreed by ALL teams before implementation — create a GitHub issue tagged `protocol-change`
