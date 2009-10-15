#!/usr/bin/python
# -*- coding: utf-8 -*-
'''
Weborf
Copyright (C) 2009  Salvo "LtWorf" Tomaselli

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

import time
import sys
import py_compile
import os
import base64

def post_escape(val):
    '''Post fields use certains escapes. This function returns the original string'''
    val=val.replace("+"," ") #Replaces all + with a space

    i=val.find("%") #% is the char for an exadecimal escape
    while i!=-1: #If there is a % in the non parsed part of the string
        s=val[i+1] + val[i+2] #Extract the exadecimal code
        if s!="37":
            #Replaces all the escapes in the string
            val=val.replace("%"+s,chr(int(s,16)))
        else:
            '''Replaces only once because this char is a % so there would be %
            that aren't escapes in the non parsed part of the string'''
            val=val.replace("%"+s,chr(int(s,16)),1)

        i=val.find("%",i+1)

    return val

def redirect(location):
    '''Sends to the client the request to redirect to another page.
    Unless php, here this can be used even after output'''
    os.write(1,"Status: 303\r\nLocation: "+location+"\r\n\r\n") #Writes location header
    sys.exit(0) #Redirects

def savesession():
    '''Saves the session to the file'''
    import csv
    if 'PHPSESSID' not in _COOKIE==None:
        return #No session to save
    #Opens the file with the session

    fp=file(TMPDIR+"/"+_COOKIE['PHPSESSID'],"w")
    writer=csv.writer(fp)

    #Converting dictionary into 2 level array for csv module
    a=[]
    for i in _SESSION:
        a.append((i,_SESSION[i]))
    writer.writerows(a)
    fp.close()

def session_start():
    '''Inits the session vars'''
    if 'PHPSESSID' not in _COOKIE==None or _COOKIE['PHPSESSID']==None: #No session, creating a new one
        import random
        import md5

        #Creating session's id with random numbers and multiple hashes
        r=random.Random()

        a=md5.md5(os.getenv("SCRIPT_FILENAME")).hexdigest()+md5.md5(str(r.random())).hexdigest()
        for i in range(10):
            a=md5.md5(a).hexdigest()+md5.md5(str(r.random())).hexdigest()

        s_id= "weborf-%s-%s" % (str(os.getpid()), a)
        setcookie('PHPSESSID',s_id)
        _COOKIE['PHPSESSID']=s_id
    else:#Session exists, loading data
        try:
            #If session expired after inactivity
            if (os.stat(TMPDIR+"/"+_COOKIE['PHPSESSID'])[7] + SESSIONEXPIRE) < time.time():
                #Deletes old session file, just to try to avoid to fill the disk
                os.unlink(TMPDIR+"/"+_COOKIE['PHPSESSID'])
                #Creating an empty session
                _COOKIE['PHPSESSID']=None

                session_start()
                return
            import csv
            fp=file(TMPDIR+"/"+_COOKIE['PHPSESSID'])
            reader=csv.reader(fp) #Creating a csv reader
            for i in reader.__iter__(): #Iterating rows
                _SESSION[i[0]]=i[1]
        except:
            #Start sessions with a new session id
            _COOKIE['PHPSESSID']=None
            session_start()

def setcookie(name,value,expires=None):
    '''Sets a cookie, by default it will be a session cookie.
    Expires is the time in seconds to wait to make the cookie expire'''    
    if expires!=None:
        s= "Set-Cookie: %s=%s; Max-Age=%s\r\n" % (str(name),str(value),str(expires))
    else:
        s= "Set-Cookie: %s=%s\r\n" % (str(name),str(value))
    sys.stdout.write(s)
    _COOKIE[str(name)]=str(value)

def finalize_headers(content="text/html"):
    sys.stdout.write("Content-Type: %s\r\n\r\n"%content)

def get_array(sep,query):
    '''Returns dictionary containing all the data passed via GET'''
    dic={}
    if query==None:
        return dic
    for p in query.split(sep):
        i=p.split("=",1)
        if len(i)!=1:
            dic[i[0]]=i[1]
        else:
            dic[i[0]]=None
    return dic

#Loading configuration from file or setting default
try:
    execfile("/etc/weborf/pywrapper.conf")
except:
    TMPDIR="/tmp"
    SESSIONEXPIRE=600

_COOKIE=get_array('; ',os.getenv("HTTP_COOKIE"))
_GET=get_array('&',os.getenv("QUERY_STRING"))
_SESSION={}
_POST={}
_RAW=None

#Reading POST Data
if 'CONTENT_LENGTH' in os.environ:
    _RAW=sys.stdin.read(int(os.getenv('CONTENT_LENGTH')))
    if os.getenv('HTTP_CONTENT_TYPE')=='application/x-www-form-urlencoded':
        for i in _RAW.split("&"):
            v=i.split("=")
            _POST[post_escape(v[0])]=post_escape(v[1])


#Executes file
execfile(os.getenv("SCRIPT_FILENAME"))

savesession()

sys.exit(0)

