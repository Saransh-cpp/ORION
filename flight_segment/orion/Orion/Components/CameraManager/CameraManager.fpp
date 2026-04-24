module Orion {

  @ Active component that acquires Earth observation imagery.
  @ In autonomous mode, periodically fetches Mapbox satellite images from
  @ SimSat, fuses GPS telemetry, and dispatches to the VlmInferenceEngine.
  @ Also supports manual TRIGGER_CAPTURE for ground-commanded operation.
  active component CameraManager {

    # --------------------------------------------------------------------------
    # Commands
    # --------------------------------------------------------------------------

    @ Manually triggers a single image capture from the GDS or sequencer.
    async command TRIGGER_CAPTURE opcode 0x00

    @ Enables autonomous periodic capture at the given interval in seconds.
    async command ENABLE_AUTO_CAPTURE(
      interval: U32 @< Seconds between captures (default 45)
    ) opcode 0x01

    @ Disables autonomous periodic capture.
    async command DISABLE_AUTO_CAPTURE opcode 0x02

    # --------------------------------------------------------------------------
    # Input ports
    # --------------------------------------------------------------------------

    @ Rate group schedule input — drives auto-capture timing.
    async input port schedIn: Svc.Sched

    @ Receives mode changes from EventAction.
    @ Only auto-captures in MEASURE mode.
    async input port modeChangeIn: ModeChangePort

    # --------------------------------------------------------------------------
    # Output ports
    # --------------------------------------------------------------------------

    @ Checks out a free 786 KB buffer from the BufferManager pool.
    output port bufferGetOut: Fw.BufferGet

    @ Synchronous call to NavTelemetry for GPS coordinates at capture time.
    output port navStateOut: NavStatePort

    @ Fires the captured image buffer and fused coordinates to the VLM queue.
    output port inferenceRequestOut: InferenceRequestPort

    @ Returns an empty or failed buffer back to the pool.
    output port bufferReturnOut: Fw.BufferSend

    # --------------------------------------------------------------------------
    # Telemetry
    # --------------------------------------------------------------------------

    @ Running total of images successfully captured and dispatched to the VLM.
    telemetry ImagesCaptured: U32 id 0x00

    @ Running total of captures dropped due to buffer pool exhaustion.
    telemetry CapturesFailed: U32 id 0x01

    # --------------------------------------------------------------------------
    # Events
    # --------------------------------------------------------------------------

    @ Emitted each time an image is successfully captured and dispatched.
    event ImageDispatched(
      lat: F64
      lon: F64
    ) \
      severity activity high \
      id 0x00 \
      format "CameraManager: Image dispatched at Lat={}, Lon={}"

    @ Emitted when the buffer pool is exhausted and a capture is dropped.
    event BufferPoolExhausted \
      severity warning high \
      id 0x01 \
      format "CameraManager: Buffer pool exhausted — capture dropped"

    @ Emitted when the camera/SimSat image acquisition fails.
    event CameraHardwareError \
      severity warning high \
      id 0x02 \
      format "CameraManager: Image acquisition failed — buffer returned to pool"

    @ Emitted when autonomous capture mode is enabled.
    event AutoCaptureEnabled(
      interval: U32
    ) \
      severity activity high \
      id 0x03 \
      format "CameraManager: Auto-capture enabled every {} seconds"

    @ Emitted when autonomous capture mode is disabled.
    event AutoCaptureDisabled \
      severity activity high \
      id 0x04 \
      format "CameraManager: Auto-capture disabled"

    @ Emitted when SimSat reports no image available for current position.
    event SimSatImageUnavailable \
      severity activity low \
      id 0x05 \
      format "CameraManager: SimSat image unavailable (over ocean or target not visible)"

    @ Emitted when a capture or auto-capture command is rejected due to wrong mode.
    event CommandRejectedWrongMode(
      currentMode: string size 16
    ) \
      severity warning low \
      id 0x06 \
      format "CameraManager: Command rejected — not in MEASURE (current: {})"

    @ Emitted when the requested capture interval is clamped to the minimum.
    event CaptureIntervalClamped(
      requested: U32
      minimum: U32
    ) \
      severity warning low \
      id 0x07 \
      format "CameraManager: Requested interval {}s too low — clamped to {}s"

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
