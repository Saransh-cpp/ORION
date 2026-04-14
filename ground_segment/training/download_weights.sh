#!/bin/bash

if [ -z "$SERVER" ]; then
    echo "❌ ERROR: The 'SERVER' environment variable is not set."
    echo "Please set it by running: export SERVER='<your_server_ip>'"
    exit 1
fi

# --- CONFIGURATION ---
SERVER_WEIGHTS_PATH="~/code/extras/ORION/ground_segment/training/orion_lora_weights"
LOCAL_DEST="./"
LOCAL_WEIGHTS_DIR="./orion_lora_weights"

# Paths to nuke on the server
SERVER_REPO_PATH="~/code/extras/ORION"
SERVER_DATA_PATH="~/hdd/gaze/datasets/orion_dataset"

# 1. Pre-Flight Check (Verify weights exist on server)
echo "🔍 Verifying payload existence on server..."
if ssh -T $SERVER "[ ! -d $SERVER_WEIGHTS_PATH ]"; then
    echo "❌ ERROR: The weights directory does not exist on the server!"
    echo "Path checked: $SERVER_WEIGHTS_PATH"
    echo "⚠️ ABORTING DOWNLINK. No local files were deleted."
    exit 1
fi
echo "✅ Payload located."

# 2. Local Cleanup (Preventing Stale Weights)
if [ -d "$LOCAL_WEIGHTS_DIR" ]; then
    echo "🗑️ Wiping existing local weights at $LOCAL_WEIGHTS_DIR..."
    rm -rf "$LOCAL_WEIGHTS_DIR"
fi

echo "📥 INITIATING DOWNLINK FROM: $SERVER..."

# 3. Pull the weights
rsync -avz --progress $SERVER:$SERVER_WEIGHTS_PATH $LOCAL_DEST

# 4. Safety Check: Did the download finish successfully?
if [ $? -eq 0 ]; then
    echo "✅ DOWNLINK COMPLETE. Payload secured locally."

    # 5. Remote Cleanup (Scorched Earth)
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
