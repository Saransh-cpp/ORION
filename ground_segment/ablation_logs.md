# Logs from ablation study

--- Condition A: Full System (Vision + Coords) ---
HIGH : 8/14 (57.1% Recall) | Precision: 8/17 (47.1%)
MEDIUM: 9/25 (36.0% Recall) | Precision: 9/13 (69.2%)
LOW : 18/21 (85.7% Recall) | Precision: 18/30 (60.0%)
TOTAL : 35/60 (58.3% Overall Accuracy)

--- Condition B: Vision Only (No Coords) ---
HIGH : 9/14 (64.3% Recall) | Precision: 9/16 (56.2%)
MEDIUM: 8/25 (32.0% Recall) | Precision: 8/11 (72.7%)
LOW : 19/21 (90.5% Recall) | Precision: 19/33 (57.6%)
TOTAL : 36/60 (60.0% Overall Accuracy)

--- Condition C: Blind LLM (Gaussian Noise + Coords) ---
HIGH : 0/14 (0.0% Recall) | Precision: 0/0 (0.0%)
MEDIUM: 0/25 (0.0% Recall) | Precision: 0/0 (0.0%)
LOW : 21/21 (100.0% Recall) | Precision: 21/60 (35.0%)
TOTAL : 21/60 (35.0% Overall Accuracy)

--- Condition D: Sensor Conflict (Real Vision + Fake Coords) ---
Model trusted Vision (Correct) : 35/60 (58.3%)
Model trusted Coords (Failure) : 12/60 (20.0%)
Model got Confused (Neither) : 13/60 (21.7%)
