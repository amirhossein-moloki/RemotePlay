import argparse
import sys
import os
from client.client import Client

def main():
    parser = argparse.ArgumentParser(description="Parsec-lite Client")
    parser.add_argument("host", help="Host IP address")
    parser.add_argument("--port", type=int, default=5005, help="Host port")
    args = parser.parse_args()

    print(f"Connecting to Parsec-lite Host at {args.host}:{args.port}...")

    client = Client(args.host, port=args.port)
    client.start_input_capture()
    client.start_receiver()

if __name__ == "__main__":
    main()
