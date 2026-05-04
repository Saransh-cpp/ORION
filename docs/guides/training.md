# Training

> For design details - QLoRA configuration, LoRA adapter parameters, training hyperparameters, and data collation internals - see the [Training Pipeline architecture](../ground-segment/training.md) page.

## Hardware Requirements

- A GPU with CUDA support is required. The training pipeline uses 4-bit quantization via `bitsandbytes` and FP16 math.
- Minimum ~8 GB VRAM (the micro-batch size is 1 with gradient accumulation).
- For detailed GPU, RAM, and disk requirements, see [Compute Budgets](../ground-segment/budgets.md).

## Prerequisites

- Data generated (see [data-gen](./data-gen.md))

## Step 1: Fine-Tuning

Move the data and the code to the training server using `upload_to_server.sh` (see [scripts.md](../ground-segment/scripts.md)). On the server, create a new environment and run fine tuning:

```bash
# make sure you are in the uv environment
# created during data generation
ORION_DATASET_ROOT=<the-dir-used-in-script> uv run fine_tune.py
```

Training logs are printed every 5 steps. Checkpoints are saved at the end of each epoch. The final LoRA adapter weights and processor are saved to `orion_lora_weights/`.

Finally, use `download_weights.sh` to transfer the weights from the training server to your local machine in `ground_segment/training` (see [scripts.md](../ground-segment/scripts.md)).

### Fine-Tuning Output

After training completes, the output directory contains:

```
orion_lora_weights/
    adapter_config.json
    adapter_model.safetensors
    tokenizer_config.json
    processor_config.json
    ...
```

## Step 2: Merging LoRA Weights (Fuse)

After training, merge the LoRA adapters into the base model to produce a standalone FP16 model. This is required before quantization to GGUF format.

```bash
uv run fuse.py
```

### Fuse Output

```
orion_merged/
    config.json
    model.safetensors
    tokenizer_config.json
    processor_config.json
    ...
```

This merged model is a complete, self-contained Hugging Face model that can be loaded without any LoRA dependencies. It is the input for the quantization step (see the [Quantization](quantization.md) guide).
