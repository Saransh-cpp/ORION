# Contributing

This guide covers the development setup, code style, CI pipeline, and how to add new components to ORION.

## Development Setup

### Prerequisites

See [Installation](guides/installation.md) for the full list. In addition, install pre-commit for automated code formatting:

```bash
pip install pre-commit
pre-commit install
```

### Clone and Build

```bash
git clone --recursive https://github.com/Saransh-cpp/ORION.git
cd ORION

# Build llama.cpp
cd ground_segment/llama.cpp
cmake -B build \
  -DBUILD_SHARED_LIBS=OFF \
  -DLLAMA_BUILD_TESTS=OFF \
  -DLLAMA_BUILD_EXAMPLES=OFF \
  -DLLAMA_BUILD_SERVER=OFF
cmake --build build -j$(nproc)
cd ../..

# Set up Python environment
uv venv
source .venv/bin/activate
uv pip install -r flight_segment/orion/lib/fprime/requirements.txt

# Build flight segment
cd flight_segment/orion
fprime-util generate
fprime-util build --all -j$(nproc)
```

### Ground Segment Dependencies

```bash
cd ground_segment
uv sync
```

## Pre-Commit Hooks

ORION uses pre-commit hooks defined in `.pre-commit-config.yaml` to enforce code quality on every commit. The hooks run automatically after `pre-commit install`.

### Running Manually

```bash
# Run on all files
pre-commit run --all-files

# Run a specific hook
pre-commit run clang-format --all-files
pre-commit run ruff-check --all-files
```

## Code Style

### C++ (.cpp, .hpp)

C++ code follows Google style with ORION-specific overrides defined in `.clang-format`:

```yaml
BasedOnStyle: google
IndentWidth: 4
ColumnLimit: 120
AccessModifierOffset: -2
```

### Python (.py)

Python code is formatted and linted by Ruff:

- Formatting follows Ruff's default style
- Linting includes auto-fixable rules with `--fix --show-fixes`
- No additional `pyproject.toml` Ruff configuration is needed beyond the defaults

### FPP (.fpp)

FPP files (F-Prime's modeling language) follow the clang-format rules since they share similar syntax.

## CI Pipeline

### Flight Segment CI (`fs_ci.yml`)

The CI runs on pushes to `main` and on pull requests that modify flight segment, llama.cpp, or Docker files.

### Documentation CI (`docs.yml`)

Runs on pushes to `main` that modify `docs/`, `mkdocs.yml`, or component SDD files. Builds and deploys documentation using MkDocs to GitHub Pages.

## Project Structure

```
ORION/
  flight_segment/
    orion/
      Orion/
        Components/       : F-Prime components (6 custom)
          CameraManager/
          EventAction/
          GroundCommsDriver/
          NavTelemetry/
          TriageRouter/
          VlmInferenceEngine/
        Ports/            : Custom FPP port definitions
        Types/            : Custom FPP type definitions
        Top/              : Topology (component wiring)
      lib/
        fprime/           : F-Prime framework (submodule)
  ground_segment/
    llama.cpp/            : llama.cpp library (submodule)
    training/             : Fine-tuning, validation, fusion scripts
    data/                 : Dataset definitions and generation
    receiver.py           : Ground receiver TCP server
  docker/
    Dockerfile.base       : ARM64 base image with build tools
    Dockerfile.pi         : Flight segment build + minimal output
  docs/                   : Documentation (this site)
  .pre-commit-config.yaml : Pre-commit hook configuration
  .clang-format           : C++ formatting rules
  docker-compose.yml      : Docker Compose for Pi cross-compilation
```
