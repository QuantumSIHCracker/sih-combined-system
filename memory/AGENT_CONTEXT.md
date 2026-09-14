# 🧠 Antigravity Agent Memory — SIH Quantum Cracker Project
> Last Updated: 2026-09-14 | Model: Claude Sonnet 4.6 (Thinking)
> This file is my permanent context store. Read this FIRST on every session.

---

## 📌 Project Identity

- **Hackathon**: Smart India Hackathon (SIH) 2026
- **Problem Statement**: #26172
- **GitHub Org**: `Quantum-SIH-Cracker`
- **User (Hardware Lead)**: Arpit Kumar — working on this WSL Ubuntu machine (`/home/arpit_ubuntu`)
- **Machine**: WSL2 Ubuntu on Windows (path: `\\wsl.localhost\Ubuntu\home\arpit_ubuntu`)
- **Workflow Root**: `/home/arpit_ubuntu/New WorkFlow/SIH-Quantum-Cracker/`

---

## 🎯 Project Summary

An **Edge-to-Cloud Voice Assistant** for Smart India Hackathon:
- **ESP32-S3** microcontroller with **INMP441 I2S MEMS microphone**
- **On-device Keyword Spotting (KWS)** using INT8 TFLite TENet model detecting wake word **"Ankit"**
- Audio streamed via **USB Serial (921600 baud)** or **Wi-Fi WebSocket** to a local **FastAPI server**
- Server runs **Silero VAD** + **Faster-Whisper (base.en, int8)** for speech-to-text
- Real-time **SSE web dashboard** at `http://localhost:8080/dashboard`

---

## 🏗️ System Architecture (Canonical)

```
INMP441 (I2S, 16kHz, 24-bit)
    → ESP32-S3 Core 0: DC High-Pass Filter → Gain Scaling (>>11) → True FIFO Ring Buffer (3s)
    → ESP32-S3 Core 1: Acoustic Gatekeeper → TENet KWS (INT8 TFLite) → Trigger Queue
                                           → Stream Manager → Packet Framer [0xAA 0x55 len_hi len_lo PCM...]

Transport: USB Serial @ 921,600 baud (demo) OR Wi-Fi WebSocket ws://ip:8080/stream (deployment)

FastAPI Server:
    → Serial/WS Listener → Session Manager
    → Silero VAD (1.2s silence detect) → Faster-Whisper (base.en, int8)
    → SSE Broadcast → Web Dashboard (port 8080)
    → Stats Logger (esp32_stats.jsonl)
```

---

## 📐 Hardware Specs (Confirmed Working)

| Component | Detail |
|---|---|
| MCU | ESP32-S3 Dev Module (Xtensa LX7 Dual Core @ 240 MHz) |
| Mic | INMP441 MEMS I2S Omnidirectional |
| Flash | 16MB (N16R8) |
| PSRAM | 8MB OPI |
| Audio | 16kHz, 16-bit PCM (after conversion from 32-bit I2S) |
| Sample Rate | 16,000 Hz |
| Baud Rate | 921,600 (USB Serial transport) |
| Ring Buffer | 3 seconds = 48,000 samples = 96,000 bytes |
| KWS Model | TENet INT8 TFLite, 57,264 bytes, input [1,51,1,10] INT8 |
| Wake Word | "Ankit" (class index 0 in 5-class softmax) |

### GPIO Pinout (Confirmed)
| Signal | GPIO |
|---|---|
| I2S WS (LRCLK) | GPIO 4 |
| I2S SCK (BCLK) | GPIO 5 |
| I2S SD (Data) | GPIO 7 |
| BOOT Button (Trigger) | GPIO 0 |
| Status LED (fallback) | GPIO 2 |
| WS2812 RGB LED | GPIO 48 |

---

## 🐛 Critical Bugs Already Solved (DO NOT REGRESS)

1. **Audio Sample Duplication**: Fixed with True FIFO ring buffer (separate write/read indices for Core 0/Core 1)
2. **DC Clipping**: Fixed with 40Hz pre-scale high-pass filter BEFORE >>11 gain scaling
3. **1000ms Serial Hang**: Fixed with non-blocking byte parser (no `readStringUntil`)
4. **VAD Auto-Finalization Hang**: Fixed with 1.2s silence detect + 4.0s no-speech + 7.0s max utterance
5. **Baud Rate Buffer Overrun**: Fixed by upgrading from 115200 to 921600 baud

---

## 📊 Benchmarks (Measured, Passing)

| Metric | Target | Measured |
|---|---|---|
| ESP32 Idle CPU | <10% | 3% |
| ESP32 RAM Usage | <256KB | 62KB used / 194KB free |
| Whisper Latency | Fast | 1.1s–1.3s |
| End-to-End Latency | Conversational | ~4.5s–5.3s |

---

## 📦 Existing Codebase Locations on this Machine

| Path | Contents |
|---|---|
| `/home/arpit_ubuntu/SIH_Voice_Assistant_Handoff/` | **Master handoff package** — firmware + server + models |
| `/home/arpit_ubuntu/SIH-ESP32-S3-Voice-Assistant/` | Git repo (arpitkumar81008-cmd) with docs + firmware + server |
| `/home/arpit_ubuntu/Smart India hackathon/` | Working directory — server.py (823 lines), dataset, screenshots |
| `/home/arpit_ubuntu/SIH_Voice_Assistant/` | Circuit diagrams, hardware diagnostics, models |
| `/home/arpit_ubuntu/New WorkFlow/SIH-Quantum-Cracker/` | **THIS PROJECT'S WORKFLOW ROOT** |

---

## 🗂️ GitHub Repositories (Quantum-SIH-Cracker org)

| Repo | Team | Branch Strategy |
|---|---|---|
| `Quantum-SIH-Cracker/sih-hardware-firmware` | Hardware | main, dev, feature/* |
| `Quantum-SIH-Cracker/sih-ml-models` | ML | main, dev, experiments/* |
| `Quantum-SIH-Cracker/sih-server-backend` | Server | main, dev, feature/* |
| `Quantum-SIH-Cracker/sih-combined-system` | All Teams | main, integration, team/* |

---

## 👥 Team Structure

| Team | Lead | Machine | Task |
|---|---|---|---|
| **Hardware** | Arpit Kumar | This machine (WSL Ubuntu) | ESP32-S3 firmware, mic integration, KWS embedding, circuit design |
| **ML** | ML Team Lead | Separate computer | KWS model training, dataset curation, MFCC feature extraction, TFLite conversion |
| **Server** | Server Team Lead | Separate computer | FastAPI server, VAD, Whisper, dashboard, API design |

---

## 🔑 Key Technical Decisions

- **Wake Word**: "Ankit" (changeable — coordinate with ML team on new word if needed)
- **Transport Protocol**: Magic framing `[0xAA, 0x55, len_hi, len_lo, ...PCM bytes...]`
- **Audio Format**: 16kHz, 16-bit signed PCM, mono, little-endian
- **Server Port**: 8080
- **VAD**: Silero (ONNX), threshold 0.35, 1.2s silence = end of utterance
- **ASR**: Faster-Whisper base.en, int8 quantization, CPU inference
- **Dashboard**: FastAPI + Server-Sent Events (SSE), no WebSocket from browser

---

## 📋 Active Tasks & Status

| Task | Team | Status |
|---|---|---|
| Generate team plans | Agent | ✅ DONE (2026-09-14) |
| Create GitHub repos | Manual (User) | ⏳ PENDING |
| Push existing code to repos | Hardware | ⏳ PENDING |
| Upgrade KWS model (larger vocab, better accuracy) | ML | ⏳ PENDING |
| Implement response actions on wake word | Server | ⏳ PENDING |
| Wi-Fi WebSocket testing end-to-end | All | ⏳ PENDING |
| Final demo preparation | All | ⏳ PENDING |

---

## 🔄 Workflow Notes

- Arpit (Hardware Lead) works on this WSL Ubuntu machine
- Other teams work on separate computers and push to GitHub
- All teams use branches (never push directly to `main`)
- PR merges happen after testing — GitHub Actions will be set up for CI
- This memory file should be updated after every significant decision/change

---

## 💡 Agent Instructions for Future Sessions

1. **Always read this file first** before starting any task
2. Check `/home/arpit_ubuntu/New WorkFlow/SIH-Quantum-Cracker/` for latest plans
3. The combined system repo is the source of truth for integration
4. When Arpit is on hardware: focus on ESP32 Arduino IDE / C++ code
5. When coordinating with other teams: reference the protocol spec in `combined/PROTOCOL_SPEC.md`
6. Never break the audio framing protocol — it's the critical integration point
