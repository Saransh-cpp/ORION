"""ORION QLoRA fine-tuning pipeline - trains a LoRA adapter on the LFM2.5-VL-1.6B base model.

Loads the base Liquid VLM in 4-bit NF4 quantisation via bitsandbytes, injects
LoRA adapters into the attention projections (q/k/v/o, rank 16, alpha 32), and
trains for 3 epochs on the ORION dataset (240 train images with coordinate-dropout
augmentation). The best checkpoint (by validation loss) is saved to
``orion_lora_weights/``.

**Requirements:** NVIDIA CUDA GPU with >= 8 GB VRAM. The training stack uses
bitsandbytes (NF4 quantisation + ``paged_adamw_8bit``), which is CUDA-only -
this script will **not** run on Apple Silicon (MPS), AMD GPUs, or CPU-only
machines. Verified on NVIDIA RTX 4070 Ti, 12 GB, CUDA 12.2, driver 535.x.

Usage:

```bash
export ORION_DATASET_ROOT=/path/to/dir/containing/orion_dataset
cd ground_segment/training
uv run fine_tune.py
```

See the [training guide](../../../../guides/training/) for the full walkthrough
and the [ground segment budgets](../../../../ground-segment/budgets/) for compute
requirements.
"""

import os
import sys
import time
import torch
from datasets import load_dataset
from PIL import Image
from transformers import (
    AutoProcessor,
    AutoModelForImageTextToText,
    BitsAndBytesConfig,
    TrainingArguments,
    Trainer,
)
from peft import LoraConfig, get_peft_model, prepare_model_for_kbit_training
from torch.utils.data import Dataset

# --- CONFIGURATION ---
MODEL_ID = "LiquidAI/LFM2.5-VL-1.6B"
OUTPUT_DIR = "orion_lora_weights"

# Dataset root resolves both the JSONL files and the image paths embedded in them.
# Set this to the directory that *contains* `orion_dataset/` (i.e., the parent of
# what `data_gen.py` produced or what `upload_to_server.sh` extracted on the server).
DATASET_ROOT = os.environ.get("ORION_DATASET_ROOT")
if not DATASET_ROOT:
    sys.exit(
        "ERROR: Set ORION_DATASET_ROOT to the directory containing orion_dataset/. "
        "See docs/guides/environment-variables-gs.md."
    )

TRAIN_FILE = f"{DATASET_ROOT}/orion_dataset/train_dataset.jsonl"
VAL_FILE = f"{DATASET_ROOT}/orion_dataset/val_dataset.jsonl"


class OrionDataset(Dataset):
    """Thin wrapper around a JSONL split for use with the HuggingFace Trainer.

    Loads the JSONL file via ``datasets.load_dataset`` at init time. Each item
    is a raw conversation dict; image loading and tokenisation are deferred to
    `VLMDataCollator` so that batching and padding happen in one place.

    Args:
        jsonl_file: Path to a ``train_dataset.jsonl`` or ``val_dataset.jsonl``
            produced by `data_gen`.
    """

    def __init__(self, jsonl_file):
        self.data = load_dataset("json", data_files={"train": jsonl_file})["train"]

    def __len__(self):
        return len(self.data)

    def __getitem__(self, idx):
        return self.data[idx]


class VLMDataCollator:
    """Batch collator that loads images and tokenises conversations for the VLM.

    For each item in the batch, opens the corresponding 512x512 RGB tile from
    disk, formats the user/assistant conversation via the processor's chat
    template, and returns a single padded batch tensor with cloned
    ``input_ids`` as ``labels`` for causal-LM loss.

    Args:
        processor: ``AutoProcessor`` instance for the LFM2.5-VL model.
        dataset_root: Root directory containing the ``orion_dataset/`` tree
            (image paths in the JSONL are relative to this).
    """

    def __init__(self, processor, dataset_root):
        self.processor = processor
        self.dataset_root = dataset_root

    def __call__(self, batch):
        """Collate a list of conversation dicts into a model-ready batch.

        Args:
            batch: List of dicts, each with ``image`` (relative path) and
                ``conversations`` (list of user/assistant message dicts).

        Returns:
            A dict of padded tensors (``input_ids``, ``attention_mask``,
            ``pixel_values``, ``labels``) suitable for ``Trainer.train()``.
        """
        images = []
        texts = []

        for item in batch:
            images.append(
                [Image.open(f"{self.dataset_root}/{item['image']}").convert("RGB")]
            )

            prompt = item["conversations"][0]["content"]
            response = item["conversations"][1]["content"]
            messages = [
                {"role": "user", "content": prompt},
                {"role": "assistant", "content": response},
            ]
            texts.append(self.processor.apply_chat_template(messages, tokenize=False))

        inputs = self.processor(
            images=images,
            text=texts,
            return_tensors="pt",
            padding=True,
        )

        inputs["labels"] = inputs["input_ids"].clone()
        return inputs


def main():
    """Run the full QLoRA fine-tuning pipeline and save the adapter weights.

    Steps: load the base LFM2.5-VL-1.6B in 4-bit NF4, prepare for k-bit
    training, monkey-patch ``enable_input_require_grads`` to handle
    non-tensor outputs safely, inject LoRA adapters (rank 16, alpha 32) into
    q/k/v/o projections, train for 3 epochs with ``paged_adamw_8bit``, and
    save the best checkpoint to ``orion_lora_weights/``.
    """
    t_start = time.perf_counter()
    print(" Initializing ORION QLoRA Training Pipeline...")

    # 1. Load Processor
    processor = AutoProcessor.from_pretrained(MODEL_ID, trust_remote_code=True)

    # 2. Configure 4-bit Quantization
    bnb_config = BitsAndBytesConfig(
        load_in_4bit=True,
        bnb_4bit_use_double_quant=True,
        bnb_4bit_quant_type="nf4",
        bnb_4bit_compute_dtype=torch.float16,
    )

    # 3. Load Base Model in 4-bit
    print(f" Loading base model {MODEL_ID} in 4-bit...")
    model = AutoModelForImageTextToText.from_pretrained(
        MODEL_ID,
        quantization_config=bnb_config,
        device_map="auto",
        trust_remote_code=True,
    )

    # Prepares base model
    model = prepare_model_for_kbit_training(model, use_gradient_checkpointing=False)

    import types

    # 1. Define our safe logic
    def safe_enable_input_require_grads(self):
        def make_inputs_require_grad(module, input, output):
            if isinstance(output, torch.Tensor) and output.is_floating_point():
                output.requires_grad_(True)

        self.get_input_embeddings().register_forward_hook(make_inputs_require_grad)

    # 2. Forcefully overwrite the model's internal method with our safe one
    model.enable_input_require_grads = types.MethodType(
        safe_enable_input_require_grads, model
    )

    # 3. Disable cache
    model.config.use_cache = False

    # 4. Inject LoRA Adapters
    # We target the Attention mechanism projections (q, k, v, o).
    lora_config = LoraConfig(
        r=16,  # Rank: Size of the adapter.
        lora_alpha=32,  # Alpha: Scaling factor. Usually 2x the Rank.
        target_modules=["q_proj", "k_proj", "v_proj", "o_proj"],
        lora_dropout=0.05,
        bias="none",
        task_type="CAUSAL_LM",
    )

    model = get_peft_model(model, lora_config)
    print(" LoRA Adapters injected.")
    model.print_trainable_parameters()
    # 5. Load Datasets (train + val)
    train_dataset = OrionDataset(TRAIN_FILE)
    val_dataset = OrionDataset(VAL_FILE)
    collator = VLMDataCollator(processor, DATASET_ROOT)
    print(f"📊 Datasets loaded. Train: {len(train_dataset)} | Val: {len(val_dataset)}")

    # 6. Training Arguments optimized for A-Series GPUs
    training_args = TrainingArguments(
        output_dir=OUTPUT_DIR,
        per_device_train_batch_size=1,  # Micro-batch size
        per_device_eval_batch_size=1,
        gradient_accumulation_steps=16,
        gradient_checkpointing=True,
        gradient_checkpointing_kwargs={"use_reentrant": False},  # <--- ADD THIS
        learning_rate=2e-4,
        num_train_epochs=3,  # 3 passes over the 240 images
        logging_steps=5,  # Print loss every 5 steps
        eval_strategy="epoch",  # Evaluate on val_dataset at end of each epoch
        save_strategy="epoch",  # Save weights at end of each epoch
        load_best_model_at_end=True,  # Restore best-eval checkpoint after training
        metric_for_best_model="eval_loss",
        greater_is_better=False,
        save_total_limit=2,  # Keep only the best + latest checkpoint
        optim="paged_adamw_8bit",  # Memory-efficient optimizer
        fp16=True,  # Use FP16 math
        remove_unused_columns=False,  # Prevents HF from dropping image tensors
        dataloader_pin_memory=False,  # Helps avoid certain CUDA memory spikes
    )

    # 7. Initialize Trainer
    trainer = Trainer(
        model=model,
        args=training_args,
        train_dataset=train_dataset,
        eval_dataset=val_dataset,
        data_collator=collator,
    )

    t_setup = time.perf_counter()
    print(f" Setup complete in {t_setup - t_start:.2f}s")
    print(" Starting fine-tuning...")
    trainer.train()
    t_train_done = time.perf_counter()

    trainer.model.save_pretrained(OUTPUT_DIR)
    processor.save_pretrained(OUTPUT_DIR)
    t_done = time.perf_counter()
    print(f" Training complete! LoRA adapters saved to: {OUTPUT_DIR}")
    print(
        f"Total runtime: {t_done - t_start:.2f}s "
        f"(setup: {t_setup - t_start:.2f}s, train: {t_train_done - t_setup:.2f}s, save: {t_done - t_train_done:.2f}s)"
    )


if __name__ == "__main__":
    main()
