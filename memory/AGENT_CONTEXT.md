# 🧠 Antigravity Agent Memory — SIH Quantum Cracker Project
> Last Updated: 2026-09-14 | Model: Claude Sonnet 4.6 (Thinking)
> **READ THIS FIRST on every new session before doing anything.**

---

## 📌 Project Identity

- **Hackathon**: Smart India Hackathon (SIH) 2026
- **Problem Statement**: #26172
- **GitHub Org**: `QuantumSIHCracker` → https://github.com/QuantumSIHCracker
- **Hardware Lead (User)**: Arpit Kumar — WSL Ubuntu machine at `/home/arpit_ubuntu`
- **Workflow Root**: `/home/arpit_ubuntu/New WorkFlow/SIH-Quantum-Cracker/`

---

## ⚠️ Critical Context

**Everything is being built fresh from scratch.** There are old code folders on this machine from a previous failed attempt — **DO NOT reference, use, or suggest code from those folders.** They exist at paths like `SIH_Voice_Assistant_Handoff/`, `Smart India hackathon/`, `SIH-ESP32-S3-Voice-Assistant/` etc. Ignore them entirely.

All plans, all code, all references point ONLY to:
- `/home/arpit_ubuntu/New WorkFlow/SIH-Quantum-Cracker/` (planning docs — this machine)
- `https://github.com/QuantumSIHCracker/sih-hardware-firmware` (hardware team repo)
- `https://github.com/QuantumSIHCracker/sih-ml-models` (ML team repo)
- `https://github.com/QuantumSIHCracker/sih-server-backend` (server team repo)
- `https://github.com/QuantumSIHCracker/sih-combined-system` (combined workflow repo)

---

## 🎯 Project Summary

**Edge-to-Cloud Voice Assistant** for SIH 2026:
- **ESP32-S3** + **INMP441 I2S MEMS microphone** — captures audio at 16kHz
- **On-device Keyword Spotting (KWS)** — TENet INT8 TFLite model, wake word: **"Ankit"**
- **Dual transport**: USB Serial @ 921,600 baud (demo) OR Wi-Fi WebSocket (deployment)
- **FastAPI server** — Silero VAD + Faster-Whisper base.en + SSE dashboard

---

## 🏗️ Architecture

```
INMP441 (I2S, 16kHz, 24-bit raw)
  → ESP32-S3 Core 0: DC High-Pass Filter (40Hz IIR) → Gain Scale (>>11) → True FIFO Ring Buffer (3s)
  → ESP32-S3 Core 0: Acoustic Energy Gate → TENet KWS (INT8 TFLite)
  → ESP32-S3 Core 1: Stream Manager → Packet Framer

Packet format: [0xAA][0x55][len_hi][len_lo][...int16 PCM LE @ 16kHz...]

Transport A: USB Serial @ 921,600 baud  → FastAPI server (serial listener)
Transport B: Wi-Fi WebSocket ws://ip:8080/stream → FastAPI server (WS endpoint)

FastAPI Server:
  → Silero VAD (ONNX) → 1.2s silence = utterance end
  → Faster-Whisper base.en (int8 CPU) → transcript
  → Server-side KWS verify (regex: Ankit)
  → SSE broadcast → Dashboard (http://localhost:8080/dashboard)
```

---

## 📐 Hardware Spec

| Item | Value |
|---|---|
| MCU | ESP32-S3 Dev Module (Xtensa LX7 Dual Core @ 240MHz) |
| Flash | 16MB or 8MB |
| PSRAM | 8MB OPI |
| Mic | INMP441 MEMS I2S Omnidirectional |
| Sample Rate | 16,000 Hz |
| Bit Depth | 16-bit signed PCM (converted from 32-bit I2S) |
| Ring Buffer | 3s = 48,000 int16 samples = 96KB |
| Serial Baud | 921,600 |
| WS Port | 8080 |

### GPIO Pinout
| Signal | GPIO |
|---|---|
| I2S WS (LRCLK) | 4 |
| I2S SCK (BCLK) | 5 |
| I2S SD (Data) | 7 |
| BOOT button trigger | 0 |
| Status LED (fallback) | 2 |
| WS2812 RGB LED | 48 |

---

## 🎯 Design Targets (Build Toward These)

| Metric | Target |
|---|---|
| ESP32 Idle CPU | < 10% |
| ESP32 RAM | < 256 KB |
| Whisper Latency | < 1.5s |
| End-to-End Latency | < 5s |
| KWS Accuracy | > 95% true positive |
| False Positive Rate | < 2% |

---

## 🚨 Known Design Pitfalls (MUST Avoid — architecture requirements)

1. **Ring buffer**: MUST use true FIFO with separate Core0 write index and Core1 read index + mutex. A lookback approach with `delay()` causes sample duplication every chunk.
2. **DC offset**: MUST apply 40Hz IIR high-pass filter on the RAW 32-bit I2S sample BEFORE gain scaling. Doing it after causes saturation/clipping.
3. **Serial reading**: MUST use non-blocking byte-by-byte parser. `Serial.readStringUntil('\n')` blocks for 1000ms timeout and stalls audio.
4. **VAD finalization**: MUST implement multiple fallbacks: silence detect (1.2s) + no-speech timeout (4s) + max utterance cap (7s). Without these the stream hangs.
5. **Baud rate**: MUST use 921,600 baud. 16kHz 16-bit audio = 32KB/s. 115,200 baud = 11.5KB/s capacity — causes buffer overflow.
6. **SD pin**: MUST set GPIO_PULLDOWN on the I2S SD pin. A floating SD line reads 0xFFFFFFFF.
7. **L/R pin**: MUST tie INMP441 L/R pin firmly to GND. Floating causes wrong channel selection.

---

## 🔑 Protocol Contract (Sacred — all teams must follow)

### Hardware → Server (audio)
```
[0xAA][0x55][len_hi][len_lo][...int16_t LE PCM @ 16kHz mono...]
Valid len: 4 to 2048 bytes
```

### Hardware → Server (control, JSON + newline)
```json
{"event":"start"}         // wake word detected, begin session
{"event":"telemetry","free_heap":N,"cpu_percent":N,"mic_peak":N,"uptime_ms":N}
```

### Server → Hardware (stop signal)
```
Serial: {"event":"stop"}\n
WebSocket: {"event":"stop"}  (text frame)
```

---

## 🗂️ KWS Model Spec

| Property | Value |
|---|---|
| Architecture | TENet (Inverted Residual CNN) or DS-CNN |
| Input | `[1, 51, 1, 10]` INT8 |
| Output | `[1, 5]` INT8 |
| Classes | 0=wake_word, 1=local_negative, 2=noise, 3=silence, 4=unknown |
| Wake word | "Ankit" (class 0) |
| Max size | 60 KB after INT8 quantization |
| MFCC | 16kHz, 512-pt FFT, 10 mel bins, 320-sample hop, 51 frames/window |

---

## 👥 Teams

| Team | Machine | Repo |
|---|---|---|
| Hardware (Arpit) | This WSL Ubuntu machine | sih-hardware-firmware |
| ML | Separate computer | sih-ml-models |
| Server | Separate computer | sih-server-backend |

---

## ✅ Setup Status

| Task | Status |
|---|---|
| Workflow docs written | ✅ Done |
| GitHub org created (QuantumSIHCracker) | ✅ Done |
| 4 private repos created | ✅ Done |
| GitHub teams created | ✅ Done |
| sih-combined-system pushed | ✅ Done |
| Hardware team: Wiring & Schematic | ✅ Done (Rev 5.0 Soldered) |
| Hardware team: Baseline Firmware | ✅ Done (in `hardware-team/opus_fixes/`) |
| Team members added to repos | ⏳ Pending (usernames not yet available) |
| Hardware team starts KWS integration | ⏳ Next |
| ML team starts training | ⏳ Next |
| Server team starts building | ⏳ Next |

---

## 📋 Agent Instructions

1. Read this file FIRST every session
2. NEVER reference or suggest old code from `SIH_Voice_Assistant_Handoff/`, `Smart India hackathon/`, or any other old folder
3. All new code goes into the GitHub repos via the branch workflow
4. When Arpit is coding (hardware team): help with ESP32-S3 C++/Arduino firmware
5. For protocol changes: always flag and require ALL teams to agree first
6. Update this file after any significant architectural decision
