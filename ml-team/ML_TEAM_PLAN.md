# 🧠 ML Team — Complete Working Plan
### Smart India Hackathon (SIH) 2026 | QuantumSIHCracker | Team: Machine Learning

> **Repository**: `https://github.com/QuantumSIHCracker/sih-ml-models`
> **Branch Strategy**: `main` (protected) → `dev` → `experiments/<name>`
> **Working Machine**: Your own computer (separate from Arpit's machine)
> **All new code lives in the repo above — nothing else.**

---

## 📌 Your Mission

You are the **ML Team**. You build and own:
1. The **Keyword Spotting (KWS) model** that runs ON the ESP32-S3 chip — no cloud, no internet
2. The **MFCC feature extraction pipeline** (must match firmware exactly)
3. A curated, augmented **dataset** for the wake word "Ankit"
4. **INT8 quantized TFLite** model delivery to the Hardware Team

You are the **intelligence inside the device**. Without you, the device cannot hear its own name.

---

## 🏗️ Where You Fit

```
       ┌─────────────── YOU ARE HERE ───────────────┐
       │                                             │
[16kHz WAV Dataset] → [MFCC Feature Extraction] → [Model Training] → [INT8 TFLite]
                                                                           │
                                          Hardware Team embeds ────────────┘
                                          this .tflite in firmware
                                          as a C byte array header
```

The model runs **on-chip** using **TensorFlow Lite Micro (TFLM)** on the ESP32-S3:
- Inference runs every 200ms on a sliding 1-second audio window
- Must complete in < 15ms per window at 240MHz
- Maximum model size: **60 KB** after INT8 quantization
- No floating point — INT8 only

---

## 🎯 Model Design Specification

### Architecture: TENet or DS-CNN
Build one of these (both are proven for on-device KWS):

**Option A — TENet (Temporal Efficient Network)**
```
Input [1, 51, 1, 10] INT8
  → Conv2D(32, 3×3) → BN → ReLU
  → Inverted Residual Block (DepthwiseConv2D + Conv2D 1×1)
  → Inverted Residual Block (DepthwiseConv2D + Conv2D 1×1)
  → Global Average Pool
  → Dense(5) → Softmax
Output [1, 5] INT8
```

**Option B — DS-CNN (Depthwise Separable CNN)**
```
Input [1, 51, 1, 10] INT8
  → Conv2D(64, 10×4) → BN → ReLU
  → 4× [DepthwiseConv2D(3×3) → BN → ReLU → Conv2D(64, 1×1) → BN → ReLU]
  → Average Pool → Flatten
  → Dense(5) → Softmax
Output [1, 5] INT8
```

### Model Constraints (Non-Negotiable)
| Constraint | Requirement |
|---|---|
| Input shape | `[1, 51, 1, 10]` INT8 |
| Output shape | `[1, 5]` INT8 |
| Max model size | 60 KB after INT8 quantization |
| Supported TFLM ops | Conv2D, DepthwiseConv2D, Add, MaxPool2D, Mean, FullyConnected, Softmax, Reshape only |
| Quantization | INT8 post-training quantization (input + output both INT8) |
| Inference time | < 15ms per 1s window on ESP32-S3 @ 240MHz |

### Output Classes
| Index | Class | Description |
|---|---|---|
| **0** | **wake_word** | **"Ankit" — the target** |
| 1 | local_negative | Phonetically similar words (Ankita, Anki, Unkit…) |
| 2 | noise | Ambient noise: fan, AC, traffic, crowd |
| 3 | silence | Pure silence or very low energy |
| 4 | unknown | General speech that is not the wake word |

### Trigger Condition (Hardware Team implements this)
```
Trigger if:
  score[0] >= threshold  (INT8 score ≥ 0, meaning ≥ 50% probability)
  AND score[0] > score[1]  (wake_word beats local_negative)
  AND score[0] > score[2]  (wake_word beats noise)
```

---

## 🔑 CRITICAL — MFCC Must Match Firmware Exactly

> ⚠️ This is the most important constraint. If your Python MFCC does not produce the same numbers as the firmware's MFCC, the model will fail completely in production even if it has 99% accuracy in Python tests.

### MFCC Parameters — Non-Negotiable

| Parameter | Value | Why |
|---|---|---|
| Sample rate | 16,000 Hz | Firmware I2S sample rate |
| Window size | 1.0 second = 16,000 samples | KWS sliding window |
| Hop length | 320 samples = 20ms | 51 frames in 1 second |
| FFT size | 512 points | Firmware FFT size |
| Mel filterbanks | 40 intermediate → 10 output | 10 MFCC coefficients |
| Frames per input | 51 | Input tensor time axis |
| Frequency min | 300 Hz | Voice band low cut |
| Frequency max | 8,000 Hz | Voice band high cut |

### Reference Python Implementation
```python
# src/features.py — MUST match firmware computation
import numpy as np
import librosa

def extract_mfcc(audio_samples: np.ndarray, sr: int = 16000) -> np.ndarray:
    """
    Extract MFCC features matching firmware parameters exactly.
    Input:  audio_samples — int16 or float32, length = 16000 (1 second at 16kHz)
    Output: mfcc array of shape (51, 10), float32
    """
    # Normalize to float32 [-1.0, 1.0] if input is int16
    if audio_samples.dtype == np.int16:
        audio = audio_samples.astype(np.float32) / 32768.0
    else:
        audio = audio_samples.astype(np.float32)

    # Pad or trim to exactly 1 second
    target_len = sr  # 16000
    if len(audio) < target_len:
        audio = np.pad(audio, (0, target_len - len(audio)))
    else:
        audio = audio[:target_len]

    mfccs = librosa.feature.mfcc(
        y=audio,
        sr=sr,
        n_mfcc=10,
        n_fft=512,
        hop_length=320,    # 20ms hop → 51 frames in 1 second
        n_mels=40,         # intermediate mel banks
        fmin=300,          # voice band start
        fmax=8000,         # voice band end
    )
    # mfccs shape: (10, ~50 or 51) — transpose to (51, 10)
    mfccs = mfccs.T
    if mfccs.shape[0] < 51:
        mfccs = np.pad(mfccs, ((0, 51 - mfccs.shape[0]), (0, 0)))
    else:
        mfccs = mfccs[:51, :]

    return mfccs  # shape (51, 10), float32


def quantize_to_int8(mfcc_float: np.ndarray,
                     scale: float = None,
                     zero_point: int = -128) -> np.ndarray:
    """Quantize float32 MFCC to INT8 for model input."""
    if scale is None:
        scale = (mfcc_float.max() - mfcc_float.min()) / 255.0 + 1e-8
    q = np.round(mfcc_float / scale + zero_point)
    return np.clip(q, -128, 127).astype(np.int8)
```

---

## 📁 Dataset Design

### Required Classes and Counts

| Class | Target Clips | Duration | Description |
|---|---|---|---|
| `wake_word` | ≥ 500 | 1s each | "Ankit" spoken naturally — various speakers, distances, styles |
| `local_negative` | ≥ 300 | 1s each | Phonetically close: "Ankita", "Anki", "Unkit", "On kit", "Ankeet" |
| `noise` | ≥ 500 | 1s each | Fan, AC, traffic, crowd, music — no speech |
| `silence` | ≥ 300 | 1s each | Pure silence or very quiet room |
| `unknown` | ≥ 500 | 1s each | General English speech — anything not the wake word |

### Audio Format (All Clips Must Be)
| Property | Value |
|---|---|
| Sample rate | 16,000 Hz |
| Bit depth | 16-bit signed PCM |
| Channels | Mono |
| Duration | 1.0 second (pad shorter, trim longer) |
| Format | `.wav` |

### Data Collection
- Record wake word clips from **multiple speakers** (male, female, different accents)
- Record at **multiple distances**: 0.5m, 1m, 2m, 3m
- Record in **multiple environments**: quiet room, room with fan, outdoor ambient
- Request real INMP441 recordings from the Hardware Team — these are the most valuable samples
- Use open-source datasets for `unknown` and `noise` classes (Google Speech Commands, ESC-50, etc.)

### Data Augmentation (Apply During Training)
```python
# Apply these to increase robustness — especially for wake_word class
augmentations = [
    "add_background_noise",       # inject noise clips at SNR 5–30 dB
    "room_impulse_response",      # simulate different room acoustics
    "volume_perturbation",        # ±6 dB gain variation
    "time_shift",                 # ±50ms shift within the 1s window
    "pitch_shift",                # ±2 semitones
    "speed_perturbation",         # 0.9× to 1.1× speed
]
```

---

## 📋 Task List (Build From Scratch)

### Phase 1 — Setup & Data (Week 1)
- [ ] Clone repo, set up Python virtual environment
- [ ] Install all dependencies from `requirements.txt`
- [ ] Collect and organize initial dataset (at minimum 100 wake word clips)
- [ ] Implement `src/features.py` — MFCC extraction with exact parameters above
- [ ] Write `tests/test_features.py` — verify output shape is `(51, 10)` for 1s of audio
- [ ] Implement `src/dataset.py` — load WAV files, extract MFCCs, split train/val/test
- [ ] Push to `experiments/data-pipeline`

### Phase 2 — Model Training (Week 2)
- [ ] Implement model architecture in `src/model.py` (TENet or DS-CNN)
- [ ] Implement training script `src/train.py` with early stopping and best-model checkpoint
- [ ] Apply data augmentation in training pipeline
- [ ] Train initial model — target > 85% val accuracy on wake word recall
- [ ] Implement `src/evaluate.py` — confusion matrix, precision/recall/F1 per class
- [ ] Implement INT8 post-training quantization in `src/convert.py`
- [ ] Verify quantized model size < 60 KB
- [ ] Push to `experiments/model-v1`

### Phase 3 — Delivery & Integration (Week 3)
- [ ] Coordinate with Hardware Team — request 50+ real INMP441 recordings
- [ ] Retrain/fine-tune with real hardware recordings
- [ ] Verify model on real hardware (send tflite to Hardware Team for test flash)
- [ ] Document model spec in `models/model_spec_v1.json`
- [ ] Open issue on `sih-hardware-firmware` tagged `model-update` with specs
- [ ] Tag `v1.0` on `main`

---

## 🛠️ Environment Setup (Your Machine)

```bash
# Clone your repo
git clone https://github.com/QuantumSIHCracker/sih-ml-models.git
cd sih-ml-models

# Create virtual environment
python3 -m venv venv
source venv/bin/activate        # Windows: venv\Scripts\activate

# Install dependencies
pip install -r requirements.txt
```

### `requirements.txt`
```
tensorflow>=2.13.0
numpy>=1.24
scipy>=1.10
librosa>=0.10
scikit-learn>=1.3
matplotlib>=3.7
soundfile>=0.12
audiomentations>=0.30
tflite-runtime>=2.13
jupyter>=1.0
```

### Recommended Repo Structure
```
sih-ml-models/
├── data/
│   ├── raw/               # Original WAV clips — add to .gitignore (too large)
│   └── processed/         # MFCC .npy arrays — add to .gitignore
├── models/
│   ├── kws_model_v1.tflite    # Quantized model — commit this
│   └── model_spec_v1.json     # Model spec for Hardware Team — commit this
├── notebooks/
│   ├── 01_data_exploration.ipynb
│   ├── 02_feature_extraction.ipynb
│   ├── 03_training.ipynb
│   └── 04_quantization_and_eval.ipynb
├── src/
│   ├── features.py        # MFCC extraction (matches firmware)
│   ├── dataset.py         # Dataset loading + augmentation
│   ├── model.py           # Model architecture
│   ├── train.py           # Training script
│   ├── evaluate.py        # Metrics + confusion matrix
│   └── convert.py         # TFLite INT8 quantization
├── tests/
│   └── test_features.py   # MUST pass before any training run
├── .gitignore
├── requirements.txt
└── README.md
```

### `.gitignore` for ML repo
```
data/raw/
data/processed/
data/augmented/
__pycache__/
*.pyc
venv/
*.h5
saved_model/
*.ckpt
```

---

## 📤 Deliverable Format for Hardware Team

When your model is ready, deliver:

### 1. TFLite File
```
models/kws_model_v1.tflite
```

### 2. Model Spec JSON
```json
{
  "model_version": "v1.0",
  "architecture": "TENet",
  "input_shape": [1, 51, 1, 10],
  "input_dtype": "INT8",
  "output_shape": [1, 5],
  "output_dtype": "INT8",
  "classes": {
    "0": "wake_word",
    "1": "local_negative",
    "2": "noise",
    "3": "silence",
    "4": "unknown"
  },
  "wake_word": "Ankit",
  "wake_word_class_index": 0,
  "recommended_threshold_int8": 0,
  "input_scale": 0.00392,
  "input_zero_point": -128,
  "file_size_bytes": 0,
  "val_accuracy": 0.0,
  "wake_word_recall": 0.0,
  "false_positive_rate": 0.0,
  "mfcc_params": {
    "sample_rate": 16000,
    "n_fft": 512,
    "hop_length": 320,
    "n_mels": 40,
    "n_mfcc": 10,
    "fmin": 300,
    "fmax": 8000,
    "frames": 51
  }
}
```

### 3. Notify Hardware Team
Open an issue on `sih-hardware-firmware` tagged `model-update`:
- Model version and file link
- Any changes to input shape or class order from previous version
- Accuracy metrics and recommended threshold

---

## 🎯 Design Targets

| Metric | Target |
|---|---|
| Wake word recall (true positive rate) | > 95% |
| False positive rate (random speech) | < 2% |
| False positive rate (silence/noise) | < 0.5% |
| Model size (INT8 quantized) | < 60 KB |
| Inference time on ESP32-S3 @ 240MHz | < 15ms |

---

## 🔗 Git Workflow

### Repository
```
https://github.com/QuantumSIHCracker/sih-ml-models
```

### Initial Setup
```bash
git clone https://github.com/QuantumSIHCracker/sih-ml-models.git
cd sih-ml-models
git config user.name "Your Name"
git config user.email "your@email.com"
```

### Daily Workflow
```bash
git checkout dev && git pull origin dev
git checkout -b experiments/<experiment-name>

# ... run experiments ...

git add models/ src/ notebooks/ tests/
git commit -m "experiment: describe what you tried and result"
git push origin experiments/<name>

# Open PR: experiments/<name> → dev (when it's a good result)
# dev → main: only when model is validated on real hardware
```

### Commit Message Format
```
experiment: TENet with noise augmentation — 91% wake recall
feat(model): add DS-CNN architecture option
feat(features): implement MFCC extraction matching firmware params
fix(dataset): correct hop_length to 320 for 51-frame output
data: add 150 real INMP441 recordings from hardware team
docs(model): update model spec JSON for v1.1
```

### Large Files
```bash
# Track model files with Git LFS
git lfs install
git lfs track "*.tflite"
git lfs track "*.npy"
git add .gitattributes
```

---

## 🤝 Integration Points

### → Hardware Team (what you deliver)
- `models/kws_model_vX.tflite` (INT8, < 60KB)
- `models/model_spec_vX.json` (input shape, classes, threshold, MFCC params)
- GitHub issue on their repo: `model-update`

### ← Hardware Team (what they give you)
- Real INMP441 audio recordings (`last_utterance.wav` from testing sessions)
- False positive/negative reports with audio clips
- Room noise floor measurements

### ↔ Server Team (shared info)
- Wake word string ("Ankit") and known Whisper mishearing variants
- Server also does a second-pass regex verification on the transcript

---

## 🤖 AI Prompt — Start Your Work

```
You are an expert ML engineer specializing in:
- Keyword Spotting (KWS) for microcontrollers
- TensorFlow/Keras model design (TENet, DS-CNN, MobileNet-V3)
- MFCC audio feature extraction with librosa
- INT8 post-training quantization with TFLite
- TensorFlow Lite Micro (TFLM) compatibility constraints

PROJECT: Smart India Hackathon 2026 — on-device wake word detection
TARGET DEVICE: ESP32-S3 (Xtensa LX7 @ 240MHz), running TFLite Micro
WAKE WORD: "Ankit" (5-class softmax: wake_word, local_negative, noise, silence, unknown)

MODEL CONSTRAINTS:
- Input: [1, 51, 1, 10] INT8 (51 time frames × 10 MFCC bins)
- Output: [1, 5] INT8
- Max size: 60KB after INT8 quantization
- TFLM ops allowed: Conv2D, DepthwiseConv2D, Add, MaxPool2D, Mean, FullyConnected, Softmax, Reshape
- Inference must complete in < 15ms at 240MHz

CRITICAL MFCC CONSTRAINT (must match firmware exactly):
- 16kHz sample rate
- 512-point FFT
- 10 mel bins (n_mfcc=10, n_mels=40)
- 320-sample hop (20ms) → 51 frames per 1-second window
- fmin=300Hz, fmax=8000Hz
- Input: 1 second of audio = 16,000 samples

TARGETS:
- Wake word recall > 95%
- False positive rate < 2%

My current task: [DESCRIBE WHAT YOU WANT TO DO]

Provide clean, well-commented, production-quality Python code.
```

---

## 📞 Communication Protocol

- **Daily standup**: `[ML] Done: X | Doing: Y | Blocked: Z`
- **Model ready**: Open issue on `sih-hardware-firmware` tagged `model-update`
- **Need real recordings**: Message Hardware Team with exact format requirements
- **Protocol changes** (input shape etc.): Must discuss ALL teams before changing — create issue tagged `protocol-change`
