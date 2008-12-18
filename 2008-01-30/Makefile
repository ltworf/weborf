CC=gcc
#DEFS=-Ddebug
DEFS=
CFLAGS=-Wall $(DEFS) $(ARCHFLAGS)
LDFLAGS=-lpthread
ARCHFLAGS= 
#-m32


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
	rm *.o weborf 

source: 
	
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
