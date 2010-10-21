#!/usr/bin/python
# -*- coding: utf-8 -*-
'''
Weborf
Copyright (C) 2010  Salvo "LtWorf" Tomaselli

Weborf is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

@author Salvo "LtWorf" Tomaselli <tiposchi@tiscali.it>


This package provides testing suite for weborf
'''

import httplib
import os
import signal
import time

host='localhost'
port=8080
weborfpid=0
weborfpath='../weborf'


#connection=class httplib.HTTPConnection(host[, port[, strict[, timeout[, source_address]]]])

def start_server(cmdline):
    cmdline=(weborfpath,) + cmdline
    '''Will start the server, cmdline is an iterable of strings'''
    pid=os.fork()
    if pid==0:
        os.execv(weborfpath,cmdline)
    else:
      time.sleep(0.5) #Sleep to make sure the server is started
      weborfpid=pid

def stop_server():
    '''Sends a SIGINT to the webserver'''
    os.kill(weborfpid,signal.SIGINT)

def connection(strict=True):
    return httplib.HTTPConnection(host, port ,strict)
