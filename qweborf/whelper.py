# -*- coding: utf-8 -*-
# Weborf
# Copyright (C) 2010  Salvo "LtWorf" Tomaselli
# 
# Relational is free software: you can redistribute it and/or modify
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

import os
import subprocess
import socket
import threading

class weborf_runner():
    def __init__(self,logfunction):
        self.logclass=logfunction
        self.logclass.logger("Software initialized")
        
        self.weborf=self.test_weborf()
        self.child=None
        self.socket=None
        self.listener=None
        
        pass
    
    def test_weborf(self):
        '''Tests if weborf binary is existing.
        It will return true if everything is OK
        and false otherwise.'''
        ret=0
        out=""
        
        try:
            p = subprocess.Popen(["weborf", "-v"], bufsize=1024, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
            out=p.stdout.read().split("\n")[0]
            p.stdin.close()
            ret=p.wait()
        except:
            ret=3
        
        if ret==0:
            self.logclass.logger(out)
            return True
        else:
            self.logclass.logger("ERROR: unable to find weborf")
            return False
    
    def start(self,options):
        '''Starts weborf,
        returns True if it is correctly started'''
        
        if not self.weborf:
            self.logclass.logger("ERROR: Weborf binary is missing")
            return False
        
        if len(options['path'])==0:
            self.logclass.logger("ERROR: Path not specified")
            return False
        
        self.logclass.logger("Starting weborf...")
        
        auth_socket=self.__create_auth_socket()
        self.__start_weborf(options,auth_socket)
        self.__listen_auth_socket(options)
        return True 
    
    def __create_auth_socket(self):
        '''Creates a unix socket and returns the path to it'''
        self.socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sockname="/tmp/weborf_auth_socket%d.socket" % os.getuid()
        
        try:
            os.remove(sockname)
        except OSError:
            pass
        
        self.socket.bind(sockname)
        return sockname
        
    def __listen_auth_socket(self,options):
        self.listener=__listener__(self.socket,self)
        self.listener.start()
        
    def socket_cback(self,sock):
        '''Recieves connection requests and decides if they have to be authorized or denied'''
        
        data = sock.recv(4096).split('\r\n')
        print data
        uri = data[0]
        client = data[1]
        method = data[2]
        username = data[3]
        password = data[4]
        
        self.logclass.logger("%s - %s %s" % (client,method, uri))
            
        sock.close()

        pass
    def __start_weborf(self,options,auth_socket):
        '''Starts a weborf in a subprocess'''
        
        self.logclass.logger("weborf -p %d -b %s -x -I .... -a %s" % (options['port'],options['path'],auth_socket))
        
        self.child = subprocess.Popen(
                ("weborf", "-p",str(options['port']),"-b",options['path'],"-x","-I","....","-a",auth_socket)
                
                , bufsize=1024, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
                
        return True

    def stop(self):
        '''Stop weborf and correlated processes.
        Will return True if it goes well
        It should be called only if start succeeded'''
        
        self.child.stdin.close()
        self.child.terminate()
        ret=self.child.wait()
        
        self.socket.close()
        self.listener.stop()
        
        return True

class __listener__(threading.Thread):
    def __init__(self,socket,cback):
        threading.Thread.__init__(self)
        self.socket=socket
        self.cback=cback
        self.socket.settimeout(2.0)
        self.cycle=True
    def stop(self):
        self.cycle=False
    def run(self):
        self.socket.listen(1)
        while self.cycle:
            try:
                sock, addr = self.socket.accept()
                self.cback.socket_cback(sock)
            except:
                pass
        pass