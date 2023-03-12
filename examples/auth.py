#!/usr/bin/python3
# This is an example authentication script. You can modify it to make it fit your needs

# Weborf
# Copyright (C) 2019-2023  Salvo "LtWorf" Tomaselli
#
# Weborf is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# author Salvo "LtWorf" Tomaselli <tiposchi@tiscali.it>

import asyncio
import os
from typing import NamedTuple
from syslog import *


class Request(NamedTuple):
    url: bytes
    addr: bytes
    method: bytes
    username: bytes
    password: bytes
    protocol: bytes

SOCKET_PATH = "/run/weborf/weborf_auth.socket"

async def main():
    try:
        os.remove(SOCKET_PATH)
    except OSError:
        pass

    l = asyncio.get_running_loop()
    server = await l.create_unix_server(Proto, SOCKET_PATH)
    await server.serve_forever()


class Proto(asyncio.Protocol):
    def allow(self):
        self.transport.close()

    def deny(self):
        self.transport.write(b' ')
        self.transport.close()

    def connection_made(self, transport):
        self.transport = transport

    def data_received(self, data: bytes) -> None:
        fields = data.split(b'\r\n')
        request = Request(*fields[:6])
        headers = {k:v for k, v in(i.split(b': ', 1) for i in fields[6:] if i)}

        # Allow all from internal network
        if request.addr.startswith(b'::ffff:10.0.'):
            self.allow()
            return

        # Require a password for some content
        if request.url.startswith(b'/movies/') or request.url.startswith(b'/music/'):
            if request.username != b'user' and request.password != b'pass':
                self.deny()
                return
            else:
                self.allow()
                return
        # Require another password for all other content
        elif request.username == b'user2' and request.password == b'pass2':
            self.allow()
            return
        self.deny()


asyncio.run(main())
