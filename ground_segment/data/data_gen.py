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
TEST_FILE = os.path.join(DATASET_DIR, "test_dataset.jsonl")


def get_prompt(lon, lat, include_coords=True):
    # Coordinate Dropout Logic: 50% of the time, the model gets no telemetry
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
    """Calculates the distance in kilometers between two points."""
    R = 6371  # Earth radius in km
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
    """Removes targets that are geographically too close to each other."""
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


def setup_dirs():
    os.makedirs(IMAGES_DIR, exist_ok=True)
    for f in [TRAIN_FILE, TEST_FILE]:
        if os.path.exists(f):
            os.remove(f)


def fetch_image(lon, lat, filename):
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
    setup_dirs()

    # 1. Deduplicate based on distance
    clean_targets = filter_overlaps(ALL_TARGETS)

    # 2. Shuffle and Split
    random.shuffle(clean_targets)
    split_idx = int(len(clean_targets) * 0.8)
    train_set = clean_targets[:split_idx]
    test_set = clean_targets[split_idx:]

    print(
        f" Starting Capture: {len(train_set)} Base Train Images | {len(test_set)} Test Images"
    )

    for idx, sample in enumerate(clean_targets):
        img_filename = f"{sample['name']}.png"
        img_path = os.path.join(IMAGES_DIR, img_filename)

        print(
            f"[{idx + 1}/{len(clean_targets)}] Fetching {sample['name']}...", end="\r"
        )

        if fetch_image(sample["lon"], sample["lat"], img_path):
            is_train = sample in train_set

            if is_train:
                # AUGMENTATION: Duplicate the sample (one WITH coords, one WITHOUT)
                for include_coords in [True, False]:
                    record = {
                        "image": img_path,
                        "conversations": [
                            {
                                "role": "user",
                                "content": f"<image>\n{get_prompt(sample['lon'], sample['lat'], include_coords)}",
                            },
                            {
                                "role": "assistant",
                                "content": json.dumps(
                                    {
                                        "reason": sample["reason"],
                                        "category": sample["cat"],
                                    }
                                ),
                            },
                        ],
                    }
                    with open(TRAIN_FILE, "a") as f:
                        f.write(json.dumps(record) + "\n")
            else:
                # TEST SET: ALWAYS include coords (Ablation script handles stripping)
                record = {
                    "image": img_path,
                    "conversations": [
                        {
                            "role": "user",
                            "content": f"<image>\n{get_prompt(sample['lon'], sample['lat'], True)}",
                        },
                        {
                            "role": "assistant",
                            "content": json.dumps(
                                {"reason": sample["reason"], "category": sample["cat"]}
                            ),
                        },
                    ],
                }
                with open(TEST_FILE, "a") as f:
                    f.write(json.dumps(record) + "\n")

        time.sleep(0.5)  # Speed up slightly, SimSat should handle 2 req/sec

    print("\n\n Dataset generated successfully (Train set augmented).")


if __name__ == "__main__":
    main()
