import socket
import sys
import struct

def send_command( host='192.168.14.49', port=12345):

    file_size = 0

    if len(command) != 4:
        print("Error: Command must be exactly 4 characters long.")
        return

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((host, port))

            # Prepare header
            header = b"multiverse:" + struct.pack('!I', file_size) + command.encode('utf-8')

            # Send header first
            s.sendall(header)

            s.shutdown(socket.SHUT_WR)  # Properly close the sending side
    except socket.error as e:
        print(f"Socket error: {e}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python send_file.py <filename> <4-char-command>")
        sys.exit(1)

    command = sys.argv[1]
    send_command()
