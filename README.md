# ORION

Orbital Real-time Inference and Observation Network.

## TODO

- [ ] Old images from disasters

## Phases

- [ ] Model
  - [x] Training data
  - [x] Ablation study (need)
  - [ ] Fine-tuning
- [ ] F-prime application
  - [ ] F-prime code
  - [ ] Model format + connect it to the F-prime app
- [ ] Deployment
  - [ ] Learn Raspberry Pi
  - [ ] Compile F-prime code for Raspberry Pi

## Ablation study results

--- Condition A: Full System (Vision + Coords) ---
HIGH : 16/19 (84.2%)
MEDIUM: 5/18 (27.8%)
LOW : 2/23 (8.7%)
TOTAL : 23/60 (38.3%)

--- Condition B: Vision Only (No Coords) ---
HIGH : 18/19 (94.7%)
MEDIUM: 5/18 (27.8%)
LOW : 4/23 (17.4%)
TOTAL : 27/60 (45.0%)

--- Condition C: Blind LLM (Gaussian Noise + Coords) ---
HIGH : 19/19 (100.0%)
MEDIUM: 0/18 (0.0%)
LOW : 0/23 (0.0%)
TOTAL : 19/60 (31.7%)

--- Condition D: Sensor Conflict (Real Vision + Fake Coords) ---
Model trusted Vision (Correct) : 21/60 (35.0%)
Model trusted Coords (Failure) : 32/60 (53.3%)
Model got Confused (Neither) : 7/60 (11.7%)
