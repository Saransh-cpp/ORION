# ORION

Orbital Real-time Inference and Observation Network

[![Flight Segment CI](https://github.com/Saransh-cpp/ORION/actions/workflows/fs_ci.yml/badge.svg)](https://github.com/Saransh-cpp/ORION/actions/workflows/fs_ci.yml)
[![Documentation](https://github.com/Saransh-cpp/ORION/actions/workflows/docs.yml/badge.svg)](https://saransh-cpp.github.io/ORION/)

An autonomous LEO satellite triage system built using:

- a fine-tuned [Liquid AI LFM2.5-VL-1.6B](https://huggingface.co/collections/LiquidAI/lfm2-vl) vision-language model (for inference),
- [NASA's F-Prime](https://github.com/nasa/fprime) (for flight software),
- [SimSat](https://github.com/DPhi-Space/SimSat) (to simulate real payload sensors - GNSS and a camera),
- a Raspberry Pi 5 (to act as satellite's OBC).

ORION solves the orbital bandwidth bottleneck: roughly 71% of Earth's surface is featureless ocean, yet a traditional satellite downlinks every captured frame. By running a Q4-quantized VLM on-board, ORION classifies each image as HIGH, MEDIUM, or LOW priority and only transmits the most strategically valuable observations in real time.

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

## Usage

The following section goes through the basic usage of this prototype. Refer to the [SDD](https://saransh-cpp.github.io/ORION/architecture/overview/) files for more commands, telemetry, and data handling.

### Prerequisites

- A compiled ORION binary (see [Installation](https://saransh-cpp.github.io/ORION/guides/installation/) and [Deployment](https://saransh-cpp.github.io/ORION/guides/deployment/))
- The GGUF model files: [`orion-q4_k_m.gguf` and `orion-mmproj-f16.gguf`](https://drive.google.com/drive/folders/1h6WGNeNzYHdfisELlJodDCKlkREkIzCN?usp=share_link)
- [SimSat](https://github.com/DPhi-Space/SimSat) running and accessible (default `http://localhost:9005`)
- [Environment variables](https://saransh-cpp.github.io/ORION/guides/environment-variables/) configured

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

```
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

```
FLUSH_MEDIUM_STORAGE
```

Files are queued to F-Prime FileDownlink at 1 file/sec. Rejected if not in DOWNLINK.

### Return to IDLE

```
SET_ECLIPSE false
```

Sun is visible: satellite returns to IDLE (charging). Model unloads, captures stop.

### Safe mode

Suspend all operations from any state:

```
ENTER_SAFE_MODE
```

All components halt. The satellite stays in SAFE until ground commands:

```
EXIT_SAFE_MODE
```

Returns to IDLE and re-evaluates conditions (comm window, eclipse) to auto-transition.

### Manual overrides

Force specific transitions for testing:

```
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
