# Quantization

> For design details - quantization scheme tradeoffs and why Q4_K_M is used - see the [Training Pipeline architecture](../ground-segment/training.md) page.

## Prerequisites

- The merged model from the fuse step (see the [Training](training.md) guide) at `ground_segment/training/orion_merged/`.
- llama.cpp built from source (for the `llama-quantize` binary).
- ~11 GB free disk space for all intermediate artifacts. See [Compute Budgets](../ground-segment/budgets.md) for details.

## Step 1: Build llama.cpp (if not built already)

```bash
cd ground_segment/llama.cpp
cmake -B build \
  -DBUILD_SHARED_LIBS=OFF \
  -DLLAMA_BUILD_TESTS=OFF \
  -DLLAMA_BUILD_EXAMPLES=OFF \
  -DLLAMA_BUILD_SERVER=OFF
cmake --build build -j$(nproc)
```

This produces the `llama-quantize` binary in the build directory.

## Step 2: Convert to GGUF (FP16)

Use the `convert_hf_to_gguf.py` script to convert the merged Hugging Face model to GGUF format:

```bash
# make sure you are in the uv environment
# created during data generation
cd ground_segment/training/

# using `uv run` does not work here
python ../llama.cpp/convert_hf_to_gguf.py \
    ./orion_merged/ \
    --outfile orion-f16.gguf \
    --outtype f16
```

This produces an FP16 GGUF file. For VLMs with a multimodal projector, the conversion also generates a separate projector file.

## Step 3: Extract the Multimodal Projector

For vision-language models, the multimodal projector (mmproj) must be extracted separately. The conversion script typically outputs this automatically. If not, you can extract it:

```bash
python ../llama.cpp/convert_hf_to_gguf.py \
    ./orion_merged/ \
    --outfile orion-mmproj-f16.gguf \
    --outtype f16 \
    --mmproj
```

The projector file stays in FP16 as it is small and does not need quantization.

## Step 4: Quantize to Q4_K_M

Quantize the main model file (not the projector):

```bash
../llama.cpp/build/bin/llama-quantize orion-f16.gguf orion-q4_k_m.gguf Q4_K_M
```

## Output Files

After quantization, you should have two files ready for deployment:

- `orion-q4_k_m.gguf`: Quantized language model (Q4_K_M), approximately 730 MB.
- `orion-mmproj-f16.gguf`: Multimodal projector (FP16), approximately 814 MB.

## Troubleshooting

**Quantized model produces degraded output**: Try Q5_K_M for higher quality at the cost of larger file size. Compare outputs against the FP16 GGUF to isolate whether the issue is from quantization or the fine-tuning itself.

**Out of memory on Pi 5**: Ensure no other large processes are running. The Q4_K_M model uses ~1.75 GB RSS (measured) on the 8 GB Pi 5, leaving ample headroom. If memory is tight, consider Q4_K_S for a slightly smaller footprint.
