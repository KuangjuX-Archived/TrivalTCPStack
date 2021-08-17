import socket
import os
import sys

def get_host_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(('8.8.8.8', 80))
        ip = s.getsockname()[0]
    finally:
        s.close()

    return ip

local_ip = get_host_ip()

client_ip = "10.0.0.1"
server_ip = "10.0.0.2"

if local_ip == client_ip:
    os.system("make client")
elif local_ip == server_ip:
    os.system("make server")
else:
    print("invalid ip: " + local_ip)
    sys.exit(0)
    
