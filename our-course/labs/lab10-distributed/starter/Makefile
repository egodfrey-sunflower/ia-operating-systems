# Lab 10 -- builds the five tools.  GIVEN.
#
# The autograder does its own graded build (its own gcc line, its own copies
# of net.c/msg.c/rpc.h/fsproto.h) -- this Makefile is the documented dev
# workflow, not the graded one.

CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=gnu11 -g

TOOLS = udpecho reliable rpcdemo fileserver fileclient

all: $(TOOLS)

udpecho: udpecho.c net.c msg.c net.h msg.h
	$(CC) $(CFLAGS) -o $@ udpecho.c net.c msg.c

reliable: reliable.c net.c msg.c net.h msg.h
	$(CC) $(CFLAGS) -o $@ reliable.c net.c msg.c

rpcdemo: rpcdemo.c rpc.c net.c msg.c rpc.h net.h msg.h
	$(CC) $(CFLAGS) -o $@ rpcdemo.c rpc.c net.c msg.c

fileserver: fileserver.c rpc.c net.c msg.c rpc.h fsproto.h net.h msg.h
	$(CC) $(CFLAGS) -o $@ fileserver.c rpc.c net.c msg.c

fileclient: fileclient.c rpc.c net.c msg.c rpc.h fsproto.h net.h msg.h
	$(CC) $(CFLAGS) -o $@ fileclient.c rpc.c net.c msg.c

test: all
	../tests/run.sh .

clean:
	rm -f $(TOOLS)

.PHONY: all test clean
