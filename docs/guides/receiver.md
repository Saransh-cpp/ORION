# Ground Receiver

> For design details - ORIO frame protocol, frame structure, field descriptions, and network configuration - see the [Receiver architecture](../ground-segment/receiver.md) page.

## Running the Receiver

```bash
cd ground_segment
python receiver.py
```

The receiver starts listening on `0.0.0.0:50050` and prints:

```
ORION Ground Station listening on 0.0.0.0:50050...
```

It runs indefinitely, accepting one connection at a time in a blocking loop. Press Ctrl+C to stop.

## Expected Output

Received frames are saved to the `./downlinked_XBand/` directory (created automatically). Each 512x512 RGB frame is saved as both the original `.raw` bytes and a viewable `.jpg`:

```
downlinked_XBand/
    orion_frame_0000.raw
    orion_frame_0000.jpg
    orion_frame_0001.raw
    orion_frame_0001.jpg
    ...
```

The receiver logs each successful frame:

```
[+] Connection established with satellite at 192.168.1.100
[*] Valid ORIO frame detected. Payload size: 786432 bytes.
[+] Saved ./downlinked_XBand/orion_frame_0000.raw + ./downlinked_XBand/orion_frame_0000.jpg (512x512 RGB)
```

## Testing Without a Satellite

You can test the receiver by sending a raw frame with a simple Python script:

```python
import socket
import struct
import numpy as np

# Generate a test image (512x512 random RGB)
image_data = np.random.randint(0, 256, (512, 512, 3), dtype=np.uint8).tobytes()

# Build the ORIO frame
header = struct.pack("!4sI", b"ORIO", len(image_data))

# Send to the receiver
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(("127.0.0.1", 50050))
sock.sendall(header + image_data)
sock.close()
```
