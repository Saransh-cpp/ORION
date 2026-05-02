module Orion {

  @ Active component that manages the simulated X-Band / S-Band radio link.
  @ Receives HIGH-priority image frames from TriageRouter and either transmits
  @ them immediately (during a comm window) or queues them to disk for later
  @ downlink. When a comm window opens, queued frames are flushed first.
  active component GroundCommsDriver {

    # --------------------------------------------------------------------------
    # Input ports
    # --------------------------------------------------------------------------

    @ Receives HIGH-priority image buffers from TriageRouter for downlink.
    async input port fileDownlinkIn: FileDownlinkPort

    @ Rate group schedule input: periodically flushes the disk queue
    @ when in DOWNLINK mode.
    async input port schedIn: Svc.Sched

    @ Receives mode changes from EventAction.
    @ Only transmits in DOWNLINK mode; queues to disk otherwise.
    async input port modeChangeIn: ModeChangePort

    # --------------------------------------------------------------------------
    # Output ports
    # --------------------------------------------------------------------------

    @ Returns image buffers to the BufferManager pool after transmit completes.
    output port bufferReturnOut: Fw.BufferSend

    # --------------------------------------------------------------------------
    # Telemetry
    # --------------------------------------------------------------------------

    @ Running count of frames successfully transmitted to the ground station.
    telemetry FramesDownlinked: U32 id 0x00

    @ Running total of raw image bytes sent over the downlink channel.
    telemetry BytesDownlinked: U32 id 0x01

    @ Running count of frames lost due to radio link or socket failure.
    telemetry TransmitFailures: U32 id 0x02

    @ Running count of frames queued to disk (outside comm window).
    telemetry FramesQueued: U32 id 0x03

    # --------------------------------------------------------------------------
    # Events
    # --------------------------------------------------------------------------

    @ Emitted for every successfully transmitted HIGH-priority frame.
    event FrameDownlinked(
      reason: string size 256
    ) \
      severity activity high \
      id 0x00 \
      format "ORION DOWNLINK: Frame transmitted. Reason: {}"

    @ Emitted when TCP connect or send to the ground station fails.
    event TransmitFailed \
      severity warning high \
      id 0x01 \
      format "GroundCommsDriver: Transmit failed - buffer returned to pool"

    @ Emitted when a frame is queued to disk because comm window is closed.
    event FrameQueued \
      severity activity low \
      id 0x02 \
      format "GroundCommsDriver: Frame queued to disk - outside comm window"

    @ Emitted when queued frames are flushed during a comm window.
    event QueueFlushed(
      count: U32
    ) \
      severity activity high \
      id 0x03 \
      format "GroundCommsDriver: Flushed {} queued frames during comm window"

    @ Emitted when writing a frame to the disk queue fails.
    event QueueWriteFailed \
      severity warning high \
      id 0x04 \
      format "GroundCommsDriver: Failed to write frame to disk queue - frame lost"

    # --------------------------------------------------------------------------
    # Required F-Prime framework ports
    # --------------------------------------------------------------------------

    time get port timeCaller
    event port eventOut
    text event port textEventOut
    telemetry port tlmOut

  }

}
