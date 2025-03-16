import socket
import sys
import struct
import zlib
from PIL import Image

# Constants
HEADER_PREFIX = b"multiverse:"
MULTICAST_IP = "239.255.111.111"  # Default multicast address
MULTICAST_PORT = 54321            # Default multicast port
BUFFER_SIZE = 4096                # 4KB buffer for file transfers
DISCOVERY_TIMEOUT = 5.0

def image_to_raw_pixels(image_path):
    try:
        with Image.open(image_path) as img:
            img = img.convert('RGB')  # Ensure RGB format
            pixels = list(img.getdata())
            raw_data = bytearray()
            for r, g, b in pixels:
                raw_data.extend([r, g, b, 255])  # Add brightness value (255)
            return raw_data, img.width, img.height
    except Exception as e:
        print(f"Error processing image '{image_path}': {e}")
        return None, None, None

def send_tcp_command(command, data=b"", host="192.168.14.49", port=54321):
    """
    Sends a command over TCP.
    If `data` is provided, it is sent as a payload after the command.
    """
    file_size = len(data)

    if len(command) != 4:
        print("Error: Command must be exactly 4 characters long.")
        return

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((host, port))

            # Prepare and send header
            header = HEADER_PREFIX + struct.pack("!I", file_size) + command.encode("utf-8")
            s.sendall(header)

            # Send file data in chunks
            if file_size > 0:
                s.sendall(data)

            print(f"Command '{command}' sent successfully to {host}:{port}")

            s.shutdown(socket.SHUT_WR)  # Properly close the sending side
    except socket.error as e:
        print(f"Socket error: {e}")

def send_multicast_message(command, listen_for_responses=False):
    """
    Sends a multicast message.
    If `listen_for_responses=True`, waits for devices to respond.
    """
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
            sock.sendto(command.encode("utf-8"), (MULTICAST_IP, MULTICAST_PORT))
            print(f"Multicast command '{command}' sent to {MULTICAST_IP}:{MULTICAST_PORT}")

    except socket.error as e:
        print(f"Multicast socket error: {e}")

def discover_displays():
    """ Sends a WHO_IS_THERE multicast message and collects responses """

    # Create UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)

    # Bind to listen for responses
    sock.bind(("", MULTICAST_PORT))

    # Join multicast group
    mreq = struct.pack("4sl", socket.inet_aton(MULTICAST_IP), socket.INADDR_ANY)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

    # Send discovery request
    message = b"dscv"
    sock.sendto(message, (MULTICAST_IP, MULTICAST_PORT))
    print("📡 Sent multicast discovery request")

    # Listen for responses
    sock.settimeout(DISCOVERY_TIMEOUT)  # ✅ 3-second timeout for responses
    try:
        while True:
            data, addr = sock.recvfrom(1024)  # ✅ Keeps listening for multiple devices
            print(f"🎯 Found Display at {addr[0]}: {data.decode()}")
    except socket.timeout:
        print("⏳ Discovery complete, no more responses.")

def send_file(filename, command, host="192.168.14.49", port=54321, compress=False):
    """
    Reads a file and sends it using the specified command.
    If `compress=True`, the file is compressed using zlib before sending.
    """

    raw_data, width, height = image_to_raw_pixels(filename)


    if compress:
        data = zlib.compress(raw_data, level=9)
        print(f"Compressed file '{filename}' from {len(data)} bytes to {len(data)} bytes")
    else:
        data = raw_data
    send_tcp_command(command, data, host, port)

if __name__ == "__main__":
    """
    Command-line interface for sending commands and files to the display.
    """
    if len(sys.argv) < 2:
        print("Usage:")
        print("  python client.py <command> [args]")
        print("\nAvailable Commands:")
        print("  RSET, BOOT, ipv4, ipv6, stor, clsc - Sent over TCP, no arguments")
        print("  sync, dscv - Sent over multicast (no arguments)")
        print("  kget <key>, kdel <key> - Sent over TCP")
        print("  kset <key> <value> - Sent over TCP")
        print("  data <filename>, sdat <filename> - Send raw image file over TCP")
        print("  zipd <filename>, szip <filename> - Send compressed image file over TCP")
        sys.exit(1)

    command = sys.argv[1]

    if command in ["RSET", "BOOT", "ipv4", "ipv6", "stor", "clsc"]:
        send_tcp_command(command)

    elif command == "sync":
        send_multicast_message(command)

    elif command == "dscv":
        discover_displays()

    elif command == "kget" and len(sys.argv) == 3:
        key = sys.argv[2]
        send_tcp_command("kget", key.encode("utf-8"))

    elif command == "kdel" and len(sys.argv) == 3:
        key = sys.argv[2]
        send_tcp_command("kdel", key.encode("utf-8"))

    elif command == "kset" and len(sys.argv) == 4:
        key = sys.argv[2]
        value = sys.argv[3]
        send_tcp_command("kset", f"{key}:{value}".encode("utf-8"))

    elif command in ["data", "sdat"] and len(sys.argv) == 3:
        filename = sys.argv[2]
        send_file(filename, command)

    elif command in ["zipd", "szip"] and len(sys.argv) == 3:
        filename = sys.argv[2]
        send_file(filename, command, compress=True)

    else:
        print("Invalid command or arguments. Run `python client.py` for usage.")