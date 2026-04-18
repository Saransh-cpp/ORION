module Orion {

  @ Mission mode state machine for the ORION satellite.
  state machine MissionModeSm {

    action broadcastMode
    action logModeChange

    signal commWindowOpened
    signal commWindowClosed
    signal sunUp
    signal eclipse
    signal fault
    signal clearFault

    guard sunIsUp

    initial enter IDLE

    @ Startup state and eclipse state. No captures, no downlink.
    state IDLE {
      entry do { broadcastMode, logModeChange }
      on sunUp enter MEASURE
      on commWindowOpened enter DOWNLINK
      on fault enter SAFE
    }

    @ Active imaging mode.
    state MEASURE {
      entry do { broadcastMode, logModeChange }
      on commWindowOpened enter DOWNLINK
      on eclipse enter IDLE
      on fault enter SAFE
    }

    @ Communication pass. Queue flushes to ground station.
    state DOWNLINK {
      entry do { broadcastMode, logModeChange }
      on commWindowClosed enter POST_DOWNLINK
      on fault enter SAFE
    }

    @ Routing choice after DOWNLINK ends.
    choice POST_DOWNLINK {
      if sunIsUp enter MEASURE else enter IDLE
    }

    @ Safe mode — all operations suspended.
    state SAFE {
      entry do { broadcastMode, logModeChange }
      on clearFault enter IDLE
    }

  }

  @ Centralized mission mode controller for the ORION satellite.
  @ Owns the MissionModeSm state machine and broadcasts mode changes
  @ to all pipeline components. Evaluates transitions based on comm
  @ window state from NavTelemetry and eclipse commands from ground.
  active component EventAction {

    # --------------------------------------------------------------------------
    # State machine
    # --------------------------------------------------------------------------

    state machine instance missionMode: MissionModeSm priority 1 assert

    # --------------------------------------------------------------------------
    # Input ports
    # --------------------------------------------------------------------------

    @ Rate group schedule input — evaluates mode transitions at 1 Hz.
    async input port schedIn: Svc.Sched

    # --------------------------------------------------------------------------
    # Output ports
    # --------------------------------------------------------------------------

    @ Synchronous query to NavTelemetry for position and comm window state.
    output port navStateIn: NavStatePort

    @ Broadcast mode changes to pipeline components.
    @ Index 0: CameraManager, 1: GroundCommsDriver,
    @ 2: VlmInferenceEngine, 3: TriageRouter
    output port modeChangeOut: [4] ModeChangePort

    @ Request file downlink via the F-Prime FileDownlink service.
    output port sendFileOut: Svc.SendFileRequest

    # --------------------------------------------------------------------------
    # Commands
    # --------------------------------------------------------------------------

    @ Sets the eclipse flag. Sun up (false) enables MEASURE mode;
    @ eclipse (true) forces return to IDLE.
    async command SET_ECLIPSE(
      inEclipse: bool @< True = eclipse (no solar power), False = sun visible
    ) opcode 0x00

    @ Forces the satellite into SAFE mode from any state.
    @ All pipeline operations are suspended until EXIT_SAFE_MODE.
    async command ENTER_SAFE_MODE opcode 0x01

    @ Returns the satellite from SAFE mode to IDLE.
    @ The state machine will then auto-transition based on conditions.
    async command EXIT_SAFE_MODE opcode 0x02

    @ Bulk-downlink all MEDIUM images via the F-Prime FileDownlink service.
    @ Only allowed in DOWNLINK mode (comm window open).
    async command FLUSH_MEDIUM_STORAGE opcode 0x03

    # --------------------------------------------------------------------------
    # Telemetry
    # --------------------------------------------------------------------------

    @ Current mission mode for ground monitoring.
    telemetry CurrentMode: MissionMode id 0x00

    # --------------------------------------------------------------------------
    # Events
    # --------------------------------------------------------------------------

    @ Emitted on every mode transition.
    event ModeChanged(
      fromMode: string size 16
      toMode: string size 16
    ) \
      severity activity high \
      id 0x00 \
      format "EventAction: {} -> {}"

    @ Emitted when entering SAFE mode.
    event SafeModeEntered \
      severity warning high \
      id 0x01 \
      format "EventAction: SAFE MODE — all operations suspended"

    @ Emitted when exiting SAFE mode.
    event SafeModeExited \
      severity activity high \
      id 0x02 \
      format "EventAction: SAFE MODE exited — returning to IDLE"

    @ Emitted when the comm window opens.
    event CommWindowOpened(
      distanceKm: F64
    ) \
      severity activity high \
      id 0x03 \
      format "EventAction: Comm window OPENED (distance {.0f} km)"

    @ Emitted when the comm window closes.
    event CommWindowClosed(
      distanceKm: F64
    ) \
      severity activity high \
      id 0x04 \
      format "EventAction: Comm window CLOSED (distance {.0f} km)"

    @ Emitted when FLUSH_MEDIUM_STORAGE queues files for downlink.
    event MediumStorageFlushed(
      count: U32
    ) \
      severity activity high \
      id 0x05 \
      format "EventAction: Queued {} MEDIUM files for downlink"

    @ Emitted when FLUSH_MEDIUM_STORAGE is rejected (not in DOWNLINK mode).
    event MediumFlushRejected(
      currentMode: string size 16
    ) \
      severity warning low \
      id 0x06 \
      format "EventAction: FLUSH_MEDIUM_STORAGE rejected — not in DOWNLINK (current: {})"

    @ Emitted when a MEDIUM file path exceeds the 100-char FileDownlink limit.
    event MediumPathTooLong \
      severity warning high \
      id 0x07 \
      format "EventAction: MEDIUM storage path too long for FileDownlink (max 100 chars)"

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
