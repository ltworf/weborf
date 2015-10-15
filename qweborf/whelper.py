# Weborf
# Copyright (C) 2010  Salvo "LtWorf" Tomaselli
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

import os
import subprocess
import socket
import threading
from PyQt5 import QtCore

import qweborf.nhelper as nhelper


class weborf_runner():

    def __init__(self, logfunction):
        self.logclass = logfunction
        self.logclass.logger("Software initialized")

        self.child = None
        self.socket = None
        self.listener = None
        self.waiter = None
        self.methods = []
        self.username = None
        self.password = None
        self.ipv6 = True
        self._running = False
        self.version = None
        self.webdav = None

        self.weborf = self._test_weborf()

    def _test_weborf(self):
        '''Tests if weborf binary is existing.
        It will return true if everything is OK
        and false otherwise.'''

        try:
            with subprocess.Popen(["weborf", "-k"], bufsize=1024, stdout=subprocess.PIPE) as p:
                out = p.stdout.read().decode('ascii').strip()
                capabilities = {key: value for key, value in (
                    i.split(':', 1) for i in (l for l in out.split('\n')))}

                self.version = capabilities['version']
                self.ipv6 = capabilities['ipv'] == '6'
                self.webdav = capabilities['webdav'] == 'yes'

                self.logclass.logger('Weborf version %s found' %
                                     self.version, self.logclass.DBG_NOTICE)
                self.logclass.logger('IPv%s protocol in use' %
                                     capabilities['ipv'], self.logclass.DBG_NOTICE)
                self.logclass.logger('Has webdav support: %s' % capabilities['webdav'], self.logclass.DBG_NOTICE)

                if capabilities['embedded_auth'] == 'yes':
                    self.logclass.logger('Binary compiled with embedded authentication', self.logclass.DBG_ERROR)
                    return False

                return True
        except FileNotFoundError:
            self.logclass.logger(
                'Unable to find weborf', self.logclass.DBG_ERROR)
        except KeyError as e:
            self.logclass.logger(
                'Capability %s not supported' % e, self.logclass.DBG_ERROR)
        except Exception as e:
            self.logclass.logger(
                'Unknown exception %s' % e, self.logclass.DBG_ERROR)

        return False

    def start(self, options):
        '''Starts weborf,
        returns True if it is correctly started'''

        if not self.weborf:
            self.logclass.logger(
                'Unable to start weborf', self.logclass.DBG_ERROR)
            return False

        if len(options['path']) == 0:
            self.logclass.logger('Path not specified', self.logclass.DBG_ERROR)
            return False

        self.logclass.logger("Starting weborf...")

        auth_socket = self.__create_auth_socket()
        self.__start_weborf(options, auth_socket)
        self.__listen_auth_socket(options)

        self.username = options['username']
        self.password = options['password']

        # Deciding which HTTP methods will be enabled
        self.methods = {'GET'}
        if options['dav']:
            self.methods = self.methods.union({'OPTIONS', 'PROPFIND'})
            self.logclass.logger(
                "DAV access enabled", self.logclass.DBG_NOTICE)

            # If writing is enabled
            if options['write']:
                self.methods = self.methods.union(
                    {'PUT', 'DELETE', 'COPY', 'MOVE', 'MKCOL'})
                self.logclass.logger(
                    'Writing access enabled. This could pose security threat', self.logclass.DBG_WARNING)

        return True

    def __create_auth_socket(self):
        '''Creates a unix socket and returns the path to it'''
        self.socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sockname = "/tmp/weborf_auth_socket%d-%d.socket" % (
            os.getuid(), os.getpid())

        try:
            os.remove(sockname)
        except OSError:
            pass

        self.socket.bind(sockname)
        return sockname

    def __listen_auth_socket(self, options):
        self.listener = __listener__(self.socket)
        self.listener.new_socket.connect(self.socket_cback)
        self.listener.start()

    @QtCore.pyqtSlot(object)
    def socket_cback(self, sock):
        '''Recieves connection requests and decides if they have to be authorized or denied'''
        data = sock.recv(4096).decode('ascii').split('\r\n')
        uri = data[0]
        client = data[1]
        method = data[2]
        username = data[3]
        password = data[4]

        # Checking if the request must be allowed or denied
        allow = True
        if self.username is not None:  # Must check username and password
            allow = self.username == username and self.password == password

        print(method, self.methods)
        if method not in self.methods:
            allow = False

        if allow:
            self.logclass.logger("%s - %s %s" % (client, method, uri))
        else:
            sock.send(b' ')
            self.logclass.logger("DENIED: %s - %s %s" % (client, method, uri))

        sock.close()

    def __start_weborf(self, options, auth_socket):
        '''Starts a weborf in a subprocess'''

        cmdline = ["weborf", "-p", str(options['port']), "-b", str(
            options['path']), "-x", "-I", "....", "-a", auth_socket]

        if options['tar']:
            cmdline.append('--tar')

        if options['ip'] != None:
            cmdline.append('-i')
            cmdline.append(options['ip'])
        self.logclass.logger(' '.join(cmdline))

        self.child = subprocess.Popen(
            cmdline, bufsize=1024, stdin=subprocess.PIPE, stdout=subprocess.PIPE)

        self.loglinks(options)

        self.waiter = __waiter__(
            self.child)  # Starts thread to wait for weborf termination

        self.waiter.child_terminated.connect(self._child_terminated)
        self.waiter.start()
        self._running = True

        return True

    def loglinks(self, options):
        '''Prints to the log all the links that weborf is listening to'''
        if options['ip'] == None:
            addrs4 = nhelper.getaddrs(False)

            if self.ipv6:
                addrs6 = nhelper.getaddrs(True)
            else:
                addrs6 = tuple()
        else:
            if self.ipv6:
                # address can be both ipv6 or mapped ipv4
                if '.' in options['ip']:
                    addrs6 = (options['ip'],)
                    addrs4 = (options['ip'][7:],)
                else:  # Normal ipv6
                    addrs4 = tuple()
                    addrs6 = (options['ip'],)
            else:
                addrs6 = tuple()
                addrs4 = (options['ip'],)

        # Output of addresses binded
        for i in addrs4:
            url = 'http://%s:%d/' % (i, options['port'])
            logentry = 'Address: <a href="%s">%s</a>' % (url, url)
            self.logclass.logger(logentry)
        for i in addrs6:
            url = 'http://[%s]:%d/' % (i, options['port'])
            logentry = 'Address: <a href="%s">%s</a>' % (url, url)
            self.logclass.logger(logentry)

    @QtCore.pyqtSlot(object, int)
    def _child_terminated(self, child, exit_code):
        '''Called when the child process is terminated
        param child is for now ignored'''
        if exit_code != 0:
            self.logclass.logger('Weborf terminated with exit code %d' %
                                 exit_code, self.logclass.DBG_ERROR)
        else:
            self.logclass.logger(
                "Termination complete", self.logclass.DBG_NOTICE)
        self._running = False
        pass

    def stop(self):
        '''Stop weborf and correlated processes.
        Will return True if it goes well
        It should be called only if start succeeded'''
        if self._running:
            self.logclass.logger(
                "Sending terminate signal and waiting for termination...")
            self.child.stdin.close()
            self.child.terminate()

        self.socket.close()
                          # Closing socket, so the accept will fail and the
                          # thread can terminate
        self.listener.stop()

        return True


class __listener__(QtCore.QThread):

    '''This class is used to listen to a socket.
    It will accept connections and send those connection
    using new_socket(PyQt_PyObject).
    '''

    new_socket = QtCore.pyqtSignal(object)

    def __init__(self, socket):
        QtCore.QThread.__init__(self)
        self.socket = socket
        self.socket.settimeout(2.0)
        self.cycle = True

    def stop(self):
        self.cycle = False

    def run(self):
        print("Ready to listen")
        self.socket.listen(1)
        while self.cycle:
            try:
                sock, addr = self.socket.accept()
                print(sock, addr)
                self.new_socket.emit(sock)
            except:
                pass


class __waiter__(QtCore.QThread):

    '''This class creates a separate thread that will wait for the
    termination of weborf, and performs a callback when it occurs.
    A more normal way to do this would have been to handle SIGCHLD but
    the use of QT libraries prevents the use of signals apparently.

    connect to child_terminated(PyQt_PyObject,PyQt_PyObject) to handle the event'''

    child_terminated = QtCore.pyqtSignal(object, object)

    def __init__(self, child):
        '''child: child process to wait
        '''
        QtCore.QThread.__init__(self)

        self.child = child

    def run(self):

        # wait termination for the child process
        t = self.child.wait()

        # Sends callback
        self.child_terminated.emit(self.child, t)
