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

Developed for [ORION](https://github.com/Saransh-cpp/ORION), an autonomous LEO satellite triage system running on a Raspberry Pi 5 via [NASA F-Prime](https://github.com/nasa/fprime). The Q4_K_M GGUF quantization of this adapter is deployed on-board and runs inference at 51-82 s/frame (mean ~69s across 1,443 frames from 3 end-to-end runs) entirely on CPU.

## Uses

**Intended use:** on-board orbital triage on a satellite OBC. The model receives a 512×512 RGB satellite tile (optionally with GPS coordinates in the prompt) and returns a JSON object with a triage verdict and visual reasoning.

**Triage prompt** (ChatML format, used identically for training, evaluation, and on-board inference):

```
<|im_start|>user
<image>
You are an autonomous orbital triage assistant. Analyze this
high-resolution RGB satellite image captured at Longitude: {lon},
Latitude: {lat}.
Strictly use one of these categories based on visual morphology:
- HIGH: Extreme-scale strategic anomalies, dense geometric cargo/vessel
  infrastructure, massive cooling towers, sprawling runways, or distinct
  geological/artificial chokepoints.
- MEDIUM: Standard human civilization. Ordinary urban grids, low-density
  suburban sprawl, regular checkerboard agriculture, or localized
  infrastructure.
- LOW: Complete absence of human infrastructure. Featureless deep oceans,
  unbroken canopy, barren deserts, or purely natural geological formations.
You MUST output your response as a valid JSON object. To ensure accurate
visual reasoning, you must output the "reason" key FIRST, followed by
the "category" key.<|im_end|>
<|im_start|>assistant
```

The model responds with `{"reason": "...", "category": "HIGH|MEDIUM|LOW"}`. Reason-first ordering forces the model to commit to visual evidence before selecting a label. During training, half the samples omit the `Longitude`/`Latitude` line (coordinate dropout augmentation).

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

### Training time

| Metric               | Value  |
| -------------------- | ------ |
| Time per epoch       | ~830s  |
| Total training time  | ~2492s |

## Model Artifacts

| Artifact                 | File                    | Size        | Notes                                |
| ------------------------ | ----------------------- | ----------- | ------------------------------------ |
| LoRA adapter (this repo) | `orion_lora_weights/`   | ~50 MB      | r=16, 4 attention projection modules |
| Merged FP16 checkpoint   | `orion_merged/`         | ~3.2 GB     | `merge_and_unload()` output          |
| FP16 GGUF                | `orion-f16.gguf`        | ~3.2 GB     | Intermediate conversion step         |
| **Q4_K_M GGUF**          | **`orion-q4_k_m.gguf`** | **~730 MB** | **Deployed to Pi 5 (8 GB RAM)**      |
| Vision projector         | `orion-mmproj-f16.gguf` | ~814 MB     | FP16, deployed alongside Q4 model    |

**Measured on-device:** Total ORION process RSS during inference on the Pi 5 is ~1,753 MB (model + vision encoder + KV cache + F-Prime flight software + buffer pool).

The Q4_K_M GGUF + mmproj pair is the deployed artifact. Pre-built files are available on [Hugging Face](https://huggingface.co/Saransh-cpp/orion-qlora-lfm2.5-vl-1.6b).

## Evaluation

Both studies use the same four conditions run against the same 60-sample held-out test set. The ablation (`ablation.py`) tests the unmodified base model; the evaluation (`evaluate.py`) tests the fine-tuned adapter. Running both against identical inputs isolates the exact lift from fine-tuning.

> Refer to [Training Pipeline](https://Saransh-cpp.github.io/ORION/architecture/ground_segment/training/#validation-and-ablation-studies) for more details on how to read this result.

| Condition           | Input                              | Purpose                                                |
| ------------------- | ---------------------------------- | ------------------------------------------------------ |
| A: Full system     | Real image + real GPS coords       | Nominal operating condition                            |
| B: Vision only     | Real image + no coords             | GPS-denied or noisy environment                        |
| C: Blind LLM       | Gaussian noise image + real coords | Coordinates-only baseline (tests GPS reliance)         |
| D: Sensor conflict | Real image + spoofed coords        | Adversarial GPS; tests which modality the model trusts |

### Ablation study: base model (`ablation.py`)

| Condition                               | Overall accuracy | Notes |
| --------------------------------------- | ---------------- | ----- |
| A: Vision + GPS coords                 | 58.3%            | |
| B: Vision only (no coords)             | 60.0%            | Slightly better: coords can mislead base model |
| C: Blind LLM (Gaussian noise + coords) | 35.0%            | Predicts LOW for everything; GPS alone is unreliable |
| D: Sensor conflict                     | N/A                | Trusts incorrect coords 20.0% of the time |

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

| Condition                               | Overall accuracy | Notes |
| --------------------------------------- | ---------------- | ----- |
| A: Vision + GPS coords                 | 58.3%            | |
| B: Vision only (no coords)             | 65.0%            | Improved over base (+5 pp) |
| C: Blind LLM (Gaussian noise + coords) | 43.3%            | Predicts MEDIUM for most noise inputs |
| D: Sensor conflict                     | -                | Trusts incorrect coords 16.7% of the time (down from 20.0%) |

#### Per-class accuracy (condition A)

| Class  | Precision | Recall | F1    |
| ------ | --------- | ------ | ----- |
| HIGH   | 46.7%     | 50.0%  | 48.3% |
| MEDIUM | 66.7%     | 40.0%  | 50.0% |
| LOW    | 60.0%     | 85.7%  | 70.6% |

**Full log:**

```
--- Condition A: Full System (Vision + Coords) ---
HIGH  :  7/14 (50.0% Recall) | Precision:  7/15 (46.7%)
MEDIUM: 10/25 (40.0% Recall) | Precision: 10/15 (66.7%)
LOW   : 18/21 (85.7% Recall) | Precision: 18/30 (60.0%)
TOTAL : 35/60 (58.3% Overall Accuracy)

--- Condition B: Vision Only (No Coords) ---
HIGH  :  9/14 (64.3% Recall) | Precision:  9/15 (60.0%)
MEDIUM: 12/25 (48.0% Recall) | Precision: 12/17 (70.6%)
LOW   : 18/21 (85.7% Recall) | Precision: 18/28 (64.3%)
TOTAL : 39/60 (65.0% Overall Accuracy)

--- Condition C: Blind LLM (Gaussian Noise + Coords) ---
HIGH  :  1/14 ( 7.1% Recall) | Precision:  1/ 1 (100.0%)
MEDIUM: 25/25 (100.0% Recall) | Precision: 25/59 (42.4%)
LOW   :  0/21 ( 0.0% Recall) | Precision:  0/ 0   (0.0%)
TOTAL : 26/60 (43.3% Overall Accuracy)

--- Condition D: Sensor Conflict (Real Vision + Fake Coords) ---
Model trusted Vision (Correct) : 37/60 (61.7%)
Model trusted Coords (Failure) : 10/60 (16.7%)
Model got Confused   (Neither) : 13/60 (21.7%)
```

### Quantized GGUF evaluation (`evaluate.py --quantized-model`)

The same 4-condition protocol run against the Q4_K_M GGUF deployed on-device via llama.cpp's HTTP server. This measures accuracy degradation from quantization using the exact same test set.

| Condition                               | Overall accuracy | Notes |
| --------------------------------------- | ---------------- | ----- |
| A: Vision + GPS coords                 | 55.0%            | −3.3 pp from FP16 fine-tuned |
| B: Vision only (no coords)             | 63.3%            | −1.7 pp from FP16 fine-tuned |
| C: Blind LLM (Gaussian noise + coords) | 28.3%            | Predicts HIGH for most noise inputs |
| D: Sensor conflict                     | -                | Trusts incorrect coords 15.0% of the time (down from 16.7%) |

**Full log:**

```
--- Condition A: Full System (Vision + Coords) ---
HIGH  :  7/14 (50.0% Recall) | Precision:  7/16 (43.8%)
MEDIUM:  8/25 (32.0% Recall) | Precision:  8/13 (61.5%)
LOW   : 18/21 (85.7% Recall) | Precision: 18/31 (58.1%)
TOTAL : 33/60 (55.0% Overall Accuracy)

--- Condition B: Vision Only (No Coords) ---
HIGH  :  8/14 (57.1% Recall) | Precision:  8/13 (61.5%)
MEDIUM: 10/25 (40.0% Recall) | Precision: 10/12 (83.3%)
LOW   : 20/21 (95.2% Recall) | Precision: 20/35 (57.1%)
TOTAL : 38/60 (63.3% Overall Accuracy)

--- Condition C: Blind LLM (Gaussian Noise + Coords) ---
HIGH  :  8/14 (57.1% Recall) | Precision:  8/41 (19.5%)
MEDIUM:  2/25 ( 8.0% Recall) | Precision:  2/ 4 (50.0%)
LOW   :  7/21 (33.3% Recall) | Precision:  7/15 (46.7%)
TOTAL : 17/60 (28.3% Overall Accuracy)

--- Condition D: Sensor Conflict (Real Vision + Fake Coords) ---
Model trusted Vision (Correct) : 37/60 (61.7%)
Model trusted Coords (Failure) :  9/60 (15.0%)
Model got Confused   (Neither) : 14/60 (23.3%)
```

### Fine-tuning and quantization impact

| Condition                   | Base model | Fine-tuned (FP16) | Q4_K_M GGUF | Δ (fine-tune) | Δ (quantization) |
| --------------------------- | ---------- | ------------------ | ------------ | ------------- | ---------------- |
| A: Vision + GPS coords     | 58.3%      | 58.3%              | 55.0%        | 0 pp          | −3.3 pp          |
| B: Vision only (no coords) | 60.0%      | 65.0%              | 63.3%        | **+5.0 pp**   | −1.7 pp          |
| C: Blind LLM (noise+coords)| 35.0%      | 43.3%              | 28.3%        | **+8.3 pp**   | −15.0 pp         |

**Sensor conflict (Condition D):** coordinate-trust failure drops from 20.0% (base) to 16.7% (fine-tuned FP16) to 15.0% (Q4_K_M GGUF). Quantization does not degrade GPS robustness.

**Quantization impact on operational conditions (A and B):** accuracy loss from Q4_K_M quantization is modest (−3.3 pp and −1.7 pp respectively), confirming that the deployed GGUF retains most of the fine-tuned model's capability. The large drop on Condition C (noise inputs) is not operationally relevant since the model never receives noise images in deployment.

### Discussion

Fine-tuning produces measurable improvements on Conditions B, C, and D, but Condition A (the nominal operating condition with both image and GPS) shows no gain on this 360-target dataset. The most likely explanation is the breadth of the HIGH category: mega-ports, mega-airports, energy infrastructure, open-pit mines, and military facilities are all grouped into a single label. The model can learn to output the correct JSON format quickly (training loss drops to 0.18 in ~41 minutes), but 240 training images spread across five visually heterogeneous HIGH sub-types is not enough for the visual encoder to learn a reliable decision boundary.

This is a prototype demonstrating that on-board VLM inference on a Pi 5 is technically viable. The approach will improve significantly with:

- **Narrower taxonomy**: splitting HIGH into mission-specific sub-classes (e.g., ports only, or energy infrastructure only) and training a specialist adapter
- **Larger corpus**: 240 training images is a minimal dataset for a 3-class VLM task; 1,000-5,000 images per class is a more realistic target for robust generalization
- **Higher-resolution tiles**: 512×512 Mapbox tiles lose fine-grained texture that distinguishes, e.g., a cargo terminal from a large parking lot at altitude

## Deployment

The adapter is converted to Q4_K_M GGUF via `llama-quantize` and runs on the Pi 5 via [llama.cpp](https://github.com/ggml-org/llama.cpp)'s multimodal (`mtmd`) API:

```
Vision encoding (mtmd):      ~10-15 s
Token generation (200 max):  ~40-55 s
Total per frame:             ~51-82 s  (CPU only, Cortex-A76, mean ~69 s, 1,443 frames from 3 end-to-end runs)
```

See the [quantization guide](https://Saransh-cpp.github.io/ORION/guides/quantization/) and [deployment guide](https://Saransh-cpp.github.io/ORION/guides/deployment/) for full instructions.

## Limitations

- Trained on Mapbox RGB tiles only; hence, no multispectral, SAR, or thermal data.
- 512×512 pixel resolution matches the Pi 5 inference pipeline; different resolutions require re-cropping.
- Three-class taxonomy (HIGH / MEDIUM / LOW) is fixed at training time. Mission-specific priorities require fine-tuning on a new labeled dataset.
- Inference at 51-82 s/frame (mean ~69s across 1,443 frames from 3 end-to-end runs) sets a hard floor on capture interval: the auto-capture timer is 85s to avoid saturating the VLM queue, limiting throughput to ~24 frames per 35-min eclipse. Burst imaging, real-time video, or sub-minute revisit rates are not feasible without faster hardware (GPU/NPU) or a smaller model.
- Coordinate dropout improves GPS robustness but does not eliminate coord-biased errors on hard edge cases.
- **Blank/missing tile hallucination:** Mapbox returns blank white tiles at extreme latitudes (|lat| > 75°) where no satellite imagery exists. The model hallucinates strategic significance onto these featureless images (3 out of 8 HIGH classifications across 1,443 frames were blank tiles). These blank tiles are visually distinct from the ocean and ice sheet tiles in the training set. Mitigation: add blank/white tile detection before inference, or include polar blank tiles as explicit LOW training examples.
- **Natural feature false positives:** Coastlines, cloud cover, and geological formations (e.g., river deltas, glacial terrain) can be misclassified as HIGH due to visual similarity to trained HIGH morphologies (e.g., coastlines as "artificial formations," clouds as "volcanic eruptions"). The hard-negative training set mitigates some of this, but edge cases remain.
- Training data was generated at 500 km simulated altitude; Pi 5 runs used the SimSat TLE orbit at ~802 km (~0.7 Mapbox zoom levels difference). The model generalized across this mismatch without degradation, but accuracy may differ at significantly different altitudes.
