import argparse
import socket
import struct
import zlib
import sys
from PIL import Image

# Constants
HEADER_PREFIX = b"multiverse:"
MULTICAST_IP = "239.255.111.111"
MULTICAST_PORT = 54321
BUFFER_SIZE = 4096
DISCOVERY_TIMEOUT = 5.0

def image_to_raw_pixels(image_path):
    try:
        with Image.open(image_path) as img:
            img = img.convert('RGB')
            pixels = list(img.getdata())
            raw_data = bytearray()
            for r, g, b in pixels:
                raw_data.extend([r, g, b, 255])
            return raw_data, img.width, img.height
    except Exception as e:
        print(f"Error processing image '{image_path}': {e}")
        return None, None, None

def send_tcp_command(command, data=b"", host=None, port=None):
    if not host or not port:
        print("Error: IP and port are required for TCP commands.")
        return

    file_size = len(data)

    if len(command) != 4:
        print("Error: Command must be exactly 4 characters long.")
        return

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((host, port))
            header = HEADER_PREFIX + struct.pack("!I", file_size) + command.encode("utf-8")
            s.sendall(header)
            if file_size > 0:
                s.sendall(data)
            print(f"✅ Command '{command}' sent successfully to {host}:{port}")
            s.shutdown(socket.SHUT_WR)
    except socket.error as e:
        print(f"❌ Socket error: {e}")

def send_multicast_message(command):
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
            sock.sendto(command.encode("utf-8"), (MULTICAST_IP, MULTICAST_PORT))
            print(f"📡 Multicast command '{command}' sent to {MULTICAST_IP}:{MULTICAST_PORT}")
    except socket.error as e:
        print(f"❌ Multicast socket error: {e}")

def discover_displays():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
    sock.bind(("", MULTICAST_PORT))

    mreq = struct.pack("4sl", socket.inet_aton(MULTICAST_IP), socket.INADDR_ANY)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

    message = b"dscv"
    sock.sendto(message, (MULTICAST_IP, MULTICAST_PORT))
    print("📡 Sent multicast discovery request")

    sock.settimeout(DISCOVERY_TIMEOUT)
    try:
        while True:
            data, addr = sock.recvfrom(1024)
            print(f"🎯 Found Display at {addr[0]}: {data.decode()}")
    except socket.timeout:
        print("⏳ Discovery complete, no more responses.")

def send_file(filename, command, host, port, compress=False):
    raw_data, _, _ = image_to_raw_pixels(filename)

    if compress:
        data = zlib.compress(raw_data, level=9)
        print(f"📦 Compressed file '{filename}' to {len(data)} bytes")
    else:
        data = raw_data

    send_tcp_command(command, data, host, port)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Client script for sending commands to LED displays.",
        formatter_class=argparse.RawTextHelpFormatter
    )

    parser.add_argument(
        "command", type=str, help="Command to send. Options:\n"
                                  "  - RSET, BOOT, ipv4, ipv6, stor, clsc (TCP commands)\n"
                                  "  - sync, dscv (Multicast commands)\n"
                                  "  - kget <key>, kdel <key>, kset <key> <value> (TCP key-value commands)\n"
                                  "  - data <filename>, sdat <filename> (Send raw image file over TCP)\n"
                                  "  - zipd <filename>, szip <filename> (Send compressed image file over TCP)"
    )

    parser.add_argument("--ip", type=str, help="Target IP address (Required for TCP commands)")
    parser.add_argument("--port", type=int, help="Target port (Required for TCP commands)")
    parser.add_argument("--key", type=str, help="Key for kget/kdel/kset commands")
    parser.add_argument("--value", type=str, help="Value for kset command")
    parser.add_argument("--file", type=str, help="Filename for data transfer")
    parser.add_argument("--compress", action="store_true", help="Compress file before sending")

    args = parser.parse_args()

    tcp_commands = ["RSET", "BOOT", "ipv4", "ipv6", "stor", "clsc", "kget", "kdel", "kset", "data", "sdat", "zipd", "szip"]

    if args.command in tcp_commands and (not args.ip or not args.port):
        print("❌ Error: TCP commands require --ip and --port arguments.")
        sys.exit(1)

    if args.command in ["RSET", "BOOT", "ipv4", "ipv6", "stor", "clsc"]:
        send_tcp_command(args.command, host=args.ip, port=args.port)

    elif args.command == "sync":
        send_multicast_message(args.command)

    elif args.command == "dscv":
        discover_displays()

    elif args.command == "kget" and args.key:
        send_tcp_command("kget", args.key.encode(), args.ip, args.port)

    elif args.command == "kdel" and args.key:
        send_tcp_command("kdel", args.key.encode(), args.ip, args.port)

    elif args.command == "kset" and args.key and args.value:
        send_tcp_command("kset", f"{args.key}:{args.value}".encode(), args.ip, args.port)

    elif args.command in ["data", "sdat"] and args.file:
        send_file(args.file, args.command, args.ip, args.port)

    elif args.command in ["zipd", "szip"] and args.file:
        send_file(args.file, args.command, args.ip, args.port, compress=True)

    else:
        parser.print_help()