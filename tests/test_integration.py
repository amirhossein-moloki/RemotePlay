import unittest
import socket
import struct
import sys
import os
import time
import threading

sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common.protocol import TYPE_INPUT, INPUT_KEY_PRESS

class TestIntegration(unittest.TestCase):
    def test_input_loopback(self):
        # This test checks if the protocol for input is consistent
        # We don't necessarily need to run the full injector which depends on GUI

        test_port = 6000
        received_data = []

        def mock_server():
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.bind(("127.0.0.1", test_port))
            sock.settimeout(2)
            try:
                data, addr = sock.recvfrom(1024)
                received_data.append(data)
            except socket.timeout:
                pass
            finally:
                sock.close()

        t = threading.Thread(target=mock_server)
        t.start()

        time.sleep(0.1)

        # Mock client sending input
        client_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        vk = 65 # 'A'
        data = struct.pack("!B B I", TYPE_INPUT, INPUT_KEY_PRESS, vk)
        client_sock.sendto(data, ("127.0.0.1", test_port))
        client_sock.close()

        t.join()

        self.assertEqual(len(received_data), 1)
        msg = received_data[0]
        self.assertEqual(msg[0], TYPE_INPUT)
        self.assertEqual(msg[1], INPUT_KEY_PRESS)
        self.assertEqual(struct.unpack("!I", msg[2:6])[0], vk)

if __name__ == "__main__":
    unittest.main()
