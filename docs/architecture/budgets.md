# Mission Budgets

Quantitative characterization of ORION's resource usage per orbit. Based on measured telemetry from Pi 5 deployment at ~800 km SSO.

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

| Stage                       | Duration    | Source                          |
| --------------------------- | ----------- | ------------------------------- |
| SimSat HTTP image fetch     | ~100-500 ms | Network dependent (LAN)         |
| Vision encoding (mtmd)      | ~10-15 s    | Included in inference total     |
| Token generation (200 max)  | ~40-55 s    | Greedy sampling, 4 threads      |
| JSON parse + triage routing | < 1 ms      | Negligible                      |
| **Total per frame**         | **50-70 s** | Measured from Pi telemetry logs |

Inference timeout is set at 120 seconds.

## Data Budget (Per Orbit)

| Metric                       | Value                  | Derivation                                                                                       |
| ---------------------------- | ---------------------- | ------------------------------------------------------------------------------------------------ |
| MEASURE window               | ~35 min (eclipse)      | Orbital parameters                                                                               |
| Capture interval             | 65 s (minimum)         | `MIN_CAPTURE_INTERVAL` in CameraManager                                                          |
| Frames captured per orbit    | ~32 frames             | 35 min / 65 s                                                                                    |
| Frames inferred per orbit    | ~32 frames             | All captured frames; inference (~60 s) ≈ capture interval (65 s), 5-frame queue absorbs overflow |
| Frames dropped per orbit     | ~0                     | 5-frame queue depth means no frames are lost under normal inference timing                       |
| Raw data per frame           | 786,432 bytes (768 KB) | 512 x 512 x 3 RGB                                                                                |
| Raw data generated per orbit | ~24 MB                 | 32 frames × 768 KB                                                                               |

### Triage Distribution (Expected)

Based on target morphology distribution (71% of Earth is ocean):

| Verdict             | Expected ratio | Data per orbit           | Action                         |
| ------------------- | -------------- | ------------------------ | ------------------------------ |
| LOW                 | ~60-70%        | 0 bytes (discarded)      | Buffer recycled                |
| MEDIUM              | ~20-30%        | ~1.5-2.3 MB (stored)     | Written to disk                |
| HIGH                | ~5-10%         | ~384-768 KB (downlinked) | Transmitted during comm window |
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
| HIGH queue (disk)   | ~384-768 KB per orbit | Flushed each comm window                 |
| MEDIUM storage      | ~1.5-2.3 MB per orbit | Accumulates until `FLUSH_MEDIUM_STORAGE` |
| MEDIUM per day      | ~12-28 MB             | 8-12 orbits with eclipses                |
| Pi microSD capacity | 32-128 GB typical     | Months of MEDIUM storage                 |

## Power Budget (Timing)

This section documents the compute duty cycle.

| Phase              | Duration per orbit        | Activity                                                      |
| ------------------ | ------------------------- | ------------------------------------------------------------- |
| IDLE (sunlit)      | ~66 min                   | NavTelemetry polling only. Model unloaded. Charging.          |
| MEASURE (eclipse)  | ~35 min                   | VLM loaded (~730 MB RAM). Captures + inference.               |
| VLM active time    | ~32 min of MEASURE        | 32 frames × ~60 s inference; nearly continuous during eclipse |
| VLM idle time      | ~3 min of MEASURE         | ~5 s gap per capture cycle × 32 cycles                        |
| DOWNLINK           | ~3-6 min (if pass occurs) | Queue flush. Model stays loaded.                              |
| **VLM duty cycle** | **~32%** of orbit         | 32 min inference / ~101 min orbit                             |

## Memory Budget

```
Total Pi 5 RAM:      4,096 MB
GGUF text model:      ~730 MB (Q4_K_M, loaded in MEASURE)
mmproj vision enc:    ~854 MB (F16, loaded with model)
KV cache (4096 ctx):   ~64 MB (allocated per inference, cleared after)
Image buffer pool:     ~16 MB (20 x 786 KB, pre-allocated at startup)
F-Prime framework:     ~20 MB (all components, rate groups, queues)
Linux + overhead:     ~200 MB
---------------------------------
Total in MEASURE:   ~1,884 MB
Total in IDLE:       ~236 MB (model unloaded)
Available headroom: ~2,212 MB (MEASURE) / ~3,860 MB (IDLE)
```

No runtime dynamic allocation is used in the ORION pipeline. The buffer pool is pre-allocated at startup, and the model weights are loaded once into RAM when entering MEASURE mode.
