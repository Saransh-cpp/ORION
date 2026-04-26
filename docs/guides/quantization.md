# Quantization

> For design details - quantization scheme tradeoffs and why Q4_K_M is used - see the [Training Pipeline architecture](../ground-segment/training.md) page.

## Prerequisites

- The merged model from the fuse step (see the [Training](training.md) guide) at `ground_segment/training/orion_merged/`.
- Python 3.10+ with the `gguf` package installed (`pip install gguf`).
- llama.cpp built from source (for the `llama-quantize` binary).
- ~11 GB free disk space for all intermediate artifacts. See [Compute Budgets](../ground-segment/budgets.md) for details.

## Step 1: Build llama.cpp

```bash
cd ground_segment/llama.cpp
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

This produces the `llama-quantize` binary in the build directory.

## Step 2: Convert to GGUF (FP16)

Use the `convert_hf_to_gguf.py` script to convert the merged Hugging Face model to GGUF format:

```bash
cd ground_segment/llama.cpp

python convert_hf_to_gguf.py \
    ../training/orion_merged/ \
    --outfile orion-f16.gguf \
    --outtype f16
```

This produces an FP16 GGUF file. For VLMs with a multimodal projector, the conversion also generates a separate projector file.

## Step 3: Extract the Multimodal Projector

For vision-language models, the multimodal projector (mmproj) must be extracted separately. The conversion script typically outputs this automatically. If not, you can extract it:

```bash
python convert_hf_to_gguf.py \
    ../training/orion_merged/ \
    --outfile orion-mmproj-f16.gguf \
    --outtype f16 \
    --mmproj
```

The projector file stays in FP16 - it is small and does not need quantization.

## Step 4: Quantize to Q4_K_M

Quantize the main model file (not the projector):

```bash
./build/bin/llama-quantize orion-f16.gguf orion-q4_k_m.gguf Q4_K_M
```

## Output Files

After quantization, you should have two files ready for deployment:

- `orion-q4_k_m.gguf` - Quantized language model (Q4_K_M), approximately 1 GB.
- `orion-mmproj-f16.gguf` - Multimodal projector (FP16), approximately 100 MB.

## Step 5: Deploy to the Raspberry Pi 5

Transfer both GGUF files to the Pi 5's filesystem:

```bash
scp orion-q4_k_m.gguf orion-mmproj-f16.gguf pi@<pi-address>:/path/to/orion/models/
```

### Verifying the Deployment

You can test the quantized model on the Pi using llama.cpp's CLI:

```bash
./llama-cli \
    -m /path/to/orion-q4_k_m.gguf \
    --mmproj /path/to/orion-mmproj-f16.gguf \
    -p "You are an autonomous orbital triage assistant..." \
    --image /path/to/test-image.png \
    -n 200
```

## Troubleshooting

**Conversion fails with "unsupported model architecture"**: Ensure you are using a recent version of llama.cpp that supports the LFM2.5-VL architecture. Pull the latest `ground_segment/llama.cpp` submodule.

**Quantized model produces degraded output**: Try Q5_K_M for higher quality at the cost of larger file size. Compare outputs against the FP16 GGUF to isolate whether the issue is from quantization or the fine-tuning itself.

**Out of memory on Pi 5**: Ensure no other large processes are running. The Q4_K_M model should fit within 4 GB RAM alongside the inference runtime. If memory is tight, consider Q4_K_S for a slightly smaller footprint.
