# Development

This guide covers building ORION from source on a development machine (macOS or Linux x86_64). For deploying to a Raspberry Pi 5, see [Deployment](deployment.md).

## Prerequisites

| Dependency   | Minimum Version | Purpose                                          |
| ------------ | --------------- | ------------------------------------------------ |
| CMake        | 3.20+           | Building llama.cpp and the F-Prime project       |
| uv           | latest          | Python environment and dependency management     |
| libcurl      | any             | SimSat HTTP client (NavTelemetry, CameraManager) |
| C++ compiler | C++17           | F-Prime and llama.cpp compilation                |
| Git          | any             | Submodule management                             |

### macOS

```bash
brew install cmake uv
# Xcode Command Line Tools provides clang and libcurl
xcode-select --install
```

### Ubuntu / Debian

```bash
sudo apt-get update
sudo apt-get install build-essential cmake libcurl4-openssl-dev libomp-dev python3 python3-venv
# Install uv
curl -LsSf https://astral.sh/uv/install.sh | sh
```

## Clone the Repository

ORION uses Git submodules for F-Prime and llama.cpp. Always clone with `--recursive`:

```bash
git clone --recursive https://github.com/Saransh-cpp/ORION.git
cd ORION
```

If you already cloned without `--recursive`, initialize the submodules manually:

```bash
git submodule update --init --recursive
```

## Build llama.cpp

The VLM inference engine links against llama.cpp as a static library. Build it before the flight segment:

```bash
cd ground_segment/llama.cpp
cmake -B build \
  -DBUILD_SHARED_LIBS=OFF \
  -DLLAMA_BUILD_TESTS=OFF \
  -DLLAMA_BUILD_EXAMPLES=OFF \
  -DLLAMA_BUILD_SERVER=OFF
cmake --build build -j$(nproc)
cd ../..
```

## Set Up the Python Environment

Create a virtual environment and install the F-Prime Python dependencies:

```bash
cd flight_segment/orion
uv venv --python 314
source .venv/bin/activate
uv pip install -r lib/fprime/requirements.txt
```

## Generate and Build the Flight Segment

Use F-Prime's build tooling to generate the autocoded sources and compile the binary:

```bash
fprime-util generate
fprime-util build --all
```

The compiled binary is located at:

```
./build-artifacts/Darwin/Orion/bin/Orion   # macOS
./build-artifacts/Linux/Orion/bin/Orion    # Linux
```

Check if the executable was indeed produced in the right location:

```bash
# macOS
file ./build-artifacts/Darwin/Orion/bin/Orion
```

## Install Ground Segment Dependencies (Optional)

If you plan to run the training pipeline, data generation scripts, or evaluation studies, install the ground segment Python dependencies:

```bash
deactivate  # deactivate the FS environment if active
cd ../../ground_segment  # if in flight_segment/orion
```

```bash
uv venv --python 314
source .venv/bin/activate
uv sync
```

This installs PyTorch, Transformers, PEFT, and other ML dependencies defined in `ground_segment/pyproject.toml`.

## Next Steps

- [Deployment](deployment.md): cross-compile for Raspberry Pi 5 and deploy
- [Usage](../index.md#usage): run a demo session with SimSat and GDS
- [Environment Variables](environment-variables.md): configure paths and endpoints
