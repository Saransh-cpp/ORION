# ORION Flight Segment — Software Design Document

## Architecture

The pipeline is built around **asynchronous, event-driven inference**: the camera and telemetry components never block on the VLM. The VLM runs on its own low-priority thread and processes requests from a queue.

All image memory is statically allocated at startup. A 512×512 RGB frame is exactly 786,432 bytes; a pool of 20 such buffers consumes ~15.7 MB. The rest of the Pi 5's RAM holds the ~700 MB Q4 GGUF weights and KV cache. No custom component performs runtime dynamic allocation.

## Real Spacecraft Mapping

| ORION Component | Real Satellite Equivalent |
|---|---|
| `EventAction` | OBC mode manager / FDIR logic |
| `NavTelemetry` | GNSS receiver payload |
| `CameraManager` | Earth observation camera payload |
| `VlmInferenceEngine` | On-board AI co-processor |
| `TriageRouter` | On-board data handling unit |
| `GroundCommsDriver` | X-band radio transmitter |
| `BufferManager` (F-Prime) | On-board mass memory |
| `comDriver` (F-Prime) | UHF radio transceiver |

## Components

### CameraManager (Active)

Acquires 512×512 RGB imagery from the SimSat Mapbox API, fuses a GPS fix from NavTelemetry, and dispatches the frame downstream to the VLM.

Commands: `TRIGGER_CAPTURE`, `ENABLE_AUTO_CAPTURE(interval)`, `DISABLE_AUTO_CAPTURE`

Mode-gated: captures are rejected outside MEASURE mode. Auto-capture respects a minimum interval of 65 seconds to avoid draining the buffer pool faster than the VLM can clear it.

### NavTelemetry (Active)

Polls SimSat every 5 seconds for orbital position (lat/lon/alt) and computes whether the satellite is within the ground station comm window via Haversine distance. Exposes a synchronous `navStateGet` port so CameraManager and EventAction can read position with no queuing overhead.

The comm window uses hysteresis (enter at `ORION_GS_RANGE_KM`, exit at `range × 1.1`) to prevent oscillation at the boundary.

Configurable via env vars: `ORION_GS_LAT`, `ORION_GS_LON`, `ORION_GS_RANGE_KM` (default: EPFL Ecublens, 2000 km range).

### VlmInferenceEngine (Active)

Runs LFM2.5-VL-1.6B (Q4_K_M GGUF) via llama.cpp on the Pi 5's CPU. Inference pipeline:

1. Wrap raw RGB buffer via `mtmd_bitmap_init`
2. Tokenize prompt + image via `mtmd_tokenize`
3. Eval chunks into KV cache via `mtmd_helper_eval_chunks`
4. Greedy decode up to 200 tokens
5. Extract `"category"` value from JSON response → HIGH / MEDIUM / LOW verdict

Self-watchdog: if inference exceeds 120 seconds, the frame is dropped and the model stays loaded. **Not wired to the F-Prime health watchdog** — the 120s wall-clock timeout is self-contained.

Auto-loads the model on MEASURE entry, auto-unloads on IDLE or SAFE entry, stays loaded through DOWNLINK.

Commands: `LOAD_MODEL`, `UNLOAD_MODEL`

Env vars: `ORION_GGUF_PATH`, `ORION_MMPROJ_PATH`

### TriageRouter (Active)

Executes the priority doctrine on each VLM verdict:

- **HIGH** → forward to GroundCommsDriver for TCP downlink
- **MEDIUM** → write raw frame to microSD (`ORION_MEDIUM_STORAGE_DIR`, default `./media/sd/medium/`)
- **LOW** → return buffer to pool immediately

All paths return the buffer. MEDIUM writes use a monotonic file index (`orion_medium_XXXXX.raw`).

### GroundCommsDriver (Active)

Queues HIGH-priority frames to disk when outside a comm window and flushes them over TCP when DOWNLINK mode is active. Each frame is prefixed with an 8-byte header: magic `ORIO` (0x4F52494F) + payload length, both in network byte order. The ground `receiver.py` parses this protocol.

Queue is stored at `ORION_DOWNLINK_QUEUE_DIR` (default `./media/sd/downlink_queue/`). TCP target: `ORION_GDS_HOST:ORION_GDS_PORT` (default `127.0.0.1:50050`).

**Known limitation:** `connect()` has no explicit timeout. If the receiver is unreachable, each attempt blocks for the OS default (~75s Linux / ~30s macOS).

### EventAction (Active)

Central state machine and mission mode orchestrator. Drives IDLE / MEASURE / DOWNLINK / SAFE transitions and broadcasts mode changes to all pipeline components via `ModeChangePort`.

```
IDLE ──(eclipse)──> MEASURE ──(comm window)──> DOWNLINK ──(window closes)──> MEASURE or IDLE
 ^                     |                                                      (based on eclipse)
 └──────(fault)──────> SAFE ──(clearFault)──> IDLE
```

**Power doctrine:** MEASURE runs during eclipse (battery compute, solar panels idle); IDLE during sunlit passes (charging). This is counter-intuitive but correct — `SET_ECLIPSE true` signals the battery is the sole power source, so the satellite uses it for inference.

Polls NavTelemetry at 1 Hz for comm window edge detection. On DOWNLINK entry, GroundCommsDriver automatically flushes its disk queue. MEDIUM storage can be bulk-flushed via `FLUSH_MEDIUM_STORAGE` (paced at 1 file/sec to avoid saturating the F-Prime FileDownlink queue).

Commands: `SET_ECLIPSE`, `ENTER_SAFE_MODE`, `EXIT_SAFE_MODE`, `FLUSH_MEDIUM_STORAGE`, `GOTO_IDLE`, `GOTO_MEASURE`, `GOTO_DOWNLINK`

### BufferManager (Standard F-Prime)

Static pool: 20 × 786,432 bytes (~15.7 MB total). No custom code — standard `Svc.BufferManager` instance configured in `instances.fpp`.

---

## Port Interfaces

Defined in `Orion/Ports/OrionPorts.fpp`.

| Port | Direction | Payload | Purpose |
|---|---|---|---|
| `NavStatePort` | Sync (guarded) | `NavState` (lat, lon, alt, inCommWindow, gsDistanceKm) | CameraManager / EventAction → NavTelemetry position query |
| `InferenceRequestPort` | Async | `Fw::Buffer`, lat, lon | CameraManager → VlmInferenceEngine |
| `TriageDecisionPort` | Async | `TriagePriority`, reason (256 chars), `Fw::Buffer` | VlmInferenceEngine → TriageRouter |
| `FileDownlinkPort` | Async | `Fw::Buffer`, reason (256 chars) | TriageRouter → GroundCommsDriver |
| `ModeChangePort` | Async (broadcast) | `MissionMode` (U8 enum) | EventAction → all pipeline components |

---

## Data Flow

1. EventAction polls NavTelemetry at 1 Hz; transitions to MEASURE when eclipse flag is set and comm window is closed
2. Mode broadcast wakes CameraManager's auto-capture loop
3. CameraManager checks out a buffer, fetches a SimSat Mapbox image, pulls GPS via synchronous NavTelemetry call
4. Fires `InferenceRequestPort` → VlmInferenceEngine queue; camera thread returns immediately
5. VLM preprocesses the RGB buffer, runs llama.cpp forward pass with 120s watchdog
6. Fires `TriageDecisionPort` → TriageRouter
7. Router executes doctrine: HIGH queued for downlink, MEDIUM written to microSD, LOW discarded
8. On comm window open, EventAction transitions to DOWNLINK; GroundCommsDriver flushes HIGH queue over TCP

---

## Rate Groups

| Rate | Components scheduled |
|---|---|
| 1 Hz | NavTelemetry, CameraManager, GroundCommsDriver, EventAction, telemetry, file downlink |
| 0.5 Hz | Command sequencer |
| 0.25 Hz | Health, buffer managers, data products |

---

## Commands Reference

| Component | Command | Notes |
|---|---|---|
| EventAction | `SET_ECLIPSE(bool)` | Drives IDLE↔MEASURE transitions |
| EventAction | `ENTER_SAFE_MODE` / `EXIT_SAFE_MODE` | Manual fault handling |
| EventAction | `FLUSH_MEDIUM_STORAGE` | Valid in DOWNLINK only; paced 1 file/sec |
| EventAction | `GOTO_IDLE` / `GOTO_MEASURE` / `GOTO_DOWNLINK` | Manual overrides |
| CameraManager | `TRIGGER_CAPTURE` | Single capture; requires MEASURE mode |
| CameraManager | `ENABLE_AUTO_CAPTURE(interval)` / `DISABLE_AUTO_CAPTURE` | Min interval: 65s |
| VlmInferenceEngine | `LOAD_MODEL` / `UNLOAD_MODEL` | `LOAD_MODEL` rejected outside MEASURE/DOWNLINK |

---

## Telemetry Reference

| Component | Channel | Type |
|---|---|---|
| CameraManager | `ImagesCaptured`, `CapturesFailed` | U32 |
| NavTelemetry | `CurrentLat`, `CurrentLon`, `CurrentAlt`, `InCommWindow` | F64/bool |
| VlmInferenceEngine | `InferenceTime_Ms`, `TotalInferences`, `InferenceFailures` | U32 |
| TriageRouter | `HighTargetsRouted`, `MediumTargetsSaved`, `LowTargetsDiscarded` | U32 |
| GroundCommsDriver | `FramesDownlinked`, `BytesDownlinked`, `FramesQueued`, `TransmitFailures` | U32 |
| EventAction | `CurrentMode` | U8 |

---

## Hardware Constraints (Pi 5, Cortex-A76, no NPU/GPU)

- **Inference latency:** 50–70 seconds per image (Q4 CPU-only, measured on Pi 5: ~10-15s vision encoding + ~40-55s token generation)
- **Self-watchdog ceiling:** 120 seconds
- **Eclipse window:** ~35 minutes per orbit (MEASURE phase duration)
- **Practical throughput:** ~7–9 classifications per eclipse pass
- **Buffer pool:** 20 slots × 786 KB; auto-capture interval set to 65s minimum to prevent pool exhaustion against ~65s median inference time
