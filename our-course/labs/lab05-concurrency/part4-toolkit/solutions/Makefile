# Lab 5 Part 4 -- the synchronisation toolkit.
#
# `make` builds libtoolkit.a. `make test` runs the autograder over this
# directory.
#
# The autograder does not use this Makefile to build the graded library: it
# runs its own gcc line. It does check that `make` works, because `make` is
# the workflow.

CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=gnu11 -g -O2 -pthread

all: libtoolkit.a

libtoolkit.a: toolkit.o
	ar rcs $@ toolkit.o

toolkit.o: toolkit.c toolkit.h
	$(CC) $(CFLAGS) -c -o $@ toolkit.c

test: all
	../tests/run.sh .

clean:
	rm -f toolkit.o libtoolkit.a

.PHONY: all test clean
