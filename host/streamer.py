import socket
import time
import mss
import numpy as np
import av
import sys
import os
import threading

# Add parent directory to path for imports
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common.protocol import create_video_packets

class HostStreamer:
    def __init__(self, port=5005, width=1280, height=720, fps=30):
        self.port = port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            self.sock.bind(("0.0.0.0", self.port))
        except OSError:
            print(f"Warning: Could not bind to port {self.port}")

        self.width = width
        self.height = height
        self.fps = fps
        self.sequence = 0
        self.clients = set() # Set of (ip, port)
        self.clients_lock = threading.Lock()

        # Initialize encoder
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

    def _listen_for_clients(self):
        print(f"Listening for clients on {self.port}...")
        while True:
            try:
                data, addr = self.sock.recvfrom(1024)
                if data == b"CONNECT":
                    with self.clients_lock:
                        if addr not in self.clients:
                            self.clients.add(addr)
                            print(f"New client connected: {addr}")
            except Exception as e:
                print(f"Error receiving from client: {e}")

    def start(self):
        # Start a thread to listen for new clients
        threading.Thread(target=self._listen_for_clients, daemon=True).start()

        with mss.mss() as sct:
            # On Linux, monitor 1 might be different. Let's use monitor 1.
            if not sct.monitors:
                print("No monitors found.")
                return
            monitor = sct.monitors[1] if len(sct.monitors) > 1 else sct.monitors[0]

            frame_time = 1.0 / self.fps
            print(f"Starting host stream at {self.width}x{self.height} @ {self.fps} FPS")

            try:
                while True:
                    start_time = time.time()

                    # Only capture if there are clients
                    with self.clients_lock:
                        has_clients = len(self.clients) > 0

                    if has_clients:
                        # 1. Capture
                        img = sct.grab(monitor)
                        # MSS returns BGRA
                        frame_raw = np.array(img)
                        # Resize if necessary
                        if frame_raw.shape[1] != self.width or frame_raw.shape[0] != self.height:
                            # We should probably resize here, but for simplicity we assume it's right
                            # or let FFmpeg handle it if we could, but from_ndarray needs exact size
                            pass

                        frame_rgb = frame_raw[:, :, :3][:, :, ::-1]

                        # 2. Encode
                        frame = av.VideoFrame.from_ndarray(frame_rgb, format='rgb24')
                        for packet in self.stream.encode(frame):
                            # 3. Send
                            video_data = packet.to_bytes()
                            udp_packets = create_video_packets(video_data, self.sequence)

                            with self.clients_lock:
                                for addr in self.clients:
                                    for p in udp_packets:
                                        try:
                                            self.sock.sendto(p, addr)
                                        except OSError:
                                            # Maybe client disconnected?
                                            pass

                        self.sequence += 1
                    else:
                        # Sleep a bit to not hog CPU when no clients
                        time.sleep(0.1)
                        continue

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
                        video_data = packet.to_bytes()
                        udp_packets = create_video_packets(video_data, self.sequence)
                        with self.clients_lock:
                            for addr in self.clients:
                                for p in udp_packets:
                                    self.sock.sendto(p, addr)
                except Exception:
                    pass
                self.container.close()

if __name__ == "__main__":
    host = HostStreamer()
    host.start()
