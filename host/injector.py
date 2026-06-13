import socket
import struct
import sys
import os
from pynput.keyboard import Controller as KController, KeyCode, Key
from pynput.mouse import Controller as MController, Button

# Add parent directory to path for imports
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common.protocol import (TYPE_INPUT, INPUT_KEY_PRESS, INPUT_KEY_RELEASE,
                             INPUT_MOUSE_MOVE, INPUT_MOUSE_CLICK, INPUT_MOUSE_SCROLL,
                             INPUT_KEY_FMT, INPUT_MOUSE_MOVE_FMT,
                             INPUT_MOUSE_CLICK_FMT, INPUT_MOUSE_SCROLL_FMT)

class InputInjector:
    def __init__(self, port=5005):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            self.sock.bind(("0.0.0.0", port))
        except OSError:
            # If port taken (e.g. by streamer), we might need to share it or use another.
            # In this simple prototype, let's try 5006 if 5005 is busy.
            self.sock.bind(("0.0.0.0", 5006))
        self.keyboard = KController()
        self.mouse = MController()

    def start(self):
        print(f"Input injector listening on port {self.sock.getsockname()[1]}...")
        while True:
            try:
                data, addr = self.sock.recvfrom(1024)
                if not data:
                    continue

                if data == b"CONNECT":
                    continue

                msg_type = data[0]
                if msg_type == TYPE_INPUT:
                    input_type = data[1]
                    payload = data[2:]

                    if input_type == INPUT_KEY_PRESS:
                        vk = struct.unpack(INPUT_KEY_FMT, payload)[0]
                        self.keyboard.press(KeyCode.from_vk(vk))
                    elif input_type == INPUT_KEY_RELEASE:
                        vk = struct.unpack(INPUT_KEY_FMT, payload)[0]
                        self.keyboard.release(KeyCode.from_vk(vk))
                    elif input_type == INPUT_MOUSE_MOVE:
                        x, y = struct.unpack(INPUT_MOUSE_MOVE_FMT, payload)
                        self.mouse.position = (x, y)
                    elif input_type == INPUT_MOUSE_CLICK:
                        x, y, btn_idx, pressed = struct.unpack(INPUT_MOUSE_CLICK_FMT, payload)
                        self.mouse.position = (x, y)
                        button = Button.left if btn_idx == 1 else Button.right if btn_idx == 2 else Button.middle
                        if pressed:
                            self.mouse.press(button)
                        else:
                            self.mouse.release(button)
                    elif input_type == INPUT_MOUSE_SCROLL:
                        x, y, dx, dy = struct.unpack(INPUT_MOUSE_SCROLL_FMT, payload)
                        self.mouse.position = (x, y)
                        self.mouse.scroll(dx, dy)
            except Exception as e:
                print(f"Injection error: {e}")

if __name__ == "__main__":
    injector = InputInjector()
    injector.start()
