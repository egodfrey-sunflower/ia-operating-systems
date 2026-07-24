# Lab 5 Part 1 -- user-level threads.
#
# `make` builds libuthread.a from uthread.c and swtch.S.
# `make test` runs the autograder over this directory.
#
# The autograder does not use this Makefile to build the graded library: it
# runs its own gcc line, so a Makefile that quietly drops -Werror changes
# nothing. It does check that `make` works, because `make` is the workflow.

CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=gnu11 -g -O2
AR      = ar

OBJS    = uthread.o swtch.o

all: libuthread.a

libuthread.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

uthread.o: uthread.c uthread.h
	$(CC) $(CFLAGS) -c -o $@ uthread.c

swtch.o: swtch.S
	$(CC) $(CFLAGS) -c -o $@ swtch.S

test: all
	../tests/run.sh .

clean:
	rm -f $(OBJS) libuthread.a

.PHONY: all test clean
