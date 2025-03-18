INCLUDE_DIRS = 
LIB_DIRS = 
CC=gcc

CDEFS=
CFLAGS= -O0 -g $(INCLUDE_DIRS) $(CDEFS)
LIBS= 

HFILES= 
CFILES= Q5_Timeout.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	Q5_Timeout

clean:
	-rm -f *.o *.d
	-rm -f Q5_Timeout

Q5_Timeout: Q5_Timeout.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o -lpthread -lm

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
