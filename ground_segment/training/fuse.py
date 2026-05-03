"""LoRA weight fusion - merges the QLoRA adapter into the base model for GGUF export.

Loads the base LFM2.5-VL-1.6B in FP16 on CPU, grafts the LoRA adapter from
``orion_lora_weights/``, calls ``merge_and_unload()`` to permanently fuse the
adapter matrices into the base weights, and saves the result as a standalone
SafeTensors model in ``orion_merged/``. The merged output is the input to
the GGUF quantisation step.

This script runs entirely on CPU (~8 GB RAM) and does not require a GPU.

Usage:

```bash
cd ground_segment/training
uv run fuse.py
```

See the [quantisation guide](../../../../guides/quantization/) for the next
step (converting the merged model to Q4_K_M GGUF for Pi deployment).
"""

import time
import torch
from transformers import AutoModelForImageTextToText, AutoProcessor
from peft import PeftModel

BASE_MODEL_ID = "LiquidAI/LFM2.5-VL-1.6B"
LORA_WEIGHTS_PATH = "./orion_lora_weights"
OUTPUT_DIR = "./orion_merged"


def main():
    """Load base model + LoRA adapter, fuse weights, and save as standalone FP16 model.

    The merged model and processor are saved to ``orion_merged/`` in
    SafeTensors format, ready for GGUF conversion via ``llama.cpp``'s
    ``convert_hf_to_gguf.py``.
    """
    t_start = time.perf_counter()
    print(" INITIATING ORION PAYLOAD MERGE PROTOCOL...")

    # 1. Load the Base Model in pure FP16 (CPU is fine for merging, avoids VRAM spikes)
    print(f" Loading base model {BASE_MODEL_ID} in FP16...")
    base_model = AutoModelForImageTextToText.from_pretrained(
        BASE_MODEL_ID,
        torch_dtype=torch.float16,
        device_map="cpu",
        trust_remote_code=True,
        low_cpu_mem_usage=True,
    )
    t_base = time.perf_counter()

    # 2. Load the LoRA Adapters
    print(f" Grafting LoRA adapters from {LORA_WEIGHTS_PATH}...")
    model = PeftModel.from_pretrained(base_model, LORA_WEIGHTS_PATH)
    t_lora = time.perf_counter()

    # 3. Fuse the weights permanently
    print(" Fusing weights (this may take a minute)...")
    merged_model = model.merge_and_unload()
    t_merged = time.perf_counter()

    # 4. Save the standalone, flight-ready Hugging Face model
    print(f" Saving standalone FP16 model to {OUTPUT_DIR}...")
    merged_model.save_pretrained(OUTPUT_DIR, safe_serialization=True)

    # 5. Bring the processor along
    processor = AutoProcessor.from_pretrained(BASE_MODEL_ID, trust_remote_code=True)
    processor.save_pretrained(OUTPUT_DIR)
    t_done = time.perf_counter()

    print(" MERGE COMPLETE. Model is ready for GGUF compilation.")
    print(
        f"Total runtime: {t_done - t_start:.2f}s "
        f"(base: {t_base - t_start:.2f}s, lora: {t_lora - t_base:.2f}s, "
        f"fuse: {t_merged - t_lora:.2f}s, save: {t_done - t_merged:.2f}s)"
    )


if __name__ == "__main__":
    main()
