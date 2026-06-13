import socket
import struct
import numpy as np
import av
import sys
import os
import cv2

# Add parent directory to path for imports
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common.protocol import TYPE_VIDEO, VIDEO_HEADER_FMT, VIDEO_HEADER_SIZE

class ClientReceiver:
    def __init__(self, port=5005):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("0.0.0.0", port))
        self.codec = av.CodecContext.create('h264', 'r')
        self.frame_buffer = {} # seq -> {frag_idx -> data}

    def start(self):
        print("Starting client receiver...")
        cv2.namedWindow("Stream", cv2.WINDOW_NORMAL)

        try:
            while True:
                data, addr = self.sock.recvfrom(2048)
                if not data:
                    continue

                header = data[:VIDEO_HEADER_SIZE]
                payload = data[VIDEO_HEADER_SIZE:]

                type_msg, seq, frag_idx, total_frags, payload_size = struct.unpack(VIDEO_HEADER_FMT, header)

                if type_msg == TYPE_VIDEO:
                    if seq not in self.frame_buffer:
                        self.frame_buffer[seq] = [None] * total_frags

                    self.frame_buffer[seq][frag_idx] = payload

                    # Check if frame is complete
                    if all(f is not None for f in self.frame_buffer[seq]):
                        frame_data = b"".join(self.frame_buffer[seq])
                        del self.frame_buffer[seq]

                        # Clean up old sequences to avoid memory leak
                        old_seqs = [s for s in self.frame_buffer if s < seq]
                        for s in old_seqs:
                            del self.frame_buffer[s]

                        # Decode
                        try:
                            packets = self.codec.parse(frame_data)
                            for packet in packets:
                                frames = self.codec.decode(packet)
                                for frame in frames:
                                    # Convert to BGR for OpenCV
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
    client = ClientReceiver()
    client.start()
