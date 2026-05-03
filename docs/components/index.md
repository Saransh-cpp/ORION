# Components Overview

ORION's flight software is built on the [F-Prime](https://fprime.jpl.nasa.gov/) component framework developed by NASA JPL. Each component is an isolated unit with typed input/output ports, commands, events, telemetry channels, and (optionally) an internal state machine. Components communicate exclusively through port connections defined in the system topology; hence, there are no shared globals or direct function calls between components.

The six ORION-specific components form a linear pipeline from sensor input to ground downlink:

| Component                                     | Purpose                                                    | Key Ports                                                       |
| --------------------------------------------- | ---------------------------------------------------------- | --------------------------------------------------------------- |
| [EventAction](event-action.md)                | Mission mode controller (IDLE / MEASURE / DOWNLINK / SAFE) | `schedIn`, `navStateIn`, `modeChangeOut[4]`, `sendFileOut`      |
| [NavTelemetry](nav-telemetry.md)              | Position tracking and comm window computation              | `schedIn`, `navStateGet`                                        |
| [CameraManager](camera-manager.md)            | Earth observation image acquisition                        | `schedIn`, `modeChangeIn`, `inferenceRequestOut`, `navStateOut` |
| [VlmInferenceEngine](vlm-inference-engine.md) | On-board VLM triage classification (HIGH / MEDIUM / LOW)   | `inferenceRequestIn`, `modeChangeIn`, `triageDecisionOut`       |
| [TriageRouter](triage-router.md)              | Routes frames by verdict: downlink, store, or discard      | `triageDecisionIn`, `modeChangeIn`, `fileDownlinkOut`           |
| [GroundCommsDriver](ground-comms-driver.md)   | Simulated X-band TCP downlink with disk queue              | `fileDownlinkIn`, `schedIn`, `modeChangeIn`                     |

In addition, the `Orion/Utils/` directory contains `SimSatClient`, a plain C++ libcurl wrapper used by NavTelemetry and CameraManager to fetch position and imagery from SimSat. It is not an F-Prime component. See the [architecture overview](../architecture/overview.md#shared-utilities-orionutils) for details.

## Communication Model

All inter-component communication flows through F-Prime's port system. Ports are either **synchronous** (caller blocks until the callee returns) or **asynchronous** (message is enqueued on the callee's thread). Key patterns in ORION:

- **Mode broadcasting:** EventAction sends mode changes to CameraManager, VlmInferenceEngine, TriageRouter, and GroundCommsDriver via four `modeChangeOut` output ports. Each receiving component uses the mode to gate its behavior.
- **Position queries:** NavTelemetry exposes a guarded (mutex-protected) synchronous input port. EventAction and CameraManager call it to read cached GPS state without blocking NavTelemetry's own polling.
- **Pipeline forwarding:** The image pipeline flows CameraManager -> VlmInferenceEngine -> TriageRouter -> GroundCommsDriver, with buffer ownership transferring at each stage. Buffers are returned to the shared BufferManager pool after use.

For detailed data, link, storage, power, and timing budgets per orbit, see [Mission Budgets](../architecture/budgets.md).
