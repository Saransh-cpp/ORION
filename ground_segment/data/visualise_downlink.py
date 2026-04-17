"""Convert raw 512x512 RGB images from ORION's downlink/storage directories to PNG."""

import glob
import os

import numpy as np
from PIL import Image


IMAGE_W, IMAGE_H = 512, 512
EXPECTED_SIZE = IMAGE_W * IMAGE_H * 3  # 786432 bytes


def convert_directory(directory):
    files = sorted(glob.glob(os.path.join(directory, "*.raw")))

    if not files:
        print(f"No .raw files found in {directory}")
        return

    for path in files:
        data = open(path, "rb").read()
        if len(data) != EXPECTED_SIZE:
            print(f"  Skipping {path}: expected {EXPECTED_SIZE} bytes, got {len(data)}")
            continue

        img = Image.fromarray(
            np.frombuffer(data, dtype=np.uint8).reshape((IMAGE_H, IMAGE_W, 3))
        )
        png_path = path.replace(".raw", ".png")
        img.save(png_path)
        print(f"  {os.path.basename(path)} -> {os.path.basename(png_path)}")


if __name__ == "__main__":
    for d in ["./orion_downlink", "./orion_medium"]:
        if os.path.isdir(d):
            print(f"\n{d}/")
            convert_directory(d)
