import struct

# Packet types
TYPE_VIDEO = 1
TYPE_INPUT = 2

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

# Input packet format: [Type (1B)] [InputType (1B)] [Data...]
# Key: [InputType (1B)] [KeyCode (4B)]
# Mouse Move: [InputType (1B)] [X (4B)] [Y (4B)]
# Mouse Click: [InputType (1B)] [X (4B)] [Y (4B)] [Button (1B)] [Pressed (1B)]
# Gamepad Axis: [InputType (1B)] [GamepadID (1B)] [Axis (1B)] [Value (2B, signed)]
# Gamepad Button: [InputType (1B)] [GamepadID (1B)] [Button (1B)] [Pressed (1B)]
