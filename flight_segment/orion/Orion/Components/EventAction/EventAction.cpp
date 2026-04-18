#include "EventAction.hpp"

#include <dirent.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>

namespace Orion {

static const char* getMediumStoragePath() {
    const char* p = ::getenv("ORION_MEDIUM_STORAGE_DIR");
    return p ? p : "/media/sd/orion/medium/";
}

EventAction::EventAction(const char* compName)
    : EventActionComponentBase(compName),
      m_portsConnected(false),
      m_inEclipse(false),
      m_prevCommWindow(false),
      m_prevMode(MissionMode::IDLE),
      m_flushingMedium(false),
      m_mediumFlushed(0) {}

EventAction::~EventAction() {}

// ---------------------------------------------------------------------------
// Schedule handler — evaluates mode transitions at 1 Hz
// ---------------------------------------------------------------------------

void EventAction::schedIn_handler(FwIndexType portNum, U32 context) {
    // First tick: ports are now connected, broadcast the initial IDLE mode
    if (!m_portsConnected) {
        m_portsConnected = true;
        MissionMode mode = stateToMode(this->missionMode_getState());
        for (FwIndexType i = 0; i < 4; i++) {
            this->modeChangeOut_out(i, mode);
        }
        this->tlmWrite_CurrentMode(mode);
    }

    // Query NavTelemetry for current position and comm window state
    NavState nav = this->navStateIn_out(0);
    bool inCommWindow = nav.get_inCommWindow();

    // Paced MEDIUM flush: queue one file per tick to avoid overwhelming
    // FileDownlink's 10-entry queue. Delete the file after successful queue.
    if (m_flushingMedium) {
        // Abort flush if we left DOWNLINK
        if (m_prevMode.e != MissionMode::DOWNLINK) {
            this->log_ACTIVITY_HI_MediumStorageFlushed(m_mediumFlushed);
            m_flushingMedium = false;
        } else {
            const char* storageDir = getMediumStoragePath();
            DIR* dir = ::opendir(storageDir);
            bool found = false;

            if (dir) {
                struct dirent* entry;
                while ((entry = ::readdir(dir)) != nullptr) {
                    if (::strncmp(entry->d_name, "orion_medium_", 13) != 0) {
                        continue;
                    }

                    char srcPath[256];
                    ::snprintf(srcPath, sizeof(srcPath), "%s%s", storageDir, entry->d_name);

                    Svc::SendFileResponse resp =
                        this->sendFileOut_out(0, Fw::String(srcPath), Fw::String(entry->d_name), 0, 0);

                    if (resp.get_status() == Svc::SendFileStatus::STATUS_OK) {
                        ::unlink(srcPath);
                        m_mediumFlushed++;
                    }
                    found = true;
                    break;  // One file per tick
                }
                ::closedir(dir);
            }

            if (!found) {
                // Directory empty or gone — flush complete
                this->log_ACTIVITY_HI_MediumStorageFlushed(m_mediumFlushed);
                m_flushingMedium = false;
            }
        }
    }

    // Detect comm window edges and send signals to the state machine
    F64 distance = nav.get_gsDistanceKm();
    if (inCommWindow && !m_prevCommWindow) {
        this->log_ACTIVITY_HI_CommWindowOpened(distance);
        this->missionMode_sendSignal_commWindowOpened();
    } else if (!inCommWindow && m_prevCommWindow) {
        this->log_ACTIVITY_HI_CommWindowClosed(distance);
        this->missionMode_sendSignal_commWindowClosed();
    }
    m_prevCommWindow = inCommWindow;
}

// ---------------------------------------------------------------------------
// Command handlers
// ---------------------------------------------------------------------------

void EventAction::SET_ECLIPSE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, bool inEclipse) {
    m_inEclipse = inEclipse;

    if (inEclipse) {
        this->missionMode_sendSignal_eclipse();
    } else {
        this->missionMode_sendSignal_sunUp();
    }

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void EventAction::ENTER_SAFE_MODE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->missionMode_sendSignal_fault();
    this->log_WARNING_HI_SafeModeEntered();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void EventAction::EXIT_SAFE_MODE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->missionMode_sendSignal_clearFault();
    this->log_ACTIVITY_HI_SafeModeExited();

    // Re-sync with current conditions so we don't miss an in-progress
    // comm window or sun state that changed while we were in SAFE.
    NavState nav = this->navStateIn_out(0);
    bool inCommWindow = nav.get_inCommWindow();
    m_prevCommWindow = inCommWindow;

    if (inCommWindow) {
        this->log_ACTIVITY_HI_CommWindowOpened(nav.get_gsDistanceKm());
        this->missionMode_sendSignal_commWindowOpened();
    } else if (!m_inEclipse) {
        this->missionMode_sendSignal_sunUp();
    }

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void EventAction::FLUSH_MEDIUM_STORAGE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Only allow during comm window
    if (m_prevMode.e != MissionMode::DOWNLINK) {
        this->log_WARNING_LO_MediumFlushRejected(Fw::String(modeToStr(m_prevMode)));
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    const char* storageDir = getMediumStoragePath();

    // Validate path length (FileDownlink limit is 100 chars)
    if (::strlen(storageDir) + 22 >= 100) {
        this->log_WARNING_HI_MediumPathTooLong();
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // Start the paced flush — schedIn will queue one file per tick
    m_flushingMedium = true;
    m_mediumFlushed = 0;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ---------------------------------------------------------------------------
// State machine actions
// ---------------------------------------------------------------------------

void EventAction::Orion_MissionModeSm_action_broadcastMode(SmId smId, Orion_MissionModeSm::Signal signal) {
    // Skip broadcast during state machine init — ports aren't connected yet
    if (!m_portsConnected) {
        return;
    }

    // NOTE: F-Prime runs entry actions BEFORE updating m_state, so
    // getState() returns the old state here. Derive target from signal.
    MissionMode mode = signalToTargetMode(signal);

    // Broadcast to all 4 pipeline components
    for (FwIndexType i = 0; i < 4; i++) {
        this->modeChangeOut_out(i, mode);
    }

    this->tlmWrite_CurrentMode(mode);
}

void EventAction::Orion_MissionModeSm_action_logModeChange(SmId smId, Orion_MissionModeSm::Signal signal) {
    // NOTE: F-Prime runs entry actions BEFORE updating m_state, so
    // getState() returns the old state here. Derive target from signal.
    MissionMode currentMode = signalToTargetMode(signal);

    this->log_ACTIVITY_HI_ModeChanged(Fw::String(modeToStr(m_prevMode)), Fw::String(modeToStr(currentMode)));

    m_prevMode = currentMode;
}

// ---------------------------------------------------------------------------
// State machine guard
// ---------------------------------------------------------------------------

bool EventAction::Orion_MissionModeSm_guard_sunIsUp(SmId smId, Orion_MissionModeSm::Signal signal) const {
    return !m_inEclipse;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

MissionMode EventAction::stateToMode(Orion_MissionModeSm::State state) {
    switch (state.e) {
        case Orion_MissionModeSm::State::IDLE:
            return MissionMode::IDLE;
        case Orion_MissionModeSm::State::MEASURE:
            return MissionMode::MEASURE;
        case Orion_MissionModeSm::State::DOWNLINK:
            return MissionMode::DOWNLINK;
        case Orion_MissionModeSm::State::SAFE:
            return MissionMode::SAFE;
        default:
            return MissionMode::IDLE;
    }
}

MissionMode EventAction::signalToTargetMode(Orion_MissionModeSm::Signal signal) const {
    switch (signal) {
        case Orion_MissionModeSm::Signal::sunUp:
            return MissionMode::MEASURE;
        case Orion_MissionModeSm::Signal::commWindowOpened:
            return MissionMode::DOWNLINK;
        case Orion_MissionModeSm::Signal::fault:
            return MissionMode::SAFE;
        case Orion_MissionModeSm::Signal::commWindowClosed:
            // POST_DOWNLINK choice routes to MEASURE (sun up) or IDLE (eclipse).
            // Can't distinguish from signal alone — check the eclipse flag.
            return m_inEclipse ? MissionMode::IDLE : MissionMode::MEASURE;
        case Orion_MissionModeSm::Signal::eclipse:
        case Orion_MissionModeSm::Signal::clearFault:
        case Orion_MissionModeSm::Signal::__FPRIME_INITIAL_TRANSITION:
        default:
            return MissionMode::IDLE;
    }
}

const char* EventAction::modeToStr(MissionMode mode) {
    switch (mode.e) {
        case MissionMode::IDLE:
            return "IDLE";
        case MissionMode::MEASURE:
            return "MEASURE";
        case MissionMode::DOWNLINK:
            return "DOWNLINK";
        case MissionMode::SAFE:
            return "SAFE";
        default:
            return "UNKNOWN";
    }
}

}  // namespace Orion
