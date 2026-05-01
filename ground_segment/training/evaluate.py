import argparse
import json
import time
import torch
import re
import random
import numpy as np
from PIL import Image
from transformers import AutoProcessor, AutoModelForImageTextToText
from peft import PeftModel

# --- CONFIGURATION ---
BASE_MODEL_ID = "LiquidAI/LFM2.5-VL-1.6B"
LORA_WEIGHTS_PATH = "./orion_lora_weights"
DATASET_DIR = "../data/orion_dataset"
TEST_FILE = f"{DATASET_DIR}/test_dataset.jsonl"  # 60 IID held-out targets, never seen during training
VAL_FILE = f"{DATASET_DIR}/val_dataset.jsonl"  # 60 IID validation targets, used during training
random.seed(42)


def extract_json(text):
    """Strictly extracts JSON, failing over to ERROR if the model disobeys formatting."""
    try:
        start = text.find("{")
        end = text.rfind("}") + 1
        if start != -1 and end != 0:
            return json.loads(text[start:end])
    except json.JSONDecodeError:
        pass

    return {"category": "ERROR", "reason": f"Raw: {text.strip()[:50]}"}


def run_inference(model, processor, image, prompt):
    """Runs a single image+prompt through the VLM and returns parsed JSON + raw text."""
    messages = [{"role": "user", "content": f"<image>\n{prompt}"}]
    text_input = processor.apply_chat_template(
        messages, tokenize=False, add_generation_prompt=True
    )

    # Use the device of the model (handles both CUDA and MPS seamlessly)
    device = next(model.parameters()).device
    inputs = processor(images=[image], text=[text_input], return_tensors="pt").to(
        device, torch.float16
    )

    with torch.no_grad():
        output = model.generate(**inputs, max_new_tokens=200, do_sample=False)

    generated_text = processor.decode(
        output[0][inputs["input_ids"].shape[1] :], skip_special_tokens=True
    )

    # Return both the parsed dictionary and the raw string for debugging
    return extract_json(generated_text), generated_text


def print_confusion_matrix(truths, preds, condition_name):
    """Prints a per-class Recall and Precision breakdown, plus an aggregate total."""
    print(f"\n--- {condition_name} ---")
    total_correct = 0
    total_samples = len(truths)

    for c in ["HIGH", "MEDIUM", "LOW"]:
        total_actual = truths.count(c)
        if total_actual == 0:
            continue

        total_predicted = preds.count(c)
        correct = sum(1 for t, p in zip(truths, preds) if t == c and p == c)
        total_correct += correct

        recall_pct = (correct / total_actual) * 100
        precision_pct = (
            (correct / total_predicted) * 100 if total_predicted > 0 else 0.0
        )

        print(
            f"{c:6s}: {correct:2d}/{total_actual:2d} ({recall_pct:.1f}% Recall) | Precision: {correct:2d}/{total_predicted:<2d} ({precision_pct:.1f}%)"
        )

    if total_samples > 0:
        print(
            f"TOTAL : {total_correct:2d}/{total_samples:2d} ({(total_correct / total_samples) * 100:.1f}% Overall Accuracy)"
        )


def main():
    t_start = time.perf_counter()

    parser = argparse.ArgumentParser(
        description="Evaluate the FINE-TUNED ORION model on a held-out set using the 4-condition protocol (A/B/C/D)."
    )
    parser.add_argument(
        "--file",
        choices=["test", "val"],
        default="test",
        help="Which split to evaluate: 'test' (60 IID held-out, judge-facing) "
        "or 'val' (60 IID validation set used during training).",
    )
    args = parser.parse_args()

    eval_file = TEST_FILE if args.file == "test" else VAL_FILE
    print(f" Initializing Fine-Tuned ORION Ablation Protocol on '{args.file}' split")
    print(f"   File: {eval_file}\n")

    # 1. Load Processor
    processor = AutoProcessor.from_pretrained(BASE_MODEL_ID, trust_remote_code=True)

    # 2. Load Base Model — pick MPS (Apple Silicon) or CPU explicitly.
    # device_map="auto" triggers accelerate's get_balanced_memory which crashes
    # when the model config stores no_split_module_classes as a set (unhashable).
    if torch.cuda.is_available():
        device = "cuda"
    elif torch.backends.mps.is_available():
        device = "mps"
    else:
        device = "cpu"
    print(f" Loading Base Model on {device}...")
    base_model = AutoModelForImageTextToText.from_pretrained(
        BASE_MODEL_ID,
        device_map=device,
        torch_dtype=torch.float16,
        trust_remote_code=True,
    )

    # 3. Inject LoRA Weights
    print(" Grafting Custom LoRA Adapters...")
    model = PeftModel.from_pretrained(base_model, LORA_WEIGHTS_PATH)
    model.eval()

    t_model_loaded = time.perf_counter()
    print(f" Model + LoRA loaded in {t_model_loaded - t_start:.2f}s.")

    # Load Data
    test_data = []
    with open(eval_file, "r") as f:
        for line in f:
            test_data.append(json.loads(line.strip()))

    print(f" Loaded {len(test_data)} samples from '{args.file}' split.\n")

    # Metrics tracking
    metrics = {
        "A": {"truths": [], "preds": []},
        "B": {"truths": [], "preds": []},
        "C": {"truths": [], "preds": []},
    }
    conflict_metrics = {
        "trusted_vision": 0,
        "trusted_coords": 0,
        "trusted_neither": 0,
        "total": 0,
    }

    # Noise image for Condition C
    np.random.seed(42)
    noise_array = np.random.randint(0, 256, (512, 512, 3), dtype=np.uint8)
    noise_image = Image.fromarray(noise_array)

    mismatch_map = {"HIGH": "LOW", "LOW": "HIGH", "MEDIUM": "HIGH"}

    for idx, row in enumerate(test_data):
        print(f"\n--- Evaluating Sample {idx + 1}/{len(test_data)} ---")

        image_path = f"../data/{row['image']}"
        real_image = Image.open(image_path).convert("RGB")
        ground_truth = json.loads(row["conversations"][1]["content"])["category"]
        full_prompt = row["conversations"][0]["content"].replace("<image>\n", "")

        # Condition B: Strip coordinates
        vision_only_prompt = re.sub(
            r" at Longitude: [-\d.]+, Latitude: [-\d.]+", "", full_prompt
        )

        # Condition D: Force mismatched coordinate
        target_mismatch = mismatch_map[ground_truth]
        mismatched_pool = [
            item
            for item in test_data
            if json.loads(item["conversations"][1]["content"])["category"]
            == target_mismatch
        ]
        mismatched_item = random.choice(mismatched_pool)
        mismatched_prompt = mismatched_item["conversations"][0]["content"].replace(
            "<image>\n", ""
        )
        mismatched_gt = target_mismatch

        # Execute Inferences (Now unpacking the tuple: dict, raw_text)
        res_a, raw_text_a = run_inference(model, processor, real_image, full_prompt)
        res_b, _ = run_inference(model, processor, real_image, vision_only_prompt)
        res_c, _ = run_inference(model, processor, noise_image, full_prompt)
        res_d, _ = run_inference(model, processor, real_image, mismatched_prompt)

        # --- DEBUG OUTPUT ---
        print(f"  Image Path: {image_path}")
        print(f" Raw Output (Cond A): {raw_text_a}")

        metrics["A"]["truths"].append(ground_truth)
        metrics["A"]["preds"].append(res_a.get("category"))

        metrics["B"]["truths"].append(ground_truth)
        metrics["B"]["preds"].append(res_b.get("category"))

        metrics["C"]["truths"].append(ground_truth)
        metrics["C"]["preds"].append(res_c.get("category"))

        pred_d = res_d.get("category")
        if pred_d == ground_truth:
            conflict_metrics["trusted_vision"] += 1
        elif pred_d == mismatched_gt:
            conflict_metrics["trusted_coords"] += 1
        else:
            conflict_metrics["trusted_neither"] += 1
        conflict_metrics["total"] += 1

        print(
            f" Truth: {ground_truth} | A: {res_a.get('category')} | B: {res_b.get('category')} | C: {res_c.get('category')} | D: {pred_d} (Fake Coords: {mismatched_gt})"
        )

    # Final Output Matrix
    print("\n" + "=" * 55)
    print("🏆 POST-LORA ABLATION STUDY RESULTS 🏆")
    print("=" * 55)

    print_confusion_matrix(
        metrics["A"]["truths"],
        metrics["A"]["preds"],
        "Condition A: Full System (Vision + Coords)",
    )
    print_confusion_matrix(
        metrics["B"]["truths"],
        metrics["B"]["preds"],
        "Condition B: Vision Only (No Coords)",
    )
    print_confusion_matrix(
        metrics["C"]["truths"],
        metrics["C"]["preds"],
        "Condition C: Blind LLM (Gaussian Noise + Coords)",
    )

    print("\n--- Condition D: Sensor Conflict (Real Vision + Fake Coords) ---")
    print(
        f"Model trusted Vision (Correct) : {conflict_metrics['trusted_vision']:2d}/{conflict_metrics['total']:2d} ({(conflict_metrics['trusted_vision'] / conflict_metrics['total']) * 100:.1f}%)"
    )
    print(
        f"Model trusted Coords (Failure) : {conflict_metrics['trusted_coords']:2d}/{conflict_metrics['total']:2d} ({(conflict_metrics['trusted_coords'] / conflict_metrics['total']) * 100:.1f}%)"
    )
    print(
        f"Model got Confused   (Neither) : {conflict_metrics['trusted_neither']:2d}/{conflict_metrics['total']:2d} ({(conflict_metrics['trusted_neither'] / conflict_metrics['total']) * 100:.1f}%)"
    )
    print("=" * 55)

    t_done = time.perf_counter()
    print(
        f"\nTotal runtime: {t_done - t_start:.2f}s "
        f"(model load: {t_model_loaded - t_start:.2f}s, eval: {t_done - t_model_loaded:.2f}s)"
    )


if __name__ == "__main__":
    main()
