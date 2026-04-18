module Orion {

  # NavStatePort
  # Type    : Synchronous (called by CameraManager at capture time)
  # Returns : NavState struct containing Latitude and Longitude
  # Purpose : Provides exact GPS coordinates with near-zero latency so the
  #           coordinates are time-aligned to the captured frame.
  port NavStatePort -> NavState

  # InferenceRequestPort
  # Type    : Asynchronous (queued in VlmInferenceEngine's message queue)
  # Payload : A Fw.Buffer handle to the 786 KB raw pixel data, plus coordinates
  # Purpose : Decouples CameraManager from VLM timing. The camera fires and
  #           forgets; the VLM drains the queue at its own pace (15-45 s/frame).
  port InferenceRequestPort(
    ref buffer: Fw.Buffer,
    lat:    F64,
    lon:    F64
  )

  # TriageDecisionPort
  # Type    : Asynchronous (queued in TriageRouter's message queue)
  # Payload : VLM verdict, reasoning string, and the original buffer handle
  # Purpose : Carries the final classification from VlmInferenceEngine to
  #           TriageRouter for routing to downlink, storage, or deletion.
  port TriageDecisionPort(
    verdict: TriagePriority,
    reason:  string size 256,
    ref buffer:  Fw.Buffer
  )

  # FileDownlinkPort
  # Type    : Asynchronous (queued in GroundCommsDriver's message queue)
  # Payload : Buffer handle and reasoning string for HIGH-priority frames
  # Purpose : Carries confirmed HIGH-priority images from TriageRouter to
  #           GroundCommsDriver for immediate X-Band / S-Band downlink.
  port FileDownlinkPort(
    ref buffer: Fw.Buffer,
    reason: string size 256
  )

  # ModeChangePort
  # Type    : Asynchronous (broadcast from EventAction to all pipeline components)
  # Payload : The new MissionMode the satellite has transitioned to
  # Purpose : Notifies components of global mode changes so they can gate their
  #           behavior accordingly (e.g. only capture in MEASURE, only downlink
  #           in DOWNLINK, suspend everything in SAFE).
  port ModeChangePort(
    mode: MissionMode
  )

}
