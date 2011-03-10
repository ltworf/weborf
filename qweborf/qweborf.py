#!/usr/bin/env python
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

from PyQt4 import QtCore, QtGui
import main
import whelper
import nhelper
import sys
import os

class qweborfForm (QtGui.QWidget):
    '''This class is the form used for the survey, needed to intercept the events.
    It also sends the data with http POST to a page hosted on galileo'''
    def setUi(self,ui):
        self.ui=ui
        self.weborf=whelper.weborf_runner(self)
        self.started=False
        
        #Listing addresses
        for i in nhelper.getaddrs(self.weborf.ipv6):
            self.ui.cmbAddress.addItem(i,None)
        
        if 'HOME' in os.environ:
            self.ui.txtPath.setText(os.environ['HOME'])
            self.defaultdir=os.environ['HOME']
        else:
            self.ui.txtPath.setText('/')
            self.defaultdir='/'
            
    def logger(self,data):
        #print data
        #print dir(self.ui.txtLog)
        
        self.ui.txtLog.appendPlainText(data)
    
    def setDefaultValues(self):
        '''Sets default values into the form GUI. It has to be
        called after the form has been initialized'''
        pass
    def dav_toggle(self,state):
        self.ui.chkWrite.setEnabled(state)
    def auth_toggle(self,state):
        self.ui.txtPassword.setEnabled(state)
        self.ui.txtUsername.setEnabled(state)
                        
    def stop_sharing(self):
        if self.weborf.stop():
            self.ui.cmdStart.setEnabled(True)
            self.ui.cmdStop.setEnabled(False)
            self.ui.tabWidget.setEnabled(True)
            self.started=False
            
    def terminate(self):
        if self.started:
            self.stop_sharing()
        sys.exit(0)
        
    def start_sharing(self):
        
        options={} #Dictionary with the chosen options
        
        options['path']=self.ui.txtPath.text()

        if self.ui.chkEnableAuth.isChecked():
            options['username']=self.ui.txtUsername.text()
            options['password']=self.ui.txtPassword.text()
        else:
            options['username']=None
            options['password']=None
            
        options['port'] = self.ui.spinPort.value()
        
        options['dav'] = self.ui.chkDav.isChecked()
        if self.ui.chkDav.isChecked():
            options['write'] = self.ui.chkWrite.isChecked()
        else:
            options['write'] = False
        
        if self.ui.cmbAddress.currentIndex()==0:
            options['ip']=None
        else:
            options['ip']=str(self.ui.cmbAddress.currentText())
            
        if self.weborf.start(options):
            self.ui.cmdStart.setEnabled(False)
            self.ui.cmdStop.setEnabled(True)
            self.ui.tabWidget.setEnabled(False)
            self.started=True
        
        
    def select_path(self):
        #filename = QtGui.QFileDialog
        #dial=QtGui.QFileDialog()
        dirname= QtGui.QFileDialog.getExistingDirectory(
            self,
            QtGui.QApplication.translate("Form", "Directory to share", None, QtGui.QApplication.UnicodeUTF8),
            self.defaultdir,
            QtGui.QFileDialog.ShowDirsOnly
        )
        
        self.ui.txtPath.setText(dirname)

if __name__ == "__main__":
    import sys
    app = QtGui.QApplication(sys.argv)
    Form = qweborfForm()
    ui = main.Ui_Form()
    ui.setupUi(Form)
    Form.setUi(ui)
    Form.show()
    res=app.exec_()
    Form.terminate()
    sys.exit(res)
