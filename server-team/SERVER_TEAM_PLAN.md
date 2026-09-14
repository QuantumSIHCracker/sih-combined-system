# 🖥️ Server Team — Complete Working Plan
### Smart India Hackathon (SIH) 2026 | QuantumSIHCracker | Team: Backend & Server

> **Repository**: `https://github.com/QuantumSIHCracker/sih-server-backend`
> **Branch Strategy**: `main` (protected) → `dev` → `feature/<name>` branches
> **Working Machine**: Your own computer (not Arpit's machine)

---

## 📌 Your Mission

You are the **Server Team**. Your job is to:
1. **Receive audio** from the ESP32-S3 hardware device via USB Serial or Wi-Fi WebSocket
2. **Run Silero VAD** to detect when speech starts and ends (dynamic endpointing)
3. **Run Faster-Whisper ASR** to transcribe speech to text
4. **Verify the wake word** from Whisper output using server-side KWS
5. **Process commands** and generate responses (future scope)
6. **Serve the live telemetry dashboard** showing all pipeline metrics in real-time
7. **Define and maintain the communication protocol** with the Hardware Team

You are the **brain of the system**. Audio arrives raw; you make it intelligent.

---

## 🏗️ System Context — Where You Fit

```
[Hardware ESP32-S3]
        │
        │  USB Serial @ 921,600 baud (prototype)
        │    OR Wi-Fi WebSocket ws://server-ip:8080/stream (deployment)
        │
        ▼
[FastAPI Ingestion Engine] (your server, port 8080)
        │
        ├── [Silero VAD] → detect speech end → finalize utterance
        ├── [Faster-Whisper base.en] → speech-to-text
        ├── [Wake Word Verification] → confirm "Ankit" from transcript
        ├── [Command Dispatcher] → future: execute commands
        │
        └── [SSE Broadcast] → Web Dashboard (http://localhost:8080/dashboard)
```

---

## 🔌 Hardware Protocol — The Contract (READ THIS CAREFULLY)

This is the **binary packet protocol** the Hardware Team sends you. You MUST parse it correctly.

### Transport 1: USB Serial (Prototype Mode)
- **Port**: Auto-detected (default: `COM7` on Windows, `/dev/ttyACM0` on Linux)
- **Baud Rate**: `921,600` — non-negotiable (required for 16kHz 16-bit audio bandwidth)
- **Character set**: Mix of binary PCM packets AND JSON text lines

### Transport 2: Wi-Fi WebSocket (Deployment Mode)
- **Endpoint**: `ws://0.0.0.0:8080/stream`
- **Binary frames**: PCM audio data
- **Text frames**: JSON control messages

### Packet Types from Hardware

#### Type 1: Audio PCM Packet (Binary)
```
Byte 0: 0xAA          (magic header byte 1)
Byte 1: 0x55          (magic header byte 2)
Byte 2: len_hi        (high byte of PCM data length in bytes)
Byte 3: len_lo        (low byte of PCM data length in bytes)
Bytes 4..4+len-1: PCM data (int16_t little-endian, 16kHz mono)
```

**Audio Format**: 
- Sample rate: 16,000 Hz
- Bit depth: 16-bit signed integer (int16_t)
- Channels: Mono (1 channel)
- Endianness: Little-endian
- Chunk size: 512 samples = 1024 bytes per packet (typical)
- Valid frame: `4 ≤ len ≤ 2048`

**Python parse example**:
```python
import struct
import numpy as np

def parse_pcm_packet(buffer):
    """Parse a framed PCM packet. Returns PCM bytes or None."""
    idx = buffer.find(b'\xAA\x55')
    if idx == -1 or len(buffer) < idx + 4:
        return None, buffer
    frame_len = (buffer[idx+2] << 8) | buffer[idx+3]
    if frame_len < 4 or frame_len > 2048:
        return None, buffer[idx+2:]  # discard false header
    if len(buffer) < idx + 4 + frame_len:
        return None, buffer  # wait for more data
    pcm_bytes = buffer[idx+4 : idx+4+frame_len]
    remaining = buffer[idx+4+frame_len:]
    return pcm_bytes, remaining

def pcm_bytes_to_float32(pcm_bytes):
    """Convert int16 PCM bytes to float32 [-1.0, 1.0]."""
    n = len(pcm_bytes) // 2
    samples = struct.unpack(f'<{n}h', pcm_bytes[:n*2])
    return np.array(samples, dtype=np.float32) / 32768.0
```

#### Type 2: JSON Control Messages (Text, newline-terminated)
```json
{"event": "start"}
```
→ Hardware detected wake word. Begin recording session. Reset VAD state.

```json
{"event": "telemetry", "free_heap": 194560, "min_free_heap": 186000, "cpu0_percent": 3, "cpu1_percent": 4, "cpu_percent": 3, "uptime_ms": 45000, "mic_peak": 2400, "raw_hex": "0x00123456"}
```
→ Hardware health metrics. Display on dashboard.

### Messages You Send TO Hardware (Stop Signal)

#### USB Serial
```json
{"event": "stop"}\n
```
Send as bytes: `b'{"event":"stop"}\n'`

#### WebSocket
```json
{"event": "stop"}
```
Send as WebSocket text frame.

#### Manual Trigger (REST API → Hardware)
```
POST http://localhost:8080/api/trigger
```
→ Server writes `b't\n'` to serial port → Hardware fires `fireWakeWordTrigger()`

---

## ⚙️ Current Server State

### Working Implementation
The file `/home/arpit_ubuntu/Smart India hackathon/server.py` (823 lines) is **fully functional**. Key components:

| Component | Implementation |
|---|---|
| Web Framework | FastAPI + Uvicorn (async) |
| Serial Listener | `asyncio`-compatible byte-parser with auto-port detection |
| WebSocket Endpoint | `/stream` — accepts binary PCM + JSON text |
| VAD | Silero VAD (ONNX), VADIterator, threshold=0.35 |
| ASR | Faster-Whisper `base.en`, int8, CPU threads |
| Dashboard | Embedded HTML (TailwindCSS) + Server-Sent Events (SSE) |
| Stats Logging | JSONL file `esp32_stats.jsonl` |

### VAD Settings (Tuned, Working)
| Parameter | Value | Reasoning |
|---|---|---|
| `VAD_THRESHOLD` | 0.35 | Increased sensitivity for edge microphones |
| `VAD_MIN_SILENCE_MS` | 1200 | 1.2s silence = end of utterance |
| `VAD_SPEECH_PAD_MS` | 60 | Preserve start/end syllables |
| `MAX_UTTERANCE_SEC` | 7.0 | Hard cap on speech |
| `NO_SPEECH_TIMEOUT_SEC` | 4.0 | Timeout if no speech after wake word |

### Session State Machine
```
IDLE → (receive "start" event) → STREAMING → (VAD detects silence) → TRANSCRIBING → IDLE
         ↑                                     (timeout: 7s max)     ↑
         └──────────────────────────────────────────────────────────┘
```

---

## 📋 Your Task List (Priority Order)

### Phase 1 — Setup & Baseline (Week 1)
- [ ] **Clone the repo** and set up Python environment
- [ ] **Install dependencies** from `requirements.txt`
- [ ] **Test server with existing firmware**: USB Serial mode → verify transcription
- [ ] **Access dashboard** at `http://localhost:8080/dashboard`
- [ ] **Document the current API** (REST endpoints, SSE events, WebSocket protocol)
- [ ] **Push working baseline** to `feature/server-baseline`

### Phase 2 — Enhancement (Week 2)
- [ ] **Add response action layer**:
  - After wake word verified: parse the command after "Ankit"
  - Implement at least 3 responses (e.g., "time", "date", "status")
- [ ] **Improve dashboard**:
  - Add live audio waveform/spectrogram visualization
  - Add latency trend graph (last 10 utterances)
  - Add connection status indicator (Serial/WebSocket connected/disconnected)
- [ ] **Add REST API endpoints**:
  - `GET /api/history` — last N transcriptions
  - `GET /api/stats` — performance metrics summary  
  - `POST /api/config` — update VAD/Whisper settings at runtime
- [ ] **Implement WebSocket Wi-Fi mode** testing end-to-end with Hardware Team
- [ ] **Persistent session logging**: save all transcriptions to SQLite database

### Phase 3 — Integration & Polish (Week 3)
- [ ] **End-to-end Wi-Fi WebSocket** integration test with Hardware Team
- [ ] **Latency optimization**: target <3.5s E2E (current: 4.5–5.3s)
  - Try Whisper `tiny.en` model for speed vs accuracy tradeoff
  - Experiment with `VAD_MIN_SILENCE_MS = 800ms` (if accuracy allows)
- [ ] **Error handling**: robust reconnection, graceful degradation
- [ ] **Multi-device support**: handle multiple ESP32 connections simultaneously
- [ ] **Final server v1.0 tag** on `main`

---

## 🛠️ Development Environment Setup (Your Machine)

### Python Environment
```bash
# Create and activate virtual environment
python3 -m venv venv
source venv/bin/activate  # Windows: venv\Scripts\activate

# Install dependencies
pip install -r requirements.txt
```

### `requirements.txt`
```
fastapi>=0.104.0
uvicorn[standard]>=0.24.0
faster-whisper>=0.9.0
silero-vad>=4.0
pyserial>=3.5
numpy>=1.24
websockets>=11.0
python-multipart>=0.0.6
aiofiles>=23.0
```

### Project Structure (your repo)
```
sih-server-backend/
├── server/
│   ├── server.py          # Main FastAPI application (START HERE)
│   ├── session.py         # Session state machine
│   ├── serial_listener.py # USB Serial transport handler
│   ├── ws_handler.py      # WebSocket transport handler
│   ├── vad_processor.py   # Silero VAD integration
│   ├── whisper_asr.py     # Faster-Whisper integration
│   ├── dashboard.py       # Dashboard HTML generation
│   └── command_handler.py # Wake word verified → command dispatch
├── tests/
│   ├── test_protocol.py   # Test packet parsing
│   ├── test_vad.py        # Test VAD with sample audio
│   └── test_transcribe.py # Test Whisper with sample audio
├── tools/
│   └── simulate_esp32.py  # Send fake PCM packets to test server without hardware
├── requirements.txt
├── .gitignore
└── README.md
```

---

## 🔑 Key Technical Implementations

### Serial Port Auto-Detection (Multi-OS)
```python
import serial.tools.list_ports

def find_esp32_port():
    """Auto-detect ESP32 serial port on any OS."""
    ports = serial.tools.list_ports.comports()
    # Try to find by USB VID/PID first (ESP32-S3 = 0x303A:0x1001)
    for p in ports:
        if p.vid == 0x303A:  # Espressif VID
            return p.device
    # Fallback: use environment variable or first available
    import os
    return os.environ.get("ESP32_SERIAL_PORT", 
                          ports[0].device if ports else "COM7")
```

### Hardware Simulator (Test Without ESP32)
```python
# tools/simulate_esp32.py — Use this to test server without hardware
import serial
import struct
import time
import json
import numpy as np

def send_audio_packet(ser, samples):
    """Send framed PCM packet matching ESP32 firmware format."""
    pcm_bytes = samples.astype('<i2').tobytes()
    header = struct.pack('>BB', 0xAA, 0x55)
    length = struct.pack('>H', len(pcm_bytes))
    ser.write(header + length + pcm_bytes)

def simulate_session(port, baud=921600):
    ser = serial.Serial(port, baud, timeout=1)
    
    # Send start event
    ser.write(b'{"event":"start"}\n')
    time.sleep(0.1)
    
    # Send 3 seconds of sine wave audio (simulating speech)
    sr = 16000
    duration = 3.0
    t = np.linspace(0, duration, int(sr * duration))
    audio = (np.sin(2 * np.pi * 440 * t) * 8000).astype(np.int16)
    
    chunk_size = 512
    for i in range(0, len(audio), chunk_size):
        send_audio_packet(ser, audio[i:i+chunk_size])
        time.sleep(0.032)  # 32ms per chunk
    
    # Send telemetry
    telemetry = {
        "event": "telemetry",
        "free_heap": 194560,
        "cpu_percent": 3,
        "mic_peak": 8000,
        "uptime_ms": 5000
    }
    ser.write((json.dumps(telemetry) + '\n').encode())
    ser.close()
```

### Server-Side Wake Word Verification
```python
import re

def verify_wake_word(transcript: str) -> tuple[bool, str]:
    """
    Verify "Ankit" wake word in transcript and extract command.
    Handles common Whisper mishearings of the wake word.
    """
    # Whisper sometimes mishears "Ankit" as these variants
    pattern = r'\b(hey\s+)?(ankit|ankith|an\s+kit|un\s*kit|on\s*kit)\b'
    match = re.search(pattern, transcript, re.IGNORECASE)
    
    if match:
        # Extract the command after the wake word
        command = re.sub(
            r'^(hey\s+)?(ankit|ankith|an\s+kit|un\s*kit|on\s*kit)[\s,.:;!?-]*', 
            '', transcript, flags=re.IGNORECASE
        ).strip()
        return True, command
    return False, transcript

# Example usage:
# verify_wake_word("Hey Ankit what time is it?") → (True, "what time is it?")
# verify_wake_word("Ankith tell me the weather") → (True, "tell me the weather")
```

---

## 🌐 API Reference

### REST Endpoints
| Method | Endpoint | Description |
|---|---|---|
| GET | `/` | Redirect to dashboard |
| GET | `/dashboard` | Live telemetry dashboard |
| GET | `/events` | SSE stream (dashboard data source) |
| GET | `/api/status` | Current metrics + last 10 transcriptions |
| POST | `/api/trigger` | Manual trigger ESP32 via serial |
| GET | `/api/history` | Full transcription history |

### SSE Event Types (sent to dashboard)
```json
{"type": "telemetry", "data": {"cpu_percent": 3, "free_heap": 194560, "mic_peak": 2400}}
{"type": "state", "state": "STREAMING"}
{"type": "state", "state": "TRANSCRIBING"}
{"type": "state", "state": "IDLE"}
{"type": "result", "data": {"transcript": "...", "whisper_latency_ms": 1200, "total_e2e_latency_ms": 4500}}
```

### Performance Targets
| Metric | Current | Target |
|---|---|---|
| Whisper latency | 1.1–1.3s | <1.0s |
| E2E latency | 4.5–5.3s | <3.5s |
| False accept rate (wake word) | ~5% | <2% |
| Server uptime | N/A | >99.9% during demo |

---

## 🔗 Git Workflow (Server Team)

### Repository
```
https://github.com/QuantumSIHCracker/sih-server-backend
```

### Initial Setup
```bash
git clone https://github.com/QuantumSIHCracker/sih-server-backend.git
cd sih-server-backend
git config user.name "Your Name"
git config user.email "your-email@example.com"
```

### Daily Workflow
```bash
# Start new feature
git checkout dev
git pull origin dev
git checkout -b feature/<feature-name>

# After changes
git add .
git commit -m "feat(server): <description>"
git push origin feature/<feature-name>

# Open PR: feature/<name> → dev
# After testing, dev → main
```

### Commit Convention
```
feat(server): add command dispatch for time/date responses
feat(dashboard): add live audio waveform visualization
fix(serial): improve reconnection after USB disconnect
fix(vad): reduce false silence detection with 3x pre-scaling
perf(whisper): switch to tiny.en for 400ms latency improvement
test(protocol): add hardware simulator tests
```

### Testing Before Push
```bash
# Always run these before pushing
python -m pytest tests/ -v

# Test server startup
python server/server.py &
curl http://localhost:8080/api/status
```

---

## 🤝 Integration Points

### ← Hardware Team (what they give you)
- **Audio packets**: `[0xAA][0x55][len_hi][len_lo][...PCM bytes...]` 
- **Control events**: JSON `{"event":"start"}` and `{"event":"telemetry",...}`
- **Trigger endpoint**: `POST /api/trigger` sends `b't\n'` to serial

### → Hardware Team (what you send them)
- **Stop signal**: `{"event":"stop"}\n` on serial, or WebSocket text
- **Tell them**: Your server IP (for Wi-Fi mode: `WS_HOST` in firmware)
- **Tell them**: Any protocol changes IMMEDIATELY via GitHub issue

### ← ML Team (what they give you)
- Wake word string and Whisper variants for the regex pattern
- Model performance data (false positive/negative rates)

### → ML Team (what you give them)
- Real transcription failures (audio clips where Whisper failed)
- Whisper mishearing patterns of the wake word

---

## ⚠️ Critical Rules — Protocol Contract with Hardware Team

1. **NEVER change** the magic bytes `0xAA 0x55` — firmware is hardcoded
2. **NEVER change** the frame format `[AA][55][len_hi][len_lo][...bytes...]`
3. **NEVER change** the audio format (16kHz, int16, mono, little-endian)
4. **NEVER change** the stop signal format `{"event":"stop"}\n`
5. If you need to change ANY protocol detail → open a GitHub issue tagged `protocol-change` and notify Hardware Team first

---

## 🤖 AI Prompt to Start Your Work

```
You are an expert Python backend engineer specializing in FastAPI, async I/O, real-time audio processing, WebSockets, Server-Sent Events (SSE), serial port communication, and AI inference pipelines (Whisper ASR, Silero VAD).

PROJECT CONTEXT:
We are building a Smart India Hackathon (SIH) voice assistant server. I am on the Server Team. Our FastAPI server receives live audio from an ESP32-S3 microcontroller (via USB Serial at 921600 baud OR Wi-Fi WebSocket), runs Silero VAD for speech endpointing, and Faster-Whisper for speech-to-text transcription.

HARDWARE PROTOCOL (DO NOT CHANGE):
Audio is sent as binary framed packets:
  Byte 0-1: Magic [0xAA, 0x55]
  Byte 2-3: Big-endian uint16 = number of PCM bytes following
  Bytes 4+: int16 LE PCM samples (16kHz, mono)

Control messages are JSON text (newline-terminated):
  {"event": "start"} → hardware detected wake word
  {"event": "telemetry", "free_heap":..., "cpu_percent":..., "mic_peak":...}

We send back to hardware:
  {"event": "stop"}\n  → on serial   |  {"event": "stop"}  → on WebSocket

CURRENT STACK:
- FastAPI + Uvicorn (async)
- Silero VAD (ONNX), VADIterator, threshold=0.35, 1.2s silence = utterance end
- Faster-Whisper base.en, int8, CPU
- SSE broadcast to web dashboard
- Session state machine: IDLE → STREAMING → TRANSCRIBING → IDLE

PERFORMANCE TARGETS:
- Whisper latency: <1.0s
- End-to-end latency: <3.5s
- Server uptime: >99.9% during demo

My current task: [DESCRIBE WHAT YOU WANT TO DO]

Please provide production-quality async Python code.
```

---

## 📞 Team Communication Protocol

- **Post server updates**: `[SVR] Feature deployed: X | API change: Y | Latency: Zms`
- **Protocol changes**: Raise GitHub issue IMMEDIATELY — tag `@hardware-team`
- **Integration testing**: Schedule sync with Hardware Team every 2 days
- **Known issues**: Track in GitHub Issues, prioritize by demo impact
