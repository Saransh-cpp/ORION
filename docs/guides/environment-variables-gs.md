# Ground Segment Environment Variables

## Remote Training Server

| Variable                    | Default    | Used By                                      | Description                                                                   |
| --------------------------- | ---------- | -------------------------------------------- | ----------------------------------------------------------------------------- |
| `SERVER`                    | (required) | `upload_to_server.sh`, `download_weights.sh` | SSH address of the remote GPU server (e.g., `user@gpu-server`)                |
| `ORION_SERVER_HDD_PATH`     | (required) | `upload_to_server.sh`                        | Server-side directory where the dataset tarball is extracted                  |
| `ORION_LOCAL_DATA_DIR`      | (required) | `upload_to_server.sh`                        | Local directory to compress and upload (e.g., `orion_dataset`)                |
| `ORION_ARCHIVE_NAME`        | (required) | `upload_to_server.sh`                        | Temporary tarball filename (e.g., `orion_data.tar.gz`)                        |
| `ORION_REPO_DIR`            | (required) | `upload_to_server.sh`                        | Server-side directory to clone/pull the ORION repo into                       |
| `ORION_SERVER_WEIGHTS_PATH` | (required) | `download_weights.sh`                        | Server-side path to the `orion_lora_weights` directory                        |
| `ORION_SERVER_REPO_PATH`    | (required) | `download_weights.sh`                        | Server-side path to the full ORION repo (deleted after successful downlink)   |
| `ORION_SERVER_DATA_PATH`    | (required) | `download_weights.sh`                        | Server-side path to the dataset directory (deleted after successful downlink) |

## Fine-Tuning

| Variable             | Default    | Used By        | Description                                                                                                          |
| -------------------- | ---------- | -------------- | -------------------------------------------------------------------------------------------------------------------- |
| `ORION_DATASET_ROOT` | (required) | `fine_tune.py` | Directory containing `orion_dataset/` (resolves both JSONL files and the relative image paths embedded inside them). |

On the training server this typically matches `ORION_SERVER_HDD_PATH`. On a local dev machine it would be the directory containing the `orion_dataset/` produced by `data_gen.py` (e.g., `ground_segment/data`).

## Receiver

`receiver.py` uses hardcoded values and does not read environment variables. See the [receiver configuration](../architecture/ground_segment/receiver.md#configuration) for details.

## Example: Training Workflow

```bash
# Local machine: upload dataset
export SERVER=saransh@gpu-server
export ORION_SERVER_HDD_PATH=~/hdd/gaze/datasets/extras
export ORION_LOCAL_DATA_DIR=orion_dataset
export ORION_ARCHIVE_NAME=orion_data.tar.gz
export ORION_REPO_DIR=~/code/extras

cd ground_segment/data
bash upload_to_server.sh

# On the training server: fine-tune (matches ORION_SERVER_HDD_PATH from above)
export ORION_DATASET_ROOT=~/hdd/gaze/datasets/extras
cd ground_segment/training
python fine_tune.py

# Local machine: download weights after training
export ORION_SERVER_WEIGHTS_PATH=~/code/extras/ORION/ground_segment/training/orion_lora_weights
export ORION_SERVER_REPO_PATH=~/code/extras/ORION
export ORION_SERVER_DATA_PATH=~/hdd/gaze/datasets/extras/orion_dataset
cd ground_segment/training
bash download_weights.sh
```
