# Utility Scripts

The ground segment includes two shell scripts for managing data and model weights across local and remote training servers.

## upload_to_server.sh

`ground_segment/data/upload_to_server.sh` uploads the generated dataset to a remote GPU server for training.

**What it does:**

1. Compresses `orion_dataset/` into a tarball
2. Transfers it to the server's HDD via `rsync`
3. Unpacks on the server, removing any previous dataset
4. Clones or pulls the ORION repository on the server

**Requires:** The `SERVER` environment variable set to the remote server address (e.g., `export SERVER=user@gpu-server`).

**Server paths (hardcoded):**

| Path                          | Purpose                       |
| ----------------------------- | ----------------------------- |
| `~/hdd/gaze/datasets/extras/` | Dataset storage on server HDD |
| `~/code/extras/ORION/`        | Repository clone on server    |

## download_weights.sh

`ground_segment/training/download_weights.sh` downloads trained LoRA weights from the remote server after fine-tuning completes.

**What it does:**

1. Verifies the weights directory exists on the server
2. Wipes any existing local weights to prevent stale data
3. Downloads `orion_lora_weights/` via `rsync`
4. On success, cleans up the repository and dataset on the server (scorched earth)

**Requires:** The `SERVER` environment variable set to the remote server address.

**Important:** On successful download, this script deletes the repository and dataset from the server. This is intentional — the weights are the only artifact needed after training.
