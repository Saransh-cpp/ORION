"""ORION ground station receiver for HIGH-priority satellite image downlink.

Listens on TCP port 50050 for ORIO-framed image transmissions from the
flight segment's GroundCommsDriver. Each connection carries one frame:
an 8-byte header (4-byte ``ORIO`` magic + 4-byte big-endian payload length)
followed by raw pixel data (typically 786,432 bytes for a 512x512 RGB image).

Received frames are saved sequentially to the ``orion_downlink/`` directory
as both ``orion_frame_XXXX.raw`` (original bytes) and ``orion_frame_XXXX.jpg``
(viewable image).

Usage:

```bash
cd ground_segment
uv run receiver.py
```

See the [ORIO frame protocol](../../../ground-segment/receiver/) documentation
for the full wire format specification.
"""

import socket
import struct
import os

import numpy as np
from PIL import Image

LISTEN_IP = "0.0.0.0"
LISTEN_PORT = 50050
OUTPUT_DIR = "./orion_downlink"
IMAGE_W, IMAGE_H = 512, 512
EXPECTED_SIZE = IMAGE_W * IMAGE_H * 3


def recv_exact(sock: socket.socket, n: int) -> bytes:
    """Read exactly *n* bytes from *sock*, returning the accumulated buffer.

    Args:
        sock: Connected TCP socket to read from.
        n: Number of bytes to read.

    Returns:
        A ``bytes`` object of length *n*, or shorter if the peer closed
        the connection before all bytes were delivered.
    """
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            break
        buf += chunk
    return buf


def raw_to_jpg(raw_path: str, payload_len: int) -> str | None:
    """Convert a raw RGB file to JPG if it matches the expected 512x512x3 size.

    Args:
        raw_path: Path to the ``.raw`` file just written.
        payload_len: Size of the payload in bytes.

    Returns:
        The JPG file path on success, or ``None`` if the size doesn't match.
    """
    if payload_len != EXPECTED_SIZE:
        return None
    data = open(raw_path, "rb").read()
    img = Image.fromarray(
        np.frombuffer(data, dtype=np.uint8).reshape((IMAGE_H, IMAGE_W, 3))
    )
    jpg_path = raw_path.replace(".raw", ".jpg")
    img.save(jpg_path)
    return jpg_path


def start_receiver() -> None:
    """Start the ORIO ground station receiver loop.

    Binds a TCP server on ``LISTEN_IP:LISTEN_PORT``, accepts one connection
    at a time, validates the ORIO header, reads the image payload, writes
    the raw bytes to disk, and converts 512x512 RGB frames to JPG.
    Runs indefinitely until interrupted.
    """
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((LISTEN_IP, LISTEN_PORT))
    server.listen(5)

    print(f"ORION Ground Station listening on {LISTEN_IP}:{LISTEN_PORT}...")

    image_counter = 0

    while True:
        client_sock, addr = server.accept()
        print(f"\n[+] Connection established with satellite at {addr[0]}")

        try:
            header = recv_exact(client_sock, 8)

            if len(header) < 8:
                print("[-] Incomplete header received. Dropping.")
                client_sock.close()
                continue

            magic, payload_len = struct.unpack("!4sI", header)

            if magic != b"ORIO":
                print(f"[-] Invalid magic word: {magic}. Dropping frame.")
                client_sock.close()
                continue

            print(f"[*] Valid ORIO frame detected. Payload size: {payload_len} bytes.")

            payload = recv_exact(client_sock, payload_len)

            if len(payload) == payload_len:
                raw_path = os.path.join(
                    OUTPUT_DIR, f"orion_frame_{image_counter:04d}.raw"
                )
                with open(raw_path, "wb") as f:
                    f.write(payload)

                jpg_path = raw_to_jpg(raw_path, payload_len)
                if jpg_path:
                    print(f"[+] Saved {raw_path} + {jpg_path} (512x512 RGB)")
                else:
                    print(
                        f"[+] Saved {raw_path} ({payload_len} bytes, non-standard size)"
                    )

                image_counter += 1
            else:
                print(
                    f"[-] Connection dropped mid-payload. Got {len(payload)}/{payload_len} bytes."
                )

        except Exception as e:
            print(f"[-] Error receiving frame: {e}")
        finally:
            client_sock.close()


if __name__ == "__main__":
    start_receiver()
