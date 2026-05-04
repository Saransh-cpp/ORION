# Deployment

This guide covers cross-compiling ORION for the Raspberry Pi 5 (ARM64), transferring the binary and model files, and running the full system.

## Architecture

ORION runs as a split system:

- **Raspberry Pi 5**: runs the flight segment binary (`Orion`) with the GGUF model
- **Development machine (Mac/Linux)**: runs the F-Prime GDS (Ground Data System) and `receiver.py`

The Pi connects to the GDS over TCP port 50000 and to the ground receiver over TCP port 50050. The Pi requires 4 GB+ RAM; see [Mission Budgets](../architecture/budgets.md) for detailed memory and compute requirements.

## Cross-Compile for Raspberry Pi 5

ORION uses Docker with QEMU emulation to cross-compile an ARM64 binary from any host architecture.

### Prerequisites (x86_64 hosts only)

Docker Desktop (macOS/Windows) includes QEMU ARM64 emulation out of the box. On bare Docker Engine (Linux x86_64), register the ARM64 binfmt handler once:

```bash
docker run --privileged --rm tonistiigi/binfmt --install arm64
```

This persists across reboots. Native ARM64 hosts (Apple Silicon, Pi, ARM CI runners) need no setup.

### Step 1: Build the Docker Images

From the root folder, run:

```bash
docker compose build base
docker compose build pi-build
```

These must be run sequentially. `docker compose build` builds all services in parallel and will fail because `pi-build` depends on the `orion-base` image produced by `base`.

- `base`: ARM64 Ubuntu 22.04 with build tools, CMake, F-Prime Python dependencies, and a static llama.cpp build
- `pi-build`: compiles the F-Prime flight segment on top of the base image and copies the binary out via a bind mount (this can be re-run frequently for debugging on the Pi as it compiles only the flight software and not the dependencies)

### Step 2: Run the Build

```bash
docker compose run --rm pi-build
```

This copies the resulting binary to `build-output/Orion` on the host via a bind mount.

### Step 3: Transfer to the Pi

Download the [pre-trained GGUF model files](https://drive.google.com/drive/folders/1h6WGNeNzYHdfisELlJodDCKlkREkIzCN?usp=share_link) (`orion-q4_k_m.gguf` and `orion-mmproj-f16.gguf`), then copy the binary and models to the Pi:

```bash
scp build-output/Orion user@<pi-ip>:/home/<user>/ORION/
# if trained (or use the model files linked above)
# set ORION_GGUF_PATH if using a different path for the file
# see [environment variables](./environment-variables.md)
scp ground_segment/training/orion-q4_k_m.gguf user@<pi-ip>:/home/<user>/ORION/
# if trained (or use the model files linked above)
# set ORION_MMPROJ_PATH if using a different path for the file
# see [environment variables](environment-variables.md)
scp ground_segment/training/orion-mmproj-f16.gguf user@<pi-ip>:/home/<user>/ORION/
```

## Configure the Pi

Set the required environment variables on the Pi before launching the binary. Add these to your shell profile or export them in your session:

```bash
# Point to the development machine running SimSat
export ORION_SIMSAT_URL=http://<machine-ip>:9005
```

See [Environment Variables](environment-variables.md) for the full list of configurable variables.

## Launch the Flight Binary on the Pi

```bash
cd /home/<user>/ORION
./Orion -a <machine-ip> -p 50000
```

- `-a <machine-ip>`: the IP address of the machine running the GDS
- `-p 50000`: the TCP port the GDS listens on

The binary will connect to SimSat for position and image data, load the VLM model on MEASURE entry, and begin autonomous operation.

## Connect the GDS (Development Machine)

On your development machine, start the F-Prime Ground Data System in headless mode:

```bash
cd flight_segment/orion
uv venv --python 314  # if not created (if did not follow installation)
source .venv/bin/activate
uv pip install -r lib/fprime/requirements.txt
fprime-gds -n --ip-address 0.0.0.0 --ip-port 50000 --file-storage-directory ../../ground_segment/data/downlinked_UHF
```

- `-n`: headless mode (no browser GUI auto-launch)
- `--ip-address 0.0.0.0`: listen on all interfaces so the Pi can connect
- `--ip-port 50000`: match the port the Pi binary is configured to use

Open `http://localhost:5000` in a browser to access the GDS web interface for sending commands and viewing events/telemetry.

## Run the Ground Receiver

On the development machine (in a new terminal), start the ground receiver to accept downlinked image frames:

```bash
cd ground_segment  # from repo root
uv venv --python 314  # if not created (if did not follow installation)
source .venv/bin/activate
uv sync
uv run receiver.py
```

The receiver listens on TCP port 50050 and saves incoming frames as `.raw` files in `ground_segment/data/downlinked_XBand/`. See [Receiver](../ground-segment/receiver.md) for protocol details.

## Verify the System

1. Confirm the GDS shows the Pi as connected (events should start flowing)
2. Check that `NavTelemetry` events appear with position updates
3. Send `SET_ECLIPSE true` via the GDS command interface to enter MEASURE mode
4. Watch for `ImageDispatched` and `InferenceComplete` events
5. If a HIGH target is detected and the satellite is in DOWNLINK, check the receiver output

## Next Steps

- [Usage](../index.md): full demo walkthrough
- [Environment Variables](environment-variables.md): complete configuration reference
