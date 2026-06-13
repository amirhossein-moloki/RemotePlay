import socket
import struct
import numpy as np
import av
import sys
import os
import cv2
from pynput import keyboard, mouse
import threading
import time

# Add parent directory to path for imports
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common.protocol import (TYPE_VIDEO, TYPE_INPUT, VIDEO_HEADER_FMT, VIDEO_HEADER_SIZE,
                             INPUT_KEY_PRESS, INPUT_KEY_RELEASE, INPUT_MOUSE_MOVE,
                             INPUT_MOUSE_CLICK, INPUT_MOUSE_SCROLL, pack_input)

class Client:
    def __init__(self, host_ip, port=5005):
        self.host_addr = (host_ip, port)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        # Bind to any port to receive video
        self.sock.bind(("0.0.0.0", 0))
        self.bound_port = self.sock.getsockname()[1]

        self.codec = av.CodecContext.create('h264', 'r')
        self.frame_buffer = {} # seq -> {frag_idx -> data}
        self.last_displayed_seq = -1

    def _get_vk(self, key):
        if hasattr(key, 'vk'):
            return key.vk
        if hasattr(key, 'value') and hasattr(key.value, 'vk'):
            return key.value.vk
        # Handle special keys if needed, but for now we prioritize VK
        return None

    def on_press(self, key):
        vk = self._get_vk(key)
        if vk is not None:
            data = pack_input(INPUT_KEY_PRESS, vk)
            self.sock.sendto(data, self.host_addr)

    def on_release(self, key):
        vk = self._get_vk(key)
        if vk is not None:
            data = pack_input(INPUT_KEY_RELEASE, vk)
            self.sock.sendto(data, self.host_addr)

    def on_move(self, x, y):
        data = pack_input(INPUT_MOUSE_MOVE, int(x), int(y))
        self.sock.sendto(data, self.host_addr)

    def on_click(self, x, y, button, pressed):
        btn_idx = 1 if button == mouse.Button.left else 2 if button == mouse.Button.right else 3
        data = pack_input(INPUT_MOUSE_CLICK, int(x), int(y), btn_idx, 1 if pressed else 0)
        self.sock.sendto(data, self.host_addr)

    def on_scroll(self, x, y, dx, dy):
        data = pack_input(INPUT_MOUSE_SCROLL, int(x), int(y), int(dx), int(dy))
        self.sock.sendto(data, self.host_addr)

    def start_input_capture(self):
        k_listener = keyboard.Listener(on_press=self.on_press, on_release=self.on_release)
        m_listener = mouse.Listener(on_move=self.on_move, on_click=self.on_click, on_scroll=self.on_scroll)
        k_listener.daemon = True
        m_listener.daemon = True
        k_listener.start()
        m_listener.start()

    def start_receiver(self):
        print(f"Client listening on port {self.bound_port}...")
        cv2.namedWindow("Stream", cv2.WINDOW_NORMAL)

        # Send CONNECT to host
        self.sock.sendto(b"CONNECT", self.host_addr)

        try:
            while True:
                data, addr = self.sock.recvfrom(65535)
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
                    # Drop stale frames
                    if seq < self.last_displayed_seq:
                        continue

                    if seq not in self.frame_buffer:
                        self.frame_buffer[seq] = [None] * total_frags

                    if frag_idx < total_frags:
                        self.frame_buffer[seq][frag_idx] = payload

                    if all(f is not None for f in self.frame_buffer[seq]):
                        frame_data = b"".join(self.frame_buffer[seq])

                        # Clean up old sequences from buffer
                        completed_seq = seq
                        old_seqs = [s for s in self.frame_buffer if s <= completed_seq]
                        for s in old_seqs:
                            del self.frame_buffer[s]

                        self.last_displayed_seq = completed_seq

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
                            print(f"Decode error: {e}")

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
