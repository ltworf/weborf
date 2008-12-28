#!/usr/bin/python
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
import sys
 
auth={} #Creates a dictionary
 
#fill it with some data
auth['worf']="password"
auth['data']="soong"
auth['picard']="locutus"
auth['riker']="smooth"
 
#Gives to args readable names
method=sys.argv[1]
page=sys.argv[2]
ip=sys.argv[3]
 
#username and password parameters are optional, checks for their presence
if len(sys.argv)>=5:
        username=sys.argv[4]
else:
        username=""
 
if len(sys.argv)>=6:
        password=sys.argv[5]
else:
        password=""
 
#If the page requested doesn't begin with the /film/ string, allow the access
#if not page[:6]=="/film/":
#        sys.exit(0)
 
#Otherwise it requires a valid username and password
if (username in auth) and (auth[username]==password):
        sys.exit(0)
 
sys.exit(1)
