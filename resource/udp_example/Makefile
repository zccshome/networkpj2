CC              = gcc
LD              = gcc
CFLAGS          = -Wall -g

LDFLAGS         =
DEFS            =
LIB             = 

all:  udps udpc

udps:  udps.c
	$(CC) $(DEFS) $(CFLAGS) -c udps.c
	$(LD) -o $@ $(LDFLAGS) udps.o $(LIB)
	rm -f udps.o

udpc:  udpc.c
	$(CC) $(DEFS) $(CFLAGS) -c udpc.c
	$(LD) -o $@ $(LDFLAGS) udpc.o $(LIB)
	rm -f udpc.o

clean:
	rm -f *.o udps udpc

