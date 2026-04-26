# Flight Segment Environment Variables

All flight segment environment variables use the `ORION_` prefix. Components read these at startup; changes require a restart.

## SimSat Connection

| Variable           | Default                 | Used By                     | Description              |
| ------------------ | ----------------------- | --------------------------- | ------------------------ |
| `ORION_SIMSAT_URL` | `http://localhost:9005` | NavTelemetry, CameraManager | SimSat REST API base URL |

## Model Paths

| Variable            | Default                 | Used By            | Description                      |
| ------------------- | ----------------------- | ------------------ | -------------------------------- |
| `ORION_GGUF_PATH`   | `./orion-q4_k_m.gguf`   | VlmInferenceEngine | Q4_K_M quantized text model      |
| `ORION_MMPROJ_PATH` | `orion-mmproj-f16.gguf` | VlmInferenceEngine | FP16 multimodal vision projector |

## Storage Directories

| Variable                   | Default                      | Used By                   | Description                                               |
| -------------------------- | ---------------------------- | ------------------------- | --------------------------------------------------------- |
| `ORION_MEDIUM_STORAGE_DIR` | `./media/sd/medium/`         | TriageRouter, EventAction | MEDIUM image storage. Path + filename must be < 100 chars |
| `ORION_DOWNLINK_QUEUE_DIR` | `./media/sd/downlink_queue/` | GroundCommsDriver         | HIGH frame disk queue (outside comm window)               |

## Ground Station Parameters

| Variable            | Default   | Used By      | Description                                       |
| ------------------- | --------- | ------------ | ------------------------------------------------- |
| `ORION_GS_LAT`      | `46.5191` | NavTelemetry | Ground station latitude (default: EPFL Ecublens)  |
| `ORION_GS_LON`      | `6.5668`  | NavTelemetry | Ground station longitude                          |
| `ORION_GS_RANGE_KM` | `2000.0`  | NavTelemetry | Comm window radius in km (10% hysteresis on exit) |

## Ground Receiver Connection

| Variable         | Default     | Used By           | Description                                     |
| ---------------- | ----------- | ----------------- | ----------------------------------------------- |
| `ORION_GDS_HOST` | `127.0.0.1` | GroundCommsDriver | IP address of the ground receiver (receiver.py) |
| `ORION_GDS_PORT` | `50050`     | GroundCommsDriver | TCP port of the ground receiver                 |

## Example: Pi-to-Mac Deployment

```bash
export ORION_SIMSAT_URL=http://192.168.1.183:9005
export ORION_GGUF_PATH=./orion-q4_k_m.gguf
export ORION_MMPROJ_PATH=orion-mmproj-f16.gguf
export ORION_MEDIUM_STORAGE_DIR=./media/sd/medium/
export ORION_DOWNLINK_QUEUE_DIR=./media/sd/downlink_queue/
export ORION_GDS_HOST=192.168.1.183
```
