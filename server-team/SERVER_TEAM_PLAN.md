# 🖥️ SIH Quantum Cracker — Server Team Plan

> **Repo:** [https://github.com/QuantumSIHCracker/sih-server-backend](https://github.com/QuantumSIHCracker/sih-server-backend)
> **Last updated:** 2026-09-14

---

## 🎯 Mission Statement

Build a **real-time voice processing backend** that receives raw PCM audio from embedded hardware (ESP32), runs wake-word verification, voice activity detection, and automatic speech recognition, then returns transcription results — all with sub-second latency on a laptop-class CPU. Every design decision prioritises **protocol stability** so that hardware and server teams can develop independently without breaking each other.

---

## 🗺️ System Context

```
┌─────────────────────────────────────────────────────────────────────┐
│                        SYSTEM OVERVIEW                              │
│                                                                     │
│   ┌──────────────┐   USB Serial / WebSocket   ┌─────────────────┐  │
│   │   ESP32-S3   │ ─────────────────────────► │  sih-server-    │  │
│   │  (Hardware)  │ ◄───────────────────────── │    backend      │  │
│   │              │   {event:stop} signal       │   (This repo)   │  │
│   └──────────────┘                             └────────┬────────┘  │
│                                                         │           │
│                                          ┌──────────────▼────────┐  │
│                                          │  Dashboard / Browser  │  │
│                                          │  FastAPI + SSE :8080  │  │
│                                          └───────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

The server team **owns everything** inside `sih-server-backend/`. The hardware team owns the ESP32 firmware. The boundary between the two is the **Hardware Protocol Spec** below — treat it as an immutable contract.

---

## 📡 HARDWARE PROTOCOL SPEC *(The Contract)*

> [!IMPORTANT]
> This section defines the exact binary and JSON framing agreed with the hardware team. **Do not change magic bytes, audio format, or JSON event names without a joint team decision and a version bump.**

### Binary Packet Format (Audio)

```
┌────────┬────────┬─────────┬─────────┬───────────────────────────┐
│  0xAA  │  0x55  │ len_hi  │ len_lo  │  ... int16 PCM LE data ...│
│ (sync) │ (sync) │ (MSB)   │ (LSB)   │  (len bytes total)        │
└────────┴────────┴─────────┴─────────┴───────────────────────────┘
  Byte 0   Byte 1   Byte 2    Byte 3    Bytes 4 … (4 + len - 1)
```

| Field | Value | Notes |
|-------|-------|-------|
| Magic bytes | `0xAA 0x55` | Sync header — **never change** |
| `len_hi` | MSB of payload length | Big-endian 16-bit length |
| `len_lo` | LSB of payload length | |
| Valid `len` range | **4 – 2048 bytes** | Packets outside range are dropped |
| Payload | Raw PCM samples | int16, little-endian |

### Audio Format

| Parameter | Value |
|-----------|-------|
| Sample rate | **16 000 Hz** |
| Bit depth | **int16 (signed 16-bit)** |
| Channels | **Mono** |
| Byte order | **Little-endian** |

### JSON Control Events

All JSON frames are UTF-8 encoded. On serial transport they are newline-terminated (`\n`). On WebSocket transport they are sent as **text frames**.

#### Start Event (Hardware → Server)
```json
{"event": "start"}
```
Sent by the ESP32 when it detects a wake word and begins streaming audio.

#### Telemetry Event (Hardware → Server)
```json
{
  "event": "telemetry",
  "free_heap": 123456,
  "cpu_percent": 42,
  "mic_peak": 3200,
  "uptime_ms": 98765
}
```
Sent periodically (design target: every 500 ms) during a streaming session.

#### Stop Signal (Server → Hardware)
```json
{"event": "stop"}
```
- **Serial:** sent as `{"event":"stop"}\n` on the same serial port.
- **WebSocket:** sent as a WebSocket **text frame** to the connected client.

### Transport 1 — USB Serial

| Parameter | Value |
|-----------|-------|
| Baud rate | **921 600** |
| Auto-detect | Yes — server scans `/dev/ttyUSB*`, `/dev/ttyACM*`, `COM*` at startup |
| Flow control | None |
| Framing | Binary packets interleaved with newline-terminated JSON |

### Transport 2 — WebSocket

| Parameter | Value |
|-----------|-------|
| Endpoint | `ws://0.0.0.0:8080/stream` |
| Binary frames | Audio packets (same `0xAA 0x55` framing) |
| Text frames | JSON control events |
| Path | `/stream` |

---

## 🐍 Python Packet Parser *(Design Reference)*

The following snippet is the **canonical reference implementation** for parsing the binary packet format. Hardware simulator and server code should both use this logic.

```python
# sih-server-backend — packet_parser.py (design reference)
# DO NOT change MAGIC_BYTES without coordinating with the hardware team.

import struct

MAGIC = b'\xAA\x55'
MIN_PAYLOAD = 4    # bytes
MAX_PAYLOAD = 2048 # bytes

def parse_packets(buf: bytearray) -> tuple[list[bytes], bytearray]:
    """
    Scan `buf` for valid audio packets.

    Returns:
        (packets, remainder)
        packets   — list of raw PCM payload bytes (each between 4-2048 bytes)
        remainder — unconsumed bytes to prepend to the next read
    """
    packets: list[bytes] = []

    while True:
        # Find magic header
        idx = buf.find(MAGIC)
        if idx == -1:
            # No header found; keep last byte in case it's a partial magic
            buf = buf[-1:] if buf else bytearray()
            break

        if idx > 0:
            buf = buf[idx:]  # discard leading garbage

        if len(buf) < 4:
            break  # need more data for header + length

        length = struct.unpack('>H', buf[2:4])[0]  # big-endian uint16

        if not (MIN_PAYLOAD <= length <= MAX_PAYLOAD):
            # Invalid length — skip this magic byte and resync
            buf = buf[1:]
            continue

        packet_end = 4 + length
        if len(buf) < packet_end:
            break  # incomplete packet — wait for more data

        payload = bytes(buf[4:packet_end])
        packets.append(payload)
        buf = buf[packet_end:]

    return packets, buf


def payload_to_pcm(payload: bytes) -> list[int]:
    """Decode payload bytes to a list of int16 PCM samples (little-endian)."""
    n_samples = len(payload) // 2
    return list(struct.unpack(f'<{n_samples}h', payload[:n_samples * 2]))
```

---

## 🤖 Hardware Simulator Tool *(Design)*

The simulator allows the server team to develop and test **without physical ESP32 hardware**. It lives at `sih-server-backend/tools/simulator.py`.

### Simulator Features

| Feature | Description |
|---------|-------------|
| **Serial mode** | Opens a virtual serial port (via `socat` or `pty`) and streams synthetic audio packets |
| **WebSocket mode** | Connects to `ws://localhost:8080/stream` and sends packets as binary frames |
| **Synthetic audio** | Generates 16 kHz white noise or reads a WAV file from disk |
| **JSON events** | Sends `{"event":"start"}`, periodic telemetry, and waits for `{"event":"stop"}` |
| **Configurable packet size** | `--chunk-bytes 512` (must be even, 4–2048) |
| **Loop mode** | `--loop` to replay audio continuously for stress testing |

### Simulator Usage (Design Target CLI)

```bash
# WebSocket mode — stream a WAV file
python tools/simulator.py --mode ws --file samples/test_audio.wav

# Serial mode — stream white noise in a loop
python tools/simulator.py --mode serial --port /dev/pts/3 --loop

# Send one session then exit (CI/test mode)
python tools/simulator.py --mode ws --file samples/ankit_sample.wav --once
```

---

## 🔊 Wake-Word Verification *(Server-Side)*

The hardware performs a first-pass wake-word detection in firmware. The server performs a **secondary verification** on the first recognised utterance to reduce false positives.

### Wake Word: *"Ankit"*

Server-side verification uses a **regex match on the ASR transcript** of the first audio segment:

```python
import re

WAKE_WORD_PATTERN = re.compile(
    r'\b(ankit|ankith|anchit|uncle|onkit)\b',
    re.IGNORECASE
)

def verify_wake_word(transcript: str) -> bool:
    """Return True if transcript likely contains the wake word 'Ankit'."""
    return bool(WAKE_WORD_PATTERN.search(transcript))
```

> [!NOTE]
> The regex includes phonetically similar variants (`ankith`, `anchit`, `uncle`, `onkit`) to tolerate ASR mishearing. Tune as needed based on real-world testing.

---

## 🔄 Session State Machine

Each connected client (serial port or WebSocket) follows this lifecycle:

```
            ┌─────────────────────────────────────────────────┐
            │                                                 │
            ▼                                                 │
         ┌──────┐   {event:start} received    ┌───────────┐  │
         │ IDLE │ ──────────────────────────► │ STREAMING │  │
         └──────┘                             └─────┬─────┘  │
            ▲                                       │        │
            │                                VAD signals     │
            │                               end-of-speech    │
            │                                       │        │
            │                               ┌───────▼──────┐ │
            │    transcription complete     │ TRANSCRIBING │ │
            └───────────────────────────────└──────────────┘ │
                                                             │
                    (also transitions back on error)  ───────┘
```

| State | Description | Transitions |
|-------|-------------|-------------|
| **IDLE** | Waiting for start event. Rejects audio packets. | → STREAMING on `{event:start}` |
| **STREAMING** | Buffering PCM audio, running VAD. Sends `{event:stop}` to hardware on end-of-speech. | → TRANSCRIBING on VAD end-of-speech; → IDLE on error/timeout |
| **TRANSCRIBING** | Running Faster-Whisper on buffered audio. Server is not accepting new packets. | → IDLE when complete; SSE event fired |

---

## 🎙️ VAD Design Parameters

Voice Activity Detection uses [Silero VAD](https://github.com/snakers4/silero-vad) via its ONNX runtime export for CPU-only inference.

| Parameter | Design Value | Notes |
|-----------|-------------|-------|
| Model | **Silero VAD ONNX** | `silero_vad.onnx` — CPU only |
| Threshold | **0.35** | Speech probability above this = voice active |
| Silence duration to end | **1.2 s** | Consecutive silence before declaring end-of-speech |
| Pre-speech pad | **60 ms** | Audio buffered before VAD trigger, preserved in output |
| Max utterance length | **7 s** | Force end-of-speech if speech exceeds this |
| No-speech timeout | **4 s** | In STREAMING state with no voice detected, return to IDLE |
| Frame size | **512 samples** (~32 ms at 16 kHz) | VAD chunk granularity |

---

## 🧠 ASR Design

Automatic Speech Recognition uses [Faster-Whisper](https://github.com/SYSTRAN/faster-whisper) for CPU-optimised inference.

| Parameter | Design Value | Notes |
|-----------|-------------|-------|
| Model | **`base.en`** | English-only, ~145 MB |
| Compute type | **`int8`** | Quantised for CPU speed |
| Device | **CPU** | No GPU required |
| Language | `en` | Fixed; skip language detection |
| Beam size | `5` | Balance speed vs accuracy |
| Target latency | **< 800 ms** for ≤ 7 s audio on modern laptop CPU | Design target |
| VAD filter | Disabled in Whisper | We use our own Silero VAD upstream |

---

## 🌐 Dashboard & API

The server exposes a unified **FastAPI** application on **port 8080**.

### HTTP API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/` | Dashboard HTML (single-page) |
| `GET` | `/health` | `{"status":"ok", "session_state":"IDLE"}` |
| `GET` | `/session` | Current session metadata (state, client, uptime) |
| `GET` | `/transcript/latest` | Last completed transcription result |
| `GET` | `/transcript/history` | Last N transcription results |
| `POST` | `/session/reset` | Force session back to IDLE |
| `GET` | `/events` | **SSE stream** — real-time events (see below) |
| `WS` | `/stream` | WebSocket audio/control endpoint for hardware |

### SSE Event Types (`GET /events`)

All events are `text/event-stream` and carry a JSON `data` field.

| Event name | Payload | Description |
|------------|---------|-------------|
| `session_start` | `{"client":"ws\|serial", "timestamp":…}` | New streaming session began |
| `audio_chunk` | `{"samples":…, "rms":…}` | Audio received (for waveform visualisation) |
| `vad_speech` | `{"start_ms":…}` | VAD detected start of speech |
| `vad_silence` | `{"duration_ms":…}` | VAD detected end of speech |
| `transcription` | `{"text":"…", "duration_ms":…, "wake_word_verified":bool}` | ASR complete |
| `telemetry` | `{"free_heap":…, "cpu_percent":…, "mic_peak":…, "uptime_ms":…}` | Hardware telemetry forwarded |
| `session_end` | `{"reason":"vad\|timeout\|error\|reset"}` | Session returned to IDLE |
| `error` | `{"code":"…", "message":"…"}` | Server-side error |

---

## 📋 Task List

### Phase 1 — Foundation *(Week 1)*

- [ ] Initialise `sih-server-backend/` repo with structure below
- [ ] Set up Python virtual environment and `requirements.txt`
- [ ] Implement `packet_parser.py` with unit tests (pytest)
- [ ] Implement serial transport layer (`transport/serial_transport.py`)
- [ ] Implement WebSocket transport layer (`transport/ws_transport.py`)
- [ ] Implement JSON event parser (start / telemetry / stop events)
- [ ] Implement session state machine (`session/state_machine.py`)
- [ ] Write hardware simulator (`tools/simulator.py`) — WebSocket mode first
- [ ] Basic FastAPI app skeleton: `/health`, `/stream` WebSocket endpoint
- [ ] Integration test: simulator → server → `/health` reflects STREAMING state

### Phase 2 — Intelligence *(Week 2)*

- [ ] Integrate Silero VAD ONNX (`vad/silero_vad.py`) with configured parameters
- [ ] Integrate Faster-Whisper ASR (`asr/whisper_engine.py`)
- [ ] Implement wake-word verification regex (`wakeword/verifier.py`)
- [ ] Wire pipeline: audio buffer → VAD → end-of-speech → ASR → transcript
- [ ] Implement `{event:stop}` send-back to hardware on end-of-speech
- [ ] Add SSE event stream (`/events` endpoint)
- [ ] Add no-speech timeout (4 s) and max utterance cutoff (7 s)
- [ ] Integration test: simulator streams audio → server returns transcript via SSE

### Phase 3 — Dashboard & Polish *(Week 3)*

- [ ] Build dashboard HTML (`static/index.html`) with live waveform and SSE-fed transcript
- [ ] Add telemetry display panel (heap, CPU, mic peak, uptime)
- [ ] Add `/transcript/history` endpoint
- [ ] Add `/session/reset` endpoint
- [ ] Add serial port auto-detect and connect on startup
- [ ] Add simulator serial mode (`tools/simulator.py --mode serial`)
- [ ] Add structured logging (`logging` + `rich` for coloured output)
- [ ] Load testing: simulator sends 10 concurrent sessions → verify graceful handling
- [ ] Write `README.md` with setup, run, and protocol reference
- [ ] Final end-to-end test with physical ESP32 (joint session with hardware team)

---

## 🐍 Python Environment Setup

### Requirements

```
# requirements.txt — sih-server-backend
fastapi==0.111.*
uvicorn[standard]==0.30.*
websockets==12.*
pyserial==3.*
faster-whisper==1.*
onnxruntime==1.18.*        # for Silero VAD
numpy==1.26.*
httpx==0.27.*              # for test client
pytest==8.*
pytest-asyncio==0.23.*
rich==13.*                 # coloured logging
```

### Setup Instructions

```bash
# Clone the repo
git clone https://github.com/QuantumSIHCracker/sih-server-backend.git
cd sih-server-backend

# Create and activate virtual environment
python3 -m venv .venv
source .venv/bin/activate          # Linux/macOS
# .venv\Scripts\activate           # Windows

# Install dependencies
pip install -r requirements.txt

# Download Silero VAD ONNX model
python tools/download_models.py

# Run the server (development mode)
uvicorn server.main:app --host 0.0.0.0 --port 8080 --reload

# Run all tests
pytest tests/ -v
```

---

## 📁 Recommended Repo Folder Structure

```
sih-server-backend/
│
├── server/
│   ├── main.py                  # FastAPI app entry point
│   ├── config.py                # Centralised config (port, thresholds, model paths)
│   └── logging_setup.py         # Rich logging configuration
│
├── transport/
│   ├── serial_transport.py      # USB serial listener (auto-detect, 921600 baud)
│   └── ws_transport.py          # WebSocket handler for /stream
│
├── session/
│   ├── state_machine.py         # IDLE / STREAMING / TRANSCRIBING states
│   └── session_manager.py       # Per-client session lifecycle
│
├── protocol/
│   ├── packet_parser.py         # Binary packet parser (0xAA 0x55 framing)
│   └── event_parser.py          # JSON control event parser
│
├── vad/
│   ├── silero_vad.py            # Silero ONNX VAD wrapper
│   └── models/
│       └── silero_vad.onnx      # Downloaded model (gitignored)
│
├── asr/
│   └── whisper_engine.py        # Faster-Whisper base.en int8 wrapper
│
├── wakeword/
│   └── verifier.py              # Regex-based wake-word verification
│
├── dashboard/
│   ├── sse_manager.py           # SSE event broadcaster
│   └── router.py                # /events, /health, /session, /transcript routes
│
├── static/
│   └── index.html               # Single-page dashboard UI
│
├── tools/
│   ├── simulator.py             # Hardware simulator (serial + WebSocket modes)
│   └── download_models.py       # Script to fetch Silero ONNX + Whisper model
│
├── tests/
│   ├── test_packet_parser.py
│   ├── test_state_machine.py
│   ├── test_vad.py
│   ├── test_asr.py
│   └── test_integration.py      # Simulator → server end-to-end
│
├── samples/
│   └── test_audio.wav           # Sample audio for simulator (gitignored if large)
│
├── requirements.txt
├── .gitignore
└── README.md
```

---

## 🚨 CRITICAL RULES

> [!CAUTION]
> Violating any of these rules will break the hardware team's firmware and require a coordinated reflash.

1. **NEVER change the magic bytes.** `0xAA 0x55` is hardcoded in ESP32 firmware. Any change requires a joint decision + firmware OTA.

2. **NEVER change the audio format.** 16 kHz, int16, mono, little-endian is hardcoded in the ADC and DMA pipeline. Changing sample rate requires hardware rework.

3. **NEVER rename JSON event keys** (`event`, `free_heap`, `cpu_percent`, `mic_peak`, `uptime_ms`) without updating firmware and bumping a protocol version field.

4. **ALWAYS send `{event:stop}\n`** on serial (or text frame on WebSocket) when end-of-speech is detected. The hardware uses this to stop the microphone and save power.

5. **Any protocol change** requires:
   - A GitHub Issue tagged `protocol-change` in both `sih-server-backend` and the hardware firmware repo.
   - Sign-off from both team leads.
   - A version field added to JSON events (`"version": 2`).

---

## 🔀 Git Workflow

Repository: [https://github.com/QuantumSIHCracker/sih-server-backend](https://github.com/QuantumSIHCracker/sih-server-backend)

### Branch Strategy

| Branch | Purpose |
|--------|---------|
| `main` | Stable, tested code only. Always runnable. |
| `dev` | Integration branch — merge here first |
| `feature/<name>` | Individual feature work |
| `fix/<name>` | Bug fixes |
| `test/<name>` | Experimental / spike work |

### Workflow

```bash
# Start a new feature
git checkout dev
git pull origin dev
git checkout -b feature/vad-integration

# Work, commit with conventional commits
git add .
git commit -m "feat(vad): integrate Silero ONNX with 0.35 threshold"

# Push and open PR → dev
git push origin feature/vad-integration
# Open PR on GitHub: feature/vad-integration → dev

# After review, merge to dev
# When dev is stable and tested → PR: dev → main
```

### Commit Message Convention

```
feat(scope): short description       # new feature
fix(scope): short description        # bug fix
test(scope): short description       # add/update tests
docs(scope): short description       # documentation
refactor(scope): short description   # refactor, no behaviour change
chore(scope): short description      # tooling, config, deps
```

---

## 🔗 Integration Points

| What | Owner | Interface |
|------|-------|-----------|
| Audio packet source | Hardware team (ESP32) | Binary `0xAA 0x55` packets over serial / WS |
| JSON control events | Hardware team (ESP32) | UTF-8 JSON, serial newline / WS text frame |
| Stop signal consumer | Hardware team (ESP32) | Listens for `{"event":"stop"}\n` |
| Dashboard consumer | Any browser | `GET /events` SSE stream, `GET /` HTML |
| Transcription API | Future app / LLM agent | `GET /transcript/latest` JSON |
| Test harness | Server team | `tools/simulator.py` replaces hardware |

---

## 🚀 AI Prompt to Start Work

Use the following prompt when asking an AI assistant (e.g., Gemini, Copilot) to help implement any part of this server:

```
You are helping build `sih-server-backend`, a Python FastAPI server that receives
real-time audio from an ESP32 microcontroller.

PROTOCOL CONTRACT (do not deviate):
- Binary audio packets: [0xAA][0x55][len_hi][len_lo][...int16 PCM LE...]
  Valid payload length: 4–2048 bytes.
- Audio format: 16 kHz, int16, mono, little-endian.
- JSON start event from hardware: {"event": "start"}
- JSON telemetry: {"event": "telemetry", "free_heap": …, "cpu_percent": …, "mic_peak": …, "uptime_ms": …}
- Stop signal to hardware: {"event": "stop"} — newline-terminated on serial, text frame on WebSocket.
- Serial: auto-detect port, 921600 baud.
- WebSocket endpoint: ws://0.0.0.0:8080/stream

STACK:
- FastAPI + uvicorn, port 8080
- Silero VAD ONNX (threshold 0.35, 1.2s silence, 60ms pad, 7s max, 4s no-speech timeout)
- Faster-Whisper base.en int8 CPU
- SSE for dashboard events

TASK: [describe what you want built here]

Follow the folder structure in SERVER_TEAM_PLAN.md. Write clean, typed Python.
Do not change magic bytes, audio format, or JSON event keys.
```

---

## 📢 Team Communication Protocol

| Situation | Action |
|-----------|--------|
| Changing anything in **HARDWARE PROTOCOL SPEC** | Open GitHub Issue tagged `protocol-change`, get sign-off from hardware team lead **before** any code change |
| Blocked by missing hardware | Use `tools/simulator.py` — never block on hardware availability |
| ASR/VAD parameter tuning | Create a branch `test/vad-tuning-<date>`, document results in PR description, merge only if latency target met |
| Found a bug in protocol framing | Tag `@hardware-team` in GitHub Issue with exact byte dump from `packet_parser.py` |
| Merging to `main` | Requires at least one other server team member review + all tests green |
| Demo day integration | Schedule joint session with hardware team ≥ 24 h before demo for end-to-end test |
| Design decisions | Document in `docs/decisions/` as ADR (Architecture Decision Record) |

---

*Built fresh for SIH Quantum Cracker. All design targets are aspirational until measured on target hardware.*
