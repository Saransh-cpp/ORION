module Orion {

  @ Active component that executes the ORION triage doctrine.
  @ Routes VLM verdicts to immediate downlink, bulk storage, or deletion.
  @ The camera and VLM pipelines are fully decoupled from this routing logic.
  active component TriageRouter {

    # --------------------------------------------------------------------------
    # Input ports
    # --------------------------------------------------------------------------

    @ Receives the VLM's parsed verdict asynchronously from VlmInferenceEngine.
    async input port triageDecisionIn: TriageDecisionPort

    @ Receives mode changes from EventAction.
    @ In SAFE mode, drops all incoming frames immediately.
    async input port modeChangeIn: ModeChangePort

    # --------------------------------------------------------------------------
    # Output ports
    # --------------------------------------------------------------------------

    @ Routes HIGH-priority image buffer and reason to GroundCommsDriver
    @ for immediate X-Band downlink.
    output port fileDownlinkOut: FileDownlinkPort

    @ Returns image buffers back to the BufferManager pool after routing.
    @ Called for MEDIUM (after file write) and LOW (immediately).
    output port bufferReturnOut: Fw.BufferSend

    # --------------------------------------------------------------------------
    # Telemetry
    # --------------------------------------------------------------------------

    @ Running count of HIGH images routed to the radio for immediate downlink.
    telemetry HighTargetsRouted: U32 id 0x00

    @ Running count of MEDIUM images saved to bulk microSD storage.
    telemetry MediumTargetsSaved: U32 id 0x01

    @ Running count of LOW frames discarded and buffers recycled.
    telemetry LowTargetsDiscarded: U32 id 0x02

    # --------------------------------------------------------------------------
    # Events
    # --------------------------------------------------------------------------

    @ Emitted for every HIGH verdict — visible in the GDS as a CRITICAL alert.
    event HighTargetDetected(
      reason: string size 256
    ) \
      severity activity high \
      id 0x00 \
      format "ORION PRIORITY: HIGH — {}"

    @ Emitted for every MEDIUM verdict — image saved to bulk storage.
    event MediumTargetStored \
      severity activity low \
      id 0x01 \
      format "ORION PRIORITY: MEDIUM — Image saved to bulk storage"

    @ Emitted for every LOW verdict — buffer recycled, zone cleared.
    event LowTargetDiscarded \
      severity activity low \
      id 0x02 \
      format "ORION PRIORITY: LOW — Buffer recycled, zone cleared"

    @ Emitted if the microSD write for a MEDIUM image fails.
    event StorageWriteFailed \
      severity warning high \
      id 0x03 \
      format "TriageRouter: microSD write failed — buffer returned to pool"

    # --------------------------------------------------------------------------
    # Required F-Prime framework ports
    # --------------------------------------------------------------------------

    time get port timeCaller
    event port eventOut
    text event port textEventOut
    telemetry port tlmOut

  }

}
