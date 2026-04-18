#include "CameraManager.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Os/File.hpp"
#include "Utils/SimSatClient.hpp"

namespace Orion {

// Image buffer size must match the BufferManager pool allocation.
static constexpr FwSizeType IMAGE_BUFFER_SIZE = 512 * 512 * 3;  // 786,432 bytes
static constexpr U32 IMAGE_WIDTH = 512;
static constexpr U32 IMAGE_HEIGHT = 512;

// Pre-converted 512x512 raw RGB test images (fallback when SimSat is unreachable).
static const char* getTestImageDir() {
    const char* p = ::getenv("ORION_TEST_IMAGE_DIR");
    return p ? p : "/home/pi/ORION/ground_segment/data/test_raw/";
}
static constexpr U32 NUM_TEST_IMAGES = 300;

static const char* modeStr(MissionMode mode) {
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

CameraManager::CameraManager(const char* compName)
    : CameraManagerComponentBase(compName),
      m_imagesCaptured(0),
      m_capturesFailed(0),
      m_imageIndex(0),
      m_autoCaptureEnabled(false),
      m_autoCaptureInterval(45),
      m_schedCounter(0),
      m_currentMode(MissionMode::IDLE) {}

CameraManager::~CameraManager() {}

// ---------------------------------------------------------------------------
// Command handlers
// ---------------------------------------------------------------------------

void CameraManager::TRIGGER_CAPTURE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    if (m_currentMode.e != MissionMode::MEASURE) {
        this->log_WARNING_LO_CommandRejectedWrongMode(Fw::String(modeStr(m_currentMode)));
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    doCapture();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void CameraManager::ENABLE_AUTO_CAPTURE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 interval) {
    if (m_currentMode.e != MissionMode::MEASURE) {
        this->log_WARNING_LO_CommandRejectedWrongMode(Fw::String(modeStr(m_currentMode)));
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    m_autoCaptureEnabled = true;
    m_autoCaptureInterval = (interval > 0) ? interval : 45;
    m_schedCounter = 0;

    this->log_ACTIVITY_HI_AutoCaptureEnabled(m_autoCaptureInterval);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void CameraManager::DISABLE_AUTO_CAPTURE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    m_autoCaptureEnabled = false;
    m_schedCounter = 0;

    this->log_ACTIVITY_HI_AutoCaptureDisabled();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ---------------------------------------------------------------------------
// Schedule handler — rate group driven auto-capture
// ---------------------------------------------------------------------------

void CameraManager::modeChangeIn_handler(FwIndexType portNum, const Orion::MissionMode& mode) {
    m_currentMode = mode;

    if (mode.e == MissionMode::MEASURE) {
        // Auto-enable capture on MEASURE entry
        m_autoCaptureEnabled = true;
        m_schedCounter = 0;
        this->log_ACTIVITY_HI_AutoCaptureEnabled(m_autoCaptureInterval);
    } else if (m_autoCaptureEnabled) {
        // Stop capturing in any other mode
        m_autoCaptureEnabled = false;
        m_schedCounter = 0;
        this->log_ACTIVITY_HI_AutoCaptureDisabled();
    }
}

void CameraManager::schedIn_handler(FwIndexType portNum, U32 context) {
    // Only auto-capture in MEASURE mode
    if (!m_autoCaptureEnabled || m_currentMode.e != MissionMode::MEASURE) {
        return;
    }

    m_schedCounter++;
    if (m_schedCounter >= m_autoCaptureInterval) {
        m_schedCounter = 0;
        doCapture();
    }
}

// ---------------------------------------------------------------------------
// Core capture-fuse-dispatch pipeline
// ---------------------------------------------------------------------------

void CameraManager::doCapture() {
    // 1. Check out a buffer from the pool.
    Fw::Buffer buf = this->bufferGetOut_out(0, IMAGE_BUFFER_SIZE);

    if (buf.getSize() == 0) {
        m_capturesFailed++;
        this->tlmWrite_CapturesFailed(m_capturesFailed);
        this->log_WARNING_HI_BufferPoolExhausted();
        return;
    }

    // 2. Fill the buffer with camera data (SimSat or test fallback).
    if (!captureIntoBuffer(buf)) {
        this->bufferReturnOut_out(0, buf);

        m_capturesFailed++;
        this->tlmWrite_CapturesFailed(m_capturesFailed);

        this->log_WARNING_HI_CameraHardwareError();
        return;
    }

    // 3. Pull GPS coordinates from NavTelemetry synchronously.
    NavState nav = this->navStateOut_out(0);

    // 4. Dispatch buffer + coordinates to VlmInferenceEngine asynchronously.
    this->inferenceRequestOut_out(0, buf, nav.get_lat(), nav.get_lon());

    // 5. Update telemetry.
    m_imagesCaptured++;
    this->tlmWrite_ImagesCaptured(m_imagesCaptured);
    this->log_ACTIVITY_HI_ImageDispatched(nav.get_lat(), nav.get_lon());
}

// ---------------------------------------------------------------------------
// Camera capture — SimSat Mapbox with test image fallback
// ---------------------------------------------------------------------------

bool CameraManager::captureIntoBuffer(Fw::Buffer& buf) {
    // Try to fetch a Mapbox image from SimSat first.
    if (SimSatClient::fetchMapboxImage(buf.getData(), IMAGE_WIDTH, IMAGE_HEIGHT)) {
        return true;
    }

    // SimSat unavailable or image not available — log and try test fallback.
    this->log_ACTIVITY_LO_SimSatImageUnavailable();

    // Fall back to test images from disk.
    char path[256];
    snprintf(path, sizeof(path), "%simage_%04u.raw", getTestImageDir(), m_imageIndex % NUM_TEST_IMAGES);
    m_imageIndex++;

    Os::File file;
    if (file.open(path, Os::File::OPEN_READ) != Os::File::OP_OK) {
        return false;
    }

    FwSizeType size = buf.getSize();
    Os::File::Status status = file.read(buf.getData(), size);
    file.close();

    if (status != Os::File::OP_OK || size != IMAGE_BUFFER_SIZE) {
        return false;
    }

    return true;
}

}  // namespace Orion
