#include "NavTelemetry.hpp"

namespace Orion {

NavTelemetry::NavTelemetry(const char* compName) : NavTelemetryComponentBase(compName), m_lat(0.0), m_lon(0.0) {}

NavTelemetry::~NavTelemetry() {}

// ---------------------------------------------------------------------------
// Port handler
// ---------------------------------------------------------------------------

NavState NavTelemetry::navStateGet_handler(FwIndexType portNum) {
    NavState state;
    state.set_lat(m_lat);
    state.set_lon(m_lon);
    return state;
}

// ---------------------------------------------------------------------------
// Command handler
// ---------------------------------------------------------------------------

void NavTelemetry::SET_POSITION_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, F64 lat, F64 lon) {
    m_lat = lat;
    m_lon = lon;

    this->tlmWrite_CurrentLat(m_lat);
    this->tlmWrite_CurrentLon(m_lon);
    this->log_ACTIVITY_HI_PositionUpdated(m_lat, m_lon);

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Orion
