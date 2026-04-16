#include "CameraManager.hpp"

#include <cstring>

namespace Orion {

// Image buffer size must match the BufferManager pool allocation.
static constexpr FwSizeType IMAGE_BUFFER_SIZE = 512 * 512 * 3;  // 786,432 bytes

CameraManager::CameraManager(const char* compName)
    : CameraManagerComponentBase(compName), m_imagesCaptured(0), m_capturesFailed(0) {}

CameraManager::~CameraManager() {}

// ---------------------------------------------------------------------------
// Command handler — runs on the CameraManager's own active thread
// ---------------------------------------------------------------------------

void CameraManager::TRIGGER_CAPTURE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // 1. Check out a buffer from the pool.
    Fw::Buffer buf = this->bufferGetOut_out(0, IMAGE_BUFFER_SIZE);

    if (buf.getSize() == 0) {
        // Pool is exhausted — drop this capture and alert the ground.
        m_capturesFailed++;
        this->tlmWrite_CapturesFailed(m_capturesFailed);
        this->log_WARNING_HI_BufferPoolExhausted();
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
        return;
    }

    // 2. Fill the buffer with camera data.
    if (!captureIntoBuffer(buf)) {
        // Camera hardware failed — return the buffer and report.
        this->bufferReturnOut_out(0, buf);

        m_capturesFailed++;
        this->tlmWrite_CapturesFailed(m_capturesFailed);

        this->log_WARNING_HI_CameraHardwareError();

        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
        return;
    }

    // 3. Pull GPS coordinates from NavTelemetry synchronously.
    //    The call is time-aligned to this exact frame.
    NavState nav = this->navStateOut_out(0);

    // 4. Dispatch buffer + coordinates to VlmInferenceEngine asynchronously.
    //    CameraManager returns immediately; the VLM drains the queue at its pace.
    this->inferenceRequestOut_out(0, buf, nav.get_lat(), nav.get_lon());

    // 5. Update telemetry and respond to the command.
    m_imagesCaptured++;
    this->tlmWrite_ImagesCaptured(m_imagesCaptured);
    this->log_ACTIVITY_HI_ImageDispatched(nav.get_lat(), nav.get_lon());
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ---------------------------------------------------------------------------
// Camera capture stub
// ---------------------------------------------------------------------------

bool CameraManager::captureIntoBuffer(Fw::Buffer& buf) {
    // TODO: Replace with libcamera API call for Pi Camera Module.
    // libcamera::CameraManager, Request, and FrameBuffer will be wired here.
    // For now, zero-fill the buffer to simulate a valid frame.
    memset(buf.getData(), 0, buf.getSize());
    return true;
}

}  // namespace Orion
