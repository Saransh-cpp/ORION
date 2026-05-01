# Deployment

This guide covers cross-compiling ORION for the Raspberry Pi 5 (ARM64), transferring the binary and model files, and running the full system.

## Architecture

ORION runs as a split system:

- **Raspberry Pi 5**: runs the flight segment binary (`Orion`) with the GGUF model
- **Development machine (Mac/Linux)**: runs the F-Prime GDS (Ground Data System) and `receiver.py`

The Pi connects to the GDS over TCP port 50000 and to the ground receiver over TCP port 50050. The Pi requires 4 GB+ RAM; see [Mission Budgets](../architecture/budgets.md) for detailed memory and compute requirements.

## Cross-Compile for Raspberry Pi 5

ORION uses Docker with QEMU emulation to cross-compile an ARM64 binary from any host architecture.

### Step 1: Build the Docker Images

```bash
docker compose build base
docker compose build pi-build
```

These must be run sequentially. `docker compose build` builds all services in parallel and will fail because `pi-build` depends on the `orion-base` image produced by `base`.

- `base`: ARM64 Ubuntu 22.04 with build tools, CMake, F-Prime Python dependencies, and a static llama.cpp build
- `pi-build`: compiles the F-Prime flight segment on top of the base image and copies the binary out via a bind mount

### Step 2: Run the Build

```bash
docker compose run --rm pi-build
```

This compiles the full flight segment and copies the resulting binary to `build-output/Orion` on the host via a bind mount.

### Step 3: Transfer to the Pi

Download the [pre-trained GGUF model files](https://drive.google.com/drive/folders/1h6WGNeNzYHdfisELlJodDCKlkREkIzCN?usp=share_link) (`orion-q4_k_m.gguf` and `orion-mmproj-f16.gguf`), then copy the binary and models to the Pi:

```bash
scp build-output/Orion user@<pi-ip>:/home/user/ORION/
scp ground_segment/training/orion-q4_k_m.gguf user@<pi-ip>:/home/user/ORION/
scp ground_segment/training/orion-mmproj-f16.gguf user@<pi-ip>:/home/user/ORION/
```

## Configure the Pi

Set the required environment variables on the Pi before launching the binary. Add these to your shell profile or export them in your session:

```bash
# Point to the development machine running SimSat
export ORION_SIMSAT_URL=http://<mac-ip>:9005

# Model paths (adjust to your layout)
export ORION_GGUF_PATH=./orion-q4_k_m.gguf
export ORION_MMPROJ_PATH=orion-mmproj-f16.gguf

# Storage directories
export ORION_MEDIUM_STORAGE_DIR=./media/sd/medium/
export ORION_DOWNLINK_QUEUE_DIR=./media/sd/downlink_queue/
```

Create the storage directories:

```bash
mkdir -p $ORION_MEDIUM_STORAGE_DIR $ORION_DOWNLINK_QUEUE_DIR
```

See [Environment Variables](environment-variables.md) for the full list of configurable variables.

## Launch the Flight Binary on the Pi

```bash
cd /home/user/ORION
./Orion -a <mac-ip> -p 50000
```

- `-a <mac-ip>`: the IP address of the machine running the GDS
- `-p 50000`: the TCP port the GDS listens on

The binary will connect to SimSat for position and image data, load the VLM model on MEASURE entry, and begin autonomous operation.

## Connect the GDS (Development Machine)

On your development machine, start the F-Prime Ground Data System in headless mode:

```bash
fprime-gds -n --ip-address 0.0.0.0 --ip-port 50000
```

- `-n`: headless mode (no browser GUI auto-launch)
- `--ip-address 0.0.0.0`: listen on all interfaces so the Pi can connect
- `--ip-port 50000`: match the port the Pi binary is configured to use

Open `http://localhost:5000` in a browser to access the GDS web interface for sending commands and viewing events/telemetry.

## Run the Ground Receiver

On the development machine, start the ground receiver to accept downlinked image frames:

```bash
cd ground_segment
python receiver.py
```

The receiver listens on TCP port 50050 and saves incoming frames as `.raw` files in `./orion_downlink/`. See [Receiver](../ground-segment/receiver.md) for protocol details.

## Verify the System

1. Confirm the GDS shows the Pi as connected (events should start flowing)
2. Check that `NavTelemetry` events appear with position updates
3. Send `SET_ECLIPSE true` via the GDS command interface to enter MEASURE mode
4. Watch for `ImageDispatched` and `InferenceComplete` events
5. If a HIGH target is detected and the satellite is in DOWNLINK, check the receiver output

## Next Steps

- [Usage](../index.md): full demo walkthrough
- [Environment Variables](environment-variables.md): complete configuration reference
