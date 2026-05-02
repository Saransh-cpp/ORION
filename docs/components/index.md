# Components Overview

ORION's flight software is built on the [F-Prime](https://fprime.jpl.nasa.gov/) component framework developed by NASA JPL. Each component is an isolated unit with typed input/output ports, commands, events, telemetry channels, and (optionally) an internal state machine. Components communicate exclusively through port connections defined in the system topology — there are no shared globals or direct function calls between components.

The six ORION-specific components form a linear pipeline from sensor input to ground downlink:

| Component                                     | Purpose                                                    | Key Ports                                                       |
| --------------------------------------------- | ---------------------------------------------------------- | --------------------------------------------------------------- |
| [EventAction](event-action.md)                | Mission mode controller (IDLE / MEASURE / DOWNLINK / SAFE) | `schedIn`, `navStateIn`, `modeChangeOut[4]`, `sendFileOut`      |
| [NavTelemetry](nav-telemetry.md)              | Position tracking and comm window computation              | `schedIn`, `navStateGet`                                        |
| [CameraManager](camera-manager.md)            | Earth observation image acquisition                        | `schedIn`, `modeChangeIn`, `inferenceRequestOut`, `navStateOut` |
| [VlmInferenceEngine](vlm-inference-engine.md) | On-board VLM triage classification (HIGH / MEDIUM / LOW)   | `inferenceRequestIn`, `modeChangeIn`, `triageDecisionOut`       |
| [TriageRouter](triage-router.md)              | Routes frames by verdict: downlink, store, or discard      | `triageDecisionIn`, `modeChangeIn`, `fileDownlinkOut`           |
| [GroundCommsDriver](ground-comms-driver.md)   | Simulated X-band TCP downlink with disk queue              | `fileDownlinkIn`, `schedIn`, `modeChangeIn`                     |

## Communication Model

All inter-component communication flows through F-Prime's port system. Ports are either **synchronous** (caller blocks until the callee returns) or **asynchronous** (message is enqueued on the callee's thread). Key patterns in ORION:

- **Mode broadcasting:** EventAction sends mode changes to CameraManager, VlmInferenceEngine, TriageRouter, and GroundCommsDriver via four `modeChangeOut` output ports. Each receiving component uses the mode to gate its behavior.
- **Position queries:** NavTelemetry exposes a guarded (mutex-protected) synchronous input port. EventAction and CameraManager call it to read cached GPS state without blocking NavTelemetry's own polling.
- **Pipeline forwarding:** The image pipeline flows CameraManager -> VlmInferenceEngine -> TriageRouter -> GroundCommsDriver, with buffer ownership transferring at each stage. Buffers are returned to the shared BufferManager pool after use.

For detailed data, link, storage, power, and timing budgets per orbit, see [Mission Budgets](../architecture/budgets.md).

## Hardware Constraints

| Resource                 | Specification                                          |
| ------------------------ | ------------------------------------------------------ |
| **Processor**            | Raspberry Pi 5, Cortex-A76 quad-core (no NPU/GPU)      |
| **RAM**                  | 4 GB minimum                                           |
| **Model**                | LFM2.5-VL-1.6B, Q4_K_M quantization (~730 MB resident) |
| **Image buffer pool**    | 20 slots x 786,432 bytes = ~15.7 MB                    |
| **Inference latency**    | 50-60 seconds per frame (CPU-only, greedy sampling)    |
| **Inference timeout**    | 120 seconds (KV cache reset on abort)                  |
| **Context window**       | 4096 tokens (N_CTX), 512 batch size                    |
| **Vision encoder**       | mtmd (multimodal), 1024 max image tokens               |
| **Threads**              | 4 (matches Pi 5 core count)                            |
| **LEO pass duration**    | 8-10 minutes (480-600 seconds)                         |
| **Max throughput**       | ~7-9 classifications per LEO pass                      |
| **Min capture interval** | 65 seconds (must exceed worst-case inference time)     |
