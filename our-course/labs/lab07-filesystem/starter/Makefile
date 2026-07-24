# Lab 7 Part 1 -- the ch. 39 file-interface tools.
# Host C only. Strict warnings: -Werror means a warning is a build failure.

CC     ?= gcc
CFLAGS := -Wall -Wextra -Werror -std=gnu11 -g
PROGS  := mystat myls mytail myfind

.PHONY: all clean test

all: $(PROGS)

# Each tool is a single translation unit.
%: %.c
	$(CC) $(CFLAGS) -o $@ $<

# Run the autograder against this directory.
#   make test TESTS=/path/to/lab07-filesystem/tests/run.sh   (if laid out apart)
TESTS ?= ../tests/run.sh

test: all
	@test -x "$(TESTS)" || { \
	  echo "make: cannot find the autograder at '$(TESTS)'."; \
	  echo "You are in $(CURDIR)."; \
	  echo "The lab expects starter/, tests/ and fixtures/ to sit side by side --"; \
	  echo "copy all three, or pass TESTS=/path/to/tests/run.sh."; \
	  exit 1; }
	$(TESTS) "$(CURDIR)"

clean:
	rm -f $(PROGS)
