# Evaluation and Ablation Studies

> For design details, evaluation conditions, mismatch logic, Gaussian noise methodology, and configuration parameters, see the [Training Pipeline architecture](../ground-segment/training.md) page.

## Evaluating the Fine-Tuned Model

```bash
# make sure the data is generated, the LoRA adapters
# are present, and the environment created during
# data generation in ground_segment is active
cd ground_segment/training
uv run evaluate.py
```

This loads the base model with the LoRA adapters grafted on, sets the model to eval mode, and runs inference under all four evaluation conditions (A, B, C, D) for each test sample.

## Running the Ablation Study (Base Model)

```bash
# make sure the data is generated and the
# environment created during data generation in
# ground_segment is active
cd ground_segment/experiments
uv run ablation.py
```

This loads the raw base model without any LoRA adapters and runs the identical four-condition evaluation.

## Interpreting Results

### Per-Class Metrics

Both scripts print per-class recall and precision for conditions A, B, and C. The format looks like this (numbers below are the actual ORION fine-tuned model results on the 60-sample test set):

```text
--- Condition A: Full System (Vision + Coords) ---
HIGH  :  7/14 (50.0% Recall) | Precision:  7/15 (46.7%)
MEDIUM: 10/25 (40.0% Recall) | Precision: 10/15 (66.7%)
LOW   : 18/21 (85.7% Recall) | Precision: 18/30 (60.0%)
TOTAL : 35/60 (58.3% Overall Accuracy)
```

- **Recall**: Of all actual HIGH/MEDIUM/LOW samples, how many did the model correctly identify?
- **Precision**: Of all samples the model labeled as HIGH/MEDIUM/LOW, how many were correct?
- **Overall Accuracy**: Total correct predictions divided by total samples.

### Condition D: Sensor Conflict

The format looks like this (numbers below are the actual ORION fine-tuned model results):

```text
--- Condition D: Sensor Conflict (Real Vision + Fake Coords) ---
Model trusted Vision (Correct) : 37/60 (61.7%)
Model trusted Coords (Failure) : 10/60 (16.7%)
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
