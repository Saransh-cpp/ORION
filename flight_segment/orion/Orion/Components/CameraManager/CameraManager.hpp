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
    // Command handler
    // -----------------------------------------------------------------------

    //! Executes the full capture-fuse-dispatch pipeline on the component thread.
    void TRIGGER_CAPTURE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    //! Fills the buffer with raw pixel data from the camera hardware.
    //! On the Pi 5 this calls libcamera. For demo, loads pre-converted
    //! test images from ground_segment/data/test_raw/.
    bool captureIntoBuffer(Fw::Buffer& buf);

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    U32 m_imagesCaptured;
    U32 m_capturesFailed;
    U32 m_imageIndex;  //!< Cycles through available test images.
};

}  // namespace Orion

#endif  // ORION_CAMERA_MANAGER_HPP
