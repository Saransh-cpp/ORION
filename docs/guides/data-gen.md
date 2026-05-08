# Data Generation

> For design details, target definitions, triage categories, and morphology sub-types, see the [Dataset architecture](../architecture/ground_segment/data.md) page.

## Prerequisites

- SimSat must be running and serving images at `http://localhost:9005/data/image/mapbox`.
- [uv](https://docs.astral.sh/uv/getting-started/installation/) installed.

## Running the Data Generator

```bash
cd ground_segment
uv venv --python 314
. .venv/bin/activate
uv sync
cd data
uv run data_gen.py
```

### What Happens

1. **Deduplication**: Targets within 2 km of each other are removed.
2. **Shuffle and Split**: The deduplicated targets are shuffled with a fixed seed (`random.seed(42)`) and split into train (240), val (60), and test (60) sets.
3. **Image Capture**: For each target, a 512x512 RGB satellite image is fetched from SimSat.
4. **Prompt Generation**: A triage prompt is generated with longitude and latitude telemetry.
5. **Coordinate Dropout (Training Only)**: Each training sample is duplicated - once with coordinates in the prompt, once without. Test samples always include coordinates.
6. **JSONL Writing**: Records are appended to the appropriate JSONL file.

The generator waits 0.5 seconds between requests to avoid overloading SimSat.

## Expected Output

After running, the following directory structure is created:

```
orion_dataset/
    images/
        high_port_rotterdam.png
        low_ocean_pacific_nemo.png
        med_city_chicago.png
        ...
    train_dataset.jsonl
    val_dataset.jsonl
    test_dataset.jsonl
```

Each line in the JSONL files is a JSON object with this structure:

```json
{
  "image": "orion_dataset/images/high_port_rotterdam.png",
  "conversations": [
    {
      "role": "user",
      "content": "<image>\nYou are an autonomous orbital triage assistant. Analyze this high-resolution RGB satellite image captured at Longitude: 4.05, Latitude: 51.95.\nStrictly use one of these categories based on visual morphology:\n- HIGH: ...\n- MEDIUM: ...\n- LOW: ...\nYou MUST output your response as a valid JSON object. To ensure accurate visual reasoning, you must output the \"reason\" key FIRST, followed by the \"category\" key."
    },
    {
      "role": "assistant",
      "content": "{\"reason\": \"Extreme-density geometric cargo terminals and massive vessel berthing.\", \"category\": \"HIGH\"}"
    }
  ]
}
```

The `<image>` token in the user content marks where the image will be injected during training.

## Customizing Targets

To add new targets, edit the target lists in `ground_segment/data/data.py`:

```python
HIGH_TARGETS = [
    # Add your target
    {
        "name": "high_my_new_target",
        "lon": -77.05,
        "lat": 38.87,
        "cat": "HIGH",
        "reason": "Describe the visual morphology that justifies this classification.",
    },
    # ... existing targets
]
```

Guidelines for adding targets:

- Use a descriptive `name` prefixed with the category abbreviation (`high_`, `med_`, `low_`).
- The `reason` should describe the **visual morphology** an orbital observer would see, not domain knowledge about the location.
- Ensure new targets are at least 2 km from existing targets to avoid being filtered out by the proximity check.
- For robust training, consider adding targets that are visually challenging for their category (e.g., natural formations that resemble human infrastructure for LOW).
