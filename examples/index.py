#!/usr/bin/python
# -*- coding: utf-8 -*-

'''
This is an example of index file.
It adds a "." to a session variable on every refresh, shows the informations and sets a cookie.
'''

import os
import sys
from cgi_weborf import * #imports module cgi

cgi.session_start() 
cgi.setcookie("id","33")

cgi.finalize_headers() #will output the final \r\n\r\n and the content-type header

if 'nome' in cgi.SESSION:
        cgi.SESSION['nome']+="."
else:
        cgi.SESSION['nome']="."

sys.stdout.write( "<html>")
sys.stdout.write( "<body>")

sys.stdout.write( "<h1>example</h1>")
cgi.pyinfo()
sys.stdout.write(  "</body>")
sys.stdout.write(  "</html>")

cgi.savesession()
