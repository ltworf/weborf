# Weborf
# Copyright (C) 2009  Salvo "LtWorf" Tomaselli
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
#OFLAGS=-O3
#-pedantic -Wextra
CFLAGS=-Wall $(DEFS) $(ARCHFLAGS)  -Wformat
#LDFLAGS=-lpthread
#ARCHFLAGS=-m64

all: weborf_cgi_wrapper weborf_py_wrapper

weborf_cgi_wrapper: wrapper.c
	$(CC) $(LDFLAGS) $(ARCHFLAGS) $(OFLAGS) $+ -o $@

weborf_py_wrapper: py_wrapper.c
	$(CC) $(LDFLAGS) $(ARCHFLAGS) $(OFLAGS) $+ -o $@

%.c: %.h

clean: 
	rm -f *.o *.orig *~ *.gz weborf_cgi_wrapper  weborf_py_wrapper

style:
	astyle --style=kr *c *h
