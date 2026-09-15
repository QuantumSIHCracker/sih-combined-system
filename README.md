# 🚀 SIH Quantum Cracker — Smart India Hackathon 2026

[![Hardware: ESP32-S3](https://img.shields.io/badge/Hardware-ESP32--S3-red)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Audio: INMP441 MEMS](https://img.shields.io/badge/Audio-INMP441%20I2S-blue)](https://invensense.tdk.com/products/inmp441/)
[![AI: TFLite Micro KWS](https://img.shields.io/badge/AI-TFLite%20Micro%20KWS-orange)](https://www.tensorflow.org/lite/microcontrollers)
[![ASR: Faster-Whisper](https://img.shields.io/badge/ASR-Faster--Whisper-green)](https://github.com/SYSTRAN/faster-whisper)
[![Server: FastAPI](https://img.shields.io/badge/Server-FastAPI-009688)](https://fastapi.tiangolo.com/)
[![Organization: QuantumSIHCracker](https://img.shields.io/badge/GitHub-QuantumSIHCracker-black)](https://github.com/QuantumSIHCracker)

An **Edge-to-Cloud Voice Assistant** built for Smart India Hackathon 2026 (Problem Statement #26172).

The system uses an ESP32-S3 microcontroller to capture voice locally, run on-device keyword spotting (wake word: "Ankit"), and stream audio to a high-performance Python server for full speech-to-text transcription.

---

## 📁 Repositories

| Repository | Description | Team |
|---|---|---|
| [`sih-hardware-firmware`](https://github.com/QuantumSIHCracker/sih-hardware-firmware) | ESP32-S3 Arduino firmware, circuit diagrams, pinout | Hardware |
| [`sih-ml-models`](https://github.com/QuantumSIHCracker/sih-ml-models) | KWS model training, MFCC pipeline, TFLite export | ML |
| [`sih-server-backend`](https://github.com/QuantumSIHCracker/sih-server-backend) | FastAPI server, Silero VAD, Faster-Whisper, dashboard | Server |
| [`sih-combined-system`](https://github.com/QuantumSIHCracker/sih-combined-system) | Workflow docs, protocol spec, integration | All Teams |

---

## 🏗️ System Architecture

```
[INMP441 Mic] →(I2S 16kHz)→ [ESP32-S3]
                                 ├─ Core 0: DC Filter + Ring Buffer + Acoustic Gate
                                 └─ Core 1: TENet KWS (INT8 TFLite) → Stream Manager
                                                          │
                              USB Serial 921600 baud  ───┤
                              Wi-Fi WebSocket ws://:8080 ┘
                                                          │
                              [FastAPI Server] ←──────────┘
                                 ├─ Silero VAD (speech endpointing)
                                 ├─ Faster-Whisper base.en (ASR)
                                 └─ Dashboard → http://localhost:8080/dashboard
```

---

## 📐 Design Targets

| Metric | Target |
|---|---|
| ESP32 Idle CPU Load | < 10% |
| ESP32 RAM Usage | < 256 KB |
| Whisper Latency | < 1.5s |
| End-to-End Latency | < 5s |
| Wake Word Accuracy | > 95% |
| False Positive Rate | < 2% |

---

## 🔌 Hardware Pinout

| INMP441 Pin | ESP32-S3 GPIO | Function |
|---|---|---|
| VDD | 3.3V | Power — never connect to 5V |
| GND | GND | Ground |
| **L/R** | **GND** | **Must be tied to GND (Left channel)** |
| WS | GPIO 4 | I2S Word Select (LRCLK) |
| SCK | GPIO 5 | I2S Serial Clock (BCLK) |
| SD | GPIO 7 | I2S Serial Data Output |
| — | GPIO 48 | WS2812 RGB LED (status) |
| — | GPIO 0 | BOOT button (manual trigger) |
| — | GPIO 2 | Simple LED fallback |

---

## 📂 Workflow Directory

All planning documents are stored at:
```
/home/arpit_ubuntu/New WorkFlow/SIH-Quantum-Cracker/
├── README.md                        ← This file
├── hardware-team/
│   └── HARDWARE_TEAM_PLAN.md        ← Hardware team full plan
├── ml-team/
│   └── ML_TEAM_PLAN.md              ← ML team full plan
├── server-team/
│   └── SERVER_TEAM_PLAN.md          ← Server team full plan
├── combined/
│   ├── WORKFLOW.md                  ← Overall workflow & protocol spec
│   ├── setup_github.py              ← GitHub repo setup script
│   └── push_to_github.py            ← Secure push helper
└── memory/
    └── AGENT_CONTEXT.md             ← AI agent context (read first each session)
```

---

## 📞 Organization

- **GitHub Org**: [github.com/QuantumSIHCracker](https://github.com/QuantumSIHCracker)
- **Hardware Lead**: Arpit Kumar — working on WSL Ubuntu @ `/home/arpit_ubuntu`

---

## 📜 License

MIT License — Smart India Hackathon 2026
