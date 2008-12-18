CC=gcc
#DEFS=-Ddebug
DEFS=
CFLAGS=-Wall $(DEFS) $(ARCHFLAGS)
LDFLAGS=-lpthread
ARCHFLAGS=-m32

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
	rm *~
	cd ..; tar cvjf weborf.tar.bz2 weborf/

moo:
	echo Questo Makefile ha i poteri della supermucca