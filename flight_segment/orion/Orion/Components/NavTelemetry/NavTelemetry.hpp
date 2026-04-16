#ifndef ORION_NAV_TELEMETRY_HPP
#define ORION_NAV_TELEMETRY_HPP

#include "Orion/Components/NavTelemetry/NavTelemetryComponentAc.hpp"

namespace Orion {

class NavTelemetry final : public NavTelemetryComponentBase {
  public:
    explicit NavTelemetry(const char* compName);
    ~NavTelemetry();

  private:
    // -----------------------------------------------------------------------
    // Port handler
    // -----------------------------------------------------------------------

    //! Returns the last cached NavState to the caller (CameraManager).
    NavState navStateGet_handler(FwIndexType portNum) override;

    // -----------------------------------------------------------------------
    // Command handler
    // -----------------------------------------------------------------------

    //! Updates the stored position. Called from GDS in the Pi 5 demo.
    void SET_POSITION_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, F64 lat, F64 lon) override;

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    F64 m_lat;
    F64 m_lon;
};

}  // namespace Orion

#endif  // ORION_NAV_TELEMETRY_HPP
