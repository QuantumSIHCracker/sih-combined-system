# 🧠 ML Team — Complete Working Plan
### Smart India Hackathon (SIH) 2026 | QuantumSIHCracker | Team: Machine Learning

> **Repository**: `https://github.com/QuantumSIHCracker/sih-ml-models`
> **Branch Strategy**: `main` (protected) → `dev` → `experiments/<name>` branches
> **Working Machine**: Your own computer (not Arpit's machine)

---

## 📌 Your Mission

You are the **ML Team**. Your job is to:
1. **Train and maintain** the Keyword Spotting (KWS) model that runs ON the ESP32-S3 chip
2. **Optimize the model** to be fast, small (< 60KB), and accurate within INT8 quantization constraints
3. **Deliver the model** as a `.tflite` file + C header for the Hardware Team to embed
4. **Benchmark and improve** accuracy on real-world noisy voice samples from the Hardware Team

You are the **intelligence inside the device**. Without you, the device can't hear its name.

---

## 🏗️ System Context — Where You Fit

```
                        ┌─── YOU ARE HERE ───┐
[Audio (16kHz, 16-bit)] → [MFCC Feature Extraction] → [TENet KWS Model] → [Wake Word Decision]
                                                              │
                                              Your .tflite → Hardware Team embeds in firmware
                                              Your training data → curated from real recordings
```

The model runs **on-chip** on the ESP32-S3 using **TensorFlow Lite Micro (TFLM)**:
- Input: `[1, N_FRAMES, 1, N_MFCC_BINS]` INT8 tensor
- Output: `[1, N_CLASSES]` INT8 softmax probabilities
- Inference: ~5–15ms per window on ESP32-S3 @ 240MHz
- **Maximum model size**: ~60KB (current: 57,264 bytes ✅)

---

## 📚 Current Model Status

### Existing Model (Baseline)
| Property | Value |
|---|---|
| Architecture | TENet (Temporal Efficient Network) — Inverted Residual CNN |
| Input Shape | `[1, 51, 1, 10]` INT8 |
| Output Shape | `[1, 5]` INT8 |
| Classes | `wake_word("Ankit")`, `local_negative`, `noise`, `ambient`, `unknown` |
| Size | 57,264 bytes (~56KB) |
| Quantization | INT8 (post-training quantization) |
| Wake Threshold | Raw INT8 score ≥ 0 (≥ 50% probability) + wake > negative + wake > noise |
| Location | `/home/arpit_ubuntu/Smart India hackathon/KWS_model 1.tflite` |

### MFCC Feature Extraction (Currently in Firmware)
| Parameter | Value |
|---|---|
| Sample Rate | 16,000 Hz |
| Window Size | 1.0 second |
| Hop Size | 200ms |
| FFT Points | 512 |
| Mel Filterbanks | 10 |
| Temporal Frames | 51 |
| Feature Vector | 51 × 10 INT8 MFCC spectrogram |

---

## 🎯 Improvement Goals

### Priority 1 — Accuracy
- **Target**: >95% True Positive Rate for "Ankit" in noisy environments
- **Target**: <2% False Positive Rate on random speech
- **Target**: <0.5% False Positive Rate on silence/noise

### Priority 2 — Latency & Size
- **Target**: Inference time <10ms per 1s window on ESP32-S3
- **Target**: Model size <60KB (INT8 quantized)
- **Target**: RAM usage <10KB during inference

### Priority 3 — Robustness
- **Target**: Works at 0.5m to 3m from microphone
- **Target**: Works in environments with TV/AC noise (SNR ≥ 10dB)
- **Target**: Works across male, female, child voices

---

## 📁 Dataset

### Existing Dataset
- Location: `/home/arpit_ubuntu/Smart India hackathon/dataset_v2-20260907T072628Z-1-001.zip`
- After extracting, review the folder structure for class organization

### Dataset Requirements
| Class | Description | Target Samples |
|---|---|---|
| `wake_word` | "Ankit" spoken naturally | ≥ 500 clips |
| `local_negative` | Phonetically similar: "Ankit", "Ankita", "Anki", "Unkit" | ≥ 300 clips |
| `noise` | Ambient noise: fan, AC, traffic, crowd | ≥ 500 clips |
| `silence` | Pure silence or very quiet rooms | ≥ 300 clips |
| `unknown` | Random English speech (not the wake word) | ≥ 500 clips |

### Audio Specifications
- **Sample Rate**: 16,000 Hz (MUST match firmware)
- **Bit Depth**: 16-bit PCM WAV
- **Duration**: 1.0 second clips (pad/trim as needed)
- **Channels**: Mono
- **Format**: `.wav` files

### Data Augmentation (Apply During Training)
```python
# Augmentations to apply for robustness:
augmentations = [
    "background_noise_injection",    # SNR 5–30 dB
    "room_impulse_response",         # Simulate different room acoustics
    "volume_perturbation",           # ±6 dB gain variation
    "time_shift",                    # ±50ms shift
    "pitch_shift",                   # ±2 semitones
    "speed_perturbation",            # 0.9x – 1.1x
    "microphone_noise",              # INMP441 noise floor simulation
]
```

### Data Collection from Hardware Team
- Hardware Team can record real INMP441 samples using the firmware's `last_utterance.wav` output
- Request 50+ real wake word recordings from various team members
- These are gold-standard samples for your test set

---

## 🏋️ Model Architecture

### Recommended: TENet (Current) — Keep if accuracy is sufficient
```python
# TENet Architecture (simplified):
# Input: [batch, 51, 1, 10] (time_frames, 1, mfcc_bins)
# 
# Block 1: Conv2D(32, 3x3) → BN → ReLU
# Block 2: DepthwiseConv2D(3x3) → BN → ReLU → Conv2D(32, 1x1) → BN → ReLU (Inverted Residual)
# Block 3: DepthwiseConv2D(3x3) → BN → ReLU → Conv2D(64, 1x1) → BN → ReLU
# Global Average Pool
# Fully Connected → Softmax(5 classes)
```

### Alternative: MobileNet-V3 Small or DS-CNN
If TENet accuracy is insufficient, these are well-proven KWS alternatives:
- **DS-CNN (Depthwise Separable CNN)**: Excellent baseline, ~32KB INT8
- **Temporal Convolution Network (TCN)**: Better for timing-sensitive wake words
- **BC-ResNet**: State-of-art but larger (~80KB)

**Constraint**: Whatever architecture you use MUST be compatible with these TFLM ops:
- `Conv2D`, `DepthwiseConv2D`, `Add`, `MaxPool2D`, `Mean`, `FullyConnected`, `Softmax`, `Reshape`

---

## 📋 Your Task List (Priority Order)

### Phase 1 — Setup & Baseline (Week 1)
- [ ] **Clone the repo** and set up Python environment
- [ ] **Extract and organize dataset** from the zip file
- [ ] **Run baseline evaluation** on existing `KWS_model_1.tflite` with your test data
- [ ] **Implement MFCC feature extraction pipeline** (must match firmware exactly):
  ```python
  # MUST match firmware parameters:
  SAMPLE_RATE = 16000
  WINDOW_SEC = 1.0  # 16000 samples
  HOP_SEC = 0.2     # 3200 samples
  FFT_SIZE = 512
  N_MELS = 10
  N_FRAMES = 51
  ```
- [ ] **Verify feature parity**: Extract features in Python, compare against firmware output
- [ ] Push setup code to `experiments/baseline-eval`

### Phase 2 — Training & Optimization (Week 2)
- [ ] **Collect additional data** — coordinate with Hardware Team for real INMP441 recordings
- [ ] **Apply data augmentation** pipeline
- [ ] **Train improved model** with expanded dataset
- [ ] **Post-training INT8 quantization** using TFLite converter:
  ```python
  converter = tf.lite.TFLiteConverter.from_saved_model(saved_model_path)
  converter.optimizations = [tf.lite.Optimize.DEFAULT]
  converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
  converter.inference_input_type = tf.int8
  converter.inference_output_type = tf.int8
  # Provide representative dataset for calibration
  converter.representative_dataset = representative_dataset_gen
  tflite_model = converter.convert()
  ```
- [ ] **Verify model size < 60KB** after quantization
- [ ] **Benchmark accuracy**: precision, recall, F1, confusion matrix
- [ ] **Verify model runs on ESP32-S3** — send `.tflite` to Hardware Team for integration test
- [ ] Push to `experiments/improved-model`

### Phase 3 — Integration & Polish (Week 3)
- [ ] **Fine-tune based on hardware team feedback** (real-world false positive/negative reports)
- [ ] **Document final model specs** (input tensor, classes, thresholds, quantization params)
- [ ] **Create model card** with accuracy metrics, training data stats, limitations
- [ ] **Tag final model v1.0** and deliver to Hardware Team
- [ ] Push to `main` as `models/kws_model_v1.tflite`

---

## 🛠️ Development Environment Setup (Your Machine)

### Python Environment
```bash
# Create and activate virtual environment
python3 -m venv venv
source venv/bin/activate  # Windows: venv\Scripts\activate

# Install dependencies
pip install tensorflow>=2.13 \
            numpy scipy librosa \
            scikit-learn matplotlib \
            soundfile audiomentations \
            tflite-runtime jupyter
```

### Recommended `requirements.txt`
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

### Project Structure (your repo)
```
sih-ml-models/
├── data/
│   ├── raw/              # Original audio clips (not committed — add to .gitignore)
│   ├── processed/        # MFCC feature arrays (.npy)
│   └── augmented/        # Augmented samples
├── models/
│   ├── kws_model_v1.tflite    # Current best model (committed)
│   └── kws_model_vX.tflite    # Experimental versions
├── notebooks/
│   ├── 01_data_exploration.ipynb
│   ├── 02_feature_extraction.ipynb
│   ├── 03_model_training.ipynb
│   ├── 04_quantization.ipynb
│   └── 05_evaluation.ipynb
├── src/
│   ├── features.py       # MFCC extraction (must match firmware)
│   ├── dataset.py        # Dataset loading and augmentation
│   ├── model.py          # Model architecture definitions
│   ├── train.py          # Training script
│   ├── evaluate.py       # Evaluation and confusion matrix
│   └── convert.py        # TFLite conversion + INT8 quantization
├── tests/
│   └── test_features.py  # Verify feature extraction matches firmware
├── requirements.txt
└── README.md
```

---

## 🔑 Critical Technical Constraint — MFCC Must Match Firmware

The firmware computes a 51×10 MFCC spectrogram on-chip. Your Python training pipeline **MUST compute the exact same features** or the model will fail in production. 

```python
# features.py — MUST match firmware's arduinoFFT computation
import librosa
import numpy as np

def extract_mfcc_firmware_compatible(audio_samples, sr=16000):
    """
    Extract MFCC features matching the ESP32-S3 firmware's arduinoFFT 
    computation exactly. Parameters are non-negotiable.
    """
    # Match firmware's 512-point FFT, 10 mel bins, 51 frames
    mfccs = librosa.feature.mfcc(
        y=audio_samples.astype(np.float32) / 32768.0,
        sr=sr,
        n_mfcc=10,
        n_fft=512,
        hop_length=320,   # 20ms hop at 16kHz = 320 samples → 51 frames in 1s
        n_mels=40,        # intermediate mel banks before DCT → 10 coefficients
        fmin=300,         # voice band start (matches firmware gatekeeper)
        fmax=8000,
    )
    # Shape: (10, ~51) → transpose and pad/trim to exactly (51, 10)
    mfccs = mfccs.T[:51, :]  # (51, 10)
    if mfccs.shape[0] < 51:
        mfccs = np.pad(mfccs, ((0, 51 - mfccs.shape[0]), (0, 0)))
    return mfccs  # Shape: (51, 10)

def quantize_to_int8(features, scale=None, zero_point=None):
    """Quantize float features to INT8 for model input."""
    if scale is None:
        scale = features.std() * 4
    if zero_point is None:
        zero_point = 0
    quantized = np.clip(np.round(features / scale + zero_point), -128, 127)
    return quantized.astype(np.int8)
```

---

## 📤 Deliverable Format for Hardware Team

When you have a new model ready:

### 1. Provide the `.tflite` file
```
models/kws_model_vX.tflite
```

### 2. Provide model specification JSON
```json
{
  "model_version": "v1.1",
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
  "input_quantization": {"scale": 0.00392, "zero_point": -128},
  "file_size_bytes": 57264,
  "test_accuracy": 0.96,
  "test_recall_wake_word": 0.94,
  "test_false_positive_rate": 0.018
}
```

### 3. Notify Hardware Team
Open a GitHub issue in `sih-hardware-firmware` tagged `model-update` with:
- Model version and accuracy stats
- Any changes to input tensor shape or class order
- Link to your PR in `sih-ml-models`

---

## 🔗 Git Workflow (ML Team)

### Repository
```
https://github.com/QuantumSIHCracker/sih-ml-models
```

### Initial Setup
```bash
git clone https://github.com/QuantumSIHCracker/sih-ml-models.git
cd sih-ml-models
git config user.name "Your Name"
git config user.email "your-email@example.com"

# Create .gitignore for large files
echo "data/raw/" >> .gitignore
echo "data/augmented/" >> .gitignore
echo "__pycache__/" >> .gitignore
echo "*.pyc" >> .gitignore
echo "venv/" >> .gitignore
echo "*.h5" >> .gitignore
echo "saved_model/" >> .gitignore
```

### Daily Workflow
```bash
# Start new experiment
git checkout dev
git pull origin dev
git checkout -b experiments/<experiment-name>

# After results
git add models/ src/ notebooks/ tests/
git commit -m "experiment: <describe what you tried and result>"
git push origin experiments/<experiment-name>

# If it's a good model, open PR: experiments/<name> → dev
# When dev has a release-ready model: dev → main
```

### Commit Convention
```
experiment: TENet + augmentation → 94% recall (was 88%)
feat(model): add BC-ResNet architecture option
fix(features): correct hop_length to 320 for 51 frames
data: add 200 real INMP441 recordings from hardware team
```

### Large File Handling
```bash
# For models > 50MB, use Git LFS:
git lfs install
git lfs track "*.tflite"
git lfs track "*.h5"
git lfs track "*.npy"
git add .gitattributes
```

---

## 🤝 Integration Points

### → Hardware Team (your deliverables)
- `.tflite` model file
- Model spec JSON (input shape, classes, threshold)
- Notify via GitHub issue when new model is ready

### ← Hardware Team (what they give you)
- Real audio recordings from INMP441 (`last_utterance.wav` files)
- False positive/negative reports from real-world testing
- Acoustic environment details (room noise floor, distance from mic)

### ← Server Team (collaboration)
- Server also does KWS verification using regex on Whisper transcript
- Coordinate on the exact wake word string ("Ankit") spelling/variants
- Server team can help evaluate ASR output for model debugging

---

## 🤖 AI Prompt to Start Your Work

```
You are an expert machine learning engineer specializing in embedded keyword spotting (KWS) for microcontrollers, TensorFlow/Keras model design, MFCC audio feature extraction, INT8 post-training quantization, and TensorFlow Lite (TFLite) model optimization.

PROJECT CONTEXT:
We are building a Smart India Hackathon (SIH) voice assistant. I am on the ML team responsible for training and maintaining the Keyword Spotting model that runs ON the ESP32-S3 chip using TensorFlow Lite Micro (TFLM).

SYSTEM:
- ESP32-S3 microcontroller (Xtensa LX7, 240MHz)
- INMP441 MEMS microphone at 16kHz, 16-bit PCM
- On-device inference using TFLite Micro
- Max model size: ~60KB after INT8 quantization
- Available TFLM ops: Conv2D, DepthwiseConv2D, Add, MaxPool2D, Mean, FullyConnected, Softmax, Reshape

CURRENT MODEL:
- Architecture: TENet (Inverted Residual CNN)
- Input: [1, 51, 1, 10] INT8 (51 time frames × 10 MFCC bins)
- Output: [1, 5] INT8 (5-class softmax: wake_word, local_negative, noise, silence, unknown)
- Wake word: "Ankit" (class index 0)
- Size: 57,264 bytes
- Trigger condition: INT8 score ≥ 0 (≥50%) AND wake > negative AND wake > noise

CRITICAL CONSTRAINT:
MFCC features in Python training MUST exactly match firmware computation:
- 16kHz sample rate, 512-point FFT, 10 mel bins, 320-sample hop (20ms), 51 frames per 1-second window
- Voice band: 300–8000 Hz

My current task: [DESCRIBE WHAT YOU WANT TO DO]

Please help me with detailed, production-ready code.
```

---

## 📞 Team Communication Protocol

- **Post model updates** in team group: `[ML] New model vX ready: accuracy=X%, recall=X%, size=XKB`
- **Request real data** from Hardware Team when you need more recordings
- **File issues** on Hardware Team's repo when you need specific audio samples or test conditions
- **Protocol changes** (changing input shape etc.): MUST be discussed and agreed BEFORE implementation
