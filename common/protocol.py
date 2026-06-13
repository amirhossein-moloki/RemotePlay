import struct

# Packet types
TYPE_VIDEO = 1
TYPE_INPUT = 2
TYPE_CONTROL = 3

# Control types
CONTROL_CONNECT = 1
CONTROL_ACK = 2
CONTROL_PING = 3
CONTROL_PONG = 4

# Input types
INPUT_KEY_PRESS = 1
INPUT_KEY_RELEASE = 2
INPUT_MOUSE_MOVE = 3
INPUT_MOUSE_CLICK = 4
INPUT_MOUSE_SCROLL = 5
INPUT_GAMEPAD_AXIS = 6
INPUT_GAMEPAD_BUTTON = 7

# Video packet header: [Type (1B)] [Sequence (4B)] [Fragment Index (2B)] [Total Fragments (2B)] [Payload Size (2B)]
VIDEO_HEADER_FMT = "!B I H H H"
VIDEO_HEADER_SIZE = struct.calcsize(VIDEO_HEADER_FMT)

# Maximum UDP payload size (to stay within MTU)
MAX_PAYLOAD_SIZE = 1400

def create_video_packets(frame_data, sequence):
    total_fragments = (len(frame_data) + MAX_PAYLOAD_SIZE - 1) // MAX_PAYLOAD_SIZE
    packets = []
    for i in range(total_fragments):
        start = i * MAX_PAYLOAD_SIZE
        end = min(start + MAX_PAYLOAD_SIZE, len(frame_data))
        payload = frame_data[start:end]
        header = struct.pack(VIDEO_HEADER_FMT, TYPE_VIDEO, sequence, i, total_fragments, len(payload))
        packets.append(header + payload)
    return packets

def create_control_packet(control_type, data=b""):
    return struct.pack("!B B", TYPE_CONTROL, control_type) + data

# Input packet serialization helpers
def serialize_key_event(input_type, vk):
    return struct.pack("!B B I", TYPE_INPUT, input_type, vk)

def serialize_mouse_move(x, y):
    return struct.pack("!B B i i", TYPE_INPUT, INPUT_MOUSE_MOVE, x, y)

def serialize_mouse_click(x, y, button, pressed):
    # button: 1=left, 2=right, 3=middle
    return struct.pack("!B B i i B B", TYPE_INPUT, INPUT_MOUSE_CLICK, x, y, button, 1 if pressed else 0)
