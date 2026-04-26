# Ground Segment Budgets

Resource requirements for training, quantization, and dataset generation.

## Training Hardware

| Resource                | Requirement              | Notes                                              |
| ----------------------- | ------------------------ | -------------------------------------------------- |
| GPU                     | CUDA-capable, 8+ GB VRAM | QLoRA loads base model in 4-bit (NF4)              |
| GPU VRAM usage          | TBD                      | Micro-batch size 1, gradient checkpointing enabled |
| System RAM              | 16+ GB recommended       | For data loading and preprocessing                 |
| Training time per epoch | TBD                      | 3 epochs total, ~480 training samples              |
| Total training time     | TBD                      | Depends on GPU                                     |

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

| Resource              | Requirement               | Notes                                    |
| --------------------- | ------------------------- | ---------------------------------------- |
| GPU VRAM              | Same as training (~8+ GB) | Model loaded in 4-bit for inference      |
| Test samples          | ~60 (20% of 300 targets)  | 4 conditions x 60 = 240 inferences total |
| Time per inference    | TBD                       | Single image + prompt per sample         |
| Total validation time | TBD                       | 240 inferences per script                |

## Dataset

| Item                | Size     | Notes                                                  |
| ------------------- | -------- | ------------------------------------------------------ |
| Target images       | ~30 MB   | 300 PNG images at ~100 KB each                         |
| train_dataset.jsonl | ~1 MB    | ~480 records (240 targets x 2 with coordinate dropout) |
| test_dataset.jsonl  | ~200 KB  | ~60 records                                            |
| Total dataset       | ~31 MB   | Images + JSONL                                         |
| Generation time     | ~2.5 min | 300 images from SimSat at ~2 req/s                     |

## Data Transfer (Remote Server)

| Operation             | Data transferred    | Method                          |
| --------------------- | ------------------- | ------------------------------- |
| Dataset upload        | ~31 MB (compressed) | `upload_to_server.sh` via rsync |
| LoRA weights download | ~50 MB              | `download_weights.sh` via rsync |

## How to Measure

- **GPU VRAM**: run `nvidia-smi` during training
- **Training time**: logged by HuggingFace Trainer at end of each epoch
- **Quantization time**: wall-clock the `llama-quantize` command
- **Validation time**: wall-clock `python validation.py`
