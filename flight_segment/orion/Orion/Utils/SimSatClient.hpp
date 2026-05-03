#ifndef ORION_SIMSAT_CLIENT_HPP
#define ORION_SIMSAT_CLIENT_HPP

#include <cstddef>
#include <cstdint>

namespace Orion {

/// Plain C++ utility wrapping libcurl to fetch data from the SimSat REST API.
/// Not an F-Prime component; hence, called directly by NavTelemetry and CameraManager.
/// Uses standard C++ types to avoid depending on F-Prime headers.
class SimSatClient {
  public:
    /// Fetch the current satellite position from SimSat.
    /// GET /data/current/position -> {"lon-lat-alt": [lon, lat, alt], ...}
    /// Returns true on success, false on HTTP or parse failure.
    static bool fetchPosition(double& lat, double& lon, double& alt);

    /// Fetch a Mapbox image for the current satellite position from SimSat,
    /// decode the PNG, resize to outWidth x outHeight, and write RGB bytes
    /// into the caller's buffer.
    /// GET /data/current/image/mapbox -> PNG bytes + mapbox_metadata header
    /// Returns true on success (image available and decoded), false otherwise.
    static bool fetchMapboxImage(uint8_t* rgbOut, uint32_t outWidth, uint32_t outHeight);

  private:
    /// Returns the SimSat base URL from env var ORION_SIMSAT_URL,
    /// defaulting to "http://localhost:9005".
    static const char* getBaseUrl();

    /// libcurl write callback: appends received data to a std::string buffer.
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);

    /// libcurl header callback: captures the mapbox_metadata header value.
    static size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userp);
};

}  // namespace Orion

#endif  // ORION_SIMSAT_CLIENT_HPP
