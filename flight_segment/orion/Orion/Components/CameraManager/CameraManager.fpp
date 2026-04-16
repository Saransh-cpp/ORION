module Orion {

  @ Active component that interfaces with the Pi Camera Module.
  @ Captures images into a pre-allocated buffer, fuses GPS telemetry,
  @ and dispatches the payload asynchronously to the VlmInferenceEngine.
  @ The camera thread never blocks on VLM inference timing.
  active component CameraManager {

    # --------------------------------------------------------------------------
    # Commands
    # --------------------------------------------------------------------------

    @ Manually triggers a single image capture from the GDS or sequencer.
    async command TRIGGER_CAPTURE opcode 0x00

    # --------------------------------------------------------------------------
    # Output ports
    # --------------------------------------------------------------------------

    @ Checks out a free 786 KB buffer from the BufferManager pool.
    output port bufferGetOut: Fw.BufferGet

    @ Synchronous call to NavTelemetry for GPS coordinates at capture time.
    @ Coordinates are time-aligned to the frame, not retrieved after the fact.
    output port navStateOut: NavStatePort

    @ Fires the captured image buffer and fused coordinates to the VLM queue.
    @ Asynchronous: CameraManager returns immediately after invoking this port.
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
    @ Ground must pace CMD_TRIGGER_CAPTURE to stay within the 13-image LEO budget.
    event BufferPoolExhausted \
      severity warning high \
      id 0x01 \
      format "CameraManager: Buffer pool exhausted — capture dropped"

    @ Emitted when the Pi Camera hardware fails to acquire a frame.
    @ The allocated buffer is safely returned to the pool.
    event CameraHardwareError \
      severity warning high \
      id 0x02 \
      format "CameraManager: Hardware read failed — buffer returned to pool"
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
