import socket
import sys
import struct

def send_file(filename, host='192.168.14.49', port=12345):
    try:
        with open(filename, 'rb') as file:
            data = file.read()
    except FileNotFoundError:
        print(f"Error: File '{filename}' not found.")
        return
    except IOError as e:
        print(f"Error reading file '{filename}': {e}")
        return

    file_size = len(data)

    if len(command) != 4:
        print("Error: Command must be exactly 4 characters long.")
        return

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((host, port))

            # Prepare header
            header = b"Multiverse:" + struct.pack('!I', file_size) + command.encode('utf-8')

            # Send header first
            s.sendall(header)

            # Send the file data
            s.sendall(data)

            print(f"File '{filename}' ({file_size} bytes) sent successfully with command '{command}' to {host}:{port}")

            s.shutdown(socket.SHUT_WR)  # Properly close the sending side
    except socket.error as e:
        print(f"Socket error: {e}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python send_file.py <filename> <4-char-command>")
        sys.exit(1)

    filename = sys.argv[1]
    command = sys.argv[2]
    send_file(filename)
