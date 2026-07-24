# Lab 6 -- three simulators. Host C, strict warnings.
CC     ?= gcc
CFLAGS := -Wall -Wextra -Werror -std=gnu11 -g
TOOLS  := iobuf disksim raidsim

.PHONY: all clean test
all: $(TOOLS)

iobuf: iobuf.c
	$(CC) $(CFLAGS) -o $@ $<
disksim: disksim.c
	$(CC) $(CFLAGS) -o $@ $<
raidsim: raidsim.c
	$(CC) $(CFLAGS) -o $@ $<

# Run the autograder against this directory.
#   make test TESTS=/path/to/lab06-io-storage/tests/run.sh
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
	rm -f $(TOOLS)
