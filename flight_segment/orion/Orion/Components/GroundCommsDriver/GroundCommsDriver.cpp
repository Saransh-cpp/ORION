#include "GroundCommsDriver.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>

namespace Orion {

// Ground station address.
// Override via env vars: ORION_getGdsHost(), ORION_getGdsPort()
static const char* getGdsHost() {
    const char* p = ::getenv("ORION_getGdsHost()");
    return p ? p : "127.0.0.1";
}
static U16 getGdsPort() {
    const char* p = ::getenv("ORION_getGdsPort()");
    return p ? static_cast<U16>(::atoi(p)) : 50000;
}

// 8-byte frame header prepended to every downlinked image:
//   Bytes 0-3 : magic "ORIO" (0x4F52494F) in network byte order
//   Bytes 4-7 : payload length in bytes, network byte order
static constexpr U32 FRAME_MAGIC = 0x4F52494Fu;

GroundCommsDriver::GroundCommsDriver(const char* compName)
    : GroundCommsDriverComponentBase(compName), m_framesDownlinked(0), m_bytesDownlinked(0), m_transmitFailures(0) {}

GroundCommsDriver::~GroundCommsDriver() {}

// ---------------------------------------------------------------------------
// Port handler
// ---------------------------------------------------------------------------

void GroundCommsDriver::fileDownlinkIn_handler(FwIndexType portNum, Fw::Buffer& buffer, const Fw::StringBase& reason) {
    if (transmit(buffer)) {
        m_framesDownlinked++;
        m_bytesDownlinked += static_cast<U32>(buffer.getSize());
        this->tlmWrite_FramesDownlinked(m_framesDownlinked);
        this->tlmWrite_BytesDownlinked(m_bytesDownlinked);
        this->log_ACTIVITY_HI_FrameDownlinked(reason);
    } else {
        m_transmitFailures++;
        this->tlmWrite_TransmitFailures(m_transmitFailures);
        this->log_WARNING_HI_TransmitFailed();
    }

    // Always return the buffer to the pool — TriageRouter transferred ownership here.
    this->bufferReturnOut_out(0, buffer);
}

// ---------------------------------------------------------------------------
// Transmit helper
// ---------------------------------------------------------------------------

bool GroundCommsDriver::transmit(const Fw::Buffer& buffer) {
    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return false;
    }

    struct sockaddr_in addr;
    ::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(getGdsPort());

    if (::inet_pton(AF_INET, getGdsHost(), &addr.sin_addr) <= 0) {
        ::close(sock);
        return false;
    }

    if (::connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(sock);
        return false;
    }

    // Send frame header.
    U8 header[8];
    U32 magic = htonl(FRAME_MAGIC);
    U32 payloadLen = htonl(static_cast<U32>(buffer.getSize()));
    ::memcpy(header, &magic, 4);
    ::memcpy(header + 4, &payloadLen, 4);

    if (::send(sock, header, sizeof(header), 0) != static_cast<ssize_t>(sizeof(header))) {
        ::close(sock);
        return false;
    }

    // Send payload in a loop to handle partial writes.
    const U8* data = buffer.getData();
    FwSizeType remaining = buffer.getSize();
    bool ok = true;

    while (remaining > 0) {
        ssize_t sent = ::send(sock, data, remaining, 0);
        if (sent <= 0) {
            ok = false;
            break;
        }
        data += sent;
        remaining -= static_cast<FwSizeType>(sent);
    }

    ::close(sock);
    return ok;
}

}  // namespace Orion
