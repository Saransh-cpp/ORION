#ifndef ORION_GROUND_COMMS_DRIVER_HPP
#define ORION_GROUND_COMMS_DRIVER_HPP

#include "Orion/Components/GroundCommsDriver/GroundCommsDriverComponentAc.hpp"

namespace Orion {

class GroundCommsDriver final : public GroundCommsDriverComponentBase {
  public:
    explicit GroundCommsDriver(const char* compName);
    ~GroundCommsDriver();

  private:
    // -----------------------------------------------------------------------
    // Port handler
    // -----------------------------------------------------------------------

    void fileDownlinkIn_handler(FwIndexType portNum, Fw::Buffer& buffer, const Fw::StringBase& reason) override;

    //! Rate group handler — flushes disk queue when in DOWNLINK mode.
    void schedIn_handler(FwIndexType portNum, U32 context) override;

    //! Mode change handler — stores the current mission mode.
    void modeChangeIn_handler(FwIndexType portNum, const Orion::MissionMode& mode) override;

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    //! Opens a TCP connection to the ground station, sends the frame header
    //! and raw pixel payload, then closes the socket. Returns true on success.
    bool transmit(const Fw::Buffer& buffer);

    //! Transmit raw bytes from a memory buffer (used for queue flush).
    bool transmitRaw(const U8* data, FwSizeType size);

    //! Save a buffer to the disk queue for later transmission.
    void saveToQueue(const Fw::Buffer& buffer);

    //! Flush all queued frames from disk (called when comm window opens).
    //! Returns the number of frames successfully transmitted.
    U32 flushQueue();

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    U32 m_framesDownlinked;
    U32 m_bytesDownlinked;
    U32 m_transmitFailures;
    U32 m_framesQueued;
    U32 m_queueFileIndex;       //!< Monotonic counter for queue filenames
    MissionMode m_currentMode;  //!< Current mission mode from EventAction
};

}  // namespace Orion

#endif  // ORION_GROUND_COMMS_DRIVER_HPP
