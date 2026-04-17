#include "SimSatClient.hpp"

#include <curl/curl.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

// stb_image declarations only — the implementation is already compiled
// inside libmtmd.a (llama.cpp's multimodal library).
#include "Vendor/stb_image.h"

// stb_image_resize2 implementation — not in llama.cpp, compiled here.
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "Vendor/stb_image_resize2.h"

namespace Orion {

// ---------------------------------------------------------------------------
// Env var helpers
// ---------------------------------------------------------------------------

const char* SimSatClient::getBaseUrl() {
    const char* p = ::getenv("ORION_SIMSAT_URL");
    return p ? p : "http://localhost:9005";
}

// ---------------------------------------------------------------------------
// libcurl callbacks
// ---------------------------------------------------------------------------

size_t SimSatClient::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    auto* buf = static_cast<std::string*>(userp);
    buf->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

size_t SimSatClient::headerCallback(char* buffer, size_t size, size_t nitems, void* userp) {
    size_t totalSize = size * nitems;
    auto* metadataOut = static_cast<std::string*>(userp);

    // Look for the mapbox_metadata header
    const char* prefix = "mapbox_metadata: ";
    size_t prefixLen = strlen(prefix);

    if (totalSize > prefixLen && strncasecmp(buffer, prefix, prefixLen) == 0) {
        // Extract the value (strip trailing \r\n)
        const char* val = buffer + prefixLen;
        size_t valLen = totalSize - prefixLen;
        while (valLen > 0 && (val[valLen - 1] == '\r' || val[valLen - 1] == '\n')) {
            valLen--;
        }
        metadataOut->assign(val, valLen);
    }

    return totalSize;
}

// ---------------------------------------------------------------------------
// Simple JSON helpers (avoids pulling in a full JSON library)
// The SimSat responses are simple enough to parse with string operations.
// ---------------------------------------------------------------------------

/// Extract a JSON array of numbers from a key like "lon-lat-alt": [1.0, 2.0, 3.0]
static bool parseJsonNumberArray(const std::string& json, const char* key, double* out, int count) {
    std::string needle = std::string("\"") + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return false;

    // Find the opening bracket
    pos = json.find('[', pos);
    if (pos == std::string::npos) return false;
    pos++;  // skip '['

    for (int i = 0; i < count; i++) {
        // Skip whitespace
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n')) pos++;
        if (pos >= json.size()) return false;

        char* end = nullptr;
        out[i] = strtod(json.c_str() + pos, &end);
        if (end == json.c_str() + pos) return false;  // no number parsed
        pos = static_cast<size_t>(end - json.c_str());

        // Skip comma
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == ',')) pos++;
    }
    return true;
}

/// Extract a JSON boolean value for a given key
static bool parseJsonBool(const std::string& json, const char* key, bool& out) {
    std::string needle = std::string("\"") + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return false;

    pos = json.find(':', pos);
    if (pos == std::string::npos) return false;
    pos++;

    // Skip whitespace
    while (pos < json.size() && json[pos] == ' ') pos++;

    if (json.compare(pos, 4, "true") == 0) {
        out = true;
        return true;
    } else if (json.compare(pos, 5, "false") == 0) {
        out = false;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool SimSatClient::fetchPosition(double& lat, double& lon, double& alt) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string url = std::string(getBaseUrl()) + "/data/current/position";
    std::string responseBody;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || httpCode != 200) {
        return false;
    }

    // Parse: {"lon-lat-alt": [lon, lat, alt], "timestamp": "..."}
    double coords[3];
    if (!parseJsonNumberArray(responseBody, "lon-lat-alt", coords, 3)) {
        return false;
    }

    // SimSat returns [longitude, latitude, altitude]
    lon = coords[0];
    lat = coords[1];
    alt = coords[2];
    return true;
}

bool SimSatClient::fetchMapboxImage(uint8_t* rgbOut, uint32_t outWidth, uint32_t outHeight) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string url = std::string(getBaseUrl()) + "/data/current/image/mapbox";
    std::string responseBody;
    std::string metadataHeader;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &metadataHeader);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || httpCode != 200) {
        return false;
    }

    // Check metadata header for image availability
    if (!metadataHeader.empty()) {
        bool imageAvailable = false;
        bool targetVisible = false;

        parseJsonBool(metadataHeader, "image_available", imageAvailable);
        parseJsonBool(metadataHeader, "target_visible", targetVisible);

        if (!imageAvailable || !targetVisible) {
            return false;
        }
    }

    // Decode PNG from response body
    int imgW = 0, imgH = 0, imgChannels = 0;
    unsigned char* decoded = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(responseBody.data()),
                                                   static_cast<int>(responseBody.size()), &imgW, &imgH, &imgChannels,
                                                   3  // force RGB output
    );

    if (!decoded) {
        return false;
    }

    // Resize to target dimensions if needed
    if (static_cast<uint32_t>(imgW) == outWidth && static_cast<uint32_t>(imgH) == outHeight) {
        // No resize needed — direct copy
        memcpy(rgbOut, decoded, outWidth * outHeight * 3);
    } else {
        // Resize using stb_image_resize2
        stbir_resize_uint8_linear(decoded, imgW, imgH, 0, rgbOut, static_cast<int>(outWidth),
                                  static_cast<int>(outHeight), 0, STBIR_RGB);
    }

    stbi_image_free(decoded);
    return true;
}

}  // namespace Orion
