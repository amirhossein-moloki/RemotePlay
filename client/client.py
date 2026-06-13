import socket
import struct
import numpy as np
import av
import sys
import os
import cv2
from pynput import keyboard, mouse
import threading

# Add parent directory to path for imports
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common.protocol import (TYPE_VIDEO, TYPE_INPUT, VIDEO_HEADER_FMT, VIDEO_HEADER_SIZE,
                             INPUT_KEY_PRESS, INPUT_KEY_RELEASE, INPUT_MOUSE_MOVE, INPUT_MOUSE_CLICK)

class Client:
    def __init__(self, host_ip, port=5005):
        self.host_addr = (host_ip, port)
        self.video_host_addr = (host_ip, 5006)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        # Bind to any port to receive video
        self.sock.bind(("0.0.0.0", 0))
        self.bound_port = self.sock.getsockname()[1]

        self.codec = av.CodecContext.create('h264', 'r')
        self.frame_buffer = {} # seq -> {frag_idx -> data}

    def _get_vk(self, key):
        if hasattr(key, 'vk'):
            return key.vk
        if hasattr(key, 'value') and hasattr(key.value, 'vk'):
            return key.value.vk
        return None

    def on_press(self, key):
        vk = self._get_vk(key)
        if vk is not None:
            data = struct.pack("!B B I", TYPE_INPUT, INPUT_KEY_PRESS, vk)
            self.sock.sendto(data, self.host_addr)

    def on_release(self, key):
        vk = self._get_vk(key)
        if vk is not None:
            data = struct.pack("!B B I", TYPE_INPUT, INPUT_KEY_RELEASE, vk)
            self.sock.sendto(data, self.host_addr)

    def on_move(self, x, y):
        data = struct.pack("!B B i i", TYPE_INPUT, INPUT_MOUSE_MOVE, int(x), int(y))
        self.sock.sendto(data, self.host_addr)

    def start_input_capture(self):
        k_listener = keyboard.Listener(on_press=self.on_press, on_release=self.on_release)
        m_listener = mouse.Listener(on_move=self.on_move)
        k_listener.daemon = True
        m_listener.daemon = True
        k_listener.start()
        m_listener.start()

    def start_receiver(self):
        print(f"Client listening on port {self.bound_port}...")
        cv2.namedWindow("Stream", cv2.WINDOW_NORMAL)

        # In a real scenario, we'd send a "connect" packet to the host so it knows our port.
        self.sock.sendto(b"CONNECT", self.video_host_addr)
        # Also connect to injector
        self.sock.sendto(b"CONNECT", self.host_addr)

        try:
            while True:
                data, addr = self.sock.recvfrom(65535) # Large enough for any UDP packet
                if not data:
                    continue

                if len(data) < VIDEO_HEADER_SIZE:
                    continue

                header = data[:VIDEO_HEADER_SIZE]
                payload = data[VIDEO_HEADER_SIZE:]

                try:
                    type_msg, seq, frag_idx, total_frags, payload_size = struct.unpack(VIDEO_HEADER_FMT, header)
                except struct.error:
                    continue

                if type_msg == TYPE_VIDEO:
                    if seq not in self.frame_buffer:
                        self.frame_buffer[seq] = [None] * total_frags

                    if frag_idx < total_frags:
                        self.frame_buffer[seq][frag_idx] = payload

                    if all(f is not None for f in self.frame_buffer[seq]):
                        frame_data = b"".join(self.frame_buffer[seq])
                        del self.frame_buffer[seq]

                        # Clean up old
                        old_seqs = [s for s in self.frame_buffer if s < seq]
                        for s in old_seqs:
                            del self.frame_buffer[s]

                        try:
                            packets = self.codec.parse(frame_data)
                            for packet in packets:
                                frames = self.codec.decode(packet)
                                for frame in frames:
                                    img = frame.to_ndarray(format='bgr24')
                                    cv2.imshow("Stream", img)
                                    if cv2.waitKey(1) & 0xFF == ord('q'):
                                        return
                        except Exception as e:
                            print(f"Decoding error: {e}")

        except KeyboardInterrupt:
            print("Stopping client...")
        finally:
            cv2.destroyAllWindows()

if __name__ == "__main__":
    import sys
    host_ip = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
    client = Client(host_ip)
    client.start_input_capture()
    client.start_receiver()
