import socket
import sys
import struct
from PIL import Image

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

def send_image(image_path, command, host='192.168.14.49', port=12345):
    raw_data, width, height = image_to_raw_pixels(image_path)
    if raw_data is None:
        return

    data_size = len(raw_data)

    if len(command) != 4:
        print("Error: Command must be exactly 4 characters long.")
        return

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((host, port))

            # Prepare header
            header = b"multiverse:" + struct.pack('!I', data_size << 2) + command.encode('utf-8')

            # Send header first
            s.sendall(header)

            # Send the raw pixel data
            s.sendall(raw_data[:32769])

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
