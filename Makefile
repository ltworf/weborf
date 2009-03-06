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
OFLAGS=-O3
#-pedantic -Wextra
CFLAGS=-Wall $(DEFS) $(ARCHFLAGS)  -Wformat
LDFLAGS=-lpthread
#ARCHFLAGS=-m64


MANDIR=/usr/local/man/man1/
BINDIR=/usr/local/bin/
DAEMONDIR=/etc/init.d/
CONFDIR=/etc/

all: weborf

weborf: listener.o queue.o instance.o mystring.o utils.o base64.o
	$(CC) $(LDFLAGS) $(ARCHFLAGS) $(OFLAGS) $+ -o $@

queue.c: queue.h
instance.c: instance.h
listener.c: listener.h
mystring.c: mystring.h
utils.c: utils.h
base64.c: base64.h

debug: listener.o queue.o instance.o mystring.o utils.o base64.o 
	$(CC) -ggdb3 $(LDFLAGS) $(ARCHFLAGS) $+ -o $@

clean: 
	rm *.o weborf debug *.orig *~ || echo Nothing to do 
purge: uninstall
	rm -f $(CONFDIR)/weborf.conf || echo ok

source: clean style
	rm -f *~ *.orig
	cd ..; tar cvjf weborf-`date +\%F | tr -d -`.tar.bz2 weborf/
style:
	astyle --style=kr *c *h
install: uninstall
	mkdir -p $(MANDIR) || echo Creating directories
	gzip -c weborf.1 > $(MANDIR)/weborf.1.gz
	
	cp weborf.pywrap.py $(BINDIR)
	cp weborf $(BINDIR)
	cp weborf.daemon $(DAEMONDIR)/weborf
	chmod u+x $(DAEMONDIR)/weborf
	chmod a+x $(BINDIR)/weborf*
	if  ! test -e $(CONFDIR)/weborf.conf; then cp weborf.conf $(CONFDIR)/; fi

uninstall:
	rm -f $(MANDIR)/weborf.1.gz || echo ok
	rm -f $(BINDIR)/weborf || echo ok
	rm -f $(DAEMONDIR)/weborf || echo ok

memcheck: debug
	valgrind --tool=memcheck --leak-check=yes --leak-resolution=high --show-reachable=yes --num-callers=20 --track-fds=yes ./debug || echo "Valgrind doesn't appear to be installed on this system"

moo:
	echo Questo Makefile ha i poteri della supermucca
