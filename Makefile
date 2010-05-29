# Weborf
# Copyright (C) 2008  Salvo "LtWorf" Tomaselli
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

CC=gcc
#DEFS=-Ddebug
OFLAGS=-O2
#-pedantic -Wextra
CFLAGS=-Wall $(DEFS) $(ARCHFLAGS)  -Wformat -g
LDFLAGS=-lpthread

#Opensolaris flags
#CFLAGS=-D_POSIX_PTHREAD_SEMANTICS  -Wall $(DEFS) $(ARCHFLAGS)  -Wformat
#LDFLAGS=-lpthread -lsocket -lnsl 
#ARCHFLAGS=-m64

MANDIR=/usr/share/man/
BINDIR=/usr/bin/
DAEMONDIR=/etc/init.d/
CONFDIR=/etc/
CGIDIR=/usr/lib/cgi-bin/
#PYDIR=/usr/lib/python$(PYVERSION)/cgi_weborf

all: weborf cgi

weborf: listener.o queue.o instance.o mystring.o utils.o base64.o buffered_reader.o webdav.o
	$(CC) $(LDFLAGS) $(ARCHFLAGS) $(OFLAGS) $+ -o $@
	strip weborf

%.c: %.h

cgi:
	cd cgi_wrapper; make

debug: listener.o queue.o instance.o mystring.o utils.o base64.o buffered_reader.o webdav.o
	$(CC) -g $(LDFLAGS) $(ARCHFLAGS) $+ -o $@

clean: 
	rm -f *.o weborf debug *.orig *~ *.gz
	cd cgi_wrapper; make clean

cleanall: clean
	rm -rf `find | fgrep .svn`
	rm -rf `find | fgrep ~`

purge: uninstall
	rm -f $(CONFDIR)/weborf.conf

source: clean style
	cd ..; tar cvzf weborf-`date +\%F | tr -d -`.tar.gz weborf/

style:
	#‑‑align‑pointer=name To use when will really work
	astyle --indent=spaces=4 -a *c *h       

installdirs:
	install -d $(DESTDIR)/$(BINDIR)/
	install -d $(DESTDIR)/$(MANDIR)/man1
	install -d $(DESTDIR)/$(MANDIR)/man5
	install -d $(DESTDIR)/$(DAEMONDIR)
	install -d $(DESTDIR)/$(CGIDIR)
	#install -d $(DESTDIR)/$(PYDIR)

install: uninstall installdirs
	# Gzip the manpages
	gzip -9 -c weborf.1 > weborf.1.gz
	gzip -9 -c weborf.conf.5 > weborf.conf.5.gz

	# Install everything
	install -m 644 weborf.1.gz $(DESTDIR)/$(MANDIR)/man1/
	install -m 644 weborf.conf.5.gz $(DESTDIR)/$(MANDIR)/man5/
	install -m 755 weborf $(DESTDIR)/$(BINDIR)/
	#install -m 755 cgi_py_weborf.py $(DESTDIR)/$(CGIDIR)/cgi_py_weborf.py

	#cgi
	install -m 755 cgi_wrapper/weborf_cgi_wrapper $(DESTDIR)/$(CGIDIR)/weborf_cgi_wrapper
	install -m 755 cgi_wrapper/weborf_py_wrapper $(DESTDIR)/$(CGIDIR)/weborf_py_wrapper
	#install -m 644 python_cgi_weborf/__init__.py $(DESTDIR)/$(PYDIR)
	#install -m 644 python_cgi_weborf/cgi.py $(DESTDIR)/$(PYDIR)

	install -m 755 weborf.daemon $(DESTDIR)/$(DAEMONDIR)/weborf

	install -m 644 weborf.conf $(DESTDIR)/$(CONFDIR)/; fi

uninstall:
	rm -f $(DESTDIR)/$(MANDIR)/man5/weborf.conf.5.gz
	rm -f $(DESTDIR)/$(MANDIR)/man1/weborf.1.gz
	rm -f $(DESTDIR)/$(BINDIR)/weborf
	rm -f $(DESTDIR)/$(DAEMONDIR)/weborf
	#rm -f $(DESTDIR)/$(CGIDIR)/py_weborf
	#rm -f $(DESTDIR)/$(CGIDIR)/weborf_cgi_wrapper
	#rm -f $(DESTDIR)/$(CGIDIR)/weborf_py_wrapper


memcheck: debug
	valgrind -v --track-origins=yes --tool=memcheck --leak-check=yes --leak-resolution=high --show-reachable=yes --num-callers=20 --track-fds=yes ./debug || echo "Valgrind doesn't appear to be installed on this system"

moo:
	echo Questo Makefile ha i poteri della supermucca
