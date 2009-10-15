#!/usr/bin/python
# -*- coding: utf-8 -*-
'''
Weborf
Copyright (C) 2008  Salvo "LtWorf" Tomaselli

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
'''
#Loading configuration from file or setting default
try:
    execfile("/etc/weborf/pywrapper.conf")
except:
    TMPDIR="/tmp"
    SESSIONEXPIRE=600
    CONTENT="text/html"

#Deconding auth field
v=os.getenv("HTTP_AUTHORIZATION")
if v!=None:
    q=v.split(" ")
    os.putenv('AUTH_TYPE',q[0])
    auth=base64.b64decode(q[1]).split(":",1)
    os.putenv('AUTH_USER',auth[0])
    os.putenv('AUTH_PW',auth[1])
    
#Changing dir to script's one
for i in range(len(sys.argv[1])-1,-1,-1):
    if sys.argv[1][i]==os.sep:
        os.chdir(sys.argv[1][0:i])
        break


#Saves session, if there is one
savesession()