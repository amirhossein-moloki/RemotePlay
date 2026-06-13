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

# Input packet formats
# [Type (1B)] [InputType (1B)] [Data...]
INPUT_HEADER_FMT = "!B B"

# Key: [KeyCode (4B)]
INPUT_KEY_FMT = "!I"
# Mouse Move: [X (4B)] [Y (4B)]
INPUT_MOUSE_MOVE_FMT = "!i i"
# Mouse Click: [X (4B)] [Y (4B)] [Button (1B)] [Pressed (1B)]
INPUT_MOUSE_CLICK_FMT = "!i i B B"
# Mouse Scroll: [X (4B)] [Y (4B)] [DX (4B)] [DY (4B)]
INPUT_MOUSE_SCROLL_FMT = "!i i i i"
# Gamepad Axis: [GamepadID (1B)] [Axis (1B)] [Value (2B, signed)]
INPUT_GAMEPAD_AXIS_FMT = "!B B h"
# Gamepad Button: [GamepadID (1B)] [Button (1B)] [Pressed (1B)]
INPUT_GAMEPAD_BUTTON_FMT = "!B B B"

def pack_input(input_type, *args):
    header = struct.pack(INPUT_HEADER_FMT, TYPE_INPUT, input_type)
    if input_type in (INPUT_KEY_PRESS, INPUT_KEY_RELEASE):
        return header + struct.pack(INPUT_KEY_FMT, *args)
    elif input_type == INPUT_MOUSE_MOVE:
        return header + struct.pack(INPUT_MOUSE_MOVE_FMT, *args)
    elif input_type == INPUT_MOUSE_CLICK:
        return header + struct.pack(INPUT_MOUSE_CLICK_FMT, *args)
    elif input_type == INPUT_MOUSE_SCROLL:
        return header + struct.pack(INPUT_MOUSE_SCROLL_FMT, *args)
    elif input_type == INPUT_GAMEPAD_AXIS:
        return header + struct.pack(INPUT_GAMEPAD_AXIS_FMT, *args)
    elif input_type == INPUT_GAMEPAD_BUTTON:
        return header + struct.pack(INPUT_GAMEPAD_BUTTON_FMT, *args)
    return header
