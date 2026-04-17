"""
Pre-convert test satellite images to 512x512 raw RGB files for CameraManager.

Input:  ground_segment/data/orion_dataset/images/*.png  (2560x2560 JPEG)
Output: ground_segment/data/test_raw/image_NNNN.raw     (512x512x3 = 786432 bytes each)

Also writes a manifest.txt listing each raw file with its source name and GPS coords
from the test_dataset.jsonl.
"""

import json
import os
import re

from PIL import Image

DATASET_DIR = os.path.join(os.path.dirname(__file__), "data", "orion_dataset")
JSONL_PATH = os.path.join(DATASET_DIR, "test_dataset.jsonl")
IMAGES_DIR = os.path.join(DATASET_DIR, "images")
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "data", "test_raw")
TARGET_SIZE = (512, 512)


def parse_coords(prompt_text):
    """Extract lat/lon from the prompt text."""
    lon_match = re.search(r"Longitude:\s*([-\d]+\.?\d*)", prompt_text)
    lat_match = re.search(r"Latitude:\s*([-\d]+\.?\d*)", prompt_text)
    lon = float(lon_match.group(1)) if lon_match else 0.0
    lat = float(lat_match.group(1)) if lat_match else 0.0
    return lat, lon


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    with open(JSONL_PATH) as f:
        entries = [json.loads(line) for line in f if line.strip()]

    manifest = []

    for i, entry in enumerate(entries):
        # Resolve image path
        img_rel = entry["image"]
        img_path = os.path.join(DATASET_DIR, "..", img_rel)
        if not os.path.exists(img_path):
            img_path = os.path.join(IMAGES_DIR, os.path.basename(img_rel))

        if not os.path.exists(img_path):
            print(f"[SKIP] {img_rel} not found")
            continue

        # Parse GPS from prompt
        prompt = entry["conversations"][0]["content"]
        lat, lon = parse_coords(prompt)

        # Parse expected category from response
        response = entry["conversations"][1]["content"]
        try:
            category = json.loads(response).get("category", "UNKNOWN")
        except json.JSONDecodeError:
            category = "UNKNOWN"

        # Load, resize, convert to raw RGB
        img = Image.open(img_path).convert("RGB").resize(TARGET_SIZE, Image.LANCZOS)
        raw_bytes = img.tobytes()

        assert len(raw_bytes) == TARGET_SIZE[0] * TARGET_SIZE[1] * 3  # 786432

        out_name = f"image_{i:04d}.raw"
        out_path = os.path.join(OUTPUT_DIR, out_name)
        with open(out_path, "wb") as f:
            f.write(raw_bytes)

        manifest.append(
            f"{out_name} {lat} {lon} {category} {os.path.basename(img_rel)}"
        )
        print(
            f"[{i:4d}] {os.path.basename(img_rel)} -> {out_name}  ({category}, {lat}, {lon})"
        )

    # Write manifest
    manifest_path = os.path.join(OUTPUT_DIR, "manifest.txt")
    with open(manifest_path, "w") as f:
        f.write("\n".join(manifest) + "\n")

    print(f"\nConverted {len(manifest)} images to {OUTPUT_DIR}/")
    print(f"Manifest: {manifest_path}")


if __name__ == "__main__":
    main()
