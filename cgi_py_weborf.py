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

import sys
import os
#import py_compile


def pyinfo():
    '''Shows information page'''
    print "<h1>Weborf Python CGI Wrapper</h1>"
    print "<p>Version 0.1</p>"
    print "<p>Written by Salvo 'LtWorf' Tomaselli <tiposchi@tiscali.it></p>"    
    
    
    i_vars=("_GET","_POST","_SERVER","_SESSION","_COOKIE","_FILES")
    for var in i_vars:
        v=eval(var)
        if isinstance(v,list):
            l=True
        else: #Dict
            l=False
        
        print "<H2>%s</H2>" % var        
        print "<table border=1>"
        for j in v:        
            if l:
                print "<tr><td>%s</td></tr>" % (j)
            else:            
                print "<tr><td>%s</td><td><code>%s</code></td></tr>" % (j,v[j])
        print "</table>"
    print "<p><h2>Weborf</h2></p><p>This program comes with ABSOLUTELY NO WARRANTY.<br>This is free software, and you are welcome to redistribute it<br>under certain conditions.<br>For details see the GPLv3 Licese.</p>"

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
    
def read_post():
    '''Reads POST data'''
    #Reading POST Data
    if 'CONTENT_LENGTH' not in os.environ:
        return None
                
    _RAW=sys.stdin.read(int(os.getenv('CONTENT_LENGTH')))
    if os.getenv('CONTENT_TYPE')=='application/x-www-form-urlencoded':
        for i in _RAW.split("&"):
            v=i.split("=")
            _POST[post_escape(v[0])]=post_escape(v[1])
    elif os.getenv('CONTENT_TYPE').startswith('multipart/form-data'):
        #Finding boundary
        for i in os.getenv('CONTENT_TYPE').split("; "):
            if i.strip().startswith("boundary"):
                boundary=i.split("=")[1]
        files=_RAW.split(boundary)
    
        for i in files:
            j=i.split("\r\n\r\n")
            if len(j)==1:
                continue

            dic={}
            dic['content']=j[1][:-2]
        
            fields=j[0].split("\r\n")
            for k in fields:
                a=k.split(": ",1)
                if len(a)==2:
                    dic[a[0]]=a[1]
                elif len(a[0])!=0:
                    dic[a[0]]=None
            for k in dic['Content-Disposition'].split("; "):
                d=k.split("=",1)
                if len(d)>1:
                    dic[d[0]]=d[1].replace("\"","")
                else:
                    dic[d[0]]=None
            _FILES.append(dic)
    return _RAW

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
    if 'PHPSESSID' not in _COOKIE or _COOKIE['PHPSESSID']==None: #No session, creating a new one
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
        import time
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
        elif len(i[0])!=0:
            dic[i[0]]=None
    return dic

def chdir_to_file(f):
    '''Changes chdir to the same one where the file is stored'''
    try:
        os.chdir(os.path.dirname(f))
    except:
        pass

def auth_fields():
    '''If there is authentication, gets username and password'''
    #Deconding auth field
    v=os.getenv("HTTP_AUTHORIZATION")
    if v!=None:
        import base64
        q=v.split(" ")
        os.environ['AUTH_TYPE']=q[0]
        auth=base64.b64decode(q[1]).split(":",1)
        os.environ['AUTH_USER']=auth[0]
        os.environ['AUTH_PW']=auth[1]


#Loading configuration from file or setting default
try:
    execfile("/etc/weborf/pywrapper.conf")
except:
    TMPDIR="/tmp"
    SESSIONEXPIRE=600

chdir_to_file(os.getenv("SCRIPT_FILENAME"))
auth_fields()


#Changing the order of those lines can be dangerous
_COOKIE=get_array('; ',os.getenv("HTTP_COOKIE"))
_GET=get_array('&',os.getenv("QUERY_STRING"))
_SESSION={}
_POST={}
_FILES=[]
_RAW=read_post()
_SERVER=os.environ


#Compiles file
#py_compile.compile(os.getenv("SCRIPT_FILENAME"))

#Executes file
execfile(os.getenv("SCRIPT_FILENAME"))

savesession()

sys.exit(0)
