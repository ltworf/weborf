#!/usr/bin/python
'''
Weborf
Copyright (C) 2007  Salvo "LtWorf" Tomaselli

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

import sys
import py_compile
import os

def hideErrors():
    '''Closes stderr so errors aren't shown anymore'''
    os.close(2)

def redirect(location):
    '''Sends to the client the request to redirect to another page.
    Unless php, here this can be used even after output'''
    os.write(4,"Location: "+location) #Writes location header
    sys.exit(33) #Tells weborf that a redirect is requested

#Changing dir to script's one
#os.chdir()

#Sets POST variables
_POST=os.environ

#Sets GET variables
_GET={}
if len(sys.argv)>2:
    for i in sys.argv[2].split("&"):
        v=i.split("=")
        _GET[v[0]]=v[1]

#Executes file
execfile(sys.argv[1])

#Extra needed headers
#os.write(4,"test")

sys.exit(0)