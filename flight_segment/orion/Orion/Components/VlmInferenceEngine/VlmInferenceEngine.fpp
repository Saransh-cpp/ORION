module Orion {

  @ Active component that runs the LFM2.5-VL-1.6B vision-language model.
  @ Holds the 700 MB Q4 GGUF file resident in RAM on a dedicated low-priority
  @ thread. For each incoming image+GPS pair, it preprocesses the raw pixel
  @ data, executes the llama.cpp forward pass, parses the JSON output, and
  @ emits a TriageDecisionPort result. Buffer ownership passes to TriageRouter
  @ on success; the engine itself returns the buffer on inference failure.
  active component VlmInferenceEngine {

    # --------------------------------------------------------------------------
    # Commands
    # --------------------------------------------------------------------------

    @ Loads the GGUF text model and mmproj vision encoder from the microSD card
    @ into RAM. Must be called before any inference requests are processed.
    @ Idempotent if the model is already loaded.
    async command LOAD_MODEL opcode 0x00

    @ Flushes the model and all associated state from RAM.
    @ Use for safe-mode power shedding or graceful shutdown.
    async command UNLOAD_MODEL opcode 0x01

    # --------------------------------------------------------------------------
    # Input ports
    # --------------------------------------------------------------------------

    @ Receives image buffer + fused GPS coordinates from CameraManager.
    @ Asynchronous: frames queue here while a prior inference is running.
    @ This component is not wired to the health watchdog — the per-frame
    @ latency (50-60 s on Pi 5) would exceed any reasonable ping budget.
    @ A self-watchdog (INFERENCE_TIMEOUT_S = 120 s) aborts stuck inferences.
    async input port inferenceRequestIn: InferenceRequestPort

    @ Receives mode changes from EventAction.
    @ In SAFE mode, drops all incoming frames immediately.
    async input port modeChangeIn: ModeChangePort

    # --------------------------------------------------------------------------
    # Output ports
    # --------------------------------------------------------------------------

    @ Emits the parsed verdict, reasoning string, and original buffer handle
    @ to TriageRouter. Buffer ownership transfers; TriageRouter is responsible
    @ for returning it to the pool after routing.
    output port triageDecisionOut: TriageDecisionPort

    @ Returns the buffer to the pool directly when inference fails and the
    @ frame cannot be forwarded to TriageRouter.
    output port bufferReturnOut: Fw.BufferSend

    # --------------------------------------------------------------------------
    # Telemetry
    # --------------------------------------------------------------------------

    @ CPU wall-clock time in milliseconds for the most recent inference pass.
    telemetry InferenceTime_Ms: U32 id 0x00

    @ Running count of frames successfully classified and forwarded.
    telemetry TotalInferences: U32 id 0x01

    @ Running count of frames dropped due to model error or JSON parse failure.
    telemetry InferenceFailures: U32 id 0x02

    # --------------------------------------------------------------------------
    # Events
    # --------------------------------------------------------------------------

    @ Emitted when the GGUF model and mmproj are resident in RAM and ready.
    event ModelLoaded \
      severity activity high \
      id 0x00 \
      format "VlmInferenceEngine: Model loaded and ready for inference"

    @ Emitted when the model is successfully flushed from RAM.
    event ModelUnloaded \
      severity activity high \
      id 0x01 \
      format "VlmInferenceEngine: Model unloaded from RAM"

    @ Emitted when llama_model_load_from_file or mtmd_init_from_file fails.
    event ModelLoadFailed(
      path: string size 128
    ) \
      severity warning high \
      id 0x02 \
      format "VlmInferenceEngine: Failed to load model from {}"

    @ Emitted when tokenization, eval, or JSON parse fails for a frame.
    event InferenceFailed \
      severity warning high \
      id 0x03 \
      format "VlmInferenceEngine: Inference failed — buffer returned to pool"

    @ Emitted when a frame is dropped because the model is not loaded.
    event FrameDroppedModelNotLoaded \
      severity warning low \
      id 0x05 \
      format "VlmInferenceEngine: Frame dropped — model not loaded"

    @ Emitted when LOAD_MODEL is rejected due to wrong mode.
    event LoadModelRejectedWrongMode(
      currentMode: string size 16
    ) \
      severity warning low \
      id 0x06 \
      format "VlmInferenceEngine: LOAD_MODEL rejected — not in MEASURE or DOWNLINK (current: {})"

    @ Emitted when inference exceeds the timeout and is aborted.
    event InferenceTimeout(
      elapsed_ms: U32
    ) \
      severity warning high \
      id 0x07 \
      format "VlmInferenceEngine: Inference timed out after {}ms — frame dropped, KV cache reset"

    @ Emitted on every successful classification with the VLM's reasoning.
    event InferenceComplete(
      category: string size 16
      reason: string size 400
      time_ms: U32
    ) \
      severity activity high \
      id 0x04 \
      format "VLM RESULT: {} — {} ({}ms)"

    # --------------------------------------------------------------------------
    # Required F-Prime framework ports
    # --------------------------------------------------------------------------

    command recv port cmdIn
    command reg port cmdRegOut
    command resp port cmdResponseOut

    time get port timeCaller
    event port eventOut
    text event port textEventOut
    telemetry port tlmOut

  }

}
