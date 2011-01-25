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

class weborf_runner():
    def __init__(self,logfunction):
        self.logclass=logfunction
        self.logclass.logger("Software initialized")
        
        self.weborf=self.test_weborf()
        
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
        self.logclass.logger("Starting weborf with options")
        return True

    def stop(self):
        '''Stop weborf and correlated processes.
        Will return True if it goes well'''
        
        return True
    