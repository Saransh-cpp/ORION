# Utility Scripts

The ground segment includes two shell scripts for managing data and model weights across local and remote training servers.

## upload_to_server.sh

`ground_segment/data/upload_to_server.sh` uploads the generated dataset to a remote GPU server for training.

**What it does:**

1. Compresses `orion_dataset/` into a tarball
2. Transfers it to the server's HDD via `rsync`
3. Unpacks on the server, removing any previous dataset
4. Clones or pulls the ORION repository on the server

**Requires:** `SERVER`, `ORION_SERVER_HDD_PATH`, `ORION_LOCAL_DATA_DIR`, `ORION_ARCHIVE_NAME`, `ORION_REPO_DIR` - see [environment-variables-gs.md](../guides/environment-variables-gs.md).

```bash
chmod +x ./upload_to_server.sh
SERVER=user@host \
ORION_SERVER_HDD_PATH=/home/schopra/hdd/gaze/datasets/extras \
ORION_LOCAL_DATA_DIR=orion_dataset \
ORION_ARCHIVE_NAME=orion_data.tar.gz \
ORION_REPO_DIR=/home/schopra/code/extras \
./upload_to_server.sh
```

## download_weights.sh

`ground_segment/training/download_weights.sh` downloads trained LoRA weights from the remote server after fine-tuning completes.

**What it does:**

1. Verifies the weights directory exists on the server
2. Wipes any existing local weights to prevent stale data
3. Downloads `orion_lora_weights/` via `rsync`
4. On success, deletes the repository and dataset from the server (scorched earth)

**Requires:** `SERVER`, `ORION_SERVER_WEIGHTS_PATH`, `ORION_SERVER_REPO_PATH`, `ORION_SERVER_DATA_PATH` — see [environment-variables-gs.md](../guides/environment-variables-gs.md).

```bash
chmod +x ./download_weights.sh
SERVER=user@host \
ORION_SERVER_WEIGHTS_PATH=~/code/extras/ORION/ground_segment/training/orion_lora_weights \
ORION_SERVER_REPO_PATH=~/code/extras/ORION \
ORION_SERVER_DATA_PATH=~/hdd/gaze/datasets/extras/orion_dataset \
./download_weights.sh
```

**Important:** On successful download, this script deletes the repository and dataset from the server. This is intentional — the weights are the only artifact needed after training.
