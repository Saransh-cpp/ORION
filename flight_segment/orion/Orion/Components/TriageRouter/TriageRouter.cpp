#include "TriageRouter.hpp"

#include <cstdio>

#include "Os/File.hpp"

namespace Orion {

// Root path for MEDIUM bulk storage on the Pi 5 microSD card.
static constexpr const char* MEDIUM_STORAGE_PATH = "/media/sd/orion/medium/";

TriageRouter::TriageRouter(const char* compName)
    : TriageRouterComponentBase(compName), m_highRouted(0), m_mediumSaved(0), m_lowDiscarded(0), m_mediumFileIndex(0) {}

TriageRouter::~TriageRouter() {}

// ---------------------------------------------------------------------------
// Port handler — dispatches to the appropriate routing arm
// ---------------------------------------------------------------------------

void TriageRouter::triageDecisionIn_handler(FwIndexType portNum, const Orion::TriagePriority& verdict,
                                            const Fw::StringBase& reason, Fw::Buffer& buffer) {
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
    // Build a unique filename for this frame on the microSD card.
    char path[128];
    snprintf(path, sizeof(path), "%sorion_medium_%05u.raw", MEDIUM_STORAGE_PATH, m_mediumFileIndex++);

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

    if (status != Os::File::OP_OK) {
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
