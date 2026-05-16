# TECHIN515 Lab 4 — Magic Wand Report

## Hardware Setup

<!-- ![hardware setup](assets/report/01_hardware_setup.jpg) -->
*(Hardware photo — to be added)*

**Connections:**
- Handle sensor (LSM6DS3): SA0 → GND → I2C 0x6A
- Sheath sensor (LSM6DS3): SA0 → 3.3V → I2C 0x6B
- NeoPixel strip (8 LEDs): D1
- Button: D0 (INPUT_PULLUP, active LOW)
- SDA: D4 / SCL: D5

---

## Part 1: Data Collection

### Gesture Definitions

| Gesture | Chinese | Motion Description |
|---------|---------|-------------------|
| `pull_out` | 拔剑 | Two sensors move apart — handle pulled from sheath |
| `push_back` | 合剑 | Two sensors move together — blade returned to sheath |
| `slash` | 挥动 | Both sensors swing together in arc |

### Dataset Overview

![dataset overview](assets/report/02_dataset_overview.png)

- Total samples: 120 (96 training / 24 test, 80/20 split)
- Capture: 1 second @ 100 Hz = 100 samples per capture
- Axes captured: ax1, ay1, az1 (handle) + ax2, ay2, az2 (sheath)
- Collectors: Felix + Student B (20 samples each per gesture)

![sample pull_out](assets/report/03_sample_pull_out.png)
![sample push_back](assets/report/04_sample_push_back.png)
![sample slash](assets/report/05_sample_slash.png)

The three gestures produce visually distinct raw waveform signatures:

- **`pull_out` (拔剑):** Both sensor groups show sustained oscillation beginning around 300ms and lasting through the capture window. The handle (ax1/ay1/az1) and sheath (ax2/ay2/az2) axes diverge progressively as the blade separates from the sheath, with amplitude around ±10 m/s².

- **`push_back` (合剑):** The signal is almost entirely flat except for a single sharp impulse around 360–480ms — the moment of contact as the blade is seated back into the sheath. Peak amplitude reaches −60 m/s² on the az axes, making this the most compact and high-amplitude single event of the three gestures.

- **`slash` (挥动):** A large compound waveform occupies the 360–720ms range, with both sensor groups swinging together. The red (ax1) and green (ax2) axes dominate, reflecting the primary swing plane. The signal then decays back toward zero as the wrist decelerates.

These distinct temporal profiles — sustained divergence, single impulse, and synchronized swing — make the gesture classes well-separated in both time and frequency domains.

**Discussion:** Using data collected by multiple people improves model robustness — individual variation in gesture speed, wrist angle, and force is a key source of real-world test failures. Training on a single person's data overfits to that person's style and performs poorly for others.

---

## Part 2: Edge Impulse Model

### Impulse Design

![impulse design](assets/report/06_impulse_design.png)

**Configuration:**
- Window size: 1000 ms
- Window stride: 100 ms
- Frequency: 100 Hz
- Input axes: 6 (ax1, ay1, az1, ax2, ay2, az2)
- Processing block: **Spectral Analysis** — extracts frequency-domain features (power spectral density, peak frequency) from each axis, effective for motion gestures with distinct frequency signatures
- Learning block: **Neural Network (Keras)** — suitable for classifying fixed-length feature vectors

**Justification for Spectral Analysis:** The three gestures differ not only in magnitude but in frequency content. `slash` has high-frequency oscillation; `pull_out`/`push_back` have slower low-frequency profiles. Spectral features expose this separation more cleanly than raw time-domain features.

### DSP Features

![feature explorer](assets/report/07_feature_explorer.png)

The Feature Explorer (2D PCA projection of spectral features) shows clear spatial separation between the three gesture classes:

- **`slash` (green):** Tightly clustered in the upper-left region. The high-frequency synchronized swing produces a consistent spectral signature with low intra-class variance — the model's easiest class to distinguish.
- **`push_back` (orange):** Clustered in the upper-right, well-separated from slash. The single high-amplitude impulse at contact creates a distinctive high-energy, short-duration spectral peak that maps to a compact feature region.
- **`pull_out` (blue):** Occupies the lower-center region. Slightly more spread than the other two classes due to natural variation in how fast different people draw the blade, but still clearly distinct from slash. Some minor overlap with push_back at the cluster boundaries is visible and expected.

Overall, the three clusters are well-separated with no major overlap, indicating that Spectral Analysis extracts features sufficient for reliable classification. The decision boundaries are approximately: slash (upper-left) / push_back (upper-right) / pull_out (lower-center), with the primary discriminating axes corresponding to frequency energy distribution across the two sensor groups.

96 training windows generated (32 per class, perfectly balanced).

### Neural Network Architecture & Training

![training results](assets/report/08_training_results.png)

**Hyperparameters:**

| Parameter | Value |
|-----------|-------|
| Training cycles | 100 |
| Learning rate | 0.0005 |
| Architecture | Dense(20) → Dense(10) → Softmax(3) |
| Minimum confidence | 0.7 |

**Training results (validation set):**

| Metric | Value |
|--------|-------|
| Accuracy | **85.0%** |
| Loss | 0.36 |
| AUC (ROC) | 0.93 |
| Weighted avg Precision | 0.86 |
| Weighted avg Recall | 0.85 |
| Weighted avg F1 | 0.85 |

**Confusion matrix (validation set):**

| | Predicted pull_out | Predicted push_back | Predicted slash |
|---|---|---|---|
| **pull_out** | **80%** | 20% | 0% |
| **push_back** | 20% | **80%** | 0% |
| **slash** | 0% | 0% | **100%** |
| F1 | 0.84 | 0.73 | 1.00 |

`slash` achieves perfect classification (F1 = 1.00). The primary error is mutual confusion between `pull_out` and `push_back` (20% each), which is expected: both gestures involve relative motion between the two sensors along the same axis, differing only in direction. Spectral Analysis captures frequency content but not directionality — a known limitation of this approach for opposing-direction gestures.

### Model Testing

![model testing](assets/report/10_model_testing.png)

**Test set results (24 samples, unoptimized float32):**

| Metric | Value |
|--------|-------|
| Test accuracy | **79.17%** |
| AUC (ROC) | 0.98 |
| Weighted avg Precision | 0.96 |
| Weighted avg Recall | 0.96 |
| Weighted avg F1 | 0.96 |

**Test confusion matrix:**

| | pull_out | push_back | slash | uncertain |
|---|---|---|---|---|
| **pull_out** | **75%** | 0% | 0% | 25% |
| **push_back** | 12.5% | **62.5%** | 0% | 25% |
| **slash** | 0% | 0% | **100%** | 0% |
| F1 | 0.80 | 0.77 | 1.00 | — |

The UNCERTAIN column represents samples that fell below the 0.7 confidence threshold and were not assigned a class. `slash` retains perfect accuracy on the test set. `pull_out` and `push_back` each have 25% uncertain predictions, and `push_back` has an additional 12.5% misclassified as `pull_out`. The high AUC (0.98) indicates the model's probability outputs are well-separated even where individual predictions fall below threshold. The gap between training accuracy (85%) and test accuracy (79%) is consistent with the small dataset size (96 training windows).

**Discussion — Two strategies to improve performance:**
1. **Increase training data diversity:** add more participants and capture gestures at varying speeds. Currently 2 people; adding 2–3 more would reduce person-specific overfitting and the pull_out/push_back confusion.
2. **Add relative motion features:** compute the difference between the two sensors' accelerations (ax1−ax2, ay1−ay2, az1−az2) as additional input axes. This explicitly encodes the direction of relative motion between handle and sheath, which is the key discriminator between pull_out and push_back.

---

## Part 3: ESP32 Deployment

### Real-time Performance

![serial monitor output](assets/report/12_serial_output.png)

Serial Monitor output showing real-time inference results with confidence scores (push_back 98.0%, pull_out 82–88%, slash 99.6%).

Testing methodology: 10 repetitions per gesture, recorded prediction vs ground truth.

| Gesture | Correct | Uncertain | Wrong | Accuracy |
|---------|---------|-----------|-------|----------|
| pull_out | | | | |
| push_back | | | | |
| slash | | | | |

### Button-triggered Inference

The wand triggers capture on button press (D0, active LOW) rather than Serial command. A rainbow LED animation plays during the 1-second capture window. After inference:

- `pull_out` → Blue sweep animation (Expecto Patronum — 拔剑)
- `push_back` → Green breathing pulse × 3 (Episkey — 合剑)
- `slash` → Red strobe × 3 then hold (Sectumsempra — 挥动)
- Uncertain → Grey solid 1.5s

---

## Part 4: Battery & Enclosure

<!-- ![enclosure](assets/report/13_enclosure.jpg) -->
*(Enclosure photo — to be added)*

---

## Demo Video

[Link to demo video — show button press → rainbow capture → LED color result]

---

## Challenges & Solutions

| Challenge | Solution |
|-----------|----------|
| Auto-detect failed for XIAO port | Added "JTAG"/"usbmodem" keywords to port detection |
| Both sensors same I2C address (0x6B) | Rewrote I2C scanner; found SA0 pin wiring error; moved handle SA0 → GND |
| MPU6050 library wrong for LSM6DS3 | Switched to `Adafruit_LSM6DS3`, updated addresses 0x6A/0x6B |
| Serial Monitor blocking Python script | Close Arduino Serial Monitor before running capture script |
| NeoPixel not lighting up | Found NeoPixel on D1 not D0; corrected pin definition in firmware |
