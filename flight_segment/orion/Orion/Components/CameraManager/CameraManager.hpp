#ifndef ORION_CAMERA_MANAGER_HPP
#define ORION_CAMERA_MANAGER_HPP

#include "Orion/Components/CameraManager/CameraManagerComponentAc.hpp"

namespace Orion {

class CameraManager final : public CameraManagerComponentBase {
  public:
    explicit CameraManager(const char* compName);
    ~CameraManager();

  private:
    // -----------------------------------------------------------------------
    // Command handlers
    // -----------------------------------------------------------------------

    //! Executes the full capture-fuse-dispatch pipeline on the component thread.
    void TRIGGER_CAPTURE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;

    //! Enables autonomous periodic capture at the given interval.
    void ENABLE_AUTO_CAPTURE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 interval) override;

    //! Disables autonomous periodic capture.
    void DISABLE_AUTO_CAPTURE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;

    // -----------------------------------------------------------------------
    // Port handlers
    // -----------------------------------------------------------------------

    //! Rate group schedule handler: drives auto-capture timing.
    void schedIn_handler(FwIndexType portNum, U32 context) override;

    //! Mode change handler: stores the current mission mode.
    void modeChangeIn_handler(FwIndexType portNum, const Orion::MissionMode& mode) override;

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    //! Fills the buffer with raw pixel data from SimSat's Mapbox API.
    bool captureIntoBuffer(Fw::Buffer& buf);

    //! Performs the full capture-fuse-dispatch pipeline (shared by
    //! TRIGGER_CAPTURE command and auto-capture timer).
    void doCapture();

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    U32 m_imagesCaptured;
    U32 m_capturesFailed;
    bool m_autoCaptureEnabled;  //!< True when autonomous capture is active
    U32 m_autoCaptureInterval;  //!< Seconds between auto-captures
    U32 m_schedCounter;         //!< Counts schedIn ticks for auto-capture timing
    MissionMode m_currentMode;  //!< Current mission mode from EventAction
};

}  // namespace Orion

#endif  // ORION_CAMERA_MANAGER_HPP
