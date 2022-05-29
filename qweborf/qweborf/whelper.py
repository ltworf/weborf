# Weborf
# Copyright (C) 2010-2018  Salvo "LtWorf" Tomaselli
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
from typing import Optional
from PyQt5 import QtCore

import qweborf.nhelper as nhelper


class weborf_runner():

    def __init__(self, logfunction):
        self.logclass = logfunction
        self.logclass.logger("Software initialized")

        self.child: Optional[subprocess.Popen] = None
        self.socket = None
        self.listener = None
        self.waiter: Optional[_Waiter] = None
        self.methods = set()
        self.username: Optional[str] = None
        self.password: Optional[str] = None
        self.ipv6 = True
        self._running = False
        self.version = ''
        self.webdav = False

        self.weborf = self._test_weborf()

    def _test_weborf(self) -> bool:
        '''Tests if weborf binary is existing.
        It will return true if everything is OK
        and false otherwise.'''

        try:
            out = subprocess.check_output(["weborf", "-k"]).decode('ascii').strip().split('\n')
        except Exception as e:
            self.logclass.logger(f'Unable to check capabilities weborf.\n{e}', self.logclass.DBG_ERROR)
            return False

        try:
            capabilities = {}
            for i in out:
                k, v = i.split(':', 1)
                capabilities[k] = v

            self.version = capabilities['version']
            self.ipv6 = capabilities['ipv'] == '6'
            self.webdav = capabilities['webdav'] == 'yes'
            self.https = capabilities.get('https') == 'yes'

            self.logclass.logger('Weborf version %s found' %
                                    self.version, self.logclass.DBG_NOTICE)
            self.logclass.logger('IPv%s protocol in use' %
                                    capabilities['ipv'], self.logclass.DBG_NOTICE)
            self.logclass.logger('Has webdav support: %s' % capabilities['webdav'], self.logclass.DBG_NOTICE)

            if capabilities['embedded_auth'] == 'yes':
                self.logclass.logger('Binary compiled with embedded authentication', self.logclass.DBG_ERROR)
                return False

            return True
        except KeyError as e:
            self.logclass.logger(
                'Capability %s not supported' % e, self.logclass.DBG_ERROR)
        except Exception as e:
            self.logclass.logger(
                'Unknown exception %s' % e, self.logclass.DBG_ERROR)

        return False

    def start(self, options) -> bool:
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
        if not self.__start_weborf(options, auth_socket):
            return False
        self.__listen_auth_socket(options)

        self.username = options['username']
        self.password = options['password']

        # Deciding which HTTP methods will be enabled
        self.methods = {'GET'}
        if options['dav']:
            self.methods.update({'OPTIONS', 'PROPFIND'})
            self.logclass.logger(
                "DAV access enabled", self.logclass.DBG_NOTICE)

            # If writing is enabled
            if options['write']:
                self.methods.update({'PUT', 'DELETE', 'COPY', 'MOVE', 'MKCOL'})
                self.logclass.logger(
                    'Writing access enabled.', self.logclass.DBG_WARNING)
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

    def socket_cback(self, sock):
        '''Recieves connection requests and decides if they have to be authorized or denied'''

        data = sock.recv(4096).split(b'\r\n')

        if len(data) < 5:
            sock.send(b' ')
            sock.close()
            self.logclass.logger("DENIED: Invalid request")
            return

        uri = data[0].decode('utf-8')
        client = data[1].decode()
        method = data[2].decode()
        username = data[3].decode('utf-8')
        password = data[4].decode('utf-8')

        # Checking if the request must be allowed or denied
        allow = True
        if self.username is not None:  # Must check username and password
            allow = self.username == username and self.password == password

        if method not in self.methods:
            allow = False

        if allow:
            self.logclass.logger("%s - %s %s" % (client, method, uri))
        else:
            sock.send(b' ')
            self.logclass.logger("DENIED: %s - %s %s" % (client, method, uri))

        sock.close()

    def __start_weborf(self, options, auth_socket) -> bool:
        '''Starts a weborf in a subprocess'''

        cmdline = ["weborf", "-p", str(options['port']), "-b", str(
            options['path']), "-I", "....", "-a", auth_socket]

        if options['tar'] and (options['cert'] or options['key']):
            self.logclass.logger('Tar and HTTPS cannot be enabled at the same time', self.logclass.DBG_ERROR)
            return False

        if options['tar']:
            cmdline.append('--tar')

        if options['cert'] or options['key']:
            cmdline.extend([
                '--cert',
                options['cert'],
                '--key',
                options['key'],
            ])

        if options['ip'] != None:
            cmdline.append('-i')
            cmdline.append(options['ip'])
        self.logclass.logger(' '.join(cmdline))

        self.child = subprocess.Popen(cmdline)

        self.loglinks(options)

        # Starts thread to wait for weborf termination
        self.waiter = _Waiter(self.child)

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
                addrs6 = []
        else:
            if self.ipv6:
                # address can be both ipv6 or mapped ipv4
                if '.' in options['ip']:
                    addrs6 = [options['ip']]
                    addrs4 = [options['ip'][7:]]
                else:  # Normal ipv6
                    addrs4 = []
                    addrs6 = [options['ip']]
            else:
                addrs6 = []
                addrs4 = [options['ip']]

        if options['cert'] or options['key']:
            protocol = 'https'
        else:
            protocol = 'http'

        # Output of addresses bound
        for i in addrs4:
            url = f'{protocol}://%s:%d/' % (i, options['port'])
            logentry = 'Address: <a href="%s">%s</a>' % (url, url)
            self.logclass.logger(logentry)
        for i in addrs6:
            url = f'{protocol}://[%s]:%d/' % (i, options['port'])
            logentry = 'Address: <a href="%s">%s</a>' % (url, url)
            self.logclass.logger(logentry)

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
            self.child.terminate()
        if self.socket:
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


class _Waiter(QtCore.QThread):

    '''This class creates a separate thread that will wait for the
    termination of weborf, and performs a callback when it occurs.
    A more normal way to do this would have been to handle SIGCHLD but
    the use of QT libraries prevents the use of signals apparently.

    connect to child_terminated(PyQt_PyObject,PyQt_PyObject) to handle the event'''

    child_terminated = QtCore.pyqtSignal(object, object)

    def __init__(self, child) -> None:
        '''child: child process to wait
        '''
        QtCore.QThread.__init__(self)

        self.child = child

    def run(self):

        # wait termination for the child process
        t = self.child.wait()

        # Sends callback
        self.child_terminated.emit(self.child, t)
