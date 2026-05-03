"""ORION dataset generator - fetches satellite tiles from SimSat and writes JSONL splits.

For each target in `data.ALL_TARGETS`, this script:

1. Fetches a 512x512 Mapbox satellite tile from SimSat's static image API.
2. Assigns the target to a deterministic train/val/test split (seeded shuffle).
3. Writes a conversation-format JSONL record suitable for LLaVA-style fine-tuning.

Train records are augmented with **coordinate dropout**: each target produces two
records, one with GPS coordinates in the prompt and one without, so the model
learns to classify from imagery alone when telemetry is unavailable.

Usage:

```bash
cd ground_segment/data
uv run data_gen.py        # requires SimSat running on localhost:9005
```

Output structure:

```
orion_dataset/
    images/              # 512x512 PNG tiles
    train_dataset.jsonl  # 2x train targets (coord augmentation)
    val_dataset.jsonl    # eval-loss tracking during training
    test_dataset.jsonl   # held-out evaluation set
```
"""

import requests
import json
import os
import time
import random
import math
from data import ALL_TARGETS

SIMSAT_STATIC_API = "http://localhost:9005/data/image/mapbox"
DATASET_DIR = "orion_dataset"
IMAGES_DIR = os.path.join(DATASET_DIR, "images")
TRAIN_FILE = os.path.join(DATASET_DIR, "train_dataset.jsonl")
VAL_FILE = os.path.join(DATASET_DIR, "val_dataset.jsonl")
TEST_FILE = os.path.join(DATASET_DIR, "test_dataset.jsonl")

SPLIT_SEED = 42
TEST_SIZE = 60
VAL_SIZE = 60


def get_prompt(lon, lat, include_coords=True):
    """Build the ChatML user prompt for a single satellite image.

    Args:
        lon: Longitude of the capture location.
        lat: Latitude of the capture location.
        include_coords: If ``False``, omit GPS coordinates from the prompt
            (coordinate dropout for training augmentation).

    Returns:
        The triage instruction prompt as a string.
    """
    if include_coords:
        telemetry_str = f" captured at Longitude: {lon}, Latitude: {lat}"
    else:
        telemetry_str = ""

    return f"""You are an autonomous orbital triage assistant. Analyze this high-resolution RGB satellite image{telemetry_str}.
Strictly use one of these categories based on visual morphology:
- HIGH: Extreme-scale strategic anomalies, dense geometric cargo/vessel infrastructure, massive cooling towers, sprawling runways, or distinct geological/artificial chokepoints.
- MEDIUM: Standard human civilization. Ordinary urban grids, low-density suburban sprawl, regular checkerboard agriculture, or localized infrastructure (malls, regional strips).
- LOW: Complete absence of human infrastructure. Featureless deep oceans, unbroken canopy, barren deserts, or purely natural geological formations (craters, natural cliffs).
You MUST output your response as a valid JSON object. To ensure accurate visual reasoning, you must output the "reason" key FIRST, followed by the "category" key."""


def haversine(lon1, lat1, lon2, lat2):
    """Compute the great-circle distance in km between two points using the Haversine formula.

    Args:
        lon1: Longitude of the first point in degrees.
        lat1: Latitude of the first point in degrees.
        lon2: Longitude of the second point in degrees.
        lat2: Latitude of the second point in degrees.

    Returns:
        Distance in kilometres.
    """
    R = 6371
    dlat = math.radians(lat2 - lat1)
    dlon = math.radians(lon2 - lon1)
    a = (
        math.sin(dlat / 2) ** 2
        + math.cos(math.radians(lat1))
        * math.cos(math.radians(lat2))
        * math.sin(dlon / 2) ** 2
    )
    c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))
    return R * c


def filter_overlaps(targets, min_dist_km=2.0):
    """Remove targets whose coordinates are within *min_dist_km* of an already-kept target.

    Uses a greedy first-come-first-kept strategy in list order. This prevents
    near-duplicate tiles from inflating a single geographic area in the dataset.

    Args:
        targets: List of target dicts (must contain ``lon`` and ``lat`` keys).
        min_dist_km: Minimum separation in km (default 2.0).

    Returns:
        Filtered list of targets with overlaps removed.
    """
    unique_targets = []
    skipped = 0
    for t in targets:
        is_too_close = False
        for u in unique_targets:
            if haversine(t["lon"], t["lat"], u["lon"], u["lat"]) < min_dist_km:
                is_too_close = True
                break
        if not is_too_close:
            unique_targets.append(t)
        else:
            skipped += 1
    print(
        f" Proximity Filter: Kept {len(unique_targets)} targets, skipped {skipped} overlaps."
    )
    return unique_targets


def make_record(sample, img_path, include_coords=True):
    """Build a single conversation-format JSONL record for LLaVA-style fine-tuning.

    Args:
        sample: Target dict with ``name``, ``lon``, ``lat``, ``cat``, and ``reason``.
        img_path: Path to the saved satellite tile image.
        include_coords: Whether to include GPS coordinates in the prompt.

    Returns:
        A dict with ``image`` and ``conversations`` keys matching the training schema.
    """
    return {
        "image": img_path,
        "conversations": [
            {
                "role": "user",
                "content": f"<image>\n{get_prompt(sample['lon'], sample['lat'], include_coords)}",
            },
            {
                "role": "assistant",
                "content": json.dumps(
                    {"reason": sample["reason"], "category": sample["cat"]}
                ),
            },
        ],
    }


def setup_dirs():
    """Create the output directory structure and clear any previous JSONL files."""
    os.makedirs(IMAGES_DIR, exist_ok=True)
    for f in [TRAIN_FILE, VAL_FILE, TEST_FILE]:
        if os.path.exists(f):
            os.remove(f)


def fetch_image(lon, lat, filename):
    """Fetch a satellite tile from SimSat's static Mapbox API and save it to disk.

    Args:
        lon: Target longitude.
        lat: Target latitude.
        filename: Output file path for the PNG image.

    Returns:
        ``True`` on success, ``False`` if the request failed.
    """
    params = {
        "lon_target": lon,
        "lat_target": lat,
        "lon_satellite": lon,
        "lat_satellite": lat,
        "alt_satellite": 500.0,
    }
    try:
        res = requests.get(SIMSAT_STATIC_API, params=params, timeout=10)
        res.raise_for_status()
        with open(filename, "wb") as f:
            f.write(res.content)
        return True
    except Exception:
        return False


def main():
    """Generate the full ORION training dataset.

    Shuffles all targets with a fixed seed, splits into train/val/test,
    fetches each tile from SimSat, and writes the corresponding JSONL records.
    Train targets are augmented with coordinate dropout (2x records).
    """
    setup_dirs()
    random.seed(SPLIT_SEED)

    # 1. Combine OLD + NEW (via ALL_TARGETS) and filter proximity overlaps once.
    clean_targets = filter_overlaps(ALL_TARGETS)

    # 2. Deterministic shuffle, then carve fixed-size test and val sets off the front.
    #    Remaining samples become train. IID so no distributional gap between splits.
    random.shuffle(clean_targets)
    test_set = clean_targets[:TEST_SIZE]
    val_set = clean_targets[TEST_SIZE : TEST_SIZE + VAL_SIZE]
    train_set = clean_targets[TEST_SIZE + VAL_SIZE :]

    print(
        f" Splits: {len(train_set)} train | {len(val_set)} val | {len(test_set)} test"
    )

    # Process every sample: fetch image once, write to the appropriate JSONL.
    all_samples = train_set + val_set + test_set
    train_names = {s["name"] for s in train_set}
    val_names = {s["name"] for s in val_set}

    for idx, sample in enumerate(all_samples):
        img_filename = f"{sample['name']}.png"
        img_path = os.path.join(IMAGES_DIR, img_filename)

        # \033[K = ANSI "erase from cursor to end of line", prevents leftover
        # characters when a shorter sample name follows a longer one.
        print(
            f"\r\033[K[{idx + 1}/{len(all_samples)}] Fetching {sample['name']}...",
            end="",
            flush=True,
        )

        if fetch_image(sample["lon"], sample["lat"], img_path):
            if sample["name"] in train_names:
                # TRAIN: augment with both coords-present and coords-absent variants.
                for include_coords in [True, False]:
                    with open(TRAIN_FILE, "a") as f:
                        f.write(
                            json.dumps(make_record(sample, img_path, include_coords))
                            + "\n"
                        )
            elif sample["name"] in val_names:
                # VAL: single record with coords. Used for eval_loss tracking
                # during training (early-stopping / best-checkpoint selection).
                with open(VAL_FILE, "a") as f:
                    f.write(json.dumps(make_record(sample, img_path, True)) + "\n")
            else:
                # TEST: held-out NEW_TARGETS, always with coords. evaluate.py
                # ablation script handles coord stripping for Conditions B/C/D.
                with open(TEST_FILE, "a") as f:
                    f.write(json.dumps(make_record(sample, img_path, True)) + "\n")

        time.sleep(0.5)  # SimSat should handle 2 req/sec

    print("\n\n Dataset generated successfully.")
    print(f"   Train (augmented): {len(train_set) * 2} records → {TRAIN_FILE}")
    print(f"   Val:               {len(val_set)} records → {VAL_FILE}")
    print(f"   Test (held-out):   {len(test_set)} records → {TEST_FILE}")


if __name__ == "__main__":
    main()
