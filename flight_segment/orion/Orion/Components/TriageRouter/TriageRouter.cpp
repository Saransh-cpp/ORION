#include "TriageRouter.hpp"

#include <sys/stat.h>

#include <cstdio>
#include <cstdlib>

#include "Os/File.hpp"

namespace Orion {

// Root path for MEDIUM bulk storage.
// Override via env var for Mac dev; defaults to Pi 5 microSD path.
static const char* getMediumStoragePath() {
    const char* p = ::getenv("ORION_MEDIUM_STORAGE_DIR");
    return p ? p : "/media/sd/orion/medium/";
}

TriageRouter::TriageRouter(const char* compName)
    : TriageRouterComponentBase(compName),
      m_highRouted(0),
      m_mediumSaved(0),
      m_lowDiscarded(0),
      m_mediumFileIndex(0),
      m_currentMode(MissionMode::IDLE) {}

TriageRouter::~TriageRouter() {}

// ---------------------------------------------------------------------------
// Port handler — dispatches to the appropriate routing arm
// ---------------------------------------------------------------------------

void TriageRouter::modeChangeIn_handler(FwIndexType portNum, const Orion::MissionMode& mode) { m_currentMode = mode; }

void TriageRouter::triageDecisionIn_handler(FwIndexType portNum, const Orion::TriagePriority& verdict,
                                            const Fw::StringBase& reason, Fw::Buffer& buffer) {
    // In SAFE mode, drop all frames immediately
    if (m_currentMode.e == MissionMode::SAFE) {
        this->bufferReturnOut_out(0, buffer);
        this->log_ACTIVITY_LO_LowTargetDiscarded();
        return;
    }

    switch (verdict.e) {
        case TriagePriority::HIGH:
            routeHigh(reason, buffer);
            break;
        case TriagePriority::MEDIUM:
            routeMedium(buffer);
            break;
        case TriagePriority::LOW:
        default:
            routeLow(buffer);
            break;
    }
}

// ---------------------------------------------------------------------------
// Routing arms
// ---------------------------------------------------------------------------

void TriageRouter::routeHigh(const Fw::StringBase& reason, Fw::Buffer& buffer) {
    // Forward to GroundCommsDriver — buffer ownership transfers to the driver,
    // which is responsible for returning it to the pool after transmission.
    this->fileDownlinkOut_out(0, buffer, reason);

    m_highRouted++;
    this->tlmWrite_HighTargetsRouted(m_highRouted);
    this->log_ACTIVITY_HI_HighTargetDetected(reason);
}

void TriageRouter::routeMedium(Fw::Buffer& buffer) {
    // Ensure storage directory exists and build a unique filename.
    const char* storageDir = getMediumStoragePath();
    ::mkdir(storageDir, 0755);

    char path[256];
    snprintf(path, sizeof(path), "%sorion_medium_%05u.raw", storageDir, m_mediumFileIndex++);

    Os::File file;
    Os::File::Status status = file.open(path, Os::File::OPEN_WRITE);

    if (status != Os::File::OP_OK) {
        this->log_WARNING_HI_StorageWriteFailed();
        this->bufferReturnOut_out(0, buffer);
        return;
    }

    FwSizeType bytesWritten = buffer.getSize();
    status = file.write(buffer.getData(), bytesWritten);
    file.close();

    if (status != Os::File::OP_OK || bytesWritten != buffer.getSize()) {
        this->log_WARNING_HI_StorageWriteFailed();
    } else {
        m_mediumSaved++;
        this->tlmWrite_MediumTargetsSaved(m_mediumSaved);
        this->log_ACTIVITY_LO_MediumTargetStored();
    }

    // Return the buffer to the pool regardless of write outcome.
    this->bufferReturnOut_out(0, buffer);
}

void TriageRouter::routeLow(Fw::Buffer& buffer) {
    // Drop the image — return the buffer to the pool immediately.
    this->bufferReturnOut_out(0, buffer);

    m_lowDiscarded++;
    this->tlmWrite_LowTargetsDiscarded(m_lowDiscarded);
    this->log_ACTIVITY_LO_LowTargetDiscarded();
}

}  // namespace Orion
