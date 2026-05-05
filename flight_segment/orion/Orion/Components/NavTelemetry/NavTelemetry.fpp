module Orion {

  @ Active component that manages the satellite's position state.
  @ Polls SimSat's orbital propagator for position updates and computes
  @ whether the satellite is within the ground station comm window.
  @ In flight, state would be driven by a hardware GNSS receiver.
  active component NavTelemetry {

    # --------------------------------------------------------------------------
    # Custom ports
    # --------------------------------------------------------------------------

    @ Synchronous getter called by CameraManager (and GroundCommsDriver) at
    @ decision time. Uses the component mutex to prevent tearing.
    guarded input port navStateGet: NavStatePort

    @ Rate group schedule input: drives periodic SimSat polling.
    async input port schedIn: Svc.Sched drop

    # --------------------------------------------------------------------------
    # Telemetry
    # --------------------------------------------------------------------------

    @ Last known latitude downlinked continuously for ground monitoring.
    telemetry CurrentLat: F64 id 0x00

    @ Last known longitude downlinked continuously for ground monitoring.
    telemetry CurrentLon: F64 id 0x01

    @ Last known altitude in km from the orbital propagator.
    telemetry CurrentAlt: F64 id 0x02

    @ Whether the satellite is currently in the ground station comm window.
    telemetry InCommWindow: bool id 0x03

    # --------------------------------------------------------------------------
    # Events
    # --------------------------------------------------------------------------

    @ Emitted when SimSat provides an updated position.
    event SimSatPositionUpdate(
      lat: F64
      lon: F64
      alt: F64
    ) \
      severity activity low \
      id 0x00 \
      format "NavTelemetry: SimSat position Lat={}, Lon={}, Alt={}km"

    @ Emitted when the HTTP connection to SimSat fails.
    event SimSatConnectionFailed \
      severity warning high \
      id 0x01 \
      format "NavTelemetry: Failed to reach SimSat API"

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
