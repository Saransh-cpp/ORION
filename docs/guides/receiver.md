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

Received frames are saved to the `./orion_downlink/` directory (created automatically):

```
orion_downlink/
    orion_frame_0000.raw
    orion_frame_0001.raw
    orion_frame_0002.raw
    ...
```

The receiver logs each successful frame:

```
[+] Connection established with satellite at 192.168.1.100
[*] Valid ORIO frame detected. Payload size: 786432 bytes.
[+] Success! Saved to ./orion_downlink/orion_frame_0000.raw (512x512 RGB)
```

## Converting Raw Files to PNG

The raw files contain flat RGB byte arrays and cannot be opened directly by image viewers. Use the `visualise_downlink.py` utility to convert them:

```bash
cd ground_segment/data
python visualise_downlink.py
```

The script scans `./orion_downlink` and `./orion_medium` for `.raw` files. For each valid 786,432-byte file (512 x 512 x 3), it saves a PNG with the same base name:

```
./orion_downlink/
    orion_frame_0000.raw -> orion_frame_0000.png

./orion_medium/
    orion_frame_0001.raw -> orion_frame_0001.png
```

Files that do not match the expected size are skipped with a warning.

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
