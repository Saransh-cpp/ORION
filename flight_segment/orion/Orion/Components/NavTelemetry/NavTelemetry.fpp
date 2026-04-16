module Orion {

  @ Passive component that acts as the satellite's source of truth for position.
  @ Stores the last known GPS state and serves it synchronously on demand.
  @ In flight, state is driven by a hardware UART/GPIO driver.
  @ In the Pi 5 demo, state is updated via GDS commands.
  passive component NavTelemetry {

    # --------------------------------------------------------------------------
    # Custom ports
    # --------------------------------------------------------------------------

    @ Synchronous getter called by CameraManager at the moment of image capture.
    @ Uses the component mutex to prevent tearing if a SET_POSITION command is writing.
    guarded input port navStateGet: NavStatePort

    # --------------------------------------------------------------------------
    # Commands
    # --------------------------------------------------------------------------

    @ Updates the stored GPS position from the ground or a simulation driver.
    @ In a production system this port would be replaced by a hardware driver.
    guarded command SET_POSITION(
      lat: F64 @< Geodetic latitude in decimal degrees
      lon: F64 @< Geodetic longitude in decimal degrees
    ) opcode 0x00

    # --------------------------------------------------------------------------
    # Telemetry
    # --------------------------------------------------------------------------

    @ Last known latitude downlinked continuously for ground monitoring.
    telemetry CurrentLat: F64 id 0x00

    @ Last known longitude downlinked continuously for ground monitoring.
    telemetry CurrentLon: F64 id 0x01

    # --------------------------------------------------------------------------
    # Events
    # --------------------------------------------------------------------------

    @ Emitted when the ground successfully updates the GPS position.
    event PositionUpdated(
      lat: F64
      lon: F64
    ) \
      severity activity high \
      id 0x00 \
      format "NavTelemetry: Position forcefully updated to Lat: {}, Lon: {}"

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
