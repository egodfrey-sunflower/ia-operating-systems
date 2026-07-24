# Lab 3 -- allocator build.
# Host C only. Strict warnings: -Werror means a warning is a build failure.

CC     ?= gcc
CFLAGS := -Wall -Wextra -Werror -std=gnu11 -g
AR     ?= ar
LIB    := libmymalloc.a

.PHONY: all clean test trace

all: $(LIB)

mymalloc.o: mymalloc.c mymalloc.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(LIB): mymalloc.o
	$(AR) rcs $@ $^

# The Part 4 trace driver. It lives with the tests because the autograder uses
# it too; TESTDIR can be overridden if you laid the lab out differently:
#   make trace TESTDIR=/path/to/lab03-allocator/tests
TESTDIR ?= ../tests

# Not made to depend on $(TESTDIR)/tracerun.c: make would then refuse with
# "no rule to make target" before the message below ever printed.
tracerun: $(LIB)
	@test -f "$(TESTDIR)/tracerun.c" || { \
	  echo "make: cannot find the trace driver at '$(TESTDIR)/tracerun.c'."; \
	  echo "You are in $(CURDIR)."; \
	  echo "The lab expects starter/, tests/ and fixtures/ to sit side by side —"; \
	  echo "copy all three, or pass TESTDIR=/path/to/tests."; \
	  exit 1; }
	$(CC) $(CFLAGS) -I. -o $@ $(TESTDIR)/tracerun.c $(LIB)

trace: tracerun

# Run the autograder against this directory.
#   make test TESTS=/path/to/lab03-allocator/tests/run.sh
TESTS ?= ../tests/run.sh

test: all
	@test -x "$(TESTS)" || { \
	  echo "make: cannot find the autograder at '$(TESTS)'."; \
	  echo "You are in $(CURDIR)."; \
	  echo "The lab expects starter/, tests/ and fixtures/ to sit side by side —"; \
	  echo "copy all three, or pass TESTS=/path/to/tests/run.sh."; \
	  exit 1; }
	$(TESTS) "$(CURDIR)"

clean:
	rm -f $(LIB) mymalloc.o tracerun
