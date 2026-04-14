#!/bin/bash

if [ -z "$SERVER" ]; then
    echo "❌ ERROR: The 'SERVER' environment variable is not set."
    echo "Please set it by running: export SERVER='<your_server_ip>'"
    exit 1
fi

# --- CONFIGURATION ---
SERVER_HDD_PATH="~/hdd/gaze/datasets/"      # <-- REPLACE WITH ACTUAL HDD MOUNT PATH
LOCAL_DATA_DIR="orion_dataset"           # The folder containing your generated data
ARCHIVE_NAME="orion_data.tar.gz"
REPO_DIR="~/code/extras"

echo "🚀 INITIATING UPLOAD PROTOCOL TO: $SERVER..."

# 1. Compress the data locally
echo "📦 1/3 Compressing dataset..."
tar -czf $ARCHIVE_NAME $LOCAL_DATA_DIR

# 2. Transfer to the server's HDD
echo "📡 2/3 Transferring data to server HDD..."
rsync -avz --progress $ARCHIVE_NAME $SERVER:$SERVER_HDD_PATH/

# 3. Remote Execution: Unpack data and handle GitHub repo
echo "💻 3/3 Executing remote setup commands..."
ssh -T $SERVER << EOF
    # Unpack the data on the HDD
    echo "   -> Extracting dataset on server..."
    cd $SERVER_HDD_PATH
    tar -xzf $ARCHIVE_NAME
    rm $ARCHIVE_NAME # Clean up the archive to save space

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

echo "✅ UPLOAD COMPLETE. Server is ready for training."
