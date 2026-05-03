"""Batch-convert 512x512 RGB .raw images to .jpg.

Usage:

```bash
uv run raw_to_jpg.py <directory>
```

Converts every .raw file in the given directory to a .jpg alongside it.
Intended for MEDIUM images downloaded via FLUSH_MEDIUM_STORAGE, but works
with any 512x512x3 RGB .raw file (including HIGH frames).
"""

import sys
from pathlib import Path

import numpy as np
from PIL import Image

IMAGE_W, IMAGE_H = 512, 512
EXPECTED_SIZE = IMAGE_W * IMAGE_H * 3


def convert(raw_path: Path) -> Path | None:
    """Convert a single 512x512 RGB .raw file to .jpg."""
    if raw_path.stat().st_size != EXPECTED_SIZE:
        print(f"  skip {raw_path.name} (unexpected size {raw_path.stat().st_size})")
        return None
    data = np.fromfile(raw_path, dtype=np.uint8).reshape((IMAGE_H, IMAGE_W, 3))
    jpg_path = raw_path.with_suffix(".jpg")
    Image.fromarray(data).save(jpg_path)
    return jpg_path


def main() -> None:
    """Convert all .raw files in the given directory to .jpg."""
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <directory>")
        sys.exit(1)

    directory = Path(sys.argv[1])
    if not directory.is_dir():
        print(f"Error: {directory} is not a directory")
        sys.exit(1)

    raw_files = sorted(directory.glob("*.raw"))
    if not raw_files:
        print(f"No .raw files found in {directory}")
        return

    converted = 0
    for raw_path in raw_files:
        result = convert(raw_path)
        if result:
            print(f"  {raw_path.name} -> {result.name}")
            converted += 1

    print(f"Converted {converted}/{len(raw_files)} files")


if __name__ == "__main__":
    main()
