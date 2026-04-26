# Ground Receiver

The ground receiver (`ground_segment/receiver.py`) is a TCP server that accepts downlinked image frames from the satellite's GroundCommsDriver component. It represents the ground-side endpoint of the simulated X-band radio link.

## ORIO Frame Protocol

Each downlinked frame uses a simple binary protocol with an 8-byte header:

| Offset | Size    | Field          | Description                                        |
| ------ | ------- | -------------- | -------------------------------------------------- |
| 0      | 4 bytes | Magic          | `0x4F52494F` ("ORIO" in ASCII), network byte order |
| 4      | 4 bytes | Payload length | Size of the raw image data in bytes, big-endian    |
| 8      | N bytes | Payload        | Raw pixel data (786,432 bytes for 512x512 RGB)     |

The receiver validates the magic bytes on each connection. Frames with invalid magic are rejected and the connection is closed. Each frame arrives on its own TCP connection (one-connection-per-frame model).

## Output

Files are saved to `./orion_downlink/` as `orion_frame_XXXX.raw`. Each `.raw` file contains raw RGB pixel data - 786,432 bytes for 512x512 images.

## Relationship to Flight Segment

GroundCommsDriver connects to the receiver using `ORION_GDS_HOST` and `ORION_GDS_PORT` (default `127.0.0.1:50050`). It sends frames in two scenarios:

- **Immediate downlink**: HIGH-priority frames transmitted directly during DOWNLINK mode
- **Queue flush**: previously queued HIGH frames flushed when entering DOWNLINK mode

MEDIUM-priority files are downlinked separately via the F-Prime FileDownlink service (triggered by `FLUSH_MEDIUM_STORAGE`), which uses the F-Prime ground link (port 50000) and does not go through `receiver.py`.

## Configuration

The receiver uses hardcoded values:

| Setting     | Value              | Notes                                           |
| ----------- | ------------------ | ----------------------------------------------- |
| Listen IP   | `0.0.0.0`          | Listens on all network interfaces               |
| Listen port | `50050`            | Must match the flight binary's `ORION_GDS_PORT` |
| Output dir  | `./orion_downlink` | Created automatically if it does not exist      |
| Backlog     | 5                  | Maximum pending connections                     |
| Chunk size  | 4096 bytes         | TCP read chunk size for payload reception       |

For instructions on running the receiver, see the [Receiver guide](../guides/receiver.md).
