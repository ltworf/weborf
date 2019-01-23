#!/usr/bin/python3
# This is an example authentication script. You can modify it to make it fit your needs
import socket,os
from typing import NamedTuple

class Request(NamedTuple):
    url: bytes
    addr: bytes
    method: bytes
    username: bytes
    password: bytes
    protocol: bytes

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
try:
    os.remove("/run/weborf/weborf_auth.socket")
except OSError:
    pass
s.bind("/run/weborf/weborf_auth.socket")
s.listen(1)
while True:
    conn, addr = s.accept()
    data = conn.recv(4096).split(b'\r\n')

    request = Request(*data[:6])
    headers = {k:v for k, v in(i.split(b': ', 1) for i in data[6:] if i)}

    # Allow all from internal network
    if request.addr.startswith(b'::ffff:10.0.'):
        conn.close()
        continue

    # Require a password for some content
    if request.url.startswith(b'/movies') or request.url.startswith(b'/music'):
        if request.username != b'user' and request.password != b'pass':
            conn.send(b' ')
    # Require another password for all other content
    elif request.username != b'user2' and request.password != b'pass2':
        conn.send(b' ')
    conn.close()
