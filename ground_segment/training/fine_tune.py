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
TRAIN_FILE = "/home/schopra/hdd/gaze/datasets/extras/orion_dataset/train_dataset.jsonl"
OUTPUT_DIR = "orion_lora_weights"


class OrionDataset(Dataset):
    """Simply loads the raw JSON data. Processing happens in the Collator."""

    def __init__(self, jsonl_file):
        self.data = load_dataset("json", data_files={"train": jsonl_file})["train"]

    def __len__(self):
        return len(self.data)

    def __getitem__(self, idx):
        return self.data[idx]


class VLMDataCollator:
    """Processes an entire batch of images and texts perfectly for the VLM."""

    def __init__(self, processor):
        self.processor = processor

    def __call__(self, batch):
        images = []
        texts = []

        for item in batch:
            images.append(
                [
                    Image.open(
                        f"/home/schopra/hdd/gaze/datasets/extras/{item['image']}"
                    ).convert("RGB")
                ]
            )

            prompt = item["conversations"][0]["content"]
            response = item["conversations"][1]["content"]
            messages = [
                {"role": "user", "content": prompt},
                {"role": "assistant", "content": response},
            ]
            texts.append(self.processor.apply_chat_template(messages, tokenize=False))

        # Process the entire batch at once!
        inputs = self.processor(
            images=images,
            text=texts,
            return_tensors="pt",
            padding=True,  # Just dynamic padding. No truncation, let the image fit!
        )

        # Define labels for the loss calculation
        inputs["labels"] = inputs["input_ids"].clone()
        return inputs


def main():
    print("🚀 Initializing ORION QLoRA Training Pipeline...")

    # 1. Load Processor
    processor = AutoProcessor.from_pretrained(MODEL_ID, trust_remote_code=True)

    # 2. Configure 4-bit Quantization (The "Q" in QLoRA)
    bnb_config = BitsAndBytesConfig(
        load_in_4bit=True,
        bnb_4bit_use_double_quant=True,
        bnb_4bit_quant_type="nf4",
        bnb_4bit_compute_dtype=torch.float16,
    )

    # 3. Load Base Model in 4-bit
    print(f"📦 Loading base model {MODEL_ID} in 4-bit...")
    model = AutoModelForImageTextToText.from_pretrained(
        MODEL_ID,
        quantization_config=bnb_config,
        device_map="auto",
        trust_remote_code=True,
    )

    # Prepares base model
    model = prepare_model_for_kbit_training(model, use_gradient_checkpointing=False)

    # --- THE ULTIMATE VLM GRADIENT BUG FIX ---
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
    # ------------------------------------------

    # 4. Inject LoRA Adapters
    # We target the Attention mechanism projections (q, k, v, o). This is the "brain" of the cross-attention.
    lora_config = LoraConfig(
        r=16,  # Rank: Size of the adapter. 16 is a great sweet spot.
        lora_alpha=32,  # Alpha: Scaling factor. Usually 2x the Rank.
        target_modules=["q_proj", "k_proj", "v_proj", "o_proj"],
        lora_dropout=0.05,
        bias="none",
        task_type="CAUSAL_LM",
    )

    model = get_peft_model(model, lora_config)
    print("✅ LoRA Adapters injected.")
    model.print_trainable_parameters()
    # 5. Load Dataset
    train_dataset = OrionDataset(TRAIN_FILE)
    collator = VLMDataCollator(processor)
    print(
        f"📊 Dataset loaded. Training on {len(train_dataset)} highly curated samples."
    )

    # 6. Training Arguments optimized for A-Series GPUs
    training_args = TrainingArguments(
        output_dir=OUTPUT_DIR,
        per_device_train_batch_size=1,  # Micro-batch size
        gradient_accumulation_steps=16,
        gradient_checkpointing=True,  # <--- ADD THIS
        gradient_checkpointing_kwargs={"use_reentrant": False},  # <--- ADD THIS
        learning_rate=2e-4,
        num_train_epochs=3,  # 3 passes over the 240 images
        logging_steps=5,  # Print loss every 5 steps
        save_strategy="epoch",  # Save weights at end of each epoch
        optim="paged_adamw_8bit",  # Memory-efficient optimizer
        fp16=True,  # Use FP16 math (A-Series excels at this)
        remove_unused_columns=False,  # CRITICAL FOR VLMs: Prevents HF from dropping image tensors!
        dataloader_pin_memory=False,  # Helps avoid certain CUDA memory spikes
    )

    # 7. Initialize Trainer
    trainer = Trainer(
        model=model,
        args=training_args,
        train_dataset=train_dataset,
        data_collator=collator,  # <--- Inject the custom collator here!
    )

    # 8. Let it rip!
    print("🔥 Starting fine-tuning...")
    trainer.train()

    # 9. Save final adapters
    trainer.model.save_pretrained(OUTPUT_DIR)
    processor.save_pretrained(OUTPUT_DIR)
    print(f"🎉 Training complete! LoRA adapters saved to: {OUTPUT_DIR}")


if __name__ == "__main__":
    main()
