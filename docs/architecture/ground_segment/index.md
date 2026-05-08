# Ground Segment

The ORION ground segment handles data reception, model training, and dataset management. It runs on the ground station host (typically the operator's Mac or a dedicated server).

## Components

| Module                           | Purpose                                                                                          |
| -------------------------------- | ------------------------------------------------------------------------------------------------ |
| [Receiver](receiver.md)          | TCP server receiving HIGH-priority image frames from the satellite via the simulated X-band link |
| [Training Pipeline](training.md) | QLoRA fine-tuning, validation, and GGUF quantization of the LFM2.5-VL-1.6B model                 |
| [Dataset](data.md)               | Target definitions, image dataset generation from SimSat, and train/test split management        |

## Integration with Flight Segment

The ground segment connects to the flight segment through two independent channels:

- **F-Prime GDS** (port 50000): Command uplink and telemetry/event downlink. Always active.
- **Ground Receiver** (port 50050): Image frame downlink via the ORIO protocol. Active only during DOWNLINK mode.

For GPU, RAM, and disk requirements across training, quantization, and validation, see [Compute Budgets](budgets.md).

SimSat runs separately and provides both orbital position data and Mapbox satellite imagery to the flight segment via HTTP.
