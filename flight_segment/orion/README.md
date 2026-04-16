# Orion Software Design Document

## 1. Architectural Philosophy

The system is designed around **Asynchronous Event-Driven Inference** and strict static memory allocation. The core flight executive and hardware interfaces must never wait on the Vision-Language Model (VLM). The architecture isolates the heavy machine learning workload into a dedicated Active Component.

To satisfy flight software constraints, all dynamic memory allocation is avoided during runtime. The system utilizes a statically allocated buffer pool. Assuming a target resolution of 512x512 RGB (3 bytes/pixel), each image requires exactly 786,432 bytes (~786 KB). A pool of 20 buffers consumes approximately 15.7 MB of RAM, leaving the Pi 5's remaining memory dedicated entirely to the VLM's `.gguf` weights and KV cache.

## 2. Component Dictionary

### A. `CameraManager` (Active Component)

- **Responsibility:** Interfaces directly with the Pi Camera Module payload.
- **Behavior:** Receives commands to capture an image. It requests an empty 786 KB memory block from the `BufferManager`, streams the raw sensor data into it, fetches the current location, dispatches the payload downstream, and immediately frees its thread.

### B. `NavTelemetry` (Passive Component)

- **Responsibility:** Acts as the satellite's definitive source of truth for location.
- **Behavior:** Continuously ingests state vectors. Because it is a Passive Component, other components can call it synchronously to get the exact Latitude/Longitude with near-zero latency.

### C. `VlmInferenceEngine` (Active Component)

- **Responsibility:** Manages image preprocessing and executes the neural network using the Pi 5's ARM CPU. The LFM2.5-VL-1.6B model was successfully converted to Q4 GGUF format using llama.cpp's convert tooling, confirming architecture compatibility with the inference backend.
- **Behavior:** Runs on an isolated, low-priority thread holding the 700MB `.gguf` file in RAM. It executes a strict two-step process:
  1. **Preprocessing:** Normalizes and flattens the native 512x512 RGB buffer into the specific tensor layout expected by `llama.cpp`'s vision encoder.
  2. **Inference:** Constructs the text prompt, executes the forward pass, and parses the JSON to emit the final category and reasoning string.
- **Critical Feature:** Must respond to F-Prime `Ping` requests between inference steps so the system Health Watchdog knows the thread is computing, not deadlocked.

### D. `TriageRouter` (Active Component)

- **Responsibility:** The decision-making hub that executes the ORION priority doctrine.
- **Behavior:** Analyzes the VLM's output. It controls what utilizes the limited radio bandwidth, what goes to bulk storage, and what gets permanently deleted to recycle buffers.

### E. `GroundCommsDriver` (Active Component)

- **Responsibility:** Manages the X-Band / S-Band radio hardware (simulated via Wi-Fi/Ethernet on the Pi).
- **Behavior:** Drains the downlink queues and transmits telemetry, events, and high-priority image buffers to the ground station.

### F. `BufferManager` (Standard F-Prime Component)

- **Responsibility:** Prevents memory leaks and RAM fragmentation.
- **Behavior:** Initializes and maintains a static pool of 20 fixed-size (786 KB) buffers. Components pass _pointers_ to these buffers. Once the `TriageRouter` resolves an image, it signals the `BufferManager` to recycle the memory slot.

---

## 3. Port Interfaces (The "Wires")

Components communicate strictly through typed Ports to ensure deterministic data handoffs.

| Port Name                  | Type         | Data Payload (Arguments)                                    | Purpose                                                                                  |
| :------------------------- | :----------- | :---------------------------------------------------------- | :--------------------------------------------------------------------------------------- |
| **`NavStatePort`**         | Synchronous  | _Returns:_ `Latitude`, `Longitude`                          | Used by the Camera to pull location instantly at the moment of capture.                  |
| **`InferenceRequestPort`** | Asynchronous | `ImageBuffer` (ptr), `Latitude`, `Longitude`                | Carries payload to the VLM. Async ensures the camera doesn't wait on the neural network. |
| **`TriageDecisionPort`**   | Asynchronous | `PriorityEnum` (0,1,2), `ReasonString`, `ImageBuffer` (ptr) | Carries the VLM's final verdict to the Router.                                           |
| **`FileDownlinkPort`**     | Asynchronous | `ImageBuffer` (ptr), `ReasonString`                         | Carries High-Priority data from the Router to the Radio/Ground Comms.                    |

---

## 4. Sequence of Operations (The Data Flow)

1. **Capture:** An automated sequencer triggers the `CameraManager`.
2. **Memory Checkout:** The `CameraManager` pulls an empty 786 KB buffer from the `BufferManager` and writes the raw pixel data.
3. **Sensor Fusion:** The `CameraManager` makes a synchronous call to `NavTelemetry` for the exact GPS coordinates.
4. **Dispatch:** The `CameraManager` packages the Buffer pointer, Lat, and Lon, fires it across the `InferenceRequestPort`, and goes back to sleep.
5. **Preprocessing:** The `VlmInferenceEngine` pops the request and normalizes the raw pixel data for the vision encoder.
6. **Inference:** The `VlmInferenceEngine` executes `llama.cpp`.
7. **Routing:** The VLM sends the parsed result to the `TriageRouter`, executing the doctrine:
   - **`HIGH`:** Router sends the image buffer and reasoning string to the `GroundCommsDriver` for immediate X-Band downlink. Emits a `CRITICAL` anomaly event.
   - **`MEDIUM`:** Routes the image buffer to the Pi's microSD card for standard bulk downlink later. Emits an `INFO` event.
   - **`LOW`:** Drops the reference entirely. The `BufferManager` instantly recycles the memory.

---

## 5. Ground Communication Strategy

The Ground Data System (GDS) manages the VLM payload via standard F-Prime communication channels, explicitly supporting fault recovery and safe-mode operations.

### Commands (Uplink)

- `CMD_LOAD_GGUF_MODEL`: Commands the VLM component to load the 700MB file from mass storage into RAM.
- `CMD_UNLOAD_MODEL`: Flushes the model from RAM. Critical fault-recovery command if the satellite must enter "Safe Mode" to shed power or memory overhead.
- `CMD_TRIGGER_CAPTURE`: Manually forces a localized image capture outside of the automated sequence.

### Telemetry (Downlink - Continuous Health)

- `VlmInferenceTime_Ms`: Tracks exact CPU execution time per image to monitor thermal throttling.
- `HighTargetsFound_Count`: Running total of strategic anomalies detected.
- `BufferPool_Usage`: Tracks current occupancy of the 20-slot image buffer pool.

### Events (Downlink - System Logs)

- _(EVR-CRITICAL)_: `"ORION PRIORITY: HIGH Target Detected. Reason: [VLM Text]"`
- _(EVR-WARNING)_: `"ORION INFERENCE FAILED: JSON Decode Error."`
- _(EVR-INFO)_: `"ORION PRIORITY: LOW. Buffer discarded."`

---

## 6. Operational Throughput & Hardware Constraints

To operate on COTS (Commercial Off-The-Shelf) edge hardware, this architecture defines strict operational boundaries based on physical hardware capabilities:

- **Inference Latency:** The Raspberry Pi 5 utilizes a Cortex-A76 ARM processor without a dedicated Neural Processing Unit (NPU) or discrete GPU. Processing a 1.6B parameter model in Q4 quantization relies entirely on CPU execution. The expected latency from image ingestion to JSON output is **15 to 45 seconds per image**.
- **Orbital Pass Parameters:** A typical Low Earth Orbit (LEO) flyover of a target zone lasts between 8 to 10 minutes (480–600 seconds). Given the 45-second inference ceiling, the ORION pipeline has a maximum operational throughput of **~12 to 13 classifications per LEO pass**.
- **Sequencer Pacing:** To prevent exhausting the 20-slot buffer pool, ground sequencers must pace `CMD_CAPTURE` commands accordingly. If capture frequency exceeds the 12-13 image baseline per pass, the buffer pool will saturate, and new captures will be dropped until the VLM clears the backlog.

---

This is officially locked and loaded. When you're ready to cross the bridge into implementation, we can start scaffolding the actual F-Prime C++ workspace on that Pi!
