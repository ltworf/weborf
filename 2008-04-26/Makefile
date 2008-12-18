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
DEFS=
CFLAGS=-Wall $(DEFS) $(ARCHFLAGS)
LDFLAGS=-lpthread
#ARCHFLAGS=-m64


MANDIR=/usr/local/man/man1/
BINDIR=/usr/local/bin/


all: weborf

weborf: listener.o queue.o instance.o mystring.o utils.o
	$(CC) $(LDFLAGS) $(ARCHFLAGS) $+ -o $@

queue.c: queue.h
instance.c: instance.h
listener.c: listener.h
mystring.c: mystring.h
utils.c: utils.h

clean: 
	rm *.o weborf || echo Nothing to do 

source: clean 
	
	rm -f *~
	cd ..; tar cvjf weborf-`date +\%F | tr -d -`.tar.bz2 weborf/

install:
	mkdir -p $(MANDIR) || echo Creazione cartelle
	gzip -c weborf.1 > $(MANDIR)/weborf.1.gz
	
	cp weborf $(BINDIR)

uninstall:
	rm -f $(MANDIR)/weborf.1.gz
	rm -f $(BINDIR)/weborf

moo:
	echo Questo Makefile ha i poteri della supermucca
