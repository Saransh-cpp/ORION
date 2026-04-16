module Orion {

  @ Active component that manages the simulated X-Band / S-Band radio link.
  @ Receives HIGH-priority image frames from TriageRouter and transmits them
  @ to the ground station over Wi-Fi/Ethernet (TCP, simulating radio downlink).
  @ Always returns the image buffer to the pool after transmission or failure.
  active component GroundCommsDriver {

    # --------------------------------------------------------------------------
    # Input ports
    # --------------------------------------------------------------------------

    @ Receives HIGH-priority image buffers from TriageRouter for immediate downlink.
    @ Asynchronous: frames queue here while a prior transmission is in progress.
    async input port fileDownlinkIn: FileDownlinkPort

    # --------------------------------------------------------------------------
    # Output ports
    # --------------------------------------------------------------------------

    @ Returns image buffers to the BufferManager pool after transmit completes.
    @ Called unconditionally — whether transmit succeeded or failed.
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
      format "GroundCommsDriver: Transmit failed — buffer returned to pool"

    # --------------------------------------------------------------------------
    # Required F-Prime framework ports
    # --------------------------------------------------------------------------

    time get port timeCaller
    event port eventOut
    text event port textEventOut
    telemetry port tlmOut

  }

}
