# ORION

Orbital Real-time Inference and Observation Network.

[![Flight Segment CI](https://github.com/Saransh-cpp/ORION/actions/workflows/fs_ci.yml/badge.svg)](https://github.com/Saransh-cpp/ORION/actions/workflows/fs_ci.yml)

export ORION_GGUF_PATH="/Users/schopra/Code/Personal/LiquidAIDPhiHack/ORION/ground_segment/training/orion-q4_k_m.gguf"
export ORION_MMPROJ_PATH="/Users/schopra/Code/Personal/LiquidAIDPhiHack/ORION/ground_segment/training/orion-mmproj-f16.gguf"
export ORION_MEDIUM_STORAGE_DIR="/tmp/orion_medium/"
export ORION_DOWNLINK_QUEUE_DIR="/Users/schopra/Code/Personal/LiquidAIDPhiHack/ORION/ground_segment/data/orion_downlink_queue/"

Pi:

docker compose build
docker compose run --rm pi-build
scp build-output/Orion saransh@baryon:/home/saransh/ORION/
scp ground_segment/training/orion-q4_k_m.gguf saransh@baryon:/home/saransh/ORION/
scp ground_segment/training/orion-mmproj-f16.gguf saransh@baryon:/home/saransh/ORION/

export ORION_SIMSAT_URL=http://192.168.1.183:9005
./Orion -a 192.168.1.183 -p 50000

Mac:

fprime-gds -n --ip-address 0.0.0.0 --ip-port 50000

## TODO

Pi deployment.

Stress testing — rapid captures (lower interval to 10s), SimSat going down mid-capture, receiver not running, buffer pool exhaustion (fill the VLM queue). Verify graceful degradation everywhere.

CI pipeline — fprime-util build in a GitHub Action. The main challenge is llama.cpp static libs + libcurl. A Docker-based CI using your existing Dockerfile.base would be the fastest path.

Ground receiver improvements — auto-convert .raw to .png on receive, + show VLM verdicts in real-time.

Docs site?

- [ ] Check which HIGH are actually being misclassified
- [ ] Old images from disasters

## Phases

- [ ] Model
  - [x] Training data
  - [x] Ablation study (need)
  - [x] Fine-tuning
  - [x] Validation
- [x] F-prime application
  - [x] F-prime code
    - [x] Downlink Medium files
    - [x] Docker
  - [x] Model format + connect it to the F-prime app
- [ ] Deployment
  - [x] Learn Raspberry Pi
  - [x] Compile F-prime code for Raspberry Pi
