import unittest
import struct
from common.protocol import (create_video_packets, VIDEO_HEADER_FMT, TYPE_VIDEO, TYPE_INPUT,
                             INPUT_KEY_PRESS, INPUT_MOUSE_MOVE, INPUT_MOUSE_CLICK,
                             pack_input, INPUT_HEADER_FMT, INPUT_KEY_FMT,
                             INPUT_MOUSE_MOVE_FMT, INPUT_MOUSE_CLICK_FMT)

class TestProtocol(unittest.TestCase):
    def test_create_video_packets(self):
        data = b"A" * 3000
        seq = 1
        packets = create_video_packets(data, seq)

        # 3000 / 1400 = 2.14 -> 3 packets
        self.assertEqual(len(packets), 3)

        # Verify header of first packet
        header_size = struct.calcsize(VIDEO_HEADER_FMT)
        header = packets[0][:header_size]
        type_msg, sequence, frag_idx, total_frags, payload_size = struct.unpack(VIDEO_HEADER_FMT, header)

        self.assertEqual(type_msg, TYPE_VIDEO)
        self.assertEqual(sequence, seq)
        self.assertEqual(frag_idx, 0)
        self.assertEqual(total_frags, 3)
        self.assertEqual(payload_size, 1400)

    def test_pack_input_key(self):
        vk = 65
        packet = pack_input(INPUT_KEY_PRESS, vk)
        header_size = struct.calcsize(INPUT_HEADER_FMT)
        msg_type, input_type = struct.unpack(INPUT_HEADER_FMT, packet[:header_size])
        self.assertEqual(msg_type, TYPE_INPUT)
        self.assertEqual(input_type, INPUT_KEY_PRESS)

        val = struct.unpack(INPUT_KEY_FMT, packet[header_size:])[0]
        self.assertEqual(val, vk)

    def test_pack_input_mouse_move(self):
        x, y = 100, 200
        packet = pack_input(INPUT_MOUSE_MOVE, x, y)
        header_size = struct.calcsize(INPUT_HEADER_FMT)
        res_x, res_y = struct.unpack(INPUT_MOUSE_MOVE_FMT, packet[header_size:])
        self.assertEqual(res_x, x)
        self.assertEqual(res_y, y)

    def test_pack_input_mouse_click(self):
        x, y, btn, pressed = 10, 20, 1, 1
        packet = pack_input(INPUT_MOUSE_CLICK, x, y, btn, pressed)
        header_size = struct.calcsize(INPUT_HEADER_FMT)
        res_x, res_y, res_btn, res_pressed = struct.unpack(INPUT_MOUSE_CLICK_FMT, packet[header_size:])
        self.assertEqual(res_x, x)
        self.assertEqual(res_y, y)
        self.assertEqual(res_btn, btn)
        self.assertEqual(res_pressed, pressed)

if __name__ == "__main__":
    unittest.main()
