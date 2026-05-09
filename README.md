# ORION

<img src="https://raw.githubusercontent.com/Saransh-cpp/ORION/main/docs/assets/orion_logo.png" alt="ORION" width="300">

[![Flight Segment CI](https://github.com/Saransh-cpp/ORION/actions/workflows/fs_ci.yml/badge.svg)](https://github.com/Saransh-cpp/ORION/actions/workflows/fs_ci.yml)
[![Documentation](https://github.com/Saransh-cpp/ORION/actions/workflows/docs.yml/badge.svg)](https://Saransh-cpp.github.io/ORION/)
[![Hugging Face](https://img.shields.io/badge/%F0%9F%A4%97%20Hugging%20Face-model-28A745?labelColor=24292E)](https://huggingface.co/Saransh-cpp/orion-qlora-lfm2.5-vl-1.6b)
[![Hugging Face](https://img.shields.io/badge/%F0%9F%A4%97%20Hugging%20Face-dataset-28A745?labelColor=24292E)](https://huggingface.co/datasets/saransh-cpp/orion-dataset)
[![YouTube](https://img.shields.io/badge/%E2%96%B6%20YouTube-presentation-FF0000?labelColor=24292E)](https://youtu.be/yWB59Yz5928)
[![YouTube](https://img.shields.io/badge/%E2%96%B6%20YouTube-demo-FF0000?labelColor=24292E)](https://youtu.be/haWxV7mRE4Y)

Orbital Real-time Inference and Observation Network

An autonomous LEO satellite triage system built using:

- a fine-tuned [Liquid AI LFM2.5-VL-1.6B](https://huggingface.co/collections/LiquidAI/lfm2-vl) vision-language model (for inference),
- [NASA's F-Prime](https://github.com/nasa/fprime) (for flight software),
- [SimSat](https://github.com/DPhi-Space/SimSat) (to simulate real payload sensors - GNSS and a camera),
- a Raspberry Pi 5 (to act as the satellite's OBC).

ORION solves the orbital bandwidth bottleneck: roughly 71% of Earth's surface is featureless ocean, yet a traditional satellite downlinks every captured frame. By running a fine-tuned (on a custom dataset collected using SimSat) Q4-quantized VLM on-board, ORION classifies each image as HIGH, MEDIUM, or LOW priority and only transmits the most strategically valuable observations in real time. The model runs directly on raw 512×512 RGB pixels and is connected to a real flight software (not a Python wrapper), making it deployable on satellite's On-Board Computer (after rigorous testing and mission-specific tweaks) for any standard camera payload.

End-to-end Pi 5 simulation results ([details below](#results)):

- **Run 1** (10h 23m): 501 frames, **95.0% bandwidth saved**, mean inference 71.7s, zero failures
- **Run 2** (9h 39m): 396 frames, **95.7% bandwidth saved**, mean inference 69.4s, zero failures
- **Run 3** (13h 17m): 546 frames, **97.4% bandwidth saved**, mean inference 66.3s, zero failures

## Motivation

This project was born from a real problem flagged on a real mission. Being the Flight Software subsystems lead on [EPFL Spacecraft Team's](https://www.epflspacecraftteam.ch) [CHESS](https://www.epflspacecraftteam.ch/project#chess) mission (part of [ESA's Fly Your Satellite! Design Booster programme](https://www.esa.int/Education/Educational_Satellites/About_Design_Booster)), I received a Review Item Discrepancy during our Final Design Review from an ESA expert:

> _"From experience I recommend thinking about pre-loading software that allows you to check that a picture is worth downloading before you do it."_

<img src="https://raw.githubusercontent.com/Saransh-cpp/ORION/main/docs/assets/rid.png" alt="ORION" width="300">

for our [NovoViz](https://novoviz.com) Single-photon avalanche diode (camera) payload.

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

The system is split into a [flight segment](https://saransh-cpp.github.io/ORION/architecture/flight_segment/components/) (6 F-Prime components on Pi 5) and a [ground segment](https://saransh-cpp.github.io/ORION/architecture/ground_segment/) (receiver, training pipeline, dataset). The flight segment runs an FPP state machine governing four mission modes (IDLE, MEASURE, DOWNLINK, SAFE), with autonomous comm window detection via Haversine distance to the ground station at EPFL. All image buffers are pre-allocated at startup (20-slot static pool), and the VLM model is loaded/unloaded on mode transitions; hence, there is no runtime dynamic allocation. The VLM runs via [llama.cpp](https://github.com/ggml-org/llama.cpp)'s C API (statically linked), and image decoding/resizing uses vendored [stb_image](https://github.com/nothings/stb) headers. The flight and ground segments communicate over two independent links: the standard F-Prime command/telemetry channel (TCP :50000) and a custom [ORIO frame protocol](https://Saransh-cpp.github.io/ORION/architecture/ground_segment/receiver/#orio-frame-protocol) for real-time HIGH-priority image downlink (TCP :50050).

| ORION Component            | Real Satellite Equivalent                                                                    |
| -------------------------- | -------------------------------------------------------------------------------------------- |
| `EventAction` (C++)        | OBC (On-Board Computer) mode manager / FDIR (Fault Detection, Isolation, and Recovery) logic |
| `NavTelemetry` (C++)       | GNSS (Glabal Navigation Satellite System) receiver payload manager                           |
| `CameraManager` (C++)      | Earth observation camera payload manager                                                     |
| `VlmInferenceEngine` (C++) | On-board AI co-processor                                                                     |
| `TriageRouter` (C++)       | On-board data handling unit                                                                  |
| `GroundCommsDriver` (C++)  | X-band radio transmitter manager                                                             |
| `BufferManager` (F-Prime)  | On-board mass memory                                                                         |
| `comDriver` (F-Prime)      | UHF (Ultra High Frequency) radio transceiver manager                                         |
| Raspberry Pi 5             | On-board computer                                                                            |
| SimSat                     | GNSS receiver hardware                                                                       |
| SimSat Mapbox API          | Earth observation camera payload hardware                                                    |
| `receiver.py`              | Ground station X-band receiver                                                               |
| F-Prime GDS                | Mission control software                                                                     |

- [System overview](https://Saransh-cpp.github.io/ORION/architecture/overview/): Component inventory, rate groups, ground segment
- [State machine](https://saransh-cpp.github.io/ORION/architecture/flight_segment/state-machine/): IDLE / MEASURE / DOWNLINK / SAFE transitions
- [Data flow](https://saransh-cpp.github.io/ORION/architecture/flight_segment/data-flow//): Capture to downlink pipeline, ORIO frame protocol

## Results

> Full quantitative breakdown: [Mission Budgets](https://saransh-cpp.github.io/ORION/architecture/flight_segment/budgets/) · [Ground Segment Budgets](https://Saransh-cpp.github.io/ORION/architecture/ground_segment/budgets/) · [Dataset & target definitions](https://Saransh-cpp.github.io/ORION/architecture/ground_segment/data/)

Unless noted otherwise, all compute numbers below (inference time, duty cycle, frames per eclipse) are derived from the pooled average across 3 end-to-end Pi 5 simulation runs (1,443 frames, ~33 hours total). Per-run breakdowns are in the [measured savings](#measured-savings) section and the [mission budgets](https://Saransh-cpp.github.io/ORION/architecture/flight_segment/budgets/#cross-run-comparison).

### Target definitions

The [custom dataset](https://Saransh-cpp.github.io/ORION/architecture/ground_segment/data/) contains 360 satellite images (120 per class) fetched from SimSat's Mapbox API, split into 240 train / 60 val / 60 test. Each class is defined by visual morphology:

| Class  | What it captures                                                               | Examples                                               |
| ------ | ------------------------------------------------------------------------------ | ------------------------------------------------------ |
| HIGH   | Extreme-scale strategic anomalies: dense geometric infrastructure, chokepoints | Mega-ports, mega-airports, nuclear plants, launch pads |
| MEDIUM | Standard human civilization: urban grids, suburban sprawl, agriculture         | City centers, farms, regional airports, rail yards     |
| LOW    | Featureless natural terrain: oceans, deserts, ice sheets, dense canopy         | Open ocean, Sahara, Antarctic ice, Amazon canopy       |

Each class includes deliberately hard sub-types (e.g., coastlines that mimic artificial structures for LOW, or isolated towns for MEDIUM) to stress-test the classifier. See the full [morphology breakdown](https://Saransh-cpp.github.io/ORION/architecture/ground_segment/data/#target-definitions) for details.

### Triage prompt

Each captured frame is paired with GPS coordinates and fed to the VLM using this [ChatML](https://github.com/openai/openai-python/blob/main/chatml.md) prompt:

```
<|im_start|>user
<image>
You are an autonomous orbital triage assistant. Analyze this
high-resolution RGB satellite image captured at Longitude: {lon},
Latitude: {lat}.
Strictly use one of these categories based on visual morphology:
- HIGH: Extreme-scale strategic anomalies, dense geometric cargo/vessel
  infrastructure, massive cooling towers, sprawling runways, or distinct
  geological/artificial chokepoints.
- MEDIUM: Standard human civilization. Ordinary urban grids, low-density
  suburban sprawl, regular checkerboard agriculture, or localized
  infrastructure.
- LOW: Complete absence of human infrastructure. Featureless deep oceans,
  unbroken canopy, barren deserts, or purely natural geological formations.
You MUST output your response as a valid JSON object. To ensure accurate
visual reasoning, you must output the "reason" key FIRST, followed by
the "category" key.<|im_end|>
<|im_start|>assistant
```

The model outputs a JSON object with a `reason` (free-text visual description) and a `category` (`HIGH`, `MEDIUM`, or `LOW`). Reason-first ordering forces the model to commit to visual evidence before selecting a label. The same prompt template is used for [training](https://Saransh-cpp.github.io/ORION/guides/data-gen/), [evaluation](https://Saransh-cpp.github.io/ORION/guides/studies/), and [on-board inference](https://Saransh-cpp.github.io/ORION/architecture/flight_segment/data-flow/#stage-2-vlm-inference).

### On-board inference (Raspberry Pi 5, Cortex-A76, no NPU/GPU)

| Metric                                    | Value                                                               |
| ----------------------------------------- | ------------------------------------------------------------------- |
| Vision encoding (mtmd)                    | ~10-15 s                                                            |
| Token generation (200 tokens max, greedy) | ~40-55 s                                                            |
| **Total per frame**                       | **51-82 s** (mean ~69 s across 1,443 frames from 3 end-to-end runs) |
| Self-watchdog ceiling                     | 120 s (frame dropped, model stays loaded)                           |
| Frames captured per 35-min eclipse        | ~24 (85 s capture interval)                                         |
| Frames inferred per eclipse               | ~24 (all captured; inference < capture interval, queue depth 0-1)   |
| VLM duty cycle per orbit                  | ~27%                                                                |

**Memory in MEASURE mode (Pi 5, 8 GB RAM):**

| Component                              | Size (estimate) |
| -------------------------------------- | --------------- |
| Q4_K_M GGUF weights                    | ~730 MB         |
| F16 vision projector (mmproj)          | ~814 MB         |
| KV cache (4096 ctx, per inference)     | ~64 MB          |
| Static frame buffer pool (20 × 786 KB) | ~16 MB          |
| F-Prime framework + Linux              | ~220 MB         |
| **Total (estimate)**                   | **~1,844 MB**   |
| **Total (measured RSS on Pi 5)**       | **~1,753 MB**   |

No runtime dynamic allocation. All frame memory is pre-allocated at startup; model loads once on MEASURE entry.

### Model accuracy (60-sample test set, 3-class: HIGH / MEDIUM / LOW)

> Full per-condition logs (recall, precision, overall accuracy) and a detailed discussion on the results are embedded in the [model card](https://Saransh-cpp.github.io/ORION/architecture/ground_segment/model-card/). For a detailed explanation of each condition and how to interpret the numbers, see the [validation and ablation studies guide](https://Saransh-cpp.github.io/ORION/guides/studies/).

The table below compares the base LFM2.5-VL-1.6B model, the ORION fine-tuned model (FP16), and the deployed Q4_K_M GGUF under four controlled conditions. Each condition isolates a different input channel (vision, GPS, or both) to measure what the model actually relies on for classification.

| Condition                              | Base model | Fine-tuned (FP16) | Q4_K_M GGUF | Δ (fine-tune) | Δ (quantization) |
| -------------------------------------- | ---------- | ----------------- | ----------- | ------------- | ---------------- |
| A: Vision + GPS coords                 | 58.3%      | 58.3%             | 55.0%       | 0 pp          | −3.3 pp          |
| B: Vision only (no coords)             | 60.0%      | 65.0%             | 63.3%       | +5.0 pp       | −1.7 pp          |
| C: Blind LLM (Gaussian noise + coords) | 35.0%      | 43.3%             | 28.3%       | +8.3 pp       | −15.0 pp         |

**Condition D: Sensor conflict (real image, spoofed GPS coords):** coordinate-trust failure drops from 20.0% (base) to 16.7% (fine-tuned FP16) to 15.0% (Q4_K_M GGUF). Quantization does not degrade GPS robustness; the deployed model is actually slightly more resistant to spoofed telemetry than the FP16 version.

Accuracy loss from Q4_K_M quantization on operational conditions (A: −3.3 pp, B: −1.7 pp) is modest, confirming that the deployed GGUF retains most of the fine-tuned model's capability. The large Condition C drop (−15.0 pp) is expected and benign: it tests coordinate memorization using noise images, a scenario that never occurs in deployment.

Condition A (nominal, vision + GPS) shows no gain on this dataset. The HIGH category spans five visually heterogeneous sub-types, mega-ports, airports, energy infrastructure, mines, and military facilities, across only 240 training images. That is not enough for the visual encoder to learn a reliable shared boundary. Fine-tuning on a narrower HIGH sub-type with a larger image corpus (1k-5k images per class) would close this gap significantly.

ORION demonstrates that on-board VLM inference on a Pi 5 is technically viable and that fine-tuning measurably improves robustness.

### Bandwidth savings

Each frame is 786 KB (512×512 RGB). The triage doctrine, HIGH downlinked immediately, MEDIUM stored for bulk transfer, LOW discarded, eliminates transmission of all LOW frames.

### Expected savings

Expected triage distribution on a random LEO track (based on target morphology distribution across the training dataset):

| Verdict             | Expected ratio | Data per orbit           | Action                         |
| ------------------- | -------------- | ------------------------ | ------------------------------ |
| LOW                 | ~60-70%        | 0 bytes (discarded)      | Buffer recycled immediately    |
| MEDIUM              | ~20-30%        | ~3.8-5.4 MB (stored)     | Written to microSD             |
| HIGH                | ~5-10%         | ~0.8-1.5 MB (downlinked) | Transmitted during comm window |
| **Bandwidth saved** | **~90-95%**    |                          | vs. downlinking every frame    |

### Measured savings

Measured triage distribution from continuous Pi 5 runs (no eclipse cycling, for stress-testing; `SET_ECLIPSE` issued once at start). Per-frame metrics (inference time, triage accuracy) are unaffected by eclipse cycling; per-orbit projections in [compute budgets](https://Saransh-cpp.github.io/ORION/architecture/flight_segment/budgets/) use the 35-min eclipse assumption.

The "Pooled average" column treats all frames across runs as a single dataset (weighted by frame count, not arithmetic mean of percentages). Raw event logs: [Run 1](https://github.com/Saransh-cpp/ORION/tree/main/flight_segment/orion/logs/run_1_2026_05_06-23_28_57/event.log) (10h 23m, 501 frames), [Run 2](https://github.com/Saransh-cpp/ORION/tree/main/flight_segment/orion/logs/run_2_2026_05_07-12_10_33/event.log) (9h 39m, 396 frames), [Run 3](https://github.com/Saransh-cpp/ORION/tree/main/flight_segment/orion/logs/run_3_2026_05_07-22_47_57/event.log) (13h 17m, 546 frames).

| Verdict             | Run 1       | Run 2       | Run 3       | Pooled average |
| ------------------- | ----------- | ----------- | ----------- | -------------- |
| LOW                 | 476 (95.0%) | 379 (95.7%) | 532 (97.4%) | 1,387 (96.1%)  |
| MEDIUM              | 23 (4.6%)   | 15 (3.8%)   | 10 (1.8%)   | 48 (3.3%)      |
| HIGH                | 2 (0.4%)    | 2 (0.5%)    | 4 (0.7%)    | 8 (0.6%)       |
| **Bandwidth saved** | **95.0%**   | **95.7%**   | **97.4%**   | **96.1%**      |

| Metric         | Run 1         | Run 2         | Run 3         | Pooled average |
| -------------- | ------------- | ------------- | ------------- | -------------- |
| Mean inference | 71.7 s        | 69.4 s        | 66.3 s        | 69.0 s         |
| Min / Max      | 52.9 / 81.6 s | 53.2 / 77.5 s | 51.4 / 71.7 s | 51.4 / 81.6 s  |
| Failures       | 0             | 0             | 0             | 0              |
| Comm windows   | 1             | 2             | 2             | 5              |

Across all three runs: HIGH + MEDIUM combined is ~3.9% of all frames; hence over 96% of captured data was discarded on-board. All HIGH frames were queued to disk outside comm windows and flushed automatically on comm window open. MEDIUM files were bulk-downloaded via `FLUSH_MEDIUM_STORAGE` during comm windows. Total comm window time used (~45 min across 5 windows) was more than sufficient for all queued data.

Full per-run breakdowns (inference timing, downlink, run parameters) in the [mission budgets measured results](https://Saransh-cpp.github.io/ORION/architecture/flight_segment/budgets/#measured-results-run-1-10h-23m-pi-5-run-2026-05-07). Raw event logs are in [`flight_segment/orion/logs/`](https://github.com/Saransh-cpp/ORION/tree/main/flight_segment/orion/logs) (channel telemetry logs excluded due to size but available on request). Downlinked images received by the ground segment: [HIGH Run 1 (X-band)](https://github.com/Saransh-cpp/ORION/tree/main/ground_segment/data/downlinked_XBand_run_1), [HIGH Run 2 (X-band)](https://github.com/Saransh-cpp/ORION/tree/main/ground_segment/data/downlinked_XBand_run_2), [HIGH Run 3 (X-band)](https://github.com/Saransh-cpp/ORION/tree/main/ground_segment/data/downlinked_XBand_run_3), [MEDIUM Run 1 (UHF)](https://github.com/Saransh-cpp/ORION/tree/main/ground_segment/data/downlinked_UHF_run_1/fprime-downlink), [MEDIUM Run 2 (UHF)](https://github.com/Saransh-cpp/ORION/tree/main/ground_segment/data/downlinked_UHF_run_2/fprime-downlink), [MEDIUM Run 3 (UHF)](https://github.com/Saransh-cpp/ORION/tree/main/ground_segment/data/downlinked_UHF_run_3/fprime-downlink).

### Faults observed

**Transport:** One MEDIUM file (in Run 2) out of 56 total (across all runs) arrived truncated (785,955 bytes vs. expected 786,432) due to a partial F-Prime FileDownlink transfer, which is a transport-layer issue, not a triage pipeline fault. File transfer success rate: 98.2%.

**VLM false positives in HIGH classification:** Of 8 HIGH frames across all runs, manual inspection shows:

- 3 frames (Run 3) are **blank white Mapbox tiles** from polar latitudes (|lat| > 75°) where no satellite imagery exists. The model hallucinated strategic significance (e.g., "massive cargo or industrial facility") onto featureless white images.
- 2–3 frames are **natural features misclassified** as artificial, such as coastlines interpreted as "massive artificial formations," clouds over terrain interpreted as "volcanic eruption," and a lake interpreted as an "artificial anomaly."
- 2 frames are plausible HIGHs (urban area near Virginia Beach, small town with airstrip).

These false positives do not affect bandwidth savings (HIGH is the rarest category at 0.6%, so false positives waste minimal downlink capacity), but they reveal two model limitations: (1) blank/missing Mapbox tiles at polar latitudes are visually distinct from the ocean and ice sheet tiles in the training set, and (2) natural edge cases (coastlines, cloud cover, geological formations) that resemble trained HIGH morphologies. See the [model card](https://saransh-cpp.github.io/ORION/architecture/ground_segment/model-card/) for mitigation strategies.

**Capture rate:** Inference at 51-82 s/frame (mean ~69s) sets a hard floor on the capture interval (85s), limiting throughput to ~24 frames per 35-min eclipse. Burst imaging or sub-minute revisit rates require faster hardware (GPU/NPU) or a smaller model.

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
- `ModelLoaded`: VLM loads into RAM (~21s on Pi)
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

This queues one file per second to F-Prime's FileDownlink service (TCP :50000). Each file is **renamed** to `.raw.sent` before transmission to avoid re-queuing. `.sent` files from a previous flush are cleaned up when the next `FLUSH_MEDIUM_STORAGE` is issued.

If the comm window closes mid-flush, files already renamed to `.sent` but not yet delivered are lost on a real spacecraft (in this simulation the GDS link stays up over WiFi, so they arrive regardless). The GDS status indicator may flicker red/green during the flush but this is cosmetic; the file transfers saturate the shared TCP link, briefly starving telemetry packets.

Files arrive in `ground_segment/data/downlinked_UHF/` (the directory set via `--file-storage-directory` when launching GDS). The command is rejected if the spacecraft is not in DOWNLINK mode. Convert the downloaded `.raw` files to viewable JPGs:

```bash
cd ground_segment
# in the ground segment venv
uv run raw_to_jpg.py ./data/downlinked_UHF/fprime-downlink
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

- [C++ API](https://saransh-cpp.github.io/ORION/flight-segment/annotated/): Classes, namespaces, and source files
- [Python API](https://Saransh-cpp.github.io/ORION/api/python/receiver/): Receiver, training, and data modules

## Contributing

ORION uses `clang-format` for C++, `ruff` for Python, and `pre-commit` hooks for automated formatting. CI runs a native clang-tidy build and a Docker ARM64 cross-compile on every push.

- [Contributing guide](https://Saransh-cpp.github.io/ORION/contributing/): Dev setup, code style, CI pipeline, adding new components

## Hackathon Rubric Coverage

### Liquid AI LFM2-VL Track

| Criterion (Weight)                          | How ORION addresses it                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| ------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Use of Satellite Imagery (10%)**          | DPhi/SimSat satellite tiles are the core data source, applied to autonomous orbital triage, which is a real operational need for Earth observation missions.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| **Innovation & Problem-Solution Fit (35%)** | Satellite imagery + LFM2-VL together enable something neither can do alone: the VLM classifies _and explains_ each frame on-board, giving operators actionable reasoning alongside the triage verdict, while eliminating ~96% of downlink volume (measured across 1,443 frames). The path to product is concrete as any EO satellite with a standard camera payload can deploy this by fine-tuning on mission-specific targets.                                                                                                                                                                                                                                                                                                                                                   |
| **Technical Implementation (35%)**          | LFM2-VL is fine-tuned via QLoRA on 480 domain-specific samples with coordinate dropout augmentation, quantized to Q4_K_M GGUF, and evaluated under a 4-condition ablation protocol. Fine-tuning yields measurable gains: Condition B +5 pp, C +8.3 pp, D coord-trust failure −3.3 pp. [Weights, training code, and evaluation scripts](https://Saransh-cpp.github.io/ORION/architecture/ground_segment/training/) are all in the repo. The model is integrated into real flight software (6 F-Prime C++ components, FPP state machine, llama.cpp) that can be deployed on a satellite after rigorous testing and mission-specific tweaks. Validated across 3 end-to-end Pi 5 simulation runs (1,443 frames, 33 hours, zero failures, [96% bandwidth savings](#measured-savings)). |
| **Demo & Communication (20%)**              | Full [documentation site](https://Saransh-cpp.github.io/ORION/) with architecture diagrams, data flow, model card, mission budgets, and step-by-step guides.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |

### General AI Track

| Criterion (Weight)                          | How ORION addresses it                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| ------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Use of Satellite Imagery (20%)**          | DPhi/SimSat satellite tiles are the sole data source. The entire system is designed around the constraints of space-based acquisition: limited comm windows, large data volumes, and the 71% ocean problem that makes blind downlink wasteful.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |
| **Innovation & Problem-Solution Fit (25%)** | The problem is real as it originates from an [ESA Review Item Discrepancy](#motivation) on the CHESS mission. On-board VLM triage is a unique way to cut downlink volume in real time; ground-based sorting requires downlinking everything first, which defeats the purpose. Measured result: [96% bandwidth savings](#measured-savings) across 1,443 frames over 33 hours of continuous operation.                                                                                                                                                                                                                                                                                                                                                              |
| **Technical Implementation (35%)**          | The app runs end-to-end: 6 custom F-Prime C++ components, an FPP state machine, llama.cpp VLM integration, autonomous mode management, pre-allocated buffer pool, Docker ARM64 cross-compilation, and a custom [ORIO frame protocol](https://Saransh-cpp.github.io/ORION/architecture/ground_segment/receiver/#orio-frame-protocol) for selective downlink. This is real flight software deployable on a satellite after rigorous testing and mission-specific tweaks. QLoRA fine-tuning, quantization, and a 4-condition evaluation protocol are documented with [publicly shared weights and code](https://Saransh-cpp.github.io/ORION/architecture/ground_segment/training/). Validated across 3 end-to-end Pi 5 runs (1,443 frames, 33 hours, zero failures). |
| **Demo & Communication (20%)**              | Full [documentation site](https://Saransh-cpp.github.io/ORION/) with architecture diagrams, data flow, model card, mission budgets, and step-by-step guides.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |

## Will CHESS use this solution?

ORION is novel and it works (both technically and mathematically, as shown by my experiments above). We have the CPU compute to run this exact configuration on CHESS, but we will not do so, as we do not want to make such a big compute change to the mission in our testing phase.

ORION will not fly on our upcoming launch, but maybe someday in the future (for our second and third launches, when I would have moved on from EPFL), new members will go through the design and debate if they should use it.

## Why "ORION"?

A little easter egg: I grew up watching the Orion constellation every winter from my home in East Delhi. I would know winter is approaching when I could spot the belt before bedtime. There was not much light or air pollution back then, so I have very fond memories of looking up to find constellations in the night sky. I was always an Astronomy kid, but I never knew I would work on a real satellite mission (CHESS at EST) when I grow up. The name felt right for a project that looks down at Earth from the orbit.
