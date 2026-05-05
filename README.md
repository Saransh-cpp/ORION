# ORION

<img src="https://raw.githubusercontent.com/Saransh-cpp/ORION/main/docs/assets/orion_logo.png" alt="ORION" width="300">

[![Flight Segment CI](https://github.com/Saransh-cpp/ORION/actions/workflows/fs_ci.yml/badge.svg)](https://github.com/Saransh-cpp/ORION/actions/workflows/fs_ci.yml)
[![Documentation](https://github.com/Saransh-cpp/ORION/actions/workflows/docs.yml/badge.svg)](https://Saransh-cpp.github.io/ORION/)
[![Hugging Face](https://img.shields.io/badge/%F0%9F%A4%97%20Hugging%20Face-model-28A745?labelColor=24292E)](https://huggingface.co/Saransh-cpp/orion-qlora-lfm2.5-vl-1.6b)
[![Hugging Face](https://img.shields.io/badge/%F0%9F%A4%97%20Hugging%20Face-dataset-28A745?labelColor=24292E)](https://huggingface.co/datasets/saransh-cpp/orion-dataset)

Orbital Real-time Inference and Observation Network

An autonomous LEO satellite triage system built using:

- a fine-tuned [Liquid AI LFM2.5-VL-1.6B](https://huggingface.co/collections/LiquidAI/lfm2-vl) vision-language model (for inference),
- [NASA's F-Prime](https://github.com/nasa/fprime) (for flight software),
- [SimSat](https://github.com/DPhi-Space/SimSat) (to simulate real payload sensors - GNSS and a camera),
- a Raspberry Pi 5 (to act as the satellite's OBC).

ORION solves the orbital bandwidth bottleneck: roughly 71% of Earth's surface is featureless ocean, yet a traditional satellite downlinks every captured frame. By running a fine-tuned (on a custom dataset collected using SimSat) Q4-quantized VLM on-board, ORION classifies each image as HIGH, MEDIUM, or LOW priority and only transmits the most strategically valuable observations in real time. The model runs directly on raw 512×512 RGB pixels and is connected to a real flight software (not a Python wrapper), making it deployable on satellite's On-Board Computer (after rigorous testing and mission-specific tweaks) for any standard camera payload.

## Motivation

This project was born from a real problem flagged on a real mission. Being the Flight Software subsystems lead on [EPFL Spacecraft Team's](https://www.epflspacecraftteam.ch) [CHESS](https://www.epflspacecraftteam.ch/project#chess) mission (part of [ESA's Fly Your Satellite! Design Booster programme](https://www.esa.int/Education/Educational_Satellites/About_Design_Booster)), I received a Review Item Discrepancy during our Final Design Review from an ESA expert:

> _"From experience I recommend thinking about pre-loading software that allows you to check that a picture is worth downloading before you do it."_

Earth observation satellites generate far more data than their limited comm windows can downlink, and most of it is open ocean, empty desert, cloud cover, which is scientifically worthless. Our mission's approach was to downlink everything and sort on the ground, which would have wasted precious bandwidth and pass time.

I was looking for a solution and then [Hack #05: AI in Space](https://luma.com/n9cw58h0?tk=diGLxQ), a hackathon co-organised by [Liquid AI](https://www.liquid.ai/) and [DPhi Space](https://www.dphi.space/), happened. [DPhi](https://www.dphi.space/) provided [SimSat](https://github.com/DPhi-Space/SimSat) (the orbital simulator that feeds ORION GNSS coordinates and Mapbox imagery), and [Liquid](https://www.liquid.ai/) provided the [LFM2.5-VL-1.6B](https://huggingface.co/collections/LiquidAI/lfm2-vl) vision-language model (the base model for the fine-tuned VLM performing on-board triage). ORION is what came out of that month.

ORION runs a vision-language model directly on the satellite's on-board computer, classifies each frame in real time, and only downlinks what matters. The "what matters" can change mission-to-mission, which would require fine-tuning the VLM on specific data, but ORION serves as a prototype, showing that this approach is technically viable. Furthermore, the local experiments (and the math) prove that this approach does cut down downlink data and runs in an orbital environment without significant issues.

One of my other main aim with this project was to show that Liquid's LFM2.5-VL models can run in an orbital environment by simulating one end-to-end. DPhi recently (during the hackathon) [validated this on actual hardware in orbit](https://www.dphispace.com/news/clustergate-2-llm-on-orbit). ORION goes further: it runs entirely on CPU (no orbital GPU) and wraps the model in a complete triage system with flight software, autonomous mode management, and selective downlink.

## Build and deployment

ORION builds natively on macOS/Linux for development, and cross-compiles for Raspberry Pi 5 via Docker. The flight segment uses CMake + F-Prime's `fprime-util` toolchain (via `uv`), and the ground segment uses only `uv` for Python dependency management.

**Flight segment:**

- [Development](https://Saransh-cpp.github.io/ORION/guides/installation/): Build from source (llama.cpp, F-Prime, Python venv)
- [Deployment](https://Saransh-cpp.github.io/ORION/guides/deployment/): Docker cross-compile, scp to Pi, GDS connection
- [Environment variables](https://Saransh-cpp.github.io/ORION/guides/environment-variables/): All configurable paths, ports, and thresholds

**Ground segment:**

- [Data generation](https://Saransh-cpp.github.io/ORION/guides/data-gen/): Generate the training dataset from SimSat
- [Training](https://Saransh-cpp.github.io/ORION/guides/training/): QLoRA fine-tuning of LFM2.5-VL-1.6B
- [Quantization](https://Saransh-cpp.github.io/ORION/guides/quantization/): Convert to GGUF Q4_K_M for Pi deployment
- [Receiver](https://Saransh-cpp.github.io/ORION/guides/receiver/): Run the ground station image receiver
- [Studies](https://Saransh-cpp.github.io/ORION/guides/studies/): Validation and ablation evaluation

## Architecture

The system is split into a [flight segment](https://Saransh-cpp.github.io/ORION/components/) (6 F-Prime components on Pi 5) and a [ground segment](https://Saransh-cpp.github.io/ORION/ground-segment/) (receiver, training pipeline, dataset). The flight segment runs an FPP state machine governing four mission modes (IDLE, MEASURE, DOWNLINK, SAFE), with autonomous comm window detection via Haversine distance to the ground station at EPFL. All image buffers are pre-allocated at startup (20-slot static pool), and the VLM model is loaded/unloaded on mode transitions; hence, there is no runtime dynamic allocation. The VLM runs via [llama.cpp](https://github.com/ggml-org/llama.cpp)'s C API (statically linked), and image decoding/resizing uses vendored [stb_image](https://github.com/nothings/stb) headers. The flight and ground segments communicate over two independent links: the standard F-Prime command/telemetry channel (TCP :50000) and a custom [ORIO frame protocol](https://Saransh-cpp.github.io/ORION/ground-segment/receiver/#orio-frame-protocol) for real-time HIGH-priority image downlink (TCP :50050).

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

- [System overview](https://Saransh-cpp.github.io/ORION/architecture/overview/): Component inventory, rate groups, ground segment
- [State machine](https://Saransh-cpp.github.io/ORION/architecture/state-machine/): IDLE / MEASURE / DOWNLINK / SAFE transitions
- [Data flow](https://Saransh-cpp.github.io/ORION/architecture/data-flow/): Capture to downlink pipeline, ORIO frame protocol

## Results

> Full quantitative breakdown: [Mission Budgets](https://Saransh-cpp.github.io/ORION/architecture/budgets/) · [Ground Segment Budgets](https://Saransh-cpp.github.io/ORION/ground-segment/budgets/) · [Dataset & target definitions](https://Saransh-cpp.github.io/ORION/ground-segment/data/)

### Target definitions

The [custom dataset](https://Saransh-cpp.github.io/ORION/ground-segment/data/) contains 360 satellite images (120 per class) fetched from SimSat's Mapbox API, split into 240 train / 60 val / 60 test. Each class is defined by visual morphology:

| Class  | What it captures                                                               | Examples                                               |
| ------ | ------------------------------------------------------------------------------ | ------------------------------------------------------ |
| HIGH   | Extreme-scale strategic anomalies: dense geometric infrastructure, chokepoints | Mega-ports, mega-airports, nuclear plants, launch pads |
| MEDIUM | Standard human civilization: urban grids, suburban sprawl, agriculture         | City centers, farms, regional airports, rail yards     |
| LOW    | Featureless natural terrain: oceans, deserts, ice sheets, dense canopy         | Open ocean, Sahara, Antarctic ice, Amazon canopy       |

Each class includes deliberately hard sub-types (e.g., coastlines that mimic artificial structures for LOW, or isolated towns for MEDIUM) to stress-test the classifier. See the full [morphology breakdown](https://Saransh-cpp.github.io/ORION/ground-segment/data/#target-definitions) for details.

### On-board inference (Raspberry Pi 5, Cortex-A76, no NPU/GPU)

| Metric                                    | Value                                                        |
| ----------------------------------------- | ------------------------------------------------------------ |
| Vision encoding (mtmd)                    | ~10-15 s                                                     |
| Token generation (200 tokens max, greedy) | ~40-55 s                                                     |
| **Total per frame**                       | **50-80 s**                                                  |
| Self-watchdog ceiling                     | 120 s (frame dropped, model stays loaded)                    |
| Frames captured per 35-min eclipse        | ~24 (85 s capture interval)                                  |
| Frames inferred per eclipse               | ~32 (all captured; 5-frame queue absorbs inference overflow) |
| VLM duty cycle per orbit                  | ~32%                                                         |

**Memory in MEASURE mode (Pi 5, 8 GB RAM):**

| Component                              | Size (estimate) |
| -------------------------------------- | --------------- |
| Q4_K_M GGUF weights                    | ~730 MB         |
| F16 vision projector (mmproj)          | ~854 MB         |
| KV cache (4096 ctx, per inference)     | ~64 MB          |
| Static frame buffer pool (20 × 786 KB) | ~16 MB          |
| F-Prime framework + Linux              | ~220 MB         |
| **Total (estimate)**                   | **~1,884 MB**   |
| **Total (measured RSS on Pi 5)**       | **~1,753 MB**   |

No runtime dynamic allocation. All frame memory is pre-allocated at startup; model loads once on MEASURE entry.

### Model accuracy (60-sample test set, 3-class: HIGH / MEDIUM / LOW)

> Full per-condition logs (recall, precision, overall accuracy) and a detailed discussion on the results are embedded in the [model card](https://Saransh-cpp.github.io/ORION/ground-segment/model-card/). For a detailed explanation of each condition and how to interpret the numbers, see the [validation and ablation studies guide](https://Saransh-cpp.github.io/ORION/guides/studies/).

The table below compares the base LFM2.5-VL-1.6B model against the ORION fine-tuned model under four controlled conditions. Each condition isolates a different input channel (vision, GPS, or both) to measure what the model actually relies on for classification. Δ is the percentage-point gain from fine-tuning.

| Condition                              | Base model | Fine-tuned | Δ       |
| -------------------------------------- | ---------- | ---------- | ------- |
| A: Vision + GPS coords                 | 58.3%      | 58.3%      | 0 pp    |
| B: Vision only (no coords)             | 60.0%      | 65.0%      | +5.0 pp |
| C: Blind LLM (Gaussian noise + coords) | 35.0%      | 43.3%      | +8.3 pp |

**Condition D: Sensor conflict (real image, spoofed GPS coords):** coordinate-trust failure drops from 20.0% to 16.7% after fine-tuning. Visual reasoning improves on Conditions B (+5 pp) and C (+8.3 pp), confirming that the adapter does sharpen classification when GPS is absent or unreliable.

Condition A (nominal, vision + GPS) shows no gain on this dataset. The HIGH category spans five visually heterogeneous sub-types, mega-ports, airports, energy infrastructure, mines, and military facilities, across only 240 training images. That is not enough for the visual encoder to learn a reliable shared boundary. Fine-tuning on a narrower HIGH sub-type with a larger image corpus (1k-5k images per class) would close this gap significantly.

ORION demonstrates that on-board VLM inference on a Pi 5 is technically viable and that fine-tuning measurably improves robustness.

### Bandwidth savings

Each frame is 786 KB (512×512 RGB). The triage doctrine, HIGH downlinked immediately, MEDIUM stored for bulk transfer, LOW discarded, eliminates transmission of all LOW frames.

Expected triage distribution on a random LEO track (based on target morphology distribution across the training dataset):

| Verdict             | Expected ratio | Data per orbit           | Action                         |
| ------------------- | -------------- | ------------------------ | ------------------------------ |
| LOW                 | ~60-70%        | 0 bytes (discarded)      | Buffer recycled immediately    |
| MEDIUM              | ~20-30%        | ~1.5-2.3 MB (stored)     | Written to microSD             |
| HIGH                | ~5-10%         | ~384-768 KB (downlinked) | Transmitted during comm window |
| **Bandwidth saved** | **~90-95%**    |                          | vs. downlinking every frame    |

> **TODO:** replace with actual triage distribution (HIGH / MEDIUM / LOW counts and %) from end-to-end Pi run

## Usage

The following section goes through the basic usage of this prototype. Refer to the [SDD](https://Saransh-cpp.github.io/ORION/architecture/overview/) files for more commands, telemetry, and data handling.

### Prerequisites

- A compiled ORION binary (see [Installation](https://Saransh-cpp.github.io/ORION/guides/installation/) and [Deployment](https://Saransh-cpp.github.io/ORION/guides/deployment/))
- The GGUF model files: [`orion-q4_k_m.gguf` and `orion-mmproj-f16.gguf`](https://huggingface.co/Saransh-cpp/orion-qlora-lfm2.5-vl-1.6b)
- [SimSat](https://github.com/DPhi-Space/SimSat) running and accessible (default `http://localhost:9005`)
- [Environment variables](https://Saransh-cpp.github.io/ORION/guides/environment-variables/) configured

### Start SimSat and connect GDS

[SimSat](https://github.com/DPhi-Space/SimSat) provides position data and Mapbox imagery. Set `MAPBOX_ACCESS_TOKEN` (see SimSat's documentation) and start it on your ground station machine / a stand-alone terminal (default port 9005 and 8000). Remember to actually start the simulation.

Launch the ORION binary on the Pi:

> [!NOTE]
> You don't need to launch the binary manually for development build (running it in a single-machine setup, on your local Linux/OSX machine).

```bash
./Orion -a <gds-host-ip> -p 50000 # on Pi
```

If running the binary on Pi, connect GDS from the ground station (your local machine):

```bash
# from flight_segment/orion
# make sure the environment created during installation is active
fprime-gds -n --ip-address 0.0.0.0 --ip-port 50000 --file-storage-directory ../../ground_segment/data/downlinked_UHF
```

If running the whole setup on your local machine, launching the GDS will automatically run the FS binary in background (and wire all the addresses automatically):

> [!WARNING]
> Make sure your environment variables are configured correctly, especially `ORION_GGUF_PATH` and `ORION_MMPROJ_PATH`. See the pre-requisites section above for more information.

```bash
# from flight_segment/orion
# make sure the environment created during installation is active
fprime-gds --file-storage-directory ../../ground_segment/data/downlinked_UHF
```

Open `http://localhost:5000`: you should see `SimSatPositionUpdate` events arriving every 5 seconds. The satellite starts in **IDLE** mode (charging).

### Enter MEASURE mode

In `Commanding` tab of the GDS, send `Orion.eventAction.SET_ECLIPSE` command with `True` as the only argument:

```text
SET_ECLIPSE true
```

IDLE transitions to MEASURE (eclipse = imaging on battery). In the `Events` tab (or in `flight_segment/orion/logs/<latest-one>/event.log`), you will see:

- `ModeChanged: IDLE -> MEASURE`
- `ModelLoaded`: VLM loads into RAM (~15s on Pi)
- `AutoCaptureEnabled every 85 seconds`

These events are sent by the satellite (Pi or your local terminal instance running the flight software binary, whether explicitly or in a background process by `fprime-gds`) to the GDS machine (your local machine or the terminal instance running GDS) over TCP.

### Observe autonomous capture and inference

Every 85 seconds, CameraManager fetches a Mapbox satellite tile, fuses GPS, and dispatches to the VLM. Watch for the following in `Events` tab (or in `flight_segment/orion/logs/<latest-one>/event.log`):

- `ImageDispatched`: image captured at Lat/Lon
- `InferenceComplete`: VLM result: `HIGH`, `MEDIUM`, or `LOW` with reasoning and inference time

After inference, TriageRouter routes the frame:

- **HIGH**: `HighTargetDetected`, frame forwarded to GroundCommsDriver for downlink on the Pi / your local machine through the process acting as the satellite's OBC (launched by fprime-gds in the background). The files are stored in `/home/<user>/ORION/media/sd/downlink_XBand_queue` (Pi) / `flight_segment/orion/media/sd/downlink_XBand_queue/` (single machine set-up) / the path overridden through environment variables.
- **MEDIUM**: `MediumTargetStored`, image saved to disk on the Pi / your local machine through the process acting as the satellite's OBC (launched by fprime-gds in the background). The files are stored in `/home/<user>/ORION/media/sd/medium` (Pi) / `flight_segment/orion/media/sd/medium/` (single machine set-up) / the path overridden through environment variables.
- **LOW**: `LowTargetDiscarded`, buffer recycled

### Downlink during comm window

When the satellite passes within 2000 km of the ground station (EPFL Ecublens) (you can speed up the simulation for this using SimSat), EventAction auto-transitions to DOWNLINK:

- `CommWindowOpened (distance XXXX km)`
- `ModeChanged: MEASURE -> DOWNLINK`
- GroundCommsDriver flushes queued HIGH frames
- `FrameDownlinked` for each transmitted frame

Start the ground receiver (in another terminal) to accept frames (can be started earlier):

```bash
cd ground_segment
# in the ground segment venv
uv run receiver.py
```

**HIGH frames** are downlinked automatically via the ORIO protocol (TCP :50050). Each file is **deleted** after successful transmission. The receiver saves them to `ground_segment/data/downlinked_XBand/` as `orion_frame_XXXX.raw` + `orion_frame_XXXX.jpg`. On a single-machine setup, both directories are on the same machine.

**MEDIUM frames** can be bulk-downloaded during the comm window by sending the following command from the `Commanding` tab of GDS:

```text
FLUSH_MEDIUM_STORAGE
```

This queues one file per second to F-Prime's FileDownlink service (TCP :50000). Each file is **renamed** to `.raw.sent` before transmission to avoid re-queuing. Files arrive in `ground_segment/data/downlinked_UHF/` (the directory set via `--file-storage-directory` when launching GDS). The command is rejected if the spacecraft is not in DOWNLINK mode. Convert the downloaded `.raw` files to viewable JPGs:

```bash
cd ground_segment
# in the ground segment venv
uv run raw_to_jpg.py ../flight_segment/orion/downlinked_UHF/fprime-downlink
```

### Return to IDLE

Send the following command from the `Commanding` tab of GDS:

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

- [C++ API](https://Saransh-cpp.github.io/ORION/api/cpp/): Classes, namespaces, and source files
- [Python API](https://Saransh-cpp.github.io/ORION/api/python/receiver/): Receiver, training, and data modules

## Contributing

ORION uses `clang-format` for C++, `ruff` for Python, and `pre-commit` hooks for automated formatting. CI runs a native clang-tidy build and a Docker ARM64 cross-compile on every push.

- [Contributing guide](https://Saransh-cpp.github.io/ORION/contributing/): Dev setup, code style, CI pipeline, adding new components

## Hackathon Rubric Coverage

### Liquid AI LFM2-VL Track

| Criterion (Weight)                          | How ORION addresses it                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| ------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Use of Satellite Imagery (10%)**          | DPhi/SimSat satellite tiles are the core data source, applied to autonomous orbital triage, which is a real operational need for Earth observation missions.                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| **Innovation & Problem-Solution Fit (35%)** | Satellite imagery + LFM2-VL together enable something neither can do alone: the VLM classifies _and explains_ each frame on-board, giving operators actionable reasoning alongside the triage verdict, while eliminating 90-95% of downlink volume. The path to product is concrete as any EO satellite with a standard camera payload can deploy this by fine-tuning on mission-specific targets.                                                                                                                                                                                                                           |
| **Technical Implementation (35%)**          | LFM2-VL is fine-tuned via QLoRA on 480 domain-specific samples with coordinate dropout augmentation, quantized to Q4_K_M GGUF, and evaluated under a 4-condition ablation protocol. Fine-tuning yields measurable gains: Condition B +5 pp, C +8.3 pp, D coord-trust failure −3.3 pp. [Weights, training code, and evaluation scripts](https://Saransh-cpp.github.io/ORION/ground-segment/training/) are all in the repo. The model is integrated into real flight software (6 F-Prime C++ components, FPP state machine, llama.cpp) that can be deployed on a satellite after rigorous testing and mission-specific tweaks. |
| **Demo & Communication (20%)**              | Full [documentation site](https://Saransh-cpp.github.io/ORION/) with architecture diagrams, data flow, model card, mission budgets, and step-by-step guides.                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |

### General AI Track

| Criterion (Weight)                          | How ORION addresses it                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| ------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **Use of Satellite Imagery (20%)**          | DPhi/SimSat satellite tiles are the sole data source. The entire system is designed around the constraints of space-based acquisition: limited comm windows, large data volumes, and the 71% ocean problem that makes blind downlink wasteful.                                                                                                                                                                                                                                                                                                                                                                                                         |
| **Innovation & Problem-Solution Fit (25%)** | The problem is real as it originates from an [ESA Review Item Discrepancy](#motivation) on the CHESS mission. On-board VLM triage is a unique way to cut downlink volume in real time; ground-based sorting requires downlinking everything first, which defeats the purpose.                                                                                                                                                                                                                                                                                                                                                                          |
| **Technical Implementation (35%)**          | The app runs end-to-end: 6 custom F-Prime C++ components, an FPP state machine, llama.cpp VLM integration, autonomous mode management, pre-allocated buffer pool, Docker ARM64 cross-compilation, and a custom [ORIO frame protocol](https://Saransh-cpp.github.io/ORION/ground-segment/receiver/#orio-frame-protocol) for selective downlink. This is real flight software deployable on a satellite after rigorous testing and mission-specific tweaks. QLoRA fine-tuning, quantization, and a 4-condition evaluation protocol are documented with [publicly shared weights and code](https://Saransh-cpp.github.io/ORION/ground-segment/training/). |
| **Demo & Communication (20%)**              | Full [documentation site](https://Saransh-cpp.github.io/ORION/) with architecture diagrams, data flow, model card, mission budgets, and step-by-step guides.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |

## Why "ORION"?

A little easter egg: I grew up watching the Orion constellation every winter from my home in East Delhi. I would know winter is approaching when I could spot the belt before bedtime. There was not much light or air pollution back then, so I have very fond memories of looking up to find constellations in the night sky. I was always an Astronomy kid, but I never knew I would work on a real satellite mission (CHESS at EST) when I grow up. The name felt right for a project that looks down at Earth from the orbit.
