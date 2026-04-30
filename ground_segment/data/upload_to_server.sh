#!/bin/bash
#
# Compress the local dataset, upload it to a training server, and ensure the
# ORION repo is cloned/up-to-date on that server.
#
# Required env vars (see docs/guides/environment-variables-gs.md):
#   SERVER                  SSH host (alias from ~/.ssh/config or user@host)
#   ORION_SERVER_HDD_PATH   Server-side directory to extract the dataset into
#   ORION_LOCAL_DATA_DIR    Local directory to compress and upload
#   ORION_ARCHIVE_NAME      Temporary tarball filename
#   ORION_REPO_DIR          Server-side directory to clone/pull the ORION repo
#
# Quick invocation (one-shot, env vars apply only to this run):
#   SERVER=user@host ORION_SERVER_HDD_PATH=/home/schopra/hdd/gaze/datasets/extras ORION_LOCAL_DATA_DIR=orion_dataset ORION_ARCHIVE_NAME=orion_data.tar.gz ORION_REPO_DIR=/home/schopra/code/extras ./upload_to_server.sh
#
# Or export them in your shell profile then just run: ./upload_to_server.sh

set -euo pipefail

required_vars=(SERVER ORION_SERVER_HDD_PATH ORION_LOCAL_DATA_DIR ORION_ARCHIVE_NAME ORION_REPO_DIR)
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
SERVER_HDD_PATH="$ORION_SERVER_HDD_PATH"
LOCAL_DATA_DIR="$ORION_LOCAL_DATA_DIR"
ARCHIVE_NAME="$ORION_ARCHIVE_NAME"
REPO_DIR="$ORION_REPO_DIR"

# Always remove the local archive on exit (success or failure).
trap 'rm -f "$ARCHIVE_NAME"' EXIT

# 0. Pre-Flight Check (Verify local data exists)
echo " Verifying local dataset existence..."
if [ ! -d "$LOCAL_DATA_DIR" ]; then
    echo " ERROR: Local dataset directory '$LOCAL_DATA_DIR' not found!"
    echo " ABORTING UPLOAD. Please run your data generation script first."
    exit 1
fi
echo " Local dataset located."

echo " INITIATING UPLOAD PROTOCOL TO: $SERVER"

# 1. Compress the data locally
# COPYFILE_DISABLE=1 suppresses AppleDouble (._*) sidecar files; --no-xattrs
# strips extended attributes (e.g. com.apple.provenance) so GNU tar on the
# server doesn't warn "Ignoring unknown extended header keyword" on every file.
echo " 1/3 Compressing dataset..."
env COPYFILE_DISABLE=1 tar --no-xattrs -czf "$ARCHIVE_NAME" "$LOCAL_DATA_DIR"

# 2. Transfer to the server's HDD
echo " 2/3 Transferring data to server HDD..."
ssh -T "$SERVER" "mkdir -p $SERVER_HDD_PATH"
rsync -avz --progress "$ARCHIVE_NAME" "$SERVER:$SERVER_HDD_PATH/"

# 3. Remote Execution: Unpack data and handle GitHub repo
echo " 3/3 Executing remote setup commands..."
ssh -T "$SERVER" << EOF
    cd $SERVER_HDD_PATH

    # WIPE the old dataset to prevent data merging
    if [ -d "$LOCAL_DATA_DIR" ]; then
        echo "   -> Nuking old dataset directory on server..."
        rm -rf "$LOCAL_DATA_DIR"
    fi

    # Unpack the new data
    echo "   -> Extracting new dataset..."
    env COPYFILE_DISABLE=1 tar -xzf "$ARCHIVE_NAME"

    # Remove the heavy archive from the server
    echo "   -> Cleaning up server archive..."
    rm "$ARCHIVE_NAME"

    # Setup the GitHub Repository
    echo "   -> Checking ORION repository status..."
    mkdir -p "$REPO_DIR"
    cd "$REPO_DIR"

    if [ ! -d "ORION" ]; then
        echo "   -> Cloning Saransh-cpp/ORION..."
        git clone git@github.com:Saransh-cpp/ORION.git
    else
        echo "   -> ORION already exists. Pulling latest changes..."
        cd ORION
        git pull origin main
    fi
EOF

# Local archive cleanup happens automatically via the EXIT trap.
echo " UPLOAD COMPLETE. Server is ready for training."
