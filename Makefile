CFLAGS  = -std=c99 -Wall -Wno-unused
CFLAGS += -O2
#CFLAGS += -ggdb

OBJS    = setop.o tlex.o tparse.o

all: setop

setop: $(OBJS)

$(OBJS): %.o: %.c $(wildcard *.h) Makefile
setop.o: tlex.h tparse.h
tlex.o: tlex.h
tparse.o: tparse.h
tparse.o: CFLAGS += -DYYERROR_VERBOSE=1

tlex.h tlex.c: tlex.l
	flex tlex.l

tparse.h tparse.c: tparse.y
	bison tparse.y

clean:
	$(RM) $(OBJS) {tparse,tlex}.[ch]

.PHONY: all clean

