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
test: all
	../tests/run.sh "$(CURDIR)"

clean:
	rm -f $(PROGS)
