# Ground Segment Budgets

Resource requirements for training, quantization, and dataset generation.

## Training Hardware

`fine_tune.py` is **CUDA only**. The training stack uses bitsandbytes (4-bit NF4 quantization + paged_adamw_8bit optimizer), which does not support Apple Silicon, AMD GPUs, or CPU-only execution.

| Resource                | Requirement                     | Notes                                              |
| ----------------------- | ------------------------------- | -------------------------------------------------- |
| GPU                     | NVIDIA CUDA-capable, 8+ GB VRAM | QLoRA loads base model in 4-bit (NF4)              |
| CUDA toolkit            | 12.x                            | Matched to the GPU driver                          |
| GPU VRAM usage          | TBD                             | Micro-batch size 1, gradient checkpointing enabled |
| System RAM              | 16+ GB recommended              | For data loading and preprocessing                 |
| Training time per epoch | TBD                             | 3 epochs total, ~480 training samples              |
| Total training time     | TBD                             | Depends on GPU                                     |

**Reference hardware (verified working):**

| Component | Spec                                   |
| --------- | -------------------------------------- |
| GPU       | NVIDIA GeForce RTX 4070 Ti, 12 GB VRAM |
| Driver    | 535.261.03                             |
| CUDA      | 12.2                                   |
| OS        | Linux                                  |

## Model Artifact Sizes

| Stage                | File                      | Size    | Notes                        |
| -------------------- | ------------------------- | ------- | ---------------------------- |
| Base model           | `LiquidAI/LFM2.5-VL-1.6B` | ~3.2 GB | Downloaded from Hugging Face |
| LoRA adapter weights | `orion_lora_weights/`     | ~50 MB  | r=16, 4 target modules       |
| Merged FP16 model    | `orion_merged/`           | ~3.2 GB | Full standalone checkpoint   |
| FP16 GGUF            | `orion-f16.gguf`          | ~3.2 GB | Intermediate conversion      |
| Q4_K_M GGUF          | `orion-q4_k_m.gguf`       | ~700 MB | Deployed to Pi               |
| Vision projector     | `orion-mmproj-f16.gguf`   | ~854 MB | FP16, deployed to Pi         |

## Quantization Compute

| Step                    | RAM required | Time | Notes                                       |
| ----------------------- | ------------ | ---- | ------------------------------------------- |
| HF to GGUF conversion   | ~8 GB        | TBD  | Full FP16 model loaded into RAM             |
| mmproj extraction       | ~4 GB        | TBD  | Vision encoder only                         |
| Q4_K_M quantization     | ~4 GB        | TBD  | Reads FP16 GGUF, writes Q4                  |
| Total disk (all stages) | ~11 GB       |      | Base + merged + F16 GGUF + Q4 GGUF + mmproj |

## Weight Fusion Compute

| Resource | Requirement    | Notes                                           |
| -------- | -------------- | ----------------------------------------------- |
| RAM      | ~8 GB          | Full FP16 model loaded on CPU (no GPU required) |
| Time     | TBD            | `merge_and_unload()` + SafeTensors save         |
| Disk     | ~3.2 GB output | Merged model saved to `orion_merged/`           |

## Validation / Ablation Studies

`ablation.py` (base model) and `evaluate.py` (fine-tuned) are **device-agnostic** — they run on CUDA, MPS (Apple Silicon), or CPU via `device_map="auto"` at FP16. CPU-only inference is functional but will be 50-100x slower than GPU.

| Resource              | Requirement                                | Notes                                    |
| --------------------- | ------------------------------------------ | ---------------------------------------- |
| GPU VRAM              | ~4 GB (FP16, no quantization in eval)      | Or run on CPU/MPS without VRAM budget    |
| Test samples          | 60 (deterministic IID carve from 360 pool) | 4 conditions x 60 = 240 inferences total |
| Val samples           | 60 (deterministic IID carve from 360 pool) | Same shape as test, used during training |
| Time per inference    | TBD                                        | ~1s on 4070 Ti, ~20-30s on Mac CPU       |
| Total validation time | TBD                                        | 240 inferences per script                |

## Dataset

| Item                | Size    | Notes                                                  |
| ------------------- | ------- | ------------------------------------------------------ |
| Target images       | ~36 MB  | 360 PNG images at ~100 KB each                         |
| train_dataset.jsonl | ~1 MB   | ~480 records (240 targets x 2 with coordinate dropout) |
| val_dataset.jsonl   | ~200 KB | 60 records                                             |
| test_dataset.jsonl  | ~200 KB | 60 records                                             |
| Total dataset       | ~37 MB  | Images + JSONL                                         |
| Generation time     | ~3 min  | 360 images from SimSat at ~2 req/s                     |

## Data Transfer (Remote Server)

| Operation             | Data transferred    | Method                          |
| --------------------- | ------------------- | ------------------------------- |
| Dataset upload        | ~31 MB (compressed) | `upload_to_server.sh` via rsync |
| LoRA weights download | ~50 MB              | `download_weights.sh` via rsync |

## How to Measure

- **GPU VRAM**: run `nvidia-smi` during training
- **Training time**: logged by HuggingFace Trainer at end of each epoch
- **Quantization time**: wall-clock the `llama-quantize` command
- **Evaluation time**: wall-clock `python evaluate.py`
