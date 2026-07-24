# Build the consistency oracle: Lab 7's reference xfsck, over the same
# image-reader library.  `make` here produces ./xfsck; point sweep at it:
#
#   make -C ../tests/oracle
#   ./sweep data --xfsck ../tests/oracle/xfsck
#
# (The autograder compiles its own copy with the same flags; this Makefile is
# for your convenience.)

CFLAGS = -Wall -Wextra -Werror -std=gnu11 -O2

xfsck: xfsck.c xv6img.c xv6fs.h
	gcc $(CFLAGS) -o $@ xfsck.c xv6img.c

clean:
	rm -f xfsck

.PHONY: clean
