# ORION Flight Segment

F-Prime deployment for the ORION on-board triage system. Runs on Raspberry Pi 5 (Cortex-A76, CPU-only).

## Components

| ORION Component            | Real Satellite Equivalent        |
| -------------------------- | -------------------------------- |
| `EventAction` (C++)        | OBC mode manager / FDIR logic    |
| `NavTelemetry` (C++)       | GNSS receiver payload            |
| `CameraManager` (C++)      | Earth observation camera payload |
| `VlmInferenceEngine` (C++) | On-board AI co-processor         |
| `TriageRouter` (C++)       | On-board data handling unit      |
| `GroundCommsDriver` (C++)  | X-band radio transmitter         |
| `BufferManager` (F-Prime)  | On-board mass memory             |
| `comDriver` (F-Prime)      | UHF radio transceiver            |

## Documentation

- [System architecture](https://Saransh-cpp.github.io/ORION/architecture/overview/): component inventory, rate groups, data flow
- [State machine](https://Saransh-cpp.github.io/ORION/architecture/state-machine/): IDLE / MEASURE / DOWNLINK / SAFE transitions
- [Mission budgets](https://Saransh-cpp.github.io/ORION/architecture/budgets/): timing, data, link, storage, power, memory
- [Component SDDs](https://Saransh-cpp.github.io/ORION/components/): per-component design documents
- [Installation](https://Saransh-cpp.github.io/ORION/guides/installation/): build from source
- [Deployment](https://Saransh-cpp.github.io/ORION/guides/deployment/): Docker cross-compile for Pi 5
