#!/bin/bash

if [ -z "$SERVER" ]; then
    echo "❌ ERROR: The 'SERVER' environment variable is not set."
    echo "Please set it by running: export SERVER='<your_server_ip>'"
    exit 1
fi

# --- CONFIGURATION ---
SERVER_WEIGHTS_PATH="~/code/extras/ORION/ground_segment/training/gorion_lora_weights"
LOCAL_DEST="./"

# Paths to nuke on the server
SERVER_REPO_PATH="~/code/extras/ORION"
SERVER_DATA_PATH="~/hdd/gaze/datasets/orion_dataset"

echo "📥 INITIATING DOWNLINK FROM: $SERVER..."

# 1. Pull the weights
rsync -avz --progress $SERVER:$SERVER_WEIGHTS_PATH $LOCAL_DEST

# 2. Safety Check: Did the download finish successfully?
if [ $? -eq 0 ]; then
    echo "✅ DOWNLINK COMPLETE. Payload secured locally."

    # 3. Remote Cleanup (Scorched Earth)
    echo "🧹 Initiating Server Cleanup Protocol..."
    ssh -T $SERVER << EOF
        echo "   -> Erasing repository: $SERVER_REPO_PATH..."
        rm -rf $SERVER_REPO_PATH

        echo "   -> Erasing dataset: $SERVER_DATA_PATH..."
        rm -rf $SERVER_DATA_PATH

        echo "✅ SERVER CLEANUP COMPLETE."
EOF
else
    echo "❌ ERROR: Downlink failed or was interrupted!"
    echo "⚠️ ABORTING CLEANUP: Server data has been preserved."
fi
