CC=	cc
CFLAGS=	-O
LIBS=	-lrt
RM=	rm -f
SHELL=	/bin/sh

all:	timertest

timertest: main.c
	$(CC) $(CFLAGS) -o timertest main.c $(LIBS)

.PHONY: clean
clean:
	$(RM) timertest
