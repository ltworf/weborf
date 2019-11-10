#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Weborf
# Copyright (C) 2010-2019  Salvo "LtWorf" Tomaselli
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

from PyQt5 import QtGui, QtWidgets, QtCore
import sys
import os

import qweborf.main as main
import qweborf.whelper as whelper
import qweborf.nhelper as nhelper


class qweborfForm (QtWidgets.QWidget):

    DBG_DEFAULT = 0
    DBG_WARNING = 1
    DBG_ERROR = 2
    DBG_NOTICE = 3

    def setUi(self, ui) -> None:
        self.ui = ui
        self.weborf = whelper.weborf_runner(self)
        self.started = False

        if self.weborf.version >= '0.13':
            self.ui.chkTar.setEnabled(True)
        else:
            self.ui.chkTar.setEnabled(False)

        if not self.weborf.webdav:
            self.ui.chkDav.setEnabled(False)
            self.ui.chkWrite.setEnabled(False)

        if not self.weborf.https:
            self.ui.txtKey.setEnabled(False)
            self.ui.txtCert.setEnabled(False)
            self.ui.cmdOpenCert.setEnabled(False)
            self.ui.cmdOpenKey.setEnabled(False)

        # Listing addresses
        for i in nhelper.getaddrs(self.weborf.ipv6):
            self.ui.cmbAddress.addItem(i, None)

        self.ui.chkNAT.setEnabled(nhelper.can_redirect())

        self.defaultdir = QtCore.QStandardPaths.writableLocation(
            QtCore.QStandardPaths.HomeLocation)

        initialdir = self.defaultdir

        if len(sys.argv) > 1 and os.path.exists(sys.argv[1]):
            initialdir = sys.argv[1]

        self.ui.txtPath.setText(initialdir)

    def logger(self, data, level=DBG_DEFAULT):
        '''logs an entry, showing it in the GUI'''
        if level == self.DBG_WARNING:
            data = '<font color="orange"><strong>WARNING</strong></font>: %s' % data
        elif level == self.DBG_ERROR:
            data = '<font color="red"><strong>ERROR</strong></font>: %s' % data
        elif level == self.DBG_NOTICE:
            data = '<font color="green"><strong>NOTICE</strong></font>: %s' % data
        self.ui.txtLog.moveCursor(QtGui.QTextCursor.End)
        self.ui.txtLog.insertHtml(data + '<br>')
        self.ui.txtLog.moveCursor(QtGui.QTextCursor.End)

    def setDefaultValues(self):
        '''Sets default values into the form GUI. It has to be
        called after the form has been initialized'''
        pass

    def stop_sharing(self):
        if self.weborf.stop():
            self.ui.cmdStart.setEnabled(True)
            self.ui.cmdStop.setEnabled(False)
            self.ui.tabWidget.setEnabled(True)
            self.started = False

    def about(self):

        self.logger('<hr><strong>Qweborf 0.16</strong>')
        self.logger('This program comes with ABSOLUTELY NO WARRANTY.'
                    ' This is free software, and you are welcome to redistribute it under certain conditions.'
                    ' For details see the <a href="http://www.gnu.org/licenses/gpl.html">GPLv3 Licese</a>.')
        self.logger(
            '<a href="http://ltworf.github.io/weborf/">Homepage</a>')
        self.logger(
            'Salvo \'LtWorf\' Tomaselli <a href="mailto:tiposchi@tiscali.it">&lt;tiposchi@tiscali.it&gt;</a>')

    def terminate(self):
        if self.started:
            self.stop_sharing()
        sys.exit(0)

    def start_sharing(self):

        options = {}  # Dictionary with the chosen options

        options['path'] = self.ui.txtPath.text()

        if self.ui.chkEnableAuth.isChecked():
            options['username'] = self.ui.txtUsername.text()
            options['password'] = self.ui.txtPassword.text()
        else:
            options['username'] = None
            options['password'] = None

        if self.ui.txtKey.text() or self.ui.txtCert.text():
            options['key'] = self.ui.txtKey.text()
            options['cert'] = self.ui.txtCert.text()
        else:
            options['key'] = None
            options['cert'] = None

        options['port'] = self.ui.spinPort.value()

        options['dav'] = self.ui.chkDav.isChecked()
        if self.ui.chkDav.isChecked():
            options['write'] = self.ui.chkWrite.isChecked()
        else:
            options['write'] = False

        options['tar'] = self.ui.chkTar.isChecked()

        if self.ui.cmbAddress.currentIndex() == 0:
            options['ip'] = None
        else:
            options['ip'] = str(self.ui.cmbAddress.currentText())

        if self.weborf.start(options):
            self.ui.cmdStart.setEnabled(False)
            self.ui.cmdStop.setEnabled(True)
            self.ui.tabWidget.setEnabled(False)
            self.started = True

        if self.ui.chkNAT.isChecked() and self.started==True:
            self.logger('Trying to use UPnP to open a redirection in the NAT device. Please wait...')
            QtCore.QCoreApplication.processEvents()
            external_addr = nhelper.externaladdr()
            self.logger('Public IP address %s' % str(external_addr))
            QtCore.QCoreApplication.processEvents()
            if external_addr:
                redirection = nhelper.open_nat(options['port'])
                if redirection:
                    self.redirection = redirection
                    url='http://%s:%d/' % (external_addr,redirection.eport)
                    logentry='Public address: <a href="%s">%s</a>' % (url,url)
                    self.logger(logentry)
                else:
                    self.logger('Could not create NAT route')
            else:
                self.logger('Could not find the external IP address')

    def select_cert(self) -> None:
        fname = QtWidgets.QFileDialog.getOpenFileName(
            self,
            'Certificate',
            self.ui.txtCert.text(),
            filter='PEM certificates(*.pem);;All files(*)',
        )[0]
        if fname:
            self.ui.txtCert.setText(fname)


    def select_key(self) -> None:
        fname = QtWidgets.QFileDialog.getOpenFileName(
            self,
            'Certificate',
            self.ui.txtKey.text(),
        )[0]
        if fname:
            self.ui.txtKey.setText(fname)

    def select_path(self) -> None:
        current = self.ui.txtPath.text()
        if len(current) == 0:
            current = self.defaultdir
        dirname = QtWidgets.QFileDialog.getExistingDirectory(
            self,
            'Directory to share',
            current,
            QtWidgets.QFileDialog.ShowDirsOnly
        )
        if len(dirname) > 0:
            self.ui.txtPath.setText(dirname)


def q_main() -> None:
    import sys
    app = QtWidgets.QApplication(sys.argv)
    Form = qweborfForm()
    ui = main.Ui_Form()
    ui.setupUi(Form)
    Form.setUi(ui)
    Form.show()
    res = app.exec_()
    Form.terminate()
    sys.exit(res)

if __name__ == "__main__":
    q_main()
