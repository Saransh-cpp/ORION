# Training Pipeline

The ORION training pipeline fine-tunes the LiquidAI LFM2.5-VL-1.6B vision-language model for orbital image triage classification. The pipeline produces a quantized GGUF model suitable for CPU-only inference on the Raspberry Pi 5.

## Pipeline Overview

```
LFM2.5-VL-1.6B (base model)
    |
    v
fine_tune.py : QLoRA fine-tuning with ORION dataset
    |
    v
orion_lora_weights/ : LoRA adapter weights
    |
    v
fuse.py : Merge LoRA into base model
    |
    v
orion_merged/ : Standalone FP16 Hugging Face model
    |
    v
llama.cpp convert + quantize : GGUF Q4_K_M quantization
    |
    v
orion-q4_k_m.gguf  (~730 MB, flight-ready)
orion-mmproj-f16.gguf  (vision encoder projection)
```

## Base Model

- **Model:** `LiquidAI/LFM2.5-VL-1.6B`
- **Architecture:** Vision-language model with 1.6 billion parameters
- **Loaded in:** 4-bit quantization via BitsAndBytes (NF4)

## QLoRA Configuration

| Parameter      | Value                                  | Description                     |
| -------------- | -------------------------------------- | ------------------------------- |
| Rank (r)       | 16                                     | LoRA adapter rank               |
| Alpha          | 32                                     | Scaling factor (2x rank)        |
| Target modules | `q_proj`, `k_proj`, `v_proj`, `o_proj` | Attention mechanism projections |
| Dropout        | 0.05                                   | LoRA dropout rate               |
| Task type      | CAUSAL_LM                              | Causal language modeling        |

## Training Configuration

| Parameter              | Value            | Description                           |
| ---------------------- | ---------------- | ------------------------------------- |
| Batch size             | 1 (micro-batch)  | Per-device training batch size        |
| Gradient accumulation  | 16 steps         | Effective batch size of 16            |
| Learning rate          | 2e-4             | AdamW learning rate                   |
| Epochs                 | 3                | Full passes over the training set     |
| Optimizer              | paged_adamw_8bit | Memory-efficient 8-bit AdamW          |
| Precision              | FP16             | Half-precision training               |
| Gradient checkpointing | Enabled          | Reduces memory at the cost of compute |

## Weight Fusion

After fine-tuning, `fuse.py` merges the LoRA adapter weights permanently into the base model. It loads the base model in FP16 on CPU, applies `merge_and_unload()`, and saves the result with SafeTensors serialization to `orion_merged/`.

## GGUF Quantization

The merged FP16 model is converted to GGUF format and quantized to Q4_K_M using llama.cpp tools. The multimodal projector (mmproj) is extracted separately and kept at FP16 precision. The two output files (`orion-q4_k_m.gguf` and `orion-mmproj-f16.gguf`) are deployed to the Pi. Pre-trained models are available for [download](https://drive.google.com/drive/folders/1h6WGNeNzYHdfisELlJodDCKlkREkIzCN?usp=share_link). For artifact sizes at each stage, see [Compute Budgets](budgets.md).

## Dependencies

The training pipeline requires the following Python packages (installed via `ground_segment/pyproject.toml`):

- `torch`, `torchvision`: PyTorch framework
- `transformers`: Hugging Face model loading and training
- `peft`: Parameter-Efficient Fine-Tuning (LoRA/QLoRA)
- `datasets`: JSONL dataset loading
- `bitsandbytes`: 4-bit quantization support
- `accelerate`: Hardware-agnostic model loading
- `gguf`: GGUF format conversion utilities

## Validation and Ablation Studies

> Full per-condition accuracy numbers, per-class precision/recall/F1, fine-tuning delta table, and raw inference logs are in the [Model Card](model-card.md#evaluation).

Both `evaluate.py` (fine-tuned model) and `ablation.py` (base model) evaluate the model under four conditions:

| Condition          | Image Input              | Prompt                 | Purpose                                          |
| ------------------ | ------------------------ | ---------------------- | ------------------------------------------------ |
| A: Full System     | Real satellite image     | Includes coordinates   | Baseline performance                             |
| B: Vision Only     | Real satellite image     | Coordinates stripped   | Measures visual reasoning without GPS hints      |
| C: Blind LLM       | Gaussian noise (512x512) | Includes coordinates   | Tests coordinate memorization (no vision)        |
| D: Sensor Conflict | Real satellite image     | Mismatched coordinates | Tests whether model trusts vision or coordinates |

### Condition D: Mismatch Logic

The script deliberately feeds coordinates from the opposite category:

- HIGH images receive LOW coordinates
- LOW images receive HIGH coordinates
- MEDIUM images receive HIGH coordinates

This stress-tests the model's ability to reason from visual evidence when coordinate telemetry is misleading.

### Condition C: Gaussian Noise

The noise image is a deterministic 512x512 random RGB array seeded with `np.random.seed(42)`. Using Gaussian noise rather than a blank image prevents the model from defaulting to "ocean" for featureless inputs.

### Metrics

For conditions A, B, and C: per-class recall and precision for HIGH, MEDIUM, and LOW, plus overall accuracy. For condition D: ratio of visual-trust (correct) versus coordinate-trust (failure).

For step-by-step instructions, see the guides for [training](../guides/training.md), [quantization](../guides/quantization.md), and [validation/ablation studies](../guides/studies.md).

## Data and Weight Transfer Scripts

Two shell scripts handle moving data and weights between the local machine and the remote training server:

- `ground_segment/data/upload_to_server.sh`: compresses the local dataset, uploads it via `rsync`, and clones/pulls the ORION repo on the server. Run this before training.
- `ground_segment/training/download_weights.sh`: pulls `orion_lora_weights/` from the server after training completes and deletes the server's repo and dataset (scorched earth).

See [Utility Scripts](scripts.md) for invocation details and [Ground Segment Environment Variables](../guides/environment-variables-gs.md) for the required env vars.
