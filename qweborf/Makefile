# Weborf
# Copyright (C) 2019-2022  Salvo "LtWorf" Tomaselli
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

qweborf/main.py: qweborf/main.ui
	pyuic5 qweborf/main.ui > qweborf/main.py

.PHONY: clean
clean:
	$(RM) qweborf/main.py
	$(RM) -r build/
	$(RM) -r .mypy_cache/

.PHONY: install
install: qweborf/main.py
	python3 setup.py install --root=$${DESTDIR:-/} --install-layout=deb
	install -D qweborf/qweborf $${DESTDIR:-/}/bin/qweborf
	install -D -m644 man/qweborf.1 $${DESTDIR:-/}/usr/share/man/man1/qweborf.1
	install -D -m644 integration/qweborf.desktop $${DESTDIR:-/}/usr/share/applications/qweborf.desktop
	install -D -m644 integration/qweborf_servicemenu.desktop $${DESTDIR:-/}/usr/share/kservices5/ServiceMenus/qweborf_servicemenu.desktop
	install -D -m644 integration/qweborf.png $${DESTDIR:-/}/usr/share/icons/hicolor/256x256/apps/qweborf.png
	install -D -m644 integration/qweborf.metainfo.xml $${DESTDIR:-/}/usr/share/metainfo/qweborf.metainfo.xml

.PHONY: mypy
mypy:
	mypy --config-file mypy.conf qweborf
