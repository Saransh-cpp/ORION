# ORION

Orbital Real-time Inference and Observation Network.

export ORION_GGUF_PATH="/Users/schopra/Code/Personal/LiquidAIDPhiHack/ORION/ground_segment/training/orion-q4_k_m.gguf"
export ORION_MMPROJ_PATH="/Users/schopra/Code/Personal/LiquidAIDPhiHack/ORION/ground_segment/training/orion-mmproj-f16.gguf"
export ORION_MEDIUM_STORAGE_DIR="/Users/schopra/Code/Personal/LiquidAIDPhiHack/ORION/ground_segment/data/orion_medium/"
export ORION_DOWNLINK_QUEUE_DIR="/Users/schopra/Code/Personal/LiquidAIDPhiHack/ORION/ground_segment/data/orion_downlink_queue/"

## TODO

- [ ] Check which HIGH are actually being misclassified
- [ ] Old images from disasters

## Phases

- [ ] Model
  - [x] Training data
  - [x] Ablation study (need)
  - [ ] Fine-tuning
  - [ ] Validation
- [ ] F-prime application
  - [ ] F-prime code
    - [ ] Downlink Medium files
    - [ ] Docker
  - [ ] Model format + connect it to the F-prime app
- [ ] Deployment
  - [ ] Learn Raspberry Pi
  - [ ] Compile F-prime code for Raspberry Pi

## Ablation study results

## First useful results

--- Condition A: Full System (Vision + Coords) ---
HIGH : 11/18 (61.1%)
MEDIUM: 14/23 (60.9%)
LOW : 16/19 (84.2%)
TOTAL : 41/60 (68.3%)

--- Condition B: Vision Only (No Coords) ---
HIGH : 14/18 (77.8%)
MEDIUM: 15/23 (65.2%)
LOW : 17/19 (89.5%)
TOTAL : 46/60 (76.7%)

--- Condition C: Blind LLM (Gaussian Noise + Coords) ---
HIGH : 0/18 (0.0%)
MEDIUM: 0/23 (0.0%)
LOW : 19/19 (100.0%)
TOTAL : 19/60 (31.7%)

--- Condition D: Sensor Conflict (Real Vision + Fake Coords) ---
Model trusted Vision (Correct) : 42/60 (70.0%)
Model trusted Coords (Failure) : 5/60 (8.3%)
Model got Confused (Neither) : 13/60 (21.7%)

## Validation results

### First useful results

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
