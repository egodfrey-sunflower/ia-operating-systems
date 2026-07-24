# Lab 0 -- Unix warm-up build.
# Host C only. Strict warnings: -Werror means a warning is a build failure.

CC     ?= gcc
CFLAGS := -Wall -Wextra -Werror -std=gnu11 -g
PROGS  := mycat myls mygrep

.PHONY: all clean test

all: $(PROGS)

# Each tool is a single translation unit.
%: %.c
	$(CC) $(CFLAGS) -o $@ $<

# Run the autograder against this directory.
# TESTS can be overridden if you laid the lab out differently:
#   make test TESTS=/path/to/lab00-toolchain/tests/run.sh
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
	rm -f $(PROGS)
