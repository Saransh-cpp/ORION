"""Base-model ablation study - 4-condition evaluation of the **untuned** LFM2.5-VL-1.6B.

Runs the base (non-fine-tuned) Liquid VLM through the same 4-condition protocol used by
the fine-tuned evaluation in `evaluate`. Comparing the two scripts' outputs quantifies
the effect of QLoRA fine-tuning on each input channel.

**Conditions:**

| ID | Name | Image | Coordinates | Tests |
|----|------|-------|-------------|-------|
| A  | Full system | Real | Real | Nominal end-to-end accuracy |
| B  | Vision only | Real | Stripped | Visual-feature reliance |
| C  | Blind LLM | Gaussian noise | Real | Coordinate memorisation |
| D  | Sensor conflict | Real | Spoofed | Vision-vs-telemetry trust |

Usage:

```bash
cd ground_segment/experiments
uv run ablation.py              # test split (default)
uv run ablation.py --file val   # validation split
```

See the [validation and ablation studies guide](../../../../guides/studies/) for
how to interpret each condition and compare against the fine-tuned results.
"""

import argparse
import json
import time
import torch
import re
import random
import numpy as np
from PIL import Image
from transformers import AutoProcessor, AutoModelForImageTextToText

MODEL_ID = "LiquidAI/LFM2.5-VL-1.6B"
DATASET_DIR = "../data/orion_dataset"
TEST_FILE = f"{DATASET_DIR}/test_dataset.jsonl"
VAL_FILE = f"{DATASET_DIR}/val_dataset.jsonl"
random.seed(42)


def extract_json(text):
    """Extract the first JSON object from VLM output, falling back to an ERROR dict.

    Scans *text* for the outermost ``{…}`` pair and attempts ``json.loads``.
    If the model produced blank output, hallucinated prose, or malformed JSON,
    returns a sentinel ``{"category": "ERROR", "reason": "…"}`` so that the
    caller always gets a dict with a ``category`` key.

    Args:
        text: Raw decoded string from the VLM's generation output.

    Returns:
        A dict with at least a ``category`` key (``HIGH``, ``MEDIUM``, ``LOW``,
        or ``ERROR``).
    """
    try:
        start = text.find("{")
        end = text.rfind("}") + 1
        if start != -1 and end != 0:
            return json.loads(text[start:end])
    except json.JSONDecodeError:
        pass

    # Complete failure (Model output blank or hallucinated garbage)
    return {"category": "ERROR", "reason": f"Raw: {text.strip()[:50]}"}


def run_inference(model, processor, image, prompt):
    """Run a single image+prompt through the VLM and return parsed JSON plus raw text.

    Applies the processor's chat template, runs greedy generation with a
    200-token cap, then parses the output via `extract_json`.

    Args:
        model: Loaded ``AutoModelForImageTextToText`` instance.
        processor: Matching ``AutoProcessor`` for tokenisation and image encoding.
        image: PIL Image (512x512 RGB) to classify.
        prompt: Text prompt including classification instructions and (optionally)
            GPS coordinates.

    Returns:
        A ``(parsed, raw)`` tuple where *parsed* is the dict from `extract_json`
        and *raw* is the full decoded generation string.
    """
    messages = [{"role": "user", "content": f"<image>\n{prompt}"}]
    text_input = processor.apply_chat_template(
        messages, tokenize=False, add_generation_prompt=True
    )

    inputs = processor(images=[image], text=[text_input], return_tensors="pt").to(
        model.device
    )

    with torch.no_grad():
        # Using greedy decoding (do_sample=False) for strict JSON classification
        output = model.generate(**inputs, max_new_tokens=200, do_sample=False)

    generated_text = processor.decode(
        output[0][inputs["input_ids"].shape[1] :], skip_special_tokens=True
    )

    # Return both the parsed dictionary and the raw string for debugging
    return extract_json(generated_text), generated_text


def print_confusion_matrix(truths, preds, condition_name):
    """Print per-class recall/precision and aggregate accuracy for one condition.

    Iterates over the three triage classes (HIGH, MEDIUM, LOW), computing
    recall and precision for each, then prints overall accuracy.

    Args:
        truths: List of ground-truth category strings.
        preds: List of predicted category strings (same length as *truths*).
        condition_name: Human-readable label printed as the section header
            (e.g. ``"Condition A: Full System (Vision + Coords)"``).
    """
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
    """Run the 4-condition ablation protocol on the base (untuned) LFM2.5-VL-1.6B.

    Loads the base model from Hugging Face, iterates over every sample in the
    chosen split, and evaluates each sample under all four conditions (A–D).
    Prints per-class recall/precision tables for Conditions A–C and a
    vision-vs-coordinate trust breakdown for Condition D.
    """
    t_start = time.perf_counter()

    parser = argparse.ArgumentParser(
        description="Run the BASE-MODEL 4-condition ablation protocol on a held-out set."
    )
    parser.add_argument(
        "--file",
        choices=["test", "val"],
        default="test",
        help="Which split to evaluate: 'test' (60 IID held-out, judge-facing baseline) "
        "or 'val' (60 IID validation set).",
    )
    args = parser.parse_args()

    eval_file = TEST_FILE if args.file == "test" else VAL_FILE
    print(f" Loading Liquid VLM for Strict Ablation Study on '{args.file}' split")
    print(f" File: {eval_file}\n")

    processor = AutoProcessor.from_pretrained(MODEL_ID, trust_remote_code=True)

    model = AutoModelForImageTextToText.from_pretrained(
        MODEL_ID, device_map="auto", torch_dtype=torch.float16, trust_remote_code=True
    )
    model.eval()

    t_model_loaded = time.perf_counter()
    print(f" Model loaded in {t_model_loaded - t_start:.2f}s.")

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

    # Generate purely random Gaussian noise for Condition C to prevent "ocean" bias
    np.random.seed(42)
    noise_array = np.random.randint(0, 256, (512, 512, 3), dtype=np.uint8)
    noise_image = Image.fromarray(noise_array)

    # Pre-calculate mapping to force extreme mismatches for Condition D
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

        # Condition D: Force an extreme mismatched coordinate
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

        # Log standard conditions
        metrics["A"]["truths"].append(ground_truth)
        metrics["A"]["preds"].append(res_a.get("category"))

        metrics["B"]["truths"].append(ground_truth)
        metrics["B"]["preds"].append(res_b.get("category"))

        metrics["C"]["truths"].append(ground_truth)
        metrics["C"]["preds"].append(res_c.get("category"))

        # Log conflict condition
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

    # --- PRINT FINAL MATRIX ---
    print("\n" + "=" * 55)
    print(" ABLATION STUDY RESULTS (PER-CLASS & AGGREGATE)")
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
