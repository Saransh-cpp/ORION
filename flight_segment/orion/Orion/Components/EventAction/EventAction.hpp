#ifndef ORION_EVENT_ACTION_HPP
#define ORION_EVENT_ACTION_HPP

#include "Orion/Components/EventAction/EventActionComponentAc.hpp"

namespace Orion {

class EventAction final : public EventActionComponentBase {
  public:
    explicit EventAction(const char* compName);
    ~EventAction();

  private:
    // -----------------------------------------------------------------------
    // Port handlers
    // -----------------------------------------------------------------------

    //! Rate group handler — evaluates mode transitions at 1 Hz.
    void schedIn_handler(FwIndexType portNum, U32 context) override;

    // -----------------------------------------------------------------------
    // Command handlers
    // -----------------------------------------------------------------------

    void SET_ECLIPSE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, bool inEclipse) override;
    void ENTER_SAFE_MODE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void EXIT_SAFE_MODE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void FLUSH_MEDIUM_STORAGE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void GOTO_IDLE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void GOTO_MEASURE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void GOTO_DOWNLINK_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;

    // -----------------------------------------------------------------------
    // State machine action/guard implementations
    // -----------------------------------------------------------------------

    void Orion_MissionModeSm_action_broadcastMode(SmId smId, Orion_MissionModeSm::Signal signal) override;

    void Orion_MissionModeSm_action_logModeChange(SmId smId, Orion_MissionModeSm::Signal signal) override;

    bool Orion_MissionModeSm_guard_sunIsUp(SmId smId, Orion_MissionModeSm::Signal signal) const override;

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    //! Maps state machine state enum to MissionMode enum.
    static MissionMode stateToMode(Orion_MissionModeSm::State state);

    //! Derives the target mode from the signal that caused the transition.
    MissionMode signalToTargetMode(Orion_MissionModeSm::Signal signal) const;

    //! Maps MissionMode to a human-readable string.
    static const char* modeToStr(MissionMode mode);

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    bool m_portsConnected;   //!< False until ports are wired (guards init-time broadcast)
    bool m_inEclipse;        //!< Eclipse flag (from SET_ECLIPSE command)
    bool m_prevCommWindow;   //!< Previous comm window state for edge detection
    MissionMode m_prevMode;  //!< Previous mode for transition logging
    bool m_flushingMedium;   //!< True while paced MEDIUM flush is in progress
    U32 m_mediumFlushed;     //!< Count of MEDIUM files queued in current flush
};

}  // namespace Orion

#endif  // ORION_EVENT_ACTION_HPP
