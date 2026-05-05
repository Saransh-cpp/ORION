# Orion::VlmInferenceEngine Component

## 1. Introduction

The `Orion::VlmInferenceEngine` component runs the LFM2.5-VL-1.6B vision-language model on the satellite's CPU. It receives raw 512x512 RGB image frames from [CameraManager](../camera-manager/), constructs a ChatML-formatted prompt with fused GPS coordinates, executes the llama.cpp forward pass, parses the JSON output into a triage verdict (HIGH/MEDIUM/LOW), and emits the result to [TriageRouter](../triage-router/).

The model is a ~730 MB Q4_K_M GGUF file loaded into RAM on demand (total process RSS measured at ~1,753 MB on the 8 GB Pi 5). Inference takes 50-80 seconds per frame on the Pi 5's Cortex-A76 cores (CPU-only, no GPU). The component runs on a dedicated low-priority thread to avoid blocking other flight software.

## 2. Requirements

| Requirement   | Description                                                                                              | Verification Method |
| ------------- | -------------------------------------------------------------------------------------------------------- | ------------------- |
| ORION-VLM-001 | VlmInferenceEngine shall load the GGUF model and mmproj vision encoder on MEASURE entry                  | System test         |
| ORION-VLM-002 | VlmInferenceEngine shall unload the model on transition to IDLE or SAFE                                  | System test         |
| ORION-VLM-003 | VlmInferenceEngine shall keep the model loaded during DOWNLINK (short pass, reload is expensive)         | Inspection          |
| ORION-VLM-004 | VlmInferenceEngine shall classify each frame as HIGH, MEDIUM, or LOW and emit the result to TriageRouter | System test         |
| ORION-VLM-005 | VlmInferenceEngine shall return buffers to the pool on inference failure                                 | Inspection          |
| ORION-VLM-006 | VlmInferenceEngine shall drop all frames in SAFE mode without inference                                  | System test         |
| ORION-VLM-007 | VlmInferenceEngine shall abort and recover from any inference exceeding `INFERENCE_TIMEOUT_S` (120 s)    | System test         |

## 3. Design

### 3.1 Data Flow

```mermaid
flowchart LR
    CM[CameraManager] -->|buffer + lat/lon| VLM[VlmInferenceEngine]
    VLM -->|verdict + reason + buffer| TR[TriageRouter]
    VLM -->|buffer return on failure| BM[BufferManager]
    EA[EventAction] -->|modeChangeIn| VLM
```

### 3.2 Inference Pipeline

For each frame, `runInference()` executes the following stages:

1. **Prompt construction**: ChatML format matching the fine-tuning template:

   ```
   <|im_start|>user
   <image>
   You are an autonomous orbital triage assistant...
   captured at Longitude: X, Latitude: Y.
   ...<|im_end|>
   <|im_start|>assistant
   ```

2. **Image encoding**: `mtmd_bitmap_init()` wraps the raw RGB buffer, `mtmd_tokenize()` replaces the image marker with vision encoder tokens

3. **KV cache evaluation**: `mtmd_helper_eval_chunks()` processes all prompt chunks (text + vision tokens) into the context. Timeout checked after eval.

4. **Autoregressive generation**: Greedy sampling up to `MAX_RESPONSE_TOKENS` (200 tokens), stopping on EOG token. Timeout checked per token.

5. **KV cache reset**: `llama_memory_clear()` and `llama_sampler_reset()` prepare for the next frame

6. **JSON parsing**: `parseVerdictJson()` extracts `"category"` and `"reason"` from the response

### 3.3 Inference Timeout

A self-watchdog checks elapsed time at two points during inference:

- **After prompt eval**: catches cases where vision encoding + context evaluation exceeds the limit
- **Per token in generation loop**: catches slow or stuck token generation

If elapsed time exceeds `INFERENCE_TIMEOUT_S` (120s), the inference is aborted: KV cache is cleared, sampler is reset, `InferenceTimeout` event is logged, and the frame is dropped. The model stays loaded and ready for the next frame; hence, no restart required.

### 3.4 JSON Parser

The model is fine-tuned to output:

```json
{ "reason": "Dense geometric infrastructure...", "category": "HIGH" }
```

The parser:

- Searches the entire response for `"HIGH"`, `"MEDIUM"` (case-sensitive, quoted) to determine category. Defaults to LOW if neither found.
- Finds the `"reason"` key, extracts the string value while handling escaped quotes
- Falls back to the raw response as the reason if no `"reason"` key is found
- Falls back to `"Empty model response"` if the response is empty

### 3.5 Model Lifecycle

```mermaid
stateDiagram-v2
    [*] --> Unloaded
    Unloaded --> Loaded : MEASURE entry / LOAD_MODEL cmd
    Loaded --> Unloaded : IDLE or SAFE entry / UNLOAD_MODEL cmd
    Loaded --> Loaded : DOWNLINK entry (stays loaded)
```

The model auto-loads on MEASURE entry and auto-unloads on IDLE or SAFE entry. During DOWNLINK, the model stays loaded to avoid the ~15s reload penalty on the Pi. Manual `LOAD_MODEL` and `UNLOAD_MODEL` commands are available for ground control.

### 3.6 Port Diagram

| Port                 | Direction   | Type                   | Description                                                    |
| -------------------- | ----------- | ---------------------- | -------------------------------------------------------------- |
| `inferenceRequestIn` | async input | `InferenceRequestPort` | Receives image buffer + GPS from CameraManager (queue depth 5) |
| `modeChangeIn`       | async input | `ModeChangePort`       | Receives mode broadcasts from EventAction                      |
| `triageDecisionOut`  | output      | `TriageDecisionPort`   | Emits verdict + reason + buffer to TriageRouter                |
| `bufferReturnOut`    | output      | `Fw.BufferSend`        | Returns buffer to pool on inference failure                    |

### 3.7 Commands

| Command        | Opcode | Behavior                                                                                           |
| -------------- | ------ | -------------------------------------------------------------------------------------------------- |
| `LOAD_MODEL`   | 0x00   | Loads GGUF text model + mmproj vision encoder. Idempotent. Rejected if not in MEASURE or DOWNLINK. |
| `UNLOAD_MODEL` | 0x01   | Frees all llama.cpp state from RAM.                                                                |

### 3.8 Events

| Event                        | Severity    | Description                                                                 |
| ---------------------------- | ----------- | --------------------------------------------------------------------------- |
| `ModelLoaded`                | ACTIVITY_HI | Model and vision encoder loaded into RAM                                    |
| `ModelUnloaded`              | ACTIVITY_HI | Model freed from RAM                                                        |
| `ModelLoadFailed`            | WARNING_HI  | GGUF file or mmproj failed to load (with path)                              |
| `InferenceFailed`            | WARNING_HI  | Tokenization, eval, or generation failed for a frame                        |
| `FrameDroppedModelNotLoaded` | WARNING_LO  | Frame arrived but model not loaded - buffer returned                        |
| `LoadModelRejectedWrongMode` | WARNING_LO  | LOAD_MODEL rejected - not in MEASURE or DOWNLINK                            |
| `InferenceTimeout`           | WARNING_HI  | Inference exceeded `INFERENCE_TIMEOUT_S`; frame dropped, model stays loaded |
| `InferenceComplete`          | ACTIVITY_HI | Successful classification with category, reason, and time in ms             |

### 3.9 Telemetry

| Channel             | Type | Description                                       |
| ------------------- | ---- | ------------------------------------------------- |
| `InferenceTime_Ms`  | U32  | Wall-clock time of the most recent inference pass |
| `TotalInferences`   | U32  | Running total of successful classifications       |
| `InferenceFailures` | U32  | Running total of failed inference attempts        |

### 3.10 llama.cpp Integration

The inference engine uses the [llama.cpp](https://github.com/ggml-org/llama.cpp) C API to run the quantized VLM entirely on CPU. The integration involves three layers:

**Static linking.** llama.cpp is built as a set of static libraries (`libllama.a`, `libmtmd.a`, `libggml.a`, `libggml-base.a`, `libggml-cpu.a`) from the `ground_segment/llama.cpp` submodule. The component's `CMakeLists.txt` links these directly into the F-Prime binary. On macOS, Metal and Accelerate frameworks are additionally linked for GPU/BLAS acceleration; on Linux/Pi 5, OpenMP (`gomp`) provides CPU parallelism.

**Header vendoring.** llama.cpp's public headers (`llama.h`, `mtmd.h`, `mtmd-helper.h`) are included directly from the submodule source tree via `include_directories()` in CMake. The headers define opaque struct types (`llama_model`, `llama_context`, `mtmd_context`, `llama_sampler`) that the component forward-declares in its own `.hpp` to avoid exposing llama.cpp includes to F-Prime's autocoded headers. A `DEPRECATED` macro conflict between F-Prime and llama.cpp is resolved with `#pragma push_macro` / `#pragma pop_macro` in the `.cpp` file.

**API surface used.** The component uses four llama.cpp subsystems:

| Subsystem        | Key functions                                                                         | Purpose                                                                  |
| ---------------- | ------------------------------------------------------------------------------------- | ------------------------------------------------------------------------ |
| Model loading    | `llama_model_load_from_file`, `llama_context_init_from_model`                         | Load GGUF text model and create inference context                        |
| Vision encoder   | `mtmd_init_from_file`, `mtmd_bitmap_init`, `mtmd_tokenize`, `mtmd_helper_eval_chunks` | Load mmproj, wrap raw RGB buffer, tokenize image, evaluate into KV cache |
| Sampling         | `llama_sampler_chain_init`, `llama_sampler_chain_add`, `llama_sampler_sample`         | Greedy autoregressive token generation                                   |
| State management | `llama_memory_clear`, `llama_sampler_reset`                                           | Reset KV cache and sampler between frames                                |

### 3.11 F-Prime Constant Overrides

The `InferenceComplete` event carries the VLM's reason string (up to 400 characters). F-Prime's default `FW_LOG_STRING_MAX_SIZE` (200) truncates this in the GDS event log. The project overrides four framework constants via `config/FpConstants.fpp` (registered as a `CONFIGURATION_OVERRIDES` target in CMake):

| Constant                      | Default | Override | Reason                                                                                           |
| ----------------------------- | ------- | -------- | ------------------------------------------------------------------------------------------------ |
| `FW_LOG_STRING_MAX_SIZE`      | 200     | 400      | Match the `InferenceComplete` reason field size                                                  |
| `FW_COM_BUFFER_MAX_SIZE`      | 512     | 768      | Accommodate larger log buffers within CCSDS TmFramer limits (payload capacity 1016, overhead 13) |
| `FW_LOG_TEXT_BUFFER_SIZE`     | 256     | 600      | Fit the fully formatted event text                                                               |
| `FW_FIXED_LENGTH_STRING_SIZE` | 256     | 400      | Must be >= `FW_LOG_STRING_MAX_SIZE` per framework static_assert                                  |

### 3.12 Configuration Constants

| Constant              | Value | Description                                |
| --------------------- | ----- | ------------------------------------------ |
| `IMAGE_W` / `IMAGE_H` | 512   | Expected input image dimensions            |
| `N_CTX`               | 4096  | KV cache context size in tokens            |
| `N_BATCH`             | 512   | Batch size for prompt evaluation           |
| `N_THREADS`           | 4     | CPU threads for inference (Pi 5 quad-core) |
| `MAX_RESPONSE_TOKENS` | 200   | Maximum tokens to generate per frame       |
| `IMAGE_MAX_TOKENS`    | 1024  | Cap on vision encoder output tokens        |
| `INFERENCE_TIMEOUT_S` | 120   | Abort inference after this many seconds    |

### 3.13 Environment Variables

| Variable            | Default                 | Description                                |
| ------------------- | ----------------------- | ------------------------------------------ |
| `ORION_GGUF_PATH`   | `./orion-q4_k_m.gguf`   | Path to the Q4_K_M quantized text model    |
| `ORION_MMPROJ_PATH` | `orion-mmproj-f16.gguf` | Path to the FP16 vision encoder projection |

## 4. Change Log

| Date       | Description                                                                               |
| ---------- | ----------------------------------------------------------------------------------------- |
| 2026-04-17 | Initial implementation: llama.cpp integration, ChatML prompt, JSON parser                 |
| 2026-04-18 | Fixed chat template (Phi-3 to ChatML), token limit, auto-lifecycle                        |
| 2026-04-18 | Fixed model not unloading on DOWNLINK → SAFE transition                                   |
| 2026-04-20 | Added mode gating, FrameDroppedModelNotLoaded, LoadModelRejectedWrongMode                 |
| 2026-04-24 | Removed health ping; added 120s self-watchdog with InferenceTimeout event                 |
| 2026-05-01 | Improved JSON parser: extract category value by key instead of global search              |
| 2026-05-02 | InferenceComplete event reason field reduced from string size 512 to 400                  |
| 2026-05-03 | Fixed SDD cross-reference links for mkdocs; corrected model size to ~730 MB               |
| 2026-05-03 | Added llama.cpp integration design, header vendoring, and FPP constant overrides sections |
