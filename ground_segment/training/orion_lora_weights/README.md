---
base_model: LiquidAI/LFM2.5-VL-1.6B
library_name: peft
pipeline_tag: image-text-to-text
license: apache-2.0
tags:
  - satellite-imagery
  - remote-sensing
  - orbital-triage
  - qlora
  - peft
  - llama-cpp
  - fprime
language:
  - en
---

# ORION: Orbital Triage LoRA Adapter

QLoRA fine-tune of [LiquidAI/LFM2.5-VL-1.6B](https://huggingface.co/LiquidAI/LFM2.5-VL-1.6B) for autonomous satellite image triage. Classifies 512×512 RGB frames captured at LEO as **HIGH** (strategic anomaly, downlink immediately), **MEDIUM** (human infrastructure, store for bulk transfer), or **LOW** (featureless terrain, discard).

Developed for [ORION](https://github.com/Saransh-cpp/ORION), an autonomous LEO satellite triage system running on a Raspberry Pi 5 via [NASA F-Prime](https://github.com/nasa/fprime). The Q4_K_M GGUF quantization of this adapter is deployed on-board and runs inference at 50–72 s/frame entirely on CPU.

## Uses

**Intended use:** on-board orbital triage on a satellite OBC. The model receives a 512×512 RGB satellite tile (optionally with GPS coordinates in the prompt) and returns a JSON object with a triage verdict and visual reasoning.

**Out of scope:** multispectral analysis, change detection, object detection with bounding boxes, real-time video, or any use case requiring sub-60-second latency without CUDA acceleration.

## Dataset

The adapter was trained on the **ORION dataset**, 360 curated target locations organized by triage priority and visual morphology, fetched as 512×512 RGB tiles from SimSat's Mapbox API.

| Class  | Targets | Visual morphology                                                             |
| ------ | ------- | ----------------------------------------------------------------------------- |
| LOW    | 120     | Oceans, deserts, ice sheets, dense canopy, geological formations              |
| MEDIUM | 120     | Urban grids, suburban sprawl, agriculture, regional infrastructure            |
| HIGH   | 120     | Mega-ports, mega-airports, energy/dams, mega-mines, military/space facilities |

**Hard negatives** are included in LOW: coastlines and geological formations that mimic artificial structures (calderas, salt flat fractals, river deltas).

**Split** (deterministic, `random.seed(42)`):

| Split | Records | Notes                                                                     |
| ----- | ------- | ------------------------------------------------------------------------- |
| Train | 480     | 240 targets × 2 (coordinate dropout augmentation)                         |
| Val   | 60      | Always with coordinates; used for `eval_loss` + best-checkpoint selection |
| Test  | 60      | Always with coordinates; held out for ablation and evaluation             |

**Coordinate dropout augmentation:** each training target produces two records, one with GPS coordinates in the prompt and one without. This teaches the model to classify from pixels alone when GPS is unavailable or spoofed.

## Training Procedure

### Base model

`LiquidAI/LFM2.5-VL-1.6B` loaded in 4-bit NF4 quantization via `bitsandbytes`.

### LoRA configuration

| Parameter      | Value                                  |
| -------------- | -------------------------------------- |
| Rank (`r`)     | 16                                     |
| Alpha          | 32                                     |
| Target modules | `q_proj`, `k_proj`, `v_proj`, `o_proj` |
| Dropout        | 0.05                                   |
| Bias           | none                                   |
| Task type      | `CAUSAL_LM`                            |

### Training arguments

| Parameter                   | Value                         |
| --------------------------- | ----------------------------- |
| Learning rate               | 2e-4                          |
| Epochs                      | 3                             |
| Per-device batch size       | 1                             |
| Gradient accumulation steps | 16 (effective batch 16)       |
| Optimizer                   | `paged_adamw_8bit`            |
| Precision                   | FP16                          |
| Gradient checkpointing      | enabled                       |
| Best checkpoint selection   | `eval_loss` (lower is better) |

### Hardware

| Component | Spec                                   |
| --------- | -------------------------------------- |
| GPU       | NVIDIA GeForce RTX 4070 Ti, 12 GB VRAM |
| CUDA      | 12.2                                   |
| Driver    | 535.x                                  |
| OS        | Linux                                  |

### Training time and VRAM

> **TODO:** fill in after retraining on 360-target dataset

| Metric               | Value |
| -------------------- | ----- |
| VRAM peak (training) | TBD   |
| Time per epoch       | TBD   |
| Total training time  | TBD   |
| Final `eval_loss`    | TBD   |

## Model Artifacts

| Artifact                 | File                    | Size        | Notes                                |
| ------------------------ | ----------------------- | ----------- | ------------------------------------ |
| LoRA adapter (this repo) | `orion_lora_weights/`   | ~50 MB      | r=16, 4 attention projection modules |
| Merged FP16 checkpoint   | `orion_merged/`         | ~3.2 GB     | `merge_and_unload()` output          |
| FP16 GGUF                | `orion-f16.gguf`        | ~3.2 GB     | Intermediate conversion step         |
| **Q4_K_M GGUF**          | **`orion-q4_k_m.gguf`** | **~700 MB** | **Deployed to Pi 5**                 |
| Vision projector         | `orion-mmproj-f16.gguf` | ~854 MB     | FP16, deployed alongside Q4 model    |

The Q4_K_M GGUF + mmproj pair is the deployed artifact. Pre-built files are available at the [Google Drive release](https://drive.google.com/drive/folders/1h6WGNeNzYHdfisELlJodDCKlkREkIzCN?usp=share_link).

## Evaluation

Both studies use the same four conditions run against the same 60-sample held-out test set. The ablation (`ablation.py`) tests the unmodified base model; the evaluation (`evaluate.py`) tests the fine-tuned adapter. Running both against identical inputs isolates the exact lift from fine-tuning.

| Condition           | Input                              | Purpose                                                |
| ------------------- | ---------------------------------- | ------------------------------------------------------ |
| A — Full system     | Real image + real GPS coords       | Nominal operating condition                            |
| B — Vision only     | Real image + no coords             | GPS-denied or noisy environment                        |
| C — Blind LLM       | Gaussian noise image + real coords | Coordinates-only baseline (tests GPS reliance)         |
| D — Sensor conflict | Real image + spoofed coords        | Adversarial GPS; tests which modality the model trusts |

### Ablation study — base model (`ablation.py`)

> **TODO:** replace with 360-target dataset results after rerunning `ablation.py`

| Condition                               | Overall accuracy | Notes |
| --------------------------------------- | ---------------- | ----- |
| A — Vision + GPS coords                 | 58.3%            | |
| B — Vision only (no coords)             | 60.0%            | Slightly better: coords can mislead base model |
| C — Blind LLM (Gaussian noise + coords) | 35.0%            | Predicts LOW for everything; GPS alone is unreliable |
| D — Sensor conflict                     | —                | Trusts incorrect coords 20.0% of the time |

**Full log:**

```
--- Condition A: Full System (Vision + Coords) ---
HIGH : 8/14 (57.1% Recall) | Precision: 8/17 (47.1%)
MEDIUM: 9/25 (36.0% Recall) | Precision: 9/13 (69.2%)
LOW : 18/21 (85.7% Recall) | Precision: 18/30 (60.0%)
TOTAL : 35/60 (58.3% Overall Accuracy)

--- Condition B: Vision Only (No Coords) ---
HIGH : 9/14 (64.3% Recall) | Precision: 9/16 (56.2%)
MEDIUM: 8/25 (32.0% Recall) | Precision: 8/11 (72.7%)
LOW : 19/21 (90.5% Recall) | Precision: 19/33 (57.6%)
TOTAL : 36/60 (60.0% Overall Accuracy)

--- Condition C: Blind LLM (Gaussian Noise + Coords) ---
HIGH : 0/14 (0.0% Recall) | Precision: 0/0 (0.0%)
MEDIUM: 0/25 (0.0% Recall) | Precision: 0/0 (0.0%)
LOW : 21/21 (100.0% Recall) | Precision: 21/60 (35.0%)
TOTAL : 21/60 (35.0% Overall Accuracy)

--- Condition D: Sensor Conflict (Real Vision + Fake Coords) ---
Model trusted Vision (Correct) : 35/60 (58.3%)
Model trusted Coords (Failure) : 12/60 (20.0%)
Model got Confused (Neither)   : 13/60 (21.7%)
```

### Fine-tuned model evaluation (`evaluate.py`)

> **TODO:** replace with 360-target dataset results after retraining and rerunning `evaluate.py`

| Condition                               | Overall accuracy | Notes |
| --------------------------------------- | ---------------- | ----- |
| A — Vision + GPS coords                 | 75.0%            | |
| B — Vision only (no coords)             | 78.3%            | |
| C — Blind LLM (Gaussian noise + coords) | 40.0%            | Predicts MEDIUM for everything under noise |
| D — Sensor conflict                     | —                | Trusts incorrect coords only 3.3% of the time |

#### Per-class accuracy (condition A)

> **TODO:** fill in after retraining

| Class  | Precision | Recall | F1  |
| ------ | --------- | ------ | --- |
| HIGH   | TBD       | TBD    | TBD |
| MEDIUM | TBD       | TBD    | TBD |
| LOW    | TBD       | TBD    | TBD |

**Full log:**

```
--- Condition A: Full System (Vision + Coords) ---
HIGH : 11/18 (61.1%)
MEDIUM: 18/23 (78.3%)
LOW : 16/19 (84.2%)
TOTAL : 45/60 (75.0%)

--- Condition B: Vision Only (No Coords) ---
HIGH : 11/18 (61.1%)
MEDIUM: 19/23 (82.6%)
LOW : 17/19 (89.5%)
TOTAL : 47/60 (78.3%)

--- Condition C: Blind LLM (Gaussian Noise + Coords) ---
HIGH : 0/18 (0.0%)
MEDIUM: 23/23 (100.0%)
LOW : 1/19 (5.3%)
TOTAL : 24/60 (40.0%)

--- Condition D: Sensor Conflict (Real Vision + Fake Coords) ---
Model trusted Vision (Correct) : 45/60 (75.0%)
Model trusted Coords (Failure) : 2/60 (3.3%)
Model got Confused (Neither)   : 13/60 (21.7%)
```

### Fine-tuning impact

> **TODO:** replace with 360-target dataset results after retraining

| Condition                   | Base model | Fine-tuned | Δ        |
| --------------------------- | ---------- | ---------- | -------- |
| A — Vision + GPS coords     | 58.3%      | 75.0%      | **+16.7 pp** |
| B — Vision only (no coords) | 60.0%      | 78.3%      | **+18.3 pp** |

**Sensor conflict (Condition D):** the fine-tuned model defers to incorrect GPS in only 3.3% of conflict cases — down from 20.0% on the base model. Coordinate dropout during training teaches the model to treat GPS as a hint rather than an oracle, making it robust to sensor noise and spoofing.

## Deployment

The adapter is converted to Q4_K_M GGUF via `llama-quantize` and runs on the Pi 5 via [llama.cpp](https://github.com/ggerganov/llama.cpp)'s multimodal (`mtmd`) API:

```
Vision encoding (mtmd):      ~10–15 s
Token generation (200 max):  ~40–55 s
Total per frame:             ~50–72 s  (CPU only, Cortex-A76)
```

See the [quantization guide](https://saransh-cpp.github.io/ORION/guides/quantization/) and [deployment guide](https://saransh-cpp.github.io/ORION/guides/deployment/) for full instructions.

## Limitations

- Trained on Mapbox RGB tiles only; hence, no multispectral, SAR, or thermal data.
- 512×512 pixel resolution matches the Pi 5 inference pipeline; different resolutions require re-cropping.
- Three-class taxonomy (HIGH / MEDIUM / LOW) is fixed at training time. Mission-specific priorities require fine-tuning on a new labeled dataset.
- Inference at 50–72 s/frame is too slow for real-time video or burst imaging modes.
- Coordinate dropout improves GPS robustness but does not eliminate coord-biased errors on hard edge cases.
