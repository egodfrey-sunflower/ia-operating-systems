# Lab 5 Part 3 -- a bounded buffer with condition variables.
#
# `make` builds libpcbuffer.a. `make test` runs the autograder over this
# directory.
#
# The autograder does not use this Makefile to build the graded library: it
# runs its own gcc line. It does check that `make` works, because `make` is
# the workflow.

CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=gnu11 -g -O2 -pthread

all: libpcbuffer.a

libpcbuffer.a: pcbuffer.o
	ar rcs $@ pcbuffer.o

pcbuffer.o: pcbuffer.c pcbuffer.h
	$(CC) $(CFLAGS) -c -o $@ pcbuffer.c

test: all
	../tests/run.sh .

clean:
	rm -f pcbuffer.o libpcbuffer.a

.PHONY: all test clean
