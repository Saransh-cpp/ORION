#!/bin/bash
#
# Download trained LoRA weights from the remote training server and optionally
# wipe the server's repo and dataset to free space.
#
# Required env vars (see docs/guides/environment-variables-gs.md):
#   SERVER                      SSH host (alias from ~/.ssh/config or user@host)
#   ORION_SERVER_WEIGHTS_PATH   Server-side path to the orion_lora_weights directory
#   ORION_SERVER_REPO_PATH      Server-side path to the ORION repo (for cleanup)
#   ORION_SERVER_DATA_PATH      Server-side path to the dataset directory (for cleanup)
#
# Quick invocation:
#   SERVER=user@host ORION_SERVER_WEIGHTS_PATH=~/code/extras/ORION/ground_segment/training/orion_lora_weights ORION_SERVER_REPO_PATH=~/code/extras/ORION ORION_SERVER_DATA_PATH=~/hdd/gaze/datasets/extras/orion_dataset ./download_weights.sh
#
# Or export them in your shell profile then just run: bash download_weights.sh

set -euo pipefail

required_vars=(SERVER ORION_SERVER_WEIGHTS_PATH ORION_SERVER_REPO_PATH ORION_SERVER_DATA_PATH)
missing=()
for v in "${required_vars[@]}"; do
    if [ -z "${!v:-}" ]; then
        missing+=("$v")
    fi
done

if [ "${#missing[@]}" -ne 0 ]; then
    echo " ERROR: Missing required env vars: ${missing[*]}"
    echo " See header comment in this script for the full export list."
    exit 1
fi

# Reject placeholder values that may have leaked in from copy-pasting examples.
case "$SERVER" in
    *"<"*|*">"*)
        echo " ERROR: SERVER='$SERVER' contains placeholder angle brackets."
        echo " Set SERVER to a real SSH host (e.g., user@host or a ~/.ssh/config alias)."
        exit 1
        ;;
esac

# --- CONFIGURATION (from env vars) ---
LOCAL_WEIGHTS_DIR="./orion_lora_weights"
LOCAL_DEST="./"

# 1. Pre-Flight Check (Verify weights exist on server)
echo " Verifying payload existence on server..."
if ssh -T "$SERVER" "[ ! -d $ORION_SERVER_WEIGHTS_PATH ]"; then
    echo " ERROR: The weights directory does not exist on the server!"
    echo " Path checked: $ORION_SERVER_WEIGHTS_PATH"
    echo " ABORTING DOWNLINK. No local files were deleted."
    exit 1
fi
echo " Payload located."

# 2. Local Cleanup (Preventing Stale Weights)
if [ -d "$LOCAL_WEIGHTS_DIR" ]; then
    echo " Wiping existing local weights at $LOCAL_WEIGHTS_DIR..."
    rm -rf "$LOCAL_WEIGHTS_DIR"
fi

echo " INITIATING DOWNLINK FROM: $SERVER..."

# 3. Pull the weights
rsync -avz --progress "$SERVER:$ORION_SERVER_WEIGHTS_PATH" "$LOCAL_DEST"

# 4. Safety Check: Did the download finish successfully?
if [ $? -eq 0 ]; then
    echo " DOWNLINK COMPLETE. Payload secured locally."

    # 5. Remote Cleanup (Scorched Earth)
    echo " Initiating Server Cleanup Protocol..."
    ssh -T "$SERVER" << EOF
        echo "   -> Erasing repository: $ORION_SERVER_REPO_PATH..."
        rm -rf $ORION_SERVER_REPO_PATH

        echo "   -> Erasing dataset: $ORION_SERVER_DATA_PATH..."
        rm -rf $ORION_SERVER_DATA_PATH

        echo " SERVER CLEANUP COMPLETE."
EOF
else
    echo " ERROR: Downlink failed or was interrupted!"
    echo " ABORTING CLEANUP: Server data has been preserved."
fi
