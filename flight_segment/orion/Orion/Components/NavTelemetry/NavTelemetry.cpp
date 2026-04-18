#include "NavTelemetry.hpp"

#include <cmath>
#include <cstdlib>

#include "Utils/SimSatClient.hpp"

namespace Orion {

// Poll SimSat every POLL_INTERVAL_TICKS schedIn calls.
// At 1 Hz rate group this means every 5 seconds.
static constexpr U32 POLL_INTERVAL_TICKS = 5;

// Earth's mean radius in km (for Haversine calculation)
static constexpr F64 EARTH_RADIUS_KM = 6371.0;

// Env var helpers for ground station config
static F64 getGsLat() {
    const char* p = ::getenv("ORION_GS_LAT");
    return p ? ::atof(p) : 40.0;
}
static F64 getGsLon() {
    const char* p = ::getenv("ORION_GS_LON");
    return p ? ::atof(p) : -74.0;
}
static F64 getGsRangeKm() {
    const char* p = ::getenv("ORION_GS_RANGE_KM");
    return p ? ::atof(p) : 2000.0;
}

NavTelemetry::NavTelemetry(const char* compName)
    : NavTelemetryComponentBase(compName),
      m_lat(0.0),
      m_lon(0.0),
      m_alt(0.0),
      m_gsDistanceKm(0.0),
      m_inCommWindow(false),
      m_schedCounter(0),
      m_gsLat(getGsLat()),
      m_gsLon(getGsLon()),
      m_gsRangeKm(getGsRangeKm()) {}

NavTelemetry::~NavTelemetry() {}

// ---------------------------------------------------------------------------
// Port handler — synchronous getter
// ---------------------------------------------------------------------------

NavState NavTelemetry::navStateGet_handler(FwIndexType portNum) {
    NavState state;
    state.set_lat(m_lat);
    state.set_lon(m_lon);
    state.set_alt(m_alt);
    state.set_inCommWindow(m_inCommWindow);
    state.set_gsDistanceKm(m_gsDistanceKm);
    return state;
}

// ---------------------------------------------------------------------------
// Schedule handler — rate group driven
// ---------------------------------------------------------------------------

void NavTelemetry::schedIn_handler(FwIndexType portNum, U32 context) {
    m_schedCounter++;
    if (m_schedCounter < POLL_INTERVAL_TICKS) {
        return;
    }
    m_schedCounter = 0;

    pollSimSat();

    // Always update comm window (even in manual override mode)
    updateCommWindow();

    // Emit telemetry every poll cycle
    this->tlmWrite_CurrentLat(m_lat);
    this->tlmWrite_CurrentLon(m_lon);
    this->tlmWrite_CurrentAlt(m_alt);
    this->tlmWrite_InCommWindow(m_inCommWindow);
}

// ---------------------------------------------------------------------------
// SimSat polling
// ---------------------------------------------------------------------------

void NavTelemetry::pollSimSat() {
    F64 lat = 0.0, lon = 0.0, alt = 0.0;

    if (!SimSatClient::fetchPosition(lat, lon, alt)) {
        this->log_WARNING_HI_SimSatConnectionFailed();
        return;
    }

    m_lat = lat;
    m_lon = lon;
    m_alt = alt;

    this->log_ACTIVITY_LO_SimSatPositionUpdate(m_lat, m_lon, m_alt);
}

// ---------------------------------------------------------------------------
// Comm window logic
// ---------------------------------------------------------------------------

F64 NavTelemetry::haversineDistanceKm(F64 lat1, F64 lon1, F64 lat2, F64 lon2) {
    // Convert degrees to radians
    F64 dLat = (lat2 - lat1) * M_PI / 180.0;
    F64 dLon = (lon2 - lon1) * M_PI / 180.0;
    F64 lat1Rad = lat1 * M_PI / 180.0;
    F64 lat2Rad = lat2 * M_PI / 180.0;

    F64 a = sin(dLat / 2.0) * sin(dLat / 2.0) + cos(lat1Rad) * cos(lat2Rad) * sin(dLon / 2.0) * sin(dLon / 2.0);
    F64 c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));

    return EARTH_RADIUS_KM * c;
}

void NavTelemetry::updateCommWindow() {
    m_gsDistanceKm = haversineDistanceKm(m_lat, m_lon, m_gsLat, m_gsLon);

    // Hysteresis: enter comm window at gsRange, exit at gsRange * 1.1.
    // Prevents oscillation when the satellite is near the boundary.
    if (m_inCommWindow) {
        m_inCommWindow = (m_gsDistanceKm < m_gsRangeKm * 1.1);
    } else {
        m_inCommWindow = (m_gsDistanceKm < m_gsRangeKm);
    }
}

}  // namespace Orion
