import socket
import time
import mss
import numpy as np
import av
import sys
import os

# Add parent directory to path for imports
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common.protocol import create_video_packets

class HostStreamer:
    def __init__(self, client_ip=None, client_port=5005, width=1280, height=720, fps=30):
        self.client_addr = (client_ip, client_port) if client_ip else None
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        # Bind to a port to receive CONNECT if client_ip is not known
        try:
            # Using a different port for video if injector is on 5005
            self.streamer_port = 5006
            self.sock.bind(("0.0.0.0", self.streamer_port))
        except OSError:
            pass

        self.width = width
        self.height = height
        self.fps = fps
        self.sequence = 0

        # Initialize encoder
        # We don't use a BytesIO object that grows indefinitely.
        # Instead, we'll use a custom output that just passes data through or handle it via a memory-limited buffer.
        # Actually, for PyAV, we can open a 'format' that we manually feed or use a null output.
        # But even better, we can just use the encode() method which returns packets.

        # We use a 'null' container for the setup, but we primarily use the stream's encode method.
        self.container = av.open(os.devnull, mode='w', format='h264')
        self.stream = self.container.add_stream('h264', rate=fps)
        self.stream.width = width
        self.stream.height = height
        self.stream.pix_fmt = 'yuv420p'
        # Low latency options
        self.stream.options = {
            'preset': 'ultrafast',
            'tune': 'zerolatency',
            'crf': '23'
        }

    def start(self):
        # Wait for client if not specified
        if not self.client_addr:
            print("Waiting for client CONNECT...")
            while True:
                data, addr = self.sock.recvfrom(1024)
                if data == b"CONNECT":
                    self.client_addr = addr
                    print(f"Client connected from {addr}")
                    break

        with mss.mss() as sct:
            monitor = sct.monitors[1] # Primary monitor

            frame_time = 1.0 / self.fps

            print(f"Starting host stream to {self.client_addr}...")

            try:
                # Set non-blocking for input reception (if we were receiving inputs here)
                self.sock.setblocking(False)
                while True:
                    start_time = time.time()

                    # 1. Capture
                    img = sct.grab(monitor)
                    # Convert to RGB (mss returns BGRA)
                    frame_raw = np.array(img)
                    frame_rgb = frame_raw[:, :, :3][:, :, ::-1]

                    # 2. Encode
                    frame = av.VideoFrame.from_ndarray(frame_rgb, format='rgb24')
                    for packet in self.stream.encode(frame):
                        # 3. Send
                        video_data = bytes(packet)
                        udp_packets = create_video_packets(video_data, self.sequence)
                        for p in udp_packets:
                            try:
                                self.sock.sendto(p, self.client_addr)
                            except OSError:
                                pass

                    self.sequence += 1

                    # Pacing
                    elapsed = time.time() - start_time
                    sleep_time = max(0, frame_time - elapsed)
                    time.sleep(sleep_time)

            except KeyboardInterrupt:
                print("Stopping host...")
            finally:
                # Flush encoder
                try:
                    for packet in self.stream.encode():
                        video_data = bytes(packet)
                        udp_packets = create_video_packets(video_data, self.sequence)
                        for p in udp_packets:
                            self.sock.sendto(p, self.client_addr)
                except Exception:
                    pass
                self.container.close()

if __name__ == "__main__":
    host = HostStreamer()
    host.start()
