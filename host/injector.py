import socket
import struct
import sys
import os
from pynput.keyboard import Controller as KController, KeyCode, Key
from pynput.mouse import Controller as MController

# Add parent directory to path for imports
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common.protocol import (TYPE_INPUT, INPUT_KEY_PRESS, INPUT_KEY_RELEASE,
                             INPUT_MOUSE_MOVE, INPUT_MOUSE_CLICK)

class InputInjector:
    def __init__(self, port=5005):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            self.sock.bind(("0.0.0.0", port))
        except OSError:
            # If port taken, we might be running alongside streamer, use another
            self.sock.bind(("0.0.0.0", 5006))
        self.keyboard = KController()
        self.mouse = MController()

    def start(self):
        print(f"Input injector listening on port {self.sock.getsockname()[1]}...")
        while True:
            data, addr = self.sock.recvfrom(1024)
            if not data:
                continue

            if data == b"CONNECT":
                print(f"Client connected from {addr}")
                continue

            msg_type = data[0]
            if msg_type == TYPE_INPUT:
                try:
                    input_type = data[1]
                    if input_type in (INPUT_KEY_PRESS, INPUT_KEY_RELEASE):
                        vk = struct.unpack("!I", data[2:6])[0]
                        # Handling special vs normal keys
                        key = KeyCode.from_vk(vk)
                        if input_type == INPUT_KEY_PRESS:
                            self.keyboard.press(key)
                        else:
                            self.keyboard.release(key)
                    elif input_type == INPUT_MOUSE_MOVE:
                        x, y = struct.unpack("!i i", data[2:10])
                        self.mouse.position = (x, y)
                except Exception as e:
                    print(f"Injection error: {e}")

if __name__ == "__main__":
    injector = InputInjector()
    injector.start()
