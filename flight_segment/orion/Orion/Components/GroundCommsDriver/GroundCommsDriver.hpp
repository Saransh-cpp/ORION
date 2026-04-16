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

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    //! Opens a TCP connection to the ground station, sends the frame header
    //! and raw pixel payload, then closes the socket. Returns true on success.
    bool transmit(const Fw::Buffer& buffer);

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    U32 m_framesDownlinked;
    U32 m_bytesDownlinked;
    U32 m_transmitFailures;
};

}  // namespace Orion

#endif  // ORION_GROUND_COMMS_DRIVER_HPP
