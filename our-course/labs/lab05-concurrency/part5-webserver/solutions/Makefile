# Lab 5 Part 5 -- a concurrent web server, twice.
#
# `make` builds both servers and the load generator. `make test` runs the
# autograder over this directory.
#
# The autograder does not use this Makefile to build the graded binaries: it
# runs its own gcc lines. It does check that `make` works and produces both
# servers, because `make` is the workflow -- and because a submission with
# one working server is not this part.

CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=gnu11 -g -O2 -pthread

all: webserver-threaded webserver-event loadgen

webserver-threaded: wsthread.c http.c pcbuffer.c http.h pcbuffer.h
	$(CC) $(CFLAGS) -o $@ wsthread.c http.c pcbuffer.c

webserver-event: wsevent.c http.c http.h
	$(CC) $(CFLAGS) -o $@ wsevent.c http.c

# The load generator for PERF.md. It lives in tests/ because the harness
# uses it too; it takes no code of yours.
loadgen: ../tests/loadgen.c
	$(CC) $(CFLAGS) -o $@ ../tests/loadgen.c

test: all
	../tests/run.sh .

clean:
	rm -f webserver-threaded webserver-event loadgen

.PHONY: all test clean
