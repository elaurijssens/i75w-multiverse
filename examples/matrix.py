import argparse
import socket
import struct
import zlib
import os

# Configuration
DEFAULT_IP = "192.168.14.49"
DEFAULT_PORT = 54321
MULTICAST_IP = "239.255.111.111"
MULTICAST_PORT = 54321
HEADER_PREFIX = "multiverse:"  # ✅ Ensure this matches the server's expected format

# Commands
COMMANDS = {
    "RSET": "RSET",
    "BOOT": "BOOT",
    "DSCV": "dscv",
    "CLSC": "clsc",
    "SYNC": "sync",
    "IPV4": "ipv4",
    "IPV6": "ipv6",
    "STOR": "stor",
    "KGET": "kget",
    "KSET": "kset",
    "KDEL": "kdel",
    "SDAT": "sdat",
    "DATA": "data",
    "SZIP": "szip",
    "ZIPD": "zipd",
}

def send_tcp_command(command, ip=DEFAULT_IP, port=DEFAULT_PORT, data=b""):
    """Send a command with optional data over TCP."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect((ip, port))

        # ✅ Ensure data is in bytes
        if isinstance(data, str):
            data = data.encode()

        # ✅ Construct header (Prefix + Data Length + Command)
        header = f"{HEADER_PREFIX}{len(data):04}{command}".encode()
        payload = header + data

        print(f"Sending {len(payload)} bytes: {command}")
        sock.sendall(payload)

def send_multicast_command(command):
    """Send a multicast command (e.g., SYNC, DSCV)."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
    sock.sendto(command.encode(), (MULTICAST_IP, MULTICAST_PORT))
    sock.close()

def receive_multicast_response():
    """Listen for responses to the discovery request."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("", MULTICAST_PORT))

    # Join multicast group
    mreq = struct.pack("4sl", socket.inet_aton(MULTICAST_IP), socket.INADDR_ANY)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

    print("Listening for responses...")
    while True:
        data, addr = sock.recvfrom(1024)
        print(f"Response from {addr}: {data.decode()}")

def send_key_value_command(command, key, value=""):
    """Send key-value operations like KGET, KSET, KDEL."""
    payload = f"{key}:{value}".encode()
    send_tcp_command(command, data=payload)

def send_image(command, filename):
    """Send an image file as raw or compressed data."""
    if not os.path.exists(filename):
        print(f"Error: File {filename} not found")
        return

    with open(filename, "rb") as f:
        img_data = f.read()

    if command in [COMMANDS["SZIP"], COMMANDS["ZIPD"]]:
        img_data = zlib.compress(img_data)
        print(f"Compressed image from {len(img_data)} bytes to {len(img_data)} bytes")

    send_tcp_command(command, data=img_data)

# Argument Parser
parser = argparse.ArgumentParser(description="LED Matrix Command Client")
parser.add_argument("command", choices=COMMANDS.keys(), help="Command to send")
parser.add_argument("--ip", default=DEFAULT_IP, help="Target device IP")
parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="Target TCP port")
parser.add_argument("--key", help="Key for KGET, KSET, KDEL")
parser.add_argument("--value", help="Value for KSET")
parser.add_argument("--file", help="Filename for SDAT, DATA, SZIP, ZIPD")

args = parser.parse_args()

# Command Execution
if args.command in ["RSET", "BOOT", "IPV4", "IPV6", "STOR", "CLSC"]:
    send_tcp_command(COMMANDS[args.command], args.ip, args.port)

elif args.command in ["SYNC", "DSCV"]:
    send_multicast_command(COMMANDS[args.command])
    if args.command == "DSCV":
        receive_multicast_response()

elif args.command in ["KGET", "KDEL"]:
    if not args.key:
        print("Error: --key is required for KGET and KDEL")
    else:
        send_key_value_command(COMMANDS[args.command], args.key)

elif args.command == "KSET":
    if not args.key or not args.value:
        print("Error: --key and --value are required for KSET")
    else:
        send_key_value_command(COMMANDS["KSET"], args.key, args.value)

elif args.command in ["SDAT", "DATA", "SZIP", "ZIPD"]:
    if not args.file:
        print("Error: --file is required for sending image data")
    else:
        send_image(COMMANDS[args.command], args.file)