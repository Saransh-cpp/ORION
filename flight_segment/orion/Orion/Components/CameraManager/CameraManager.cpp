#include "CameraManager.hpp"

#include <cstdio>
#include <cstring>

#include "Os/File.hpp"

namespace Orion {

// Image buffer size must match the BufferManager pool allocation.
static constexpr FwSizeType IMAGE_BUFFER_SIZE = 512 * 512 * 3;  // 786,432 bytes

// Pre-converted 512x512 raw RGB test images.
// Override via env var: ORION_TEST_IMAGE_DIR
static const char* getTestImageDir() {
    const char* p = ::getenv("ORION_TEST_IMAGE_DIR");
    return p ? p : "/home/pi/ORION/ground_segment/data/test_raw/";
}
static constexpr U32 NUM_TEST_IMAGES = 300;

CameraManager::CameraManager(const char* compName)
    : CameraManagerComponentBase(compName), m_imagesCaptured(0), m_capturesFailed(0), m_imageIndex(0) {}

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
    // Load a pre-converted 512x512 raw RGB test image from disk.
    // On the Pi 5, replace this with libcamera API calls.
    char path[256];
    snprintf(path, sizeof(path), "%simage_%04u.raw", getTestImageDir(), m_imageIndex % NUM_TEST_IMAGES);
    m_imageIndex++;

    Os::File file;
    if (file.open(path, Os::File::OPEN_READ) != Os::File::OP_OK) {
        // Fall back to zero-fill if test images aren't available.
        memset(buf.getData(), 0, buf.getSize());
        return true;
    }

    FwSizeType size = buf.getSize();
    Os::File::Status status = file.read(buf.getData(), size);
    file.close();

    if (status != Os::File::OP_OK || size != IMAGE_BUFFER_SIZE) {
        memset(buf.getData(), 0, buf.getSize());
    }

    return true;
}

}  // namespace Orion
