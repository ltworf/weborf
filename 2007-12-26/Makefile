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
	tar -czvv *.c *.h rfc2616.txt COPYING README Makefile > weborf.tgz
