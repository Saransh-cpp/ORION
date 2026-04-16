module Orion {

  # GPS coordinates captured at the moment of image acquisition.
  struct NavState {
    lat: F64
    lon: F64
  }

  # Triage verdict emitted by the VLM. Maps directly to downlink doctrine:
  # LOW = discard, MEDIUM = bulk store, HIGH = immediate downlink.
  enum TriagePriority: U8 {
    LOW    = 0,
    MEDIUM = 1,
    HIGH   = 2
  }

}
