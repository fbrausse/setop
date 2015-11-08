CFLAGS  = -std=c99 -Wall -Wno-unused -D_POSIX_C_SOURCE=200809L
YFLAGS  =
OBJS    = setop.o tnode.o tlex.o tparse.o
LEX     = lex
YACC    = yacc

all: setop
all: CFLAGS += -O2

debug: setop
debug: CFLAGS += -ggdb
debug: YFLAGS += -t -g -v

setop: $(OBJS)

$(OBJS): %.o: %.c $(wildcard *.h) Makefile
setop.o: tlex.h tparse.h
tlex.o: tlex.h
tparse.o: tparse.h
tparse.o: CFLAGS += -DYYERROR_VERBOSE=1

tlex.h tlex.c: tlex.l Makefile
	$(LEX) tlex.l

tparse.h tparse.c: tparse.y Makefile
	$(YACC) $(YFLAGS) tparse.y

clean:
	$(RM) $(OBJS) {tparse,tlex}.[ch] tparse.{output,dot}

.PHONY: all clean

