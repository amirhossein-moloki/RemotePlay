import unittest
import struct
from common.protocol import create_video_packets, VIDEO_HEADER_FMT, TYPE_VIDEO

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

if __name__ == "__main__":
    unittest.main()
