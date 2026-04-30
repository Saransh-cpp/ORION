# Evaluation and Ablation Studies

> For design details - evaluation conditions, mismatch logic, Gaussian noise methodology, and configuration parameters - see the [Training Pipeline architecture](../ground-segment/training.md) page.

## Evaluating the Fine-Tuned Model

```bash
cd ground_segment/training
python evaluate.py
```

This loads the base model with the LoRA adapters grafted on, sets the model to eval mode, and runs inference under all four evaluation conditions (A, B, C, D) for each test sample.

## Running the Ablation Study (Base Model)

```bash
cd ground_segment/experiments
python ablation.py
```

This loads the raw base model without any LoRA adapters and runs the identical four-condition evaluation.

## Interpreting Results

### Per-Class Metrics

Both scripts print per-class recall and precision for conditions A, B, and C:

```text
--- Condition A: Full System (Vision + Coords) ---
HIGH  : 11/18 (61.1% Recall) | Precision: 11/14 (78.6%)
MEDIUM: 18/23 (78.3% Recall) | Precision: 18/22 (81.8%)
LOW   : 16/19 (84.2% Recall) | Precision: 16/24 (66.7%)
TOTAL : 45/60 (75.0% Overall Accuracy)
```

- **Recall**: Of all actual HIGH/MEDIUM/LOW samples, how many did the model correctly identify?
- **Precision**: Of all samples the model labeled as HIGH/MEDIUM/LOW, how many were correct?
- **Overall Accuracy**: Total correct predictions divided by total samples.

### Condition D: Sensor Conflict

```text
--- Condition D: Sensor Conflict (Real Vision + Fake Coords) ---
Model trusted Vision (Correct) : 45/60 (75.0%)
Model trusted Coords (Failure) : 2/60 (3.3%)
Model got Confused   (Neither) : 13/60 (21.7%)
```

- **Trusted Vision**: The model correctly classified based on the image despite misleading coordinates. This is the desired behavior.
- **Trusted Coords**: The model classified according to the fake coordinates, ignoring the image. This is a failure mode.
- **Neither**: The model output a category that matches neither the image truth nor the fake coordinates.

### Key Comparisons

When comparing ablation (base model) vs. evaluation (fine-tuned model), look for:

1. **Condition A improvement**: Does fine-tuning improve overall accuracy?
2. **Condition B vs. A gap**: A small gap means the model relies primarily on visual features. A large gap means it depends on coordinates.
3. **Condition C accuracy**: Should be near chance (~33%). If higher, the model may be memorizing coordinate-to-category mappings.
4. **Condition D vision trust rate**: Higher is better. The model should trust its visual analysis over misleading telemetry.

## Historical Results

For historical study results, refer to:

- **`ground_segment/ablation_logs.md`** - Full sample-by-sample ablation study output with aggregate results from the base model.
- **`ground_segment/evaluate_logs.md`** - Full sample-by-sample evaluation output with aggregate results from the fine-tuned model.
