#!/bin/bash

if [ -z "$SERVER" ]; then
    echo "❌ ERROR: The 'SERVER' environment variable is not set."
    echo "Please set it by running: export SERVER='<your_server_ip>'"
    exit 1
fi

# --- CONFIGURATION ---
SERVER_HDD_PATH="~/hdd/gaze/datasets"
LOCAL_DATA_DIR="orion_dataset"
ARCHIVE_NAME="orion_data.tar.gz"
REPO_DIR="~/code/extras"

# 0. Pre-Flight Check (Verify local data exists)
echo "🔍 Verifying local dataset existence..."
if [ ! -d "$LOCAL_DATA_DIR" ]; then
    echo "❌ ERROR: Local dataset directory '$LOCAL_DATA_DIR' not found!"
    echo "⚠️ ABORTING UPLOAD. Please run your data generation script first."
    exit 1
fi
echo "✅ Local dataset located."

echo "🚀 INITIATING UPLOAD PROTOCOL TO: $SERVER..."

# 1. Compress the data locally
echo "📦 1/3 Compressing dataset..."
tar -czf $ARCHIVE_NAME $LOCAL_DATA_DIR

# 2. Transfer to the server's HDD
echo "📡 2/3 Transferring data to server HDD..."
# Ensure the parent directory exists on the server first
ssh -T $SERVER "mkdir -p $SERVER_HDD_PATH"
rsync -avz --progress $ARCHIVE_NAME $SERVER:$SERVER_HDD_PATH/

# 3. Remote Execution: Unpack data and handle GitHub repo
echo "💻 3/3 Executing remote setup commands..."
ssh -T $SERVER << EOF
    cd $SERVER_HDD_PATH

    # WIPE the old dataset to prevent data merging
    if [ -d "$LOCAL_DATA_DIR" ]; then
        echo "   -> Nuking old dataset directory on server..."
        rm -rf $LOCAL_DATA_DIR
    fi

    # Unpack the new data
    echo "   -> Extracting new dataset..."
    tar -xzf $ARCHIVE_NAME

    # Remove the heavy archive from the server
    echo "   -> Cleaning up server archive..."
    rm $ARCHIVE_NAME

    # Setup the GitHub Repository
    echo "   -> Checking ORION repository status..."
    mkdir -p $REPO_DIR
    cd $REPO_DIR

    if [ ! -d "ORION" ]; then
        echo "   -> Cloning Saransh-cpp/ORION..."
        git clone git@github.com:Saransh-cpp/ORION.git
    else
        echo "   -> ORION already exists. Pulling latest changes..."
        cd ORION
        git pull origin main
    fi
EOF

# 4. Local Cleanup
echo "🧹 Cleaning up local archive..."
rm $ARCHIVE_NAME

echo "✅ UPLOAD COMPLETE. Server is ready for training."
