# 🚀 SIH Quantum Cracker — Smart India Hackathon 2026

[![Hardware: ESP32-S3](https://img.shields.io/badge/Hardware-ESP32--S3-red)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Audio: INMP441 MEMS](https://img.shields.io/badge/Audio-INMP441%20I2S-blue)](https://invensense.tdk.com/products/inmp441/)
[![AI: TFLite Micro KWS](https://img.shields.io/badge/AI-TFLite%20Micro%20KWS-orange)](https://www.tensorflow.org/lite/microcontrollers)
[![ASR: Faster-Whisper](https://img.shields.io/badge/ASR-Faster--Whisper-green)](https://github.com/SYSTRAN/faster-whisper)
[![Server: FastAPI](https://img.shields.io/badge/Server-FastAPI-009688)](https://fastapi.tiangolo.com/)
[![Organization: QuantumSIHCracker](https://img.shields.io/badge/GitHub-Quantum--SIH--Cracker-black)](https://github.com/QuantumSIHCracker)

An **Edge-to-Cloud Voice Assistant** built for Smart India Hackathon 2026 (Problem Statement #26172).

The system uses an ESP32-S3 microcontroller to capture voice locally, run on-device keyword spotting ("Ankit"), and stream audio to a high-performance Python server for full speech-to-text transcription.

---

## 📁 Repositories

| Repository | Description | Team |
|---|---|---|
| [`sih-hardware-firmware`](https://github.com/QuantumSIHCracker/sih-hardware-firmware) | ESP32-S3 Arduino firmware, circuit diagrams, pinout | Hardware |
| [`sih-ml-models`](https://github.com/QuantumSIHCracker/sih-ml-models) | KWS model training, MFCC pipeline, TFLite export | ML |
| [`sih-server-backend`](https://github.com/QuantumSIHCracker/sih-server-backend) | FastAPI server, Silero VAD, Faster-Whisper, dashboard | Server |
| [`sih-combined-system`](https://github.com/QuantumSIHCracker/sih-combined-system) | Integration testing, combined docs, releases | All Teams |

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

## ⚡ Quick Start

### Hardware Team (Arpit's machine)
```bash
# Flash firmware (Arduino IDE)
# Open: firmware/esp32_sih_hard_ware_firmware.ino
# Board: ESP32S3 Dev Module | USB CDC On Boot: Enabled | Upload Speed: 921600

# Run the server while testing
cd server && pip install -r requirements.txt && python server.py
```

### Server Team (your machine)
```bash
git clone https://github.com/QuantumSIHCracker/sih-server-backend.git
cd sih-server-backend
python3 -m venv venv && source venv/bin/activate
pip install -r requirements.txt
python server/server.py
# Dashboard: http://localhost:8080/dashboard
```

### ML Team (your machine)
```bash
git clone https://github.com/QuantumSIHCracker/sih-ml-models.git
cd sih-ml-models
python3 -m venv venv && source venv/bin/activate
pip install -r requirements.txt
# See notebooks/ for training pipeline
```

---

## 📊 Performance Benchmarks

| Metric | Target | Measured |
|---|---|---|
| ESP32 Idle CPU | <10% | **3%** ✅ |
| ESP32 RAM Usage | <256KB | **62KB** ✅ |
| Whisper Latency | Fast | **1.1–1.3s** ✅ |
| End-to-End Latency | Conversational | **4.5–5.3s** ✅ |

---

## 🔌 Hardware Pinout

| INMP441 Pin | ESP32-S3 GPIO | Function |
|---|---|---|
| VDD | 3.3V | Power |
| GND | GND | Ground |
| **L/R** | **GND** | **Tie to GND (Left channel)** |
| WS | GPIO 4 | I2S Word Select |
| SCK | GPIO 5 | I2S Serial Clock |
| SD | GPIO 7 | I2S Serial Data |
| — | GPIO 48 | WS2812 RGB LED |
| — | GPIO 0 | BOOT button (manual trigger) |

---

## 📞 Team Contacts & Repositories

- **Hardware Lead**: Arpit Kumar — WSL Ubuntu @ `/home/arpit_ubuntu`
- **Organization**: [github.com/QuantumSIHCracker](https://github.com/QuantumSIHCracker)

---

## 📄 Documentation

- [Hardware Team Plan](hardware-team/HARDWARE_TEAM_PLAN.md)
- [ML Team Plan](ml-team/ML_TEAM_PLAN.md)  
- [Server Team Plan](server-team/SERVER_TEAM_PLAN.md)
- [Combined Workflow](combined/WORKFLOW.md)
- [Protocol Specification](combined/WORKFLOW.md#-communication-protocol-specification)
- [Agent Memory / Context](memory/AGENT_CONTEXT.md)

---

## 📜 License

MIT License — Smart India Hackathon 2026
