# Dataset

The ORION dataset is a curated collection of satellite imagery and triage labels used to fine-tune the VLM for orbital image classification. Images are fetched from SimSat's Mapbox API and paired with classification prompts and ground-truth labels.

## Dataset Structure

```
ground_segment/data/orion_dataset/
  images/
    low_ocean_pacific_nemo.png
    med_city_chicago.png
    high_port_rotterdam.png
    ...
  train_dataset.jsonl
  test_dataset.jsonl
```

- **images/**: 512x512 RGB satellite images fetched from SimSat
- **train_dataset.jsonl**: Training samples (80% of targets, with coordinate dropout augmentation)
- **test_dataset.jsonl**: Test samples (20% of targets, always with coordinates)

## Target Definitions

`ground_segment/data/data.py` defines 300 target locations organized by triage priority and visual morphology:

| Class  | Count | Visual Morphology                                                                                  |
| ------ | ----- | -------------------------------------------------------------------------------------------------- |
| LOW    | 100   | Featureless natural terrain: oceans, deserts, ice sheets, dense canopy, geological formations      |
| MEDIUM | 100   | Standard human civilization: urban grids, suburban sprawl, agriculture, towns, infrastructure      |
| HIGH   | 100   | Strategic anomalies: mega-ports, mega-airports, energy/dams, mega-mines, military/space facilities |

### LOW Morphologies

1. Standard voids: oceans and water bodies
2. Standard voids: deserts, ice sheets, dense canopy
3. Hard LOW: coastlines and boundaries that resemble artificial structures
4. Hard LOW: geological anomalies (craters, calderas) that mimic mines
5. Hard LOW: fractals and textures (deltas, salt flats, reefs) that mimic city streets

### MEDIUM Morphologies

1. Urban grids: dense city centers worldwide
2. Suburban sprawl: low-density residential areas
3. Agriculture: crop fields, pivot irrigation, terracing
4. Standard infrastructure: regional airports, rail yards, commercial zones
5. Towns and settlements: isolated clusters in varied terrain

### HIGH Morphologies

1. Mega-ports: Rotterdam, Singapore, LA, etc.
2. Mega-airports: Atlanta, Denver, Dubai, Daxing, etc.
3. Energy and dams: nuclear plants, solar farms, hydroelectric dams
4. Mega-mines and extreme industrial: open-pit mines, refineries
5. Space, military, and chokepoints: launch pads, naval bases, canals

## Generation Process

`ground_segment/data/data_gen.py` generates the dataset by:

1. **Proximity filter**: removes targets closer than 2 km to each other (Haversine distance) to avoid duplicate imagery
2. **Shuffle and split**: 80% train, 20% test (randomized)
3. **Image fetch**: for each target, fetches a 512x512 satellite image from SimSat's Mapbox static image API at `GET http://localhost:9005/data/image/mapbox`
4. **JSONL generation**: creates conversation-format records for fine-tuning

### JSONL Record Format

Each line in the JSONL files is a JSON object:

```json
{
  "image": "orion_dataset/images/high_port_rotterdam.png",
  "conversations": [
    {
      "role": "user",
      "content": "<image>\nYou are an autonomous orbital triage assistant..."
    },
    {
      "role": "assistant",
      "content": "{\"reason\": \"Extreme-density geometric cargo terminals...\", \"category\": \"HIGH\"}"
    }
  ]
}
```

## Coordinate Dropout Augmentation

For training samples, each target produces two JSONL records: one with GPS coordinates in the prompt and one without. This 50% coordinate dropout teaches the model to classify based on visual features alone, making it robust when GPS data is noisy or unavailable.

## Prompt Template

The prompt matches the ChatML format used by the fine-tuned model and the on-board VlmInferenceEngine:

```
You are an autonomous orbital triage assistant. Analyze this high-resolution RGB
satellite image captured at Longitude: X, Latitude: Y.
Strictly use one of these categories based on visual morphology:
- HIGH: ...
- MEDIUM: ...
- LOW: ...
You MUST output your response as a valid JSON object. To ensure accurate visual
reasoning, you must output the "reason" key FIRST, followed by the "category" key.
```

## Expected Model Output

```json
{
  "reason": "Extreme-density geometric cargo terminals and massive vessel berthing.",
  "category": "HIGH"
}
```

For instructions on generating the dataset, see the [Data Generation guide](../guides/data-gen.md).
