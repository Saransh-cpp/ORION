# Results from validation of the fine-tuned model

--- Condition A: Full System (Vision + Coords) ---
HIGH : 11/18 (61.1%)
MEDIUM: 18/23 (78.3%)
LOW : 16/19 (84.2%)
TOTAL : 45/60 (75.0%)

--- Condition B: Vision Only (No Coords) ---
HIGH : 11/18 (61.1%)
MEDIUM: 19/23 (82.6%)
LOW : 17/19 (89.5%)
TOTAL : 47/60 (78.3%)

--- Condition C: Blind LLM (Gaussian Noise + Coords) ---
HIGH : 0/18 (0.0%)
MEDIUM: 23/23 (100.0%)
LOW : 1/19 (5.3%)
TOTAL : 24/60 (40.0%)

--- Condition D: Sensor Conflict (Real Vision + Fake Coords) ---
Model trusted Vision (Correct) : 45/60 (75.0%)
Model trusted Coords (Failure) : 2/60 (3.3%)
Model got Confused (Neither) : 13/60 (21.7%)
