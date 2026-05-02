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
    // Port handlers
    // -----------------------------------------------------------------------

    //! Returns the last cached NavState to the caller.
    NavState navStateGet_handler(FwIndexType portNum) override;

    //! Rate group schedule handler: polls SimSat periodically.
    void schedIn_handler(FwIndexType portNum, U32 context) override;

    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    //! Poll SimSat for current position and update state.
    void pollSimSat();

    //! Compute great-circle distance (Haversine) between two lat/lon points.
    //! Returns distance in kilometers.
    static F64 haversineDistanceKm(F64 lat1, F64 lon1, F64 lat2, F64 lon2);

    //! Update comm window state based on current position vs ground station.
    void updateCommWindow();

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    F64 m_lat;
    F64 m_lon;
    F64 m_alt;
    F64 m_gsDistanceKm;  ///< Cached great-circle distance to ground station
    bool m_inCommWindow;
    U32 m_schedCounter;  ///< Counts schedIn ticks for polling interval

    // Ground station configuration (read from env vars at construction)
    F64 m_gsLat;
    F64 m_gsLon;
    F64 m_gsRangeKm;
};

}  // namespace Orion

#endif  // ORION_NAV_TELEMETRY_HPP
