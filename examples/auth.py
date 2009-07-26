#!/usr/bin/python
# -*- coding: utf-8 -*-
# Auth script
# Copyright (C) 2008  Salvo "LtWorf" Tomaselli
#
# Relation is free software: you can redistribute it and/or modify
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

# This is an example authentication script. You can modify it to make it fit your needs
import socket,os

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
try:
    os.remove("/tmp/weborf_auth.socket")
except OSError:
    pass
s.bind("/tmp/weborf_auth.socket")
s.listen(1)
while 1:
    conn, addr = s.accept()
    data = conn.recv(4096).split('\r\n')
    print data
    if data[0].startswith('/film'):
        if not data[1].startswith('::ffff:10.0.'):
            conn.send(' ')
    conn.close()
