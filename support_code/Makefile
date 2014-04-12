TARGET 	= daemon

CC		= gcc
LD		= gcc

CFLAGS	= -O2 -Wall
LFLAGS	= 
LIBS	= -pthread

OBJECTS = rtlib.o rtgrading.o csapp.o daemon.o

$(TARGET): $(OBJECTS)
	$(LD) $(LFLAGS) $(LIBS) -o $@ $^

daemon.o: daemon.c daemon.h rtgrading.h csapp.h sircd.h
	$(CC) $(CFLAGS) -c -o $@ $<

rtlib.o: rtlib.c rtlib.h csapp.h
	$(CC) $(CFLAGS) -c -o $@ $<

rtgrading.o: rtgrading.c rtgrading.h
	$(CC) $(CFLAGS) -c -o $@ $<

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o
	rm -f $(TARGET)

.PHONY: clean
