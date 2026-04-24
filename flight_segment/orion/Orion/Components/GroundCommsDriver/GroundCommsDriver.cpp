#include "GroundCommsDriver.hpp"

#include <arpa/inet.h>
#include <dirent.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Orion {

// Ground station address.
static const char* getGdsHost() {
    const char* p = ::getenv("ORION_GDS_HOST");
    return p ? p : "127.0.0.1";
}
static U16 getGdsPort() {
    const char* p = ::getenv("ORION_GDS_PORT");
    return p ? static_cast<U16>(::atoi(p)) : 50050;
}

// Disk queue directory for frames buffered outside comm window.
static const char* getQueueDir() {
    const char* p = ::getenv("ORION_DOWNLINK_QUEUE_DIR");
    return p ? p : "/home/saransh/ORION/media/sd/downlink_queue/";
}

// 8-byte frame header prepended to every downlinked image:
//   Bytes 0-3 : magic "ORIO" (0x4F52494F) in network byte order
//   Bytes 4-7 : payload length in bytes, network byte order
static constexpr U32 FRAME_MAGIC = 0x4F52494Fu;

GroundCommsDriver::GroundCommsDriver(const char* compName)
    : GroundCommsDriverComponentBase(compName),
      m_framesDownlinked(0),
      m_bytesDownlinked(0),
      m_transmitFailures(0),
      m_framesQueued(0),
      m_queueFileIndex(0),
      m_currentMode(MissionMode::IDLE) {}

GroundCommsDriver::~GroundCommsDriver() {}

// ---------------------------------------------------------------------------
// Port handler
// ---------------------------------------------------------------------------

void GroundCommsDriver::fileDownlinkIn_handler(FwIndexType portNum, Fw::Buffer& buffer, const Fw::StringBase& reason) {
    if (m_currentMode.e == MissionMode::DOWNLINK) {
        // In comm window — flush any previously queued frames first
        U32 flushed = flushQueue();
        if (flushed > 0) {
            this->log_ACTIVITY_HI_QueueFlushed(flushed);
        }

        // Transmit the current frame
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
    } else {
        // Outside comm window — save to disk queue
        saveToQueue(buffer);
        m_framesQueued++;
        this->tlmWrite_FramesQueued(m_framesQueued);
        this->log_ACTIVITY_LO_FrameQueued();
    }

    // Always return the buffer to the pool.
    this->bufferReturnOut_out(0, buffer);
}

// ---------------------------------------------------------------------------
// Schedule handler — periodic queue flush
// ---------------------------------------------------------------------------

void GroundCommsDriver::modeChangeIn_handler(FwIndexType portNum, const Orion::MissionMode& mode) {
    m_currentMode = mode;

    // When entering DOWNLINK, immediately flush the queue
    if (mode.e == MissionMode::DOWNLINK) {
        U32 flushed = flushQueue();
        if (flushed > 0) {
            this->log_ACTIVITY_HI_QueueFlushed(flushed);
        }
    }
}

void GroundCommsDriver::schedIn_handler(FwIndexType portNum, U32 context) {
    if (m_currentMode.e == MissionMode::DOWNLINK) {
        U32 flushed = flushQueue();
        if (flushed > 0) {
            this->log_ACTIVITY_HI_QueueFlushed(flushed);
        }
    }
}

// ---------------------------------------------------------------------------
// Transmit helpers
// ---------------------------------------------------------------------------

bool GroundCommsDriver::transmit(const Fw::Buffer& buffer) { return transmitRaw(buffer.getData(), buffer.getSize()); }

bool GroundCommsDriver::transmitRaw(const U8* data, FwSizeType size) {
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
    U32 payloadLen = htonl(static_cast<U32>(size));
    ::memcpy(header, &magic, 4);
    ::memcpy(header + 4, &payloadLen, 4);

    if (::send(sock, header, sizeof(header), 0) != static_cast<ssize_t>(sizeof(header))) {
        ::close(sock);
        return false;
    }

    // Send payload in a loop to handle partial writes.
    FwSizeType remaining = size;
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

// ---------------------------------------------------------------------------
// Disk queue
// ---------------------------------------------------------------------------

void GroundCommsDriver::saveToQueue(const Fw::Buffer& buffer) {
    // Ensure queue directory exists
    ::mkdir(getQueueDir(), 0755);

    char path[256];
    snprintf(path, sizeof(path), "%sorion_queued_%05u.raw", getQueueDir(), m_queueFileIndex++);

    FILE* f = ::fopen(path, "wb");
    if (!f) {
        this->log_WARNING_HI_QueueWriteFailed();
        return;
    }
    ::fwrite(buffer.getData(), 1, buffer.getSize(), f);
    ::fclose(f);
}

U32 GroundCommsDriver::flushQueue() {
    DIR* dir = ::opendir(getQueueDir());
    if (!dir) {
        return 0;
    }

    U32 count = 0;
    struct dirent* entry;

    while ((entry = ::readdir(dir)) != nullptr) {
        // Only process our queued files
        if (strncmp(entry->d_name, "orion_queued_", 13) != 0) {
            continue;
        }

        char path[256];
        snprintf(path, sizeof(path), "%s%s", getQueueDir(), entry->d_name);

        // Read file into a temporary stack buffer
        FILE* f = ::fopen(path, "rb");
        if (!f) continue;

        ::fseek(f, 0, SEEK_END);
        long fileSize = ::ftell(f);
        ::fseek(f, 0, SEEK_SET);

        if (fileSize <= 0 || fileSize > 1024 * 1024) {
            // Skip invalid files (>1MB safety check)
            ::fclose(f);
            ::unlink(path);
            continue;
        }

        U8* tmpBuf = new U8[static_cast<size_t>(fileSize)];
        size_t bytesRead = ::fread(tmpBuf, 1, static_cast<size_t>(fileSize), f);
        ::fclose(f);

        bool sent = false;
        if (bytesRead == static_cast<size_t>(fileSize)) {
            if (transmitRaw(tmpBuf, static_cast<FwSizeType>(fileSize))) {
                m_framesDownlinked++;
                m_bytesDownlinked += static_cast<U32>(fileSize);
                count++;
                sent = true;
            }
        }

        delete[] tmpBuf;

        if (sent) {
            ::unlink(path);  // Only delete after successful transmit
        } else {
            // Stop flushing — receiver likely down, no point retrying rest
            break;
        }
    }

    ::closedir(dir);

    if (count > 0) {
        this->tlmWrite_FramesDownlinked(m_framesDownlinked);
        this->tlmWrite_BytesDownlinked(m_bytesDownlinked);
    }

    return count;
}

}  // namespace Orion
