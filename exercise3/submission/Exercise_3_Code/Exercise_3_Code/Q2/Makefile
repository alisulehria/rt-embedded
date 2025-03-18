INCLUDE_DIRS = 
LIB_DIRS = 
CC=gcc

CDEFS=
CFLAGS= -O0 -g $(INCLUDE_DIRS) $(CDEFS)
LIBS= 

HFILES= 
CFILES= Q2_MUTEX.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	Q2_MUTEX

clean:
	-rm -f *.o *.d
	-rm -f Q2_MUTEX

Q2_MUTEX: Q2_MUTEX.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o -lpthread -lm

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
