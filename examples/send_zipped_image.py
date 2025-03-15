import socket
import sys
import struct
from PIL import Image
import zlib

MULTICAST_IP = "239.255.0.1"
MULTICAST_PORT = 4321

def send_sync():
    """ Sends a multicast sync command """
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)  # Set TTL

    message = b"sync"
    sock.sendto(message, (MULTICAST_IP, MULTICAST_PORT))
    print("🔄 Sent multicast sync command")


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



# ✅ Configurable server details
SERVER_IP = "192.168.1.100"  # Change to your Pico's IP
SERVER_PORT = 12345

# ✅ Commands
CMD_ZIPPED = b"zdtw"
CMD_SHOWZIPPED = b"zdat"

def send_image(image_path, command, host='192.168.14.49', port=12345):
    raw_data, width, height = image_to_raw_pixels(image_path)
    if raw_data is None:
        return

    zipped_data = zlib.compress(raw_data, level=9)

    data_size = len(zipped_data)

    if len(command) != 4:
        print("Error: Command must be exactly 4 characters long.")
        return

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((host, port))

            # Prepare header
            header = b"multiverse:" + struct.pack('!I', data_size) + command.encode('utf-8')

            # Send header first
            s.sendall(header)

            # Send the raw pixel data
            s.sendall(zipped_data)

            print(f"Image '{image_path}' ({width}x{height}, {data_size} bytes) sent successfully with command '{command}' to {host}:{port}")

            s.shutdown(socket.SHUT_WR)  # Properly close the sending side
    except socket.error as e:
        print(f"Socket error: {e}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python send_image.py <image_file> <4-char-command>")
        sys.exit(1)

    image_path = sys.argv[1]
    command = sys.argv[2]
    send_image(image_path, command)
    send_sync()

