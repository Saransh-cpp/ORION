# Mission Budgets

Quantitative characterization of ORION's resource usage per orbit. Based on measured telemetry from Pi 5 deployment at ~800 km SSO.

Unless noted otherwise, all compute numbers in the budget tables below (inference time, duty cycle, frames per orbit) are derived from the pooled average across 3 end-to-end Pi 5 simulation runs (1,443 frames, ~33 hours total). Per-run breakdowns and methodology are in the [Cross-Run Comparison](#cross-run-comparison) section.

## Orbital Parameters

| Parameter             | Value                 | Source                              |
| --------------------- | --------------------- | ----------------------------------- |
| Altitude              | ~800 km               | SimSat telemetry (measured ~806 km) |
| Orbit type            | Sun-synchronous (SSO) | SimSat configuration                |
| Orbital period        | ~101 minutes          | Computed from altitude              |
| Eclipse duration      | ~35 minutes per orbit | ~35% of orbit in Earth's shadow     |
| Sunlit duration       | ~66 minutes per orbit | Remaining ~65%                      |
| Ground track velocity | ~7.45 km/s            | Computed from altitude              |

## Timing Budget (Per Frame)

Measured on Raspberry Pi 5 (Cortex-A76 quad-core, CPU-only):

| Stage                       | Duration    | Source                                                                                  |
| --------------------------- | ----------- | --------------------------------------------------------------------------------------- |
| SimSat HTTP image fetch     | ~100-500 ms | Network dependent (LAN)                                                                 |
| Vision encoding (mtmd)      | ~10-15 s    | Included in inference total                                                             |
| Token generation (200 max)  | ~40-55 s    | Greedy sampling, 4 threads                                                              |
| JSON parse + triage routing | < 1 ms      | Negligible                                                                              |
| **Total per frame**         | **51-82 s** | Measured from Pi telemetry logs (mean ~69 s across 1,443 frames from 3 end-to-end runs) |

Inference timeout is set at 120 seconds.

## Data Budget (Per Orbit)

| Metric                       | Value                  | Derivation                                                                                     |
| ---------------------------- | ---------------------- | ---------------------------------------------------------------------------------------------- |
| MEASURE window               | ~35 min (eclipse)      | Orbital parameters                                                                             |
| Capture interval             | 85 s (minimum)         | `MIN_CAPTURE_INTERVAL` in CameraManager                                                        |
| Frames captured per orbit    | ~24 frames             | 35 min / 85 s                                                                                  |
| Frames inferred per orbit    | ~24 frames             | All captured frames; inference (~69 s avg) < capture interval (85 s), queue stays at depth 0-1 |
| Frames dropped per orbit     | ~0                     | 5-frame queue depth means no frames are lost under normal inference timing                     |
| Raw data per frame           | 786,432 bytes (768 KB) | 512 x 512 x 3 RGB                                                                              |
| Raw data generated per orbit | ~18 MB                 | 24 frames × 768 KB                                                                             |

### Triage Distribution (Expected)

> Check Measured Results per run at the end of this page for real numbers.

Based on target morphology distribution (71% of Earth is ocean):

| Verdict             | Expected ratio | Data per orbit           | Action                         |
| ------------------- | -------------- | ------------------------ | ------------------------------ |
| LOW                 | ~60-70%        | 0 bytes (discarded)      | Buffer recycled                |
| MEDIUM              | ~20-30%        | ~3.8-5.4 MB (stored)     | Written to disk                |
| HIGH                | ~5-10%         | ~0.8-1.5 MB (downlinked) | Transmitted during comm window |
| **Bandwidth saved** | **~90-95%**    |                          | vs. downlinking all frames     |

## Link Budget (Per Comm Window)

| Parameter                      | Value                            | Derivation                                              |
| ------------------------------ | -------------------------------- | ------------------------------------------------------- |
| Ground station                 | EPFL Ecublens (46.52N, 6.57E)    | Default configuration                                   |
| Comm window radius             | 2000 km (enter) / 2200 km (exit) | 10% hysteresis                                          |
| Orbital velocity               | ~7.45 km/s                       | v = sqrt(GM/r), r = 6371 + 800 km                       |
| Ground track velocity          | ~6.66 km/s                       | Adjusted for Earth's rotation                           |
| Max comm window (zenith pass)  | ~10 min                          | 2 x 2000 km / 6.66 km/s = 601 s                         |
| Typical comm window            | ~5-8 min                         | Non-zenith passes traverse a shorter chord              |
| Low-elevation pass             | ~2-3 min                         | Satellite barely enters the 2000 km circle              |
| Passes per day over EPFL       | ~2-4                             | Depends on SSO LTAN; not every orbit passes over Europe |
| Frame TX time (WiFi LAN)       | ~1-2 s per frame                 | 786 KB over WiFi; real X-band would differ              |
| Typical HIGH frames per window | ~1-2 frames                      | Most frames are LOW/MEDIUM                              |

In practice, the comm window is far wider than needed for HIGH frame downlink. The bottleneck is the VLM inference rate, not the radio link.

## Storage Budget

| Storage             | Rate                  | Retention                                |
| ------------------- | --------------------- | ---------------------------------------- |
| HIGH queue (disk)   | ~0.8-1.5 MB per orbit | Flushed each comm window                 |
| MEDIUM storage      | ~3.8-5.4 MB per orbit | Accumulates until `FLUSH_MEDIUM_STORAGE` |
| MEDIUM per day      | ~30-65 MB             | 8-12 orbits with eclipses                |
| Pi microSD capacity | 32-128 GB typical     | Months of MEDIUM storage                 |

## Power Budget (Timing)

This section documents the compute duty cycle.

| Phase              | Duration per orbit        | Activity                                                      |
| ------------------ | ------------------------- | ------------------------------------------------------------- |
| IDLE (sunlit)      | ~66 min                   | NavTelemetry polling only. Model unloaded. Charging.          |
| MEASURE (eclipse)  | ~35 min                   | VLM loaded (~1.75 GB RSS measured). Captures + inference.     |
| VLM active time    | ~28 min of MEASURE        | 24 frames × ~69 s inference; nearly continuous during eclipse |
| VLM idle time      | ~7 min of MEASURE         | ~16 s gap per capture cycle × 24 cycles                       |
| DOWNLINK           | ~3-6 min (if pass occurs) | Queue flush. Model stays loaded.                              |
| **VLM duty cycle** | **~27%** of orbit         | 28 min inference / ~101 min orbit                             |

## Memory Budget

```
Total Pi 5 RAM:      8,192 MB
GGUF text model:      ~730 MB (Q4_K_M, loaded in MEASURE)
mmproj vision enc:    ~814 MB (F16, loaded with model)
KV cache (4096 ctx):   ~64 MB (allocated per inference, cleared after)
Image buffer pool:     ~16 MB (20 x 786 KB, pre-allocated at startup)
F-Prime framework:     ~20 MB (all components, rate groups, queues)
Linux + overhead:     ~200 MB
---------------------------------
Total in MEASURE:   ~1,844 MB (estimate)
                    ~1,753 MB (measured RSS on Pi 5)
Total in IDLE:       ~236 MB (model unloaded)
Available headroom: ~6,439 MB (MEASURE) / ~7,956 MB (IDLE)
```

No runtime dynamic allocation is used in the ORION pipeline. The buffer pool is pre-allocated at startup, and the model weights are loaded once into RAM when entering MEASURE mode.

## Measured Results: Run 1 (10h 23m Pi 5 Run, 2026-05-07)

Single continuous MEASURE session on Raspberry Pi 5 (no eclipse cycling — `SET_ECLIPSE` issued once at start). The satellite was not in range of the ground station (EPFL) for ~11 hours; one comm window occurred near the end of the run. Raw event log: [`flight_segment/orion/logs/2026_05_06-23_28_57/event.log`](https://github.com/Saransh-cpp/ORION/tree/main/flight_segment/orion/logs/run_1_2026_05_06-23_28_57/event.log).

### Run Parameters

| Parameter          | Value                              |
| ------------------ | ---------------------------------- |
| Run duration       | 10h 23m (23:28 – 09:52 UTC+2)      |
| MEASURE duration   | ~11h 50m (continuous)              |
| Comm windows       | 1 (10m 15s, distance 1982 km open) |
| Model load time    | ~21 s                              |
| Inference failures | 0                                  |
| Inference timeouts | 0                                  |
| Frames dropped     | 0                                  |
| Capture failures   | 0                                  |

### Inference Timing (501 frames)

| Metric | Value            |
| ------ | ---------------- |
| Min    | 52.9 s           |
| Max    | 81.6 s           |
| Mean   | 71.7 s           |
| Total  | 598 min (9h 58m) |

### Triage Distribution (501 frames, 384 MB total)

| Verdict             | Count | Ratio     | Data                | Action                                 |
| ------------------- | ----- | --------- | ------------------- | -------------------------------------- |
| LOW                 | 476   | 95.0%     | 0 bytes             | Buffer recycled                        |
| MEDIUM              | 23    | 4.6%      | 17.3 MB (stored)    | Written to microSD                     |
| HIGH                | 2     | 0.4%      | 1.5 MB (downlinked) | Queued to disk, flushed on comm window |
| **Bandwidth saved** |       | **95.0%** |                     | vs. downlinking every frame            |

HIGH + MEDIUM combined: 25 frames (5.0% of total). 99.6% of data was discarded or deferred vs. blind downlink.

### Downlink

- 2 HIGH frames queued to disk (outside comm window), flushed automatically on comm window open.
- 23 MEDIUM files bulk-downloaded via `FLUSH_MEDIUM_STORAGE` during comm window.
- Comm window duration (10m 15s) was more than sufficient for all queued data.

Downlinked images: [HIGH Run 1 (X-band)](https://github.com/Saransh-cpp/ORION/tree/main/ground_segment/data/downlinked_XBand_run_1/), [MEDIUM Run 1 (UHF)](https://github.com/Saransh-cpp/ORION/tree/main/ground_segment/data/downlinked_UHF_run_1/fprime-downlink/).

## Measured Results: Run 2 (9h 39m Pi 5 Run, 2026-05-07)

Single continuous MEASURE session on Raspberry Pi 5 (no eclipse cycling). Two comm windows occurred during the run. Raw event log: [`flight_segment/orion/logs/2026_05_07-12_10_33/event.log`](https://github.com/Saransh-cpp/ORION/tree/main/flight_segment/orion/logs/run_2_2026_05_07-12_10_33/event.log).

### Run Parameters

| Parameter          | Value                                    |
| ------------------ | ---------------------------------------- |
| Run duration       | 9h 39m (12:10 – 21:49 UTC+2)             |
| MEASURE duration   | ~9h 39m (continuous)                     |
| Comm windows       | 2 (7m 50s at 1984 km; 6m 50s at 1981 km) |
| Model load time    | ~0.5 s (page cache; cold load is ~21 s)  |
| Inference failures | 0                                        |
| Inference timeouts | 0                                        |
| Frames dropped     | 0                                        |
| Capture failures   | 0                                        |

### Inference Timing (396 frames)

| Metric | Value            |
| ------ | ---------------- |
| Min    | 53.2 s           |
| Max    | 77.5 s           |
| Mean   | 69.4 s           |
| Total  | 458 min (7h 38m) |

### Triage Distribution (396 frames, 297 MB total)

| Verdict             | Count | Ratio     | Data                | Action                                 |
| ------------------- | ----- | --------- | ------------------- | -------------------------------------- |
| LOW                 | 379   | 95.7%     | 0 bytes             | Buffer recycled                        |
| MEDIUM              | 15    | 3.8%      | 11.2 MB (stored)    | Written to microSD                     |
| HIGH                | 2     | 0.5%      | 1.5 MB (downlinked) | Queued to disk, flushed on comm window |
| **Bandwidth saved** |       | **95.7%** |                     | vs. downlinking every frame            |

HIGH + MEDIUM combined: 17 frames (4.3% of total). 99.5% of data was discarded or deferred vs. blind downlink.

### Downlink

- 2 HIGH frames queued to disk (outside comm window), flushed automatically when comm window 2 opened.
- 15 MEDIUM files bulk-downloaded via `FLUSH_MEDIUM_STORAGE` during comm window 2.
- 2 comm windows totaling ~14m 40s, more than sufficient for all queued data.
- 1 MEDIUM file (`orion_medium_00006.raw` in run 2) arrived truncated at 785,955 bytes (477 bytes short of expected 786,432). Cause: partial F-Prime FileDownlink transfer, likely due to ComQueue contention on the shared TCP :50000 link during the MEDIUM flush. Transport-layer issue, not a triage pipeline fault. File transfer success rate across Run 2: 14/15 (93.3%).

Downlinked images: [HIGH Run 2 (X-band)](https://github.com/Saransh-cpp/ORION/tree/main/ground_segment/data/downlinked_XBand_run_2/), [MEDIUM Run 2 (UHF)](https://github.com/Saransh-cpp/ORION/tree/main/ground_segment/data/downlinked_UHF_run_2/fprime-downlink/).

## Measured Results: Run 3 (13h 17m Pi 5 Run, 2026-05-07/08)

Single continuous MEASURE session on Raspberry Pi 5 (no eclipse cycling). Two comm windows occurred during the run. Raw event log: [`flight_segment/orion/logs/run_3_2026_05_07-22_47_57/event.log`](https://github.com/Saransh-cpp/ORION/tree/main/flight_segment/orion/logs/run_3_2026_05_07-22_47_57/event.log).

### Run Parameters

| Parameter          | Value                                      |
| ------------------ | ------------------------------------------ |
| Run duration       | 13h 17m (22:47 May 7 – 12:04 May 8, UTC+2) |
| MEASURE duration   | ~13h 17m (continuous)                      |
| Comm windows       | 2 (10m 25s at 1979 km; 9m 20s at 1996 km)  |
| Model load time    | ~0.5 s (page cache; cold load is ~21 s)    |
| Inference failures | 0                                          |
| Inference timeouts | 0                                          |
| Frames dropped     | 0                                          |
| Capture failures   | 0                                          |

### Inference Timing (546 frames)

| Metric | Value             |
| ------ | ----------------- |
| Min    | 51.4 s            |
| Max    | 71.7 s            |
| Mean   | 66.3 s            |
| Total  | 603 min (10h 03m) |

### Triage Distribution (546 frames, 410 MB total)

| Verdict             | Count | Ratio     | Data                | Action                                 |
| ------------------- | ----- | --------- | ------------------- | -------------------------------------- |
| LOW                 | 532   | 97.4%     | 0 bytes             | Buffer recycled                        |
| MEDIUM              | 10    | 1.8%      | 7.5 MB (stored)     | Written to microSD                     |
| HIGH                | 4     | 0.7%      | 3.0 MB (downlinked) | Queued to disk, flushed on comm window |
| **Bandwidth saved** |       | **97.4%** |                     | vs. downlinking every frame            |

HIGH + MEDIUM combined: 14 frames (2.6% of total). 97.4% of data was discarded on-board vs. blind downlink.

### Downlink

- 4 HIGH frames queued to disk (outside comm window), flushed automatically when comm window 2 opened.
- 10 MEDIUM files bulk-downloaded via `FLUSH_MEDIUM_STORAGE` during comm window 2.
- 2 comm windows totaling ~19m 45s — more than sufficient for all queued data.

Downlinked images: [HIGH Run 3 (X-band)](https://github.com/Saransh-cpp/ORION/tree/main/ground_segment/data/downlinked_XBand_run_3/), [MEDIUM Run 3 (UHF)](https://github.com/Saransh-cpp/ORION/tree/main/ground_segment/data/downlinked_UHF_run_3/fprime-downlink/).

## Cross-Run Comparison

All runs are continuous MEASURE sessions (no eclipse cycling, for stress-testing) on the same Raspberry Pi 5 hardware (Cortex-A76, CPU-only, 8 GB RAM). In real orbital operations, MEASURE runs only during the ~35-min eclipse window per ~101-min orbit; continuous MEASURE is more demanding since the VLM never gets an IDLE rest period. Per-frame metrics (inference time, triage distribution) are unaffected by this difference; per-orbit numbers in the budget tables above are derived from the 35-min eclipse assumption, not from raw run duration.

The "Average (pooled)" column treats all frames across runs as a single dataset: triage counts are summed and percentages recomputed from the total, mean inference time is weighted by frame count, and min/max are the extremes across all runs. This avoids giving equal weight to runs of different lengths.

### Inference Timing

| Metric         | Run 1 (10h 23m) | Run 2 (9h 39m) | Run 3 (13h 17m) | Average (pooled) |
| -------------- | --------------- | -------------- | --------------- | ---------------- |
| Frames         | 501             | 396            | 546             | 1,443            |
| Min            | 52.9 s          | 53.2 s         | 51.4 s          | 51.4 s           |
| Max            | 81.6 s          | 77.5 s         | 71.7 s          | 81.6 s           |
| Mean           | 71.7 s          | 69.4 s         | 66.3 s          | 69.0 s           |
| Total run time | 10h 23m         | 9h 39m         | 13h 17m         | 33h 19m          |

### Triage Distribution

| Verdict             | Run 1       | Run 2       | Run 3       | Average (pooled) |
| ------------------- | ----------- | ----------- | ----------- | ---------------- |
| LOW                 | 476 (95.0%) | 379 (95.7%) | 532 (97.4%) | 1,387 (96.1%)    |
| MEDIUM              | 23 (4.6%)   | 15 (3.8%)   | 10 (1.8%)   | 48 (3.3%)        |
| HIGH                | 2 (0.4%)    | 2 (0.5%)    | 4 (0.7%)    | 8 (0.6%)         |
| **Bandwidth saved** | **95.0%**   | **95.7%**   | **97.4%**   | **96.1%**        |

### Reliability

| Metric             | Run 1 | Run 2 | Run 3 | Total |
| ------------------ | ----- | ----- | ----- | ----- |
| Inference failures | 0     | 0     | 0     | 0     |
| Inference timeouts | 0     | 0     | 0     | 0     |
| Frames dropped     | 0     | 0     | 0     | 0     |
| Capture failures   | 0     | 0     | 0     | 0     |
| Files truncated    | 0     | 1     | 0     | 1     |
| Comm windows       | 1     | 2     | 2     | 5     |
