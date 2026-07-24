# Lab 5 Part 2 -- locks.
#
# `make` builds libmylock.a. `make test` runs the autograder over this
# directory. `make bench` builds the measurement driver for LOCKS.md.
#
# The autograder does not use this Makefile to build the graded library: it
# runs its own gcc line. It does check that `make` works, because `make` is
# the workflow.
#
# -O2 is not optional here. A spin loop over a variable the compiler does not
# believe can change is a loop it may hoist the load out of, and then your
# lock waits for ever on a value it read once. That is the bug this part is
# about, and at -O0 you would never meet it.

CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=gnu11 -g -O2 -pthread
LDLIBS  = -pthread

all: libmylock.a

libmylock.a: mylock.o
	ar rcs $@ mylock.o

mylock.o: mylock.c mylock.h hbnotate.h
	$(CC) $(CFLAGS) -c -o $@ mylock.c

test: all
	../tests/run.sh .

bench: all
	$(CC) $(CFLAGS) -I. -o bench ../tests/bench.c libmylock.a $(LDLIBS)

clean:
	rm -f mylock.o libmylock.a bench

.PHONY: all test bench clean
