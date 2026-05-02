# Results from validation of the fine-tuned model

--- Condition A: Full System (Vision + Coords) ---
HIGH : 7/14 (50.0% Recall) | Precision: 7/15 (46.7%)
MEDIUM: 10/25 (40.0% Recall) | Precision: 10/15 (66.7%)
LOW : 18/21 (85.7% Recall) | Precision: 18/30 (60.0%)
TOTAL : 35/60 (58.3% Overall Accuracy)

--- Condition B: Vision Only (No Coords) ---
HIGH : 9/14 (64.3% Recall) | Precision: 9/15 (60.0%)
MEDIUM: 12/25 (48.0% Recall) | Precision: 12/17 (70.6%)
LOW : 18/21 (85.7% Recall) | Precision: 18/28 (64.3%)
TOTAL : 39/60 (65.0% Overall Accuracy)

--- Condition C: Blind LLM (Gaussian Noise + Coords) ---
HIGH : 1/14 ( 7.1% Recall) | Precision: 1/ 1 (100.0%)
MEDIUM: 25/25 (100.0% Recall) | Precision: 25/59 (42.4%)
LOW : 0/21 ( 0.0% Recall) | Precision: 0/ 0 (0.0%)
TOTAL : 26/60 (43.3% Overall Accuracy)

--- Condition D: Sensor Conflict (Real Vision + Fake Coords) ---
Model trusted Vision (Correct) : 37/60 (61.7%)
Model trusted Coords (Failure) : 10/60 (16.7%)
Model got Confused (Neither) : 13/60 (21.7%)
