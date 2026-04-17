import socket
import struct
import os

# Configuration
LISTEN_IP = "0.0.0.0"  # Listen on all network interfaces
LISTEN_PORT = 50050
OUTPUT_DIR = "./orion_downlink"


def start_receiver():
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # Create a TCP socket
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
            # 1. Read the 8-byte header
            header = b""
            while len(header) < 8:
                chunk = client_sock.recv(8 - len(header))
                if not chunk:
                    break
                header += chunk

            if len(header) < 8:
                print("[-] Incomplete header received. Dropping.")
                client_sock.close()
                continue

            # 2. Unpack the header
            # '!4sI' means: Network byte order (!), 4-byte string (4s), Unsigned 32-bit int (I)
            magic, payload_len = struct.unpack("!4sI", header)

            if magic != b"ORIO":
                print(f"[-] Invalid magic word: {magic}. Dropping frame.")
                client_sock.close()
                continue

            print(f"[*] Valid ORIO frame detected. Payload size: {payload_len} bytes.")

            # 3. Read the payload
            payload = b""
            while len(payload) < payload_len:
                chunk = client_sock.recv(min(4096, payload_len - len(payload)))
                if not chunk:
                    break
                payload += chunk

            if len(payload) == payload_len:
                filename = os.path.join(
                    OUTPUT_DIR, f"orion_frame_{image_counter:04d}.raw"
                )
                with open(filename, "wb") as f:
                    f.write(payload)
                expected = 512 * 512 * 3  # 786432 bytes for 512x512 RGB
                dims = (
                    "512x512 RGB" if payload_len == expected else f"{payload_len} bytes"
                )
                print(f"[+] Success! Saved to {filename} ({dims})")
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
