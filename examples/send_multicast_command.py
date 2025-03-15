import socket
import sys
import struct

MULTICAST_IP = "239.255.111.111"
MULTICAST_PORT = 54321

def send_sync():
    """ Sends a multicast sync command """
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)  # Set TTL

    message = b"sync"
    sock.sendto(message, (MULTICAST_IP, MULTICAST_PORT))
    print("🔄 Sent multicast sync command")



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

def discover_displays():
    """ Sends a WHO_IS_THERE multicast message and collects responses """
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
    sock.settimeout(3.0)  # Wait 3 seconds for responses
    try:
        while True:
            data, addr = sock.recvfrom(1024)
            print(f"🎯 Found Display at {addr[0]}: {data.decode()}")
    except socket.timeout:
        print("⏳ Discovery complete, no more responses.")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python send_file.py <filename> <4-char-command>")
        sys.exit(1)

    command = sys.argv[1]
    if command == "sync":
        send_sync()
    elif command == "dscv":
        discover_displays()