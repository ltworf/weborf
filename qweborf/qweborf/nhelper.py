# Weborf
# Copyright (C) 2011-2020  Salvo "LtWorf" Tomaselli
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
# original code http://carnivore.it/2010/07/22/python_-_getifaddrs
# edited Salvo "LtWorf" Tomaselli <tiposchi@tiscali.it>

from ctypes import *
from socket import AF_INET, AF_INET6, AF_PACKET, inet_ntop
from sys import platform
import subprocess
import re
from typing import List, NamedTuple, Optional


def getifaddrs():
    # getifaddr structs
    class ifa_ifu_u(Union):
        _fields_ = [
            ("ifu_broadaddr", c_void_p),
            ("ifu_dstaddr",   c_void_p)
        ]

    class ifaddrs(Structure):
        _fields_ = [
            ("ifa_next",    c_void_p),
            ("ifa_name",    c_char_p),
            ("ifa_flags",   c_uint),
            ("ifa_addr",    c_void_p),
            ("ifa_netmask", c_void_p),
            ("ifa_ifu",     ifa_ifu_u),
            ("ifa_data",    c_void_p)
        ]

    # AF_UNKNOWN / generic
    class sockaddr(Structure):
        if platform.startswith("darwin") or platform.startswith("freebsd"):
            _fields_ = [
                ("sa_len",     c_uint8),
                ("sa_family",  c_uint8),
                ("sa_data",   (c_uint8 * 14))]
        else:
            _fields_ = [
                ("sa_family", c_uint16),
                ("sa_data",   (c_uint8 * 14))
            ]

    # AF_INET / IPv4
    class in_addr(Union):
        _fields_ = [
            ("s_addr", c_uint32),
        ]

    class sockaddr_in(Structure):
        _fields_ = [
            ("sin_family", c_short),
            ("sin_port",   c_ushort),
            ("sin_addr",   in_addr),
            ("sin_zero",   (c_char * 8)),  # padding
        ]

    # AF_INET6 / IPv6
    class in6_u(Union):
        _fields_ = [
            ("u6_addr8",  (c_uint8 * 16)),
            ("u6_addr16", (c_uint16 * 8)),
            ("u6_addr32", (c_uint32 * 4))
        ]

    class in6_addr(Union):
        _fields_ = [
            ("in6_u", in6_u),
        ]

    class sockaddr_in6(Structure):
        _fields_ = [
            ("sin6_family",   c_short),
            ("sin6_port",     c_ushort),
            ("sin6_flowinfo", c_uint32),
            ("sin6_addr",     in6_addr),
            ("sin6_scope_id", c_uint32),
        ]

    # AF_PACKET / Linux
    class sockaddr_ll(Structure):
        _fields_ = [
            ("sll_family",   c_uint16),
            ("sll_protocol", c_uint16),
            ("sll_ifindex",  c_uint32),
            ("sll_hatype",   c_uint16),
            ("sll_pktype",   c_uint8),
            ("sll_halen",    c_uint8),
            ("sll_addr",     (c_uint8 * 8))
        ]

    # AF_LINK / BSD|OSX
    class sockaddr_dl(Structure):
        _fields_ = [
            ("sdl_len",    c_uint8),
            ("sdl_family", c_uint8),
            ("sdl_index",  c_uint16),
            ("sdl_type",   c_uint8),
            ("sdl_nlen",   c_uint8),
            ("sdl_alen",   c_uint8),
            ("sdl_slen",   c_uint8),
            ("sdl_data",   (c_uint8 * 46))
        ]

    libc = CDLL("libc.so.6")
    ptr = c_void_p(None)
    result = libc.getifaddrs(pointer(ptr))
    if result:
        return None
    ifa = ifaddrs.from_address(ptr.value)
    result = {}

    while True:
        # name = ifa.ifa_name
        name = ifa.ifa_name.decode('ascii')  # use this for python3

        if name not in result:
            result[name] = {}

        if ifa.ifa_addr != None:
            sa = sockaddr.from_address(ifa.ifa_addr)

        if sa.sa_family not in result[name]:
            result[name][sa.sa_family] = []

        data = {}

        if sa.sa_family == AF_INET:
            if ifa.ifa_addr is not None:
                si = sockaddr_in.from_address(ifa.ifa_addr)
                data['addr'] = inet_ntop(si.sin_family, si.sin_addr)
            if ifa.ifa_netmask is not None:
                si = sockaddr_in.from_address(ifa.ifa_netmask)
                data['netmask'] = inet_ntop(si.sin_family, si.sin_addr)

        if sa.sa_family == AF_INET6:
            if ifa.ifa_addr is not None:
                si = sockaddr_in6.from_address(ifa.ifa_addr)
                data['addr'] = inet_ntop(si.sin6_family, si.sin6_addr)
                if data['addr'].startswith('fe80:'):
                    data['scope'] = si.sin6_scope_id
            if ifa.ifa_netmask is not None:
                si = sockaddr_in6.from_address(ifa.ifa_netmask)
                data['netmask'] = inet_ntop(si.sin6_family, si.sin6_addr)

        if sa.sa_family == AF_PACKET:
            if ifa.ifa_addr is not None:
                si = sockaddr_ll.from_address(ifa.ifa_addr)
                addr = ""
                for i in range(si.sll_halen):
                    addr += "%02x:" % si.sll_addr[i]
                addr = addr[:-1]
                data['addr'] = addr

        if len(data) > 0:
            result[name][sa.sa_family].append(data)

        if ifa.ifa_next:
            ifa = ifaddrs.from_address(ifa.ifa_next)
        else:
            break

    libc.freeifaddrs(ptr)
    return result


class Redirection(NamedTuple):
    '''This class represents a NAT redirection'''
    lport: int
    eport: int
    ip: str
    proto: str
    description: str

    def __str__(self) -> str:
        return '%s %d->%s:%d\t\'%s\'' % (self.proto, self.eport, self.ip, self.lport, self.description)

    def create_redirection(self) -> bool:
        '''Activates the redirection.
        Returns true if the redirection becomes active'''
        try:
            subprocess.check_call(
                ['upnpc', '-a', self.ip, str(self.lport), str(self.eport), self.proto]
            )
        except:
            return False

        for i in get_redirections():
            if i == self:
                return True
        return False

    def remove_redirection(self) -> None:
        subprocess.check_call(['upnpc', '-d', str(self.eport), self.proto])


def getaddrs(ipv6: bool=True) -> List[str]:
    '''Returns the list of the IP addresses it is possible to bind to

    ipv6: if true, ONLY ipv6 addresses will be returned (ipv4 will be mapped to ipv6)
    '''
    ifaces = getifaddrs()
    l_ipv4 = []
    l_ipv6 = []

    # Cycle the interfaces
    for iface in ifaces:

        # Cycle address family
        for family in ifaces[iface]:
            # Cycle addresses
            for addr in ifaces[iface][family]:
                if 'addr' in addr:
                    if family == AF_INET:
                        l_ipv4.append(str(addr['addr']))
                    elif family == AF_INET6:
                        l_ipv6.append(str(addr['addr']))

    if ipv6 == True:
        # Transforming ipv4 into ipv6-mapped
        for i in range(len(l_ipv4)):
            l_ipv4[i] = '::ffff:%s' % l_ipv4[i]
        return l_ipv4 + l_ipv6
    else:
        return l_ipv4


def open_nat(port: int) -> Optional[Redirection]:
    '''Tries to open a port in the nat device
    directing it to the current host.
    '''
    # Chooses the external port
    eport = port
    used_ports = {i.eport for i in get_redirections()}

    while True:
        if eport in used_ports:
            eport += 1
        else:
            break

    # Chooses the local ip
    ip = [i for i in getaddrs(False) if re.match(
        r'^10\..*', i) != None or re.match(r'^192\.168\..*', i) != None][0]

    r = Redirection(port, eport, ip, "TCP", "weborf")

    if r.create_redirection():
        return r
    return None


def externaladdr() -> Optional[str]:
    '''Returns the public IP address.
    none in case of error'''
    out = subprocess.check_output(['external-ip']).decode('ascii').strip()

    if re.match(r'[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}', out) == None:
        return None
    return out


def can_redirect() -> bool:
    '''Returns true if upnpc is installed and NAT traversal can
    be attempted'''

    try:
        subprocess.call(['upnpc'], stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
        return True
    except:
        return False


def get_redirections() -> List[Redirection]:
    '''Returns a list of current NAT redirections'''
    out = subprocess.check_output(['upnpc', '-l']).decode('ascii').strip().split('\n')

    redirections = []

    for i in out:
        m = re.match(
            r'[ ]*([0-9]+)[ ]+(UDP|TCP)[ ]+([0-9]+)->([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}):([0-9]+)[ ]+\'(.*)\' \'\'', i)
        if m is not None:
            redirect = m.groups()
            r = Redirection(
                int(redirect[4]),
                int(redirect[2]),
                redirect[3],
                redirect[1],
                redirect[5],
            )
            redirections.append(r)
    return redirections


def printifconfig() -> None:
    ifaces = getifaddrs()
    for iface in ifaces:
        print(iface)
        for family in ifaces[iface]:
            print("\t%s" %
                  {AF_INET: 'IPv4', AF_INET6: 'IPv6', AF_PACKET: 'HW'}[family])
            for addr in ifaces[iface][family]:
                for i in ['addr', 'netmask', 'scope']:
                    if i in addr:
                        print("\t\t%s %s" % (i, str(addr[i])))
                print("")
