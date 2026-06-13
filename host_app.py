import argparse
import threading
import sys
import os
from host.streamer import HostStreamer
from host.injector import InputInjector

def main():
    parser = argparse.ArgumentParser(description="Parsec-lite Host")
    parser.add_argument("--port", type=int, default=5005, help="Port to listen on")
    parser.add_argument("--width", type=int, default=1280, help="Stream width")
    parser.add_argument("--height", type=int, default=720, help="Stream height")
    parser.add_argument("--fps", type=int, default=30, help="Stream FPS")
    args = parser.parse_args()

    # Unified Host Entry Point
    injector = InputInjector(port=args.port)
    streamer = HostStreamer(client_port=args.port, width=args.width, height=args.height, fps=args.fps)

    print(f"Starting Parsec-lite Host on port {args.port}...")

    injector_thread = threading.Thread(target=injector.start, daemon=True)
    injector_thread.start()

    # streamer.start() will block and handle the video stream
    streamer.start()

if __name__ == "__main__":
    main()
