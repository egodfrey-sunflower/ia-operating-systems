# Lab 0 -- Unix warm-up build.
# Host C only: strict warnings, C11, debug info.

CC      ?= gcc
CFLAGS  := -Wall -Wextra -Werror -std=c11 -g
PROGS   := mycat myls mygrep

.PHONY: all clean test

all: $(PROGS)

# Each tool is a single translation unit.
%: %.c
	$(CC) $(CFLAGS) -o $@ $<

# Run the shared autograder against this directory.
test: all
	../tests/run.sh "$(CURDIR)"

clean:
	rm -f $(PROGS)
