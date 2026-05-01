# ORION

<img src="https://raw.githubusercontent.com/Saransh-cpp/ORION/main/docs/assets/orion_logo.png" alt="ORION" width="300">

[![Flight Segment CI](https://github.com/Saransh-cpp/ORION/actions/workflows/fs_ci.yml/badge.svg)](https://github.com/Saransh-cpp/ORION/actions/workflows/fs_ci.yml)
[![Documentation](https://github.com/Saransh-cpp/ORION/actions/workflows/docs.yml/badge.svg)](https://saransh-cpp.github.io/ORION/)

Orbital Real-time Inference and Observation Network

An autonomous LEO satellite triage system built using:

- a fine-tuned [Liquid AI LFM2.5-VL-1.6B](https://huggingface.co/collections/LiquidAI/lfm2-vl) vision-language model (for inference),
- [NASA's F-Prime](https://github.com/nasa/fprime) (for flight software),
- [SimSat](https://github.com/DPhi-Space/SimSat) (to simulate real payload sensors - GNSS and a camera),
- a Raspberry Pi 5 (to act as satellite's OBC).

ORION solves the orbital bandwidth bottleneck: roughly 71% of Earth's surface is featureless ocean, yet a traditional satellite downlinks every captured frame. By running a Q4-quantized VLM on-board, ORION classifies each image as HIGH, MEDIUM, or LOW priority and only transmits the most strategically valuable observations in real time. The model runs directly on raw 512×512 RGB pixels, making it deployable on satellite's On-Board Computer (OBC) for any standard camera payload.

## Motivation

This project was born from a real problem flagged on a real mission. Being the Flight Software subsystems lead on [EPFL Spacecraft Team's](https://www.epflspacecraftteam.ch) [CHESS](https://www.epflspacecraftteam.ch/project#chess) mission (part of [ESA's Fly Your Satellite! Design Booster programme](https://www.esa.int/Education/Educational_Satellites/About_Design_Booster)), I received a Review Item Discrepancy (RID) during our Final Design Review (FDR) from an ESA expert:

> _"From experience I recommend thinking about pre-loading software that allows you to check that a picture is worth downloading before you do it."_

Earth observation satellites generate far more data than their limited comm windows can downlink, and most of it is open ocean, empty desert, cloud cover, which is scientifically worthless. Our mission's approach was to downlink everything and sort on the ground, which would have wasted precious bandwidth and pass time.

ORION runs a vision-language model directly on the satellite's on-board computer, classifies each frame in real time, and only downlinks what matters. The "what matters" can change mission-to-mission, which would require fine-tuning the VLM on specific data, but ORION serves as a prototype, showing that this approach is technically viable. Furthermore, the local experiments (and the math) proves that this approach does cut-down downlink data and runs in an orbital environment without much issues.

## Build and deployment

ORION builds natively on macOS/Linux for development, and cross-compiles for Raspberry Pi 5 via Docker. The flight segment uses CMake + F-Prime's `fprime-util` toolchain, and the ground segment uses `uv` for Python dependency management.

**Flight segment:**

- [Development](https://saransh-cpp.github.io/ORION/guides/installation/): Build from source (llama.cpp, F-Prime, Python venv)
- [Deployment](https://saransh-cpp.github.io/ORION/guides/deployment/): Docker cross-compile, scp to Pi, GDS connection
- [Environment variables](https://saransh-cpp.github.io/ORION/guides/environment-variables/): All configurable paths, ports, and thresholds

**Ground segment:**

- [Data generation](https://saransh-cpp.github.io/ORION/guides/data-gen/): Generate the training dataset from SimSat
- [Training](https://saransh-cpp.github.io/ORION/guides/training/): QLoRA fine-tuning of LFM2.5-VL-1.6B
- [Quantization](https://saransh-cpp.github.io/ORION/guides/quantization/): Convert to GGUF Q4_K_M for Pi deployment
- [Receiver](https://saransh-cpp.github.io/ORION/guides/receiver/): Run the ground station image receiver
- [Studies](https://saransh-cpp.github.io/ORION/guides/studies/): Validation and ablation evaluation

## Architecture

The system is split into a [flight segment](https://saransh-cpp.github.io/ORION/components/) (6 F-Prime components on Pi 5) and a [ground segment](https://saransh-cpp.github.io/ORION/ground-segment/) (receiver, training pipeline, dataset). The flight segment runs an FPP state machine governing four mission modes, with autonomous comm window detection via Haversine distance to the ground station at EPFL.

- [System overview](https://saransh-cpp.github.io/ORION/architecture/overview/): Component inventory, rate groups, ground segment
- [State machine](https://saransh-cpp.github.io/ORION/architecture/state-machine/): IDLE / MEASURE / DOWNLINK / SAFE transitions
- [Data flow](https://saransh-cpp.github.io/ORION/architecture/data-flow/): Capture to downlink pipeline, ORIO frame protocol

## Results

> Full quantitative breakdown: [Mission Budgets](https://saransh-cpp.github.io/ORION/architecture/budgets/) · [Ground Segment Budgets](https://saransh-cpp.github.io/ORION/ground-segment/budgets/)

### On-board inference (Raspberry Pi 5, Cortex-A76, no NPU/GPU)

| Metric                                    | Value                                          |
| ----------------------------------------- | ---------------------------------------------- |
| Vision encoding (mtmd)                    | ~10–15 s                                       |
| Token generation (200 tokens max, greedy) | ~40–55 s                                       |
| **Total per frame**                       | **50–72 s**                                    |
| Self-watchdog ceiling                     | 120 s (frame dropped, model stays loaded)      |
| Frames captured per 35-min eclipse        | ~32 (65 s capture interval)                    |
| Frames inferred per eclipse               | ~7–9 (bottleneck: inference time, not capture) |
| VLM duty cycle per orbit                  | ~7–9%                                          |

**Memory in MEASURE mode (Pi 5, 4 GB RAM):**

| Component                              | Size          |
| -------------------------------------- | ------------- |
| Q4_K_M GGUF weights                    | ~700 MB       |
| F16 vision projector (mmproj)          | ~50 MB        |
| KV cache (4096 ctx, per inference)     | ~64 MB        |
| Static frame buffer pool (20 × 786 KB) | ~16 MB        |
| F-Prime framework + Linux              | ~220 MB       |
| **Total**                              | **~1,050 MB** |

No runtime dynamic allocation. All frame memory is pre-allocated at startup; model loads once on MEASURE entry.

### Model accuracy (60-sample test set, 3-class: HIGH / MEDIUM / LOW)

> **TODO:** replace with 360-target dataset results after retraining

> Each row in the logs below includes the satellite image, the VLM's full reasoning string, and the ground-truth label — not just summary numbers. Full per-sample logs: [ablation_logs.md](ground_segment/ablation_logs.md) (base model, 60 samples × 4 conditions) · [evaluate_logs.md](ground_segment/evaluate_logs.md) (fine-tuned, 60 samples × 4 conditions). Both are embedded in full in the [model card](https://saransh-cpp.github.io/ORION/ground-segment/model-card/).

| Condition                               | Base model | Fine-tuned | Δ        |
| --------------------------------------- | ---------- | ---------- | -------- |
| A — Vision + GPS coords                 | 58.3%      | 75.0%      | +16.7 pp |
| B — Vision only (no coords)             | 60.0%      | 78.3%      | +18.3 pp |
| C — Blind LLM (Gaussian noise + coords) | 35.0%      | 40.0%      | —        |

**Condition D — Sensor conflict (real image, spoofed GPS coords):** after fine-tuning, the model trusts visual evidence in 75.0% of conflict cases and defers to the incorrect GPS coordinate in only 3.3% — down from 20.0% on the base model. Coordinate dropout during training teaches the model to treat GPS as a hint rather than an oracle.

### Bandwidth savings

Each frame is 786 KB (512×512 RGB). The triage doctrine — HIGH downlinked immediately, MEDIUM stored for bulk transfer, LOW discarded — eliminates transmission of all LOW frames.

Expected triage distribution on a random LEO track (based on target morphology distribution across the training dataset):

| Verdict             | Expected ratio | Data per orbit           | Action                         |
| ------------------- | -------------- | ------------------------ | ------------------------------ |
| LOW                 | ~60–70%        | 0 bytes (discarded)      | Buffer recycled immediately    |
| MEDIUM              | ~20–30%        | ~1.5–2.3 MB (stored)     | Written to microSD             |
| HIGH                | ~5–10%         | ~384–768 KB (downlinked) | Transmitted during comm window |
| **Bandwidth saved** | **~90–95%**    |                          | vs. downlinking every frame    |

> **TODO:** replace with actual triage distribution (HIGH / MEDIUM / LOW counts and %) from end-to-end Pi run

## Usage

The following section goes through the basic usage of this prototype. Refer to the [SDD](https://saransh-cpp.github.io/ORION/architecture/overview/) files for more commands, telemetry, and data handling.

### Prerequisites

- A compiled ORION binary (see [Installation](https://saransh-cpp.github.io/ORION/guides/installation/) and [Deployment](https://saransh-cpp.github.io/ORION/guides/deployment/))
- The GGUF model files: [`orion-q4_k_m.gguf` and `orion-mmproj-f16.gguf`](https://drive.google.com/drive/folders/1h6WGNeNzYHdfisELlJodDCKlkREkIzCN?usp=share_link)
- [SimSat](https://github.com/DPhi-Space/SimSat) running and accessible (default `http://localhost:9005`)
- [Environment variables](https://saransh-cpp.github.io/ORION/guides/environment-variables/) configured

### What each part maps to on a real satellite

| ORION Component            | Real Satellite Equivalent                 |
| -------------------------- | ----------------------------------------- |
| `EventAction` (C++)        | OBC mode manager / FDIR logic             |
| `NavTelemetry` (C++)       | GNSS receiver payload                     |
| `CameraManager` (C++)      | Earth observation camera payload          |
| `VlmInferenceEngine` (C++) | On-board AI co-processor                  |
| `TriageRouter` (C++)       | On-board data handling unit               |
| `GroundCommsDriver` (C++)  | X-band radio transmitter                  |
| `BufferManager` (F-Prime)  | On-board mass memory                      |
| `comDriver` (F-Prime)      | UHF radio transceiver                     |
| Raspberry Pi 5             | On-board computer                         |
| SimSat                     | GNSS receiver hardware                    |
| SimSat Mapbox API          | Earth observation camera payload hardware |
| `receiver.py`              | Ground station X-band receiver            |
| F-Prime GDS                | Mission control software                  |

### Start SimSat and connect GDS

[SimSat](https://github.com/DPhi-Space/SimSat) provides position data and Mapbox imagery. Start it on your ground station machine (default port 9005).

Launch the ORION binary on the Pi (or locally for development):

```bash
./Orion -a <gds-host-ip> -p 50000
```

Connect GDS from the ground station:

```bash
fprime-gds -n --ip-address 0.0.0.0 --ip-port 50000
```

Open `http://localhost:5000`: you should see `SimSatPositionUpdate` events arriving every 5 seconds. The satellite starts in **IDLE** mode (charging).

### Enter MEASURE mode

```text
SET_ECLIPSE true
```

IDLE transitions to MEASURE (eclipse = imaging on battery). You will see:

- `ModeChanged: IDLE -> MEASURE`
- `ModelLoaded`: VLM loads into RAM (~15s on Pi)
- `AutoCaptureEnabled every 65 seconds`

### Observe autonomous capture and inference

Every 65 seconds, CameraManager fetches a Mapbox satellite tile, fuses GPS, and dispatches to the VLM. Watch for:

- `ImageDispatched`: image captured at Lat/Lon
- `InferenceComplete`: VLM result: `HIGH`, `MEDIUM`, or `LOW` with reasoning and inference time

After inference, TriageRouter routes the frame:

- **HIGH**: `HighTargetDetected`, frame forwarded to GroundCommsDriver for downlink
- **MEDIUM**: `MediumTargetStored`, image saved to disk
- **LOW**: `LowTargetDiscarded`, buffer recycled

### Downlink during comm window

When the satellite passes within 2000 km of the ground station (EPFL Ecublens), EventAction auto-transitions to DOWNLINK:

- `CommWindowOpened (distance XXXX km)`
- `ModeChanged: MEASURE -> DOWNLINK`
- GroundCommsDriver flushes queued HIGH frames
- `FrameDownlinked` for each transmitted frame

Start the ground receiver to accept frames:

```bash
python ground_segment/receiver.py
```

To bulk-download MEDIUM images during the comm window:

```text
FLUSH_MEDIUM_STORAGE
```

Files are queued to F-Prime FileDownlink at 1 file/sec. Rejected if not in DOWNLINK.

### Return to IDLE

```text
SET_ECLIPSE false
```

Sun is visible: satellite returns to IDLE (charging). Model unloads, captures stop.

### Safe mode

Suspend all operations from any state:

```text
ENTER_SAFE_MODE
```

All components halt. The satellite stays in SAFE until ground commands:

```text
EXIT_SAFE_MODE
```

Returns to IDLE and re-evaluates conditions (comm window, eclipse) to auto-transition.

### Manual overrides

Force specific transitions for testing:

```text
GOTO_IDLE          # From MEASURE or DOWNLINK
GOTO_MEASURE       # From IDLE only
GOTO_DOWNLINK      # From IDLE only
```

Rejected with `GotoRejected` if the transition is not allowed from the current state.

## API reference

Auto-generated API documentation for both the C++ flight segment (via Doxygen) and the Python ground segment (via mkdocstrings).

- [C++ API](https://saransh-cpp.github.io/ORION/api/cpp/): Classes, namespaces, and source files
- [Python API](https://saransh-cpp.github.io/ORION/api/python/receiver/): Receiver, training, and data modules

## Contributing

ORION uses `clang-format` for C++, `ruff` for Python, and `pre-commit` hooks for automated formatting. CI runs a native clang-tidy build and a Docker ARM64 cross-compile on every push.

- [Contributing guide](https://saransh-cpp.github.io/ORION/contributing/): Dev setup, code style, CI pipeline, adding new components

## Why "ORION"?

A little easter egg: I grew up watching the Orion constellation every winter from my home in East Delhi. I would know winter is approaching when I could spot the belt before bedtime. There was not much light or air pollution back then, so I have very fond memories of looking up to find constellations in the night sky. I was always an Astronomy kid, but I never knew I would work on a real satellite mission (CHESS at EST) when I grow up. The name felt right for a project that looks down at Earth from the orbit.
