# Training

> For design details - QLoRA configuration, LoRA adapter parameters, training hyperparameters, and data collation internals - see the [Training Pipeline architecture](../ground-segment/training.md) page.

## Hardware Requirements

- A GPU with CUDA support is required. The training pipeline uses 4-bit quantization via `bitsandbytes` and FP16 math.
- Minimum ~8 GB VRAM (the micro-batch size is 1 with gradient accumulation).
- For detailed GPU, RAM, and disk requirements, see [Compute Budgets](../ground-segment/budgets.md).

## Prerequisites

Install the project dependencies from the ground segment:

```bash
cd ground_segment
pip install -e .
```

This installs: `torch`, `torchvision`, `transformers`, `accelerate`, `peft`, `datasets`, `bitsandbytes`, and other required packages.

Ensure you have already generated the dataset (see the [Data Generation](data-gen.md) guide).

## Step 1: Fine-Tuning

Before running, update the `TRAIN_FILE` path in `fine_tune.py` to point to your generated dataset, and update the image path prefix in the `VLMDataCollator.__call__` method.

```bash
cd ground_segment/training
python fine_tune.py
```

Training logs are printed every 5 steps. Checkpoints are saved at the end of each epoch. The final LoRA adapter weights and processor are saved to `orion_lora_weights/`.

### Fine-Tuning Output

After training completes, the output directory contains:

```
orion_lora_weights/
    adapter_config.json
    adapter_model.safetensors
    tokenizer_config.json
    preprocessor_config.json
    ...
```

## Step 2: Merging LoRA Weights (Fuse)

After training, merge the LoRA adapters into the base model to produce a standalone FP16 model. This is required before quantization to GGUF format.

```bash
cd ground_segment/training
python fuse.py
```

### Fuse Output

```
orion_merged/
    config.json
    model.safetensors
    tokenizer_config.json
    preprocessor_config.json
    ...
```

This merged model is a complete, self-contained Hugging Face model that can be loaded without any LoRA dependencies. It is the input for the quantization step (see the [Quantization](quantization.md) guide).
