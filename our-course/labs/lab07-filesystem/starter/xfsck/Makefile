# Lab 7 Parts 4-5: the userspace checker and its test tools.
#
# You write xfsck.c. mkimg, corrupt and xv6img.c are given -- do not change
# them. The autograder compiles xfsck.c itself with these same flags, so
# weakening them here does not weaken the grade.

CFLAGS = -Wall -Wextra -Werror -std=gnu11 -g
CC ?= cc

all: xfsck mkimg corrupt

# The checker (your work) plus the given image-reader library.
xfsck: xfsck.c xv6img.c xv6fs.h
	$(CC) $(CFLAGS) -o $@ xfsck.c xv6img.c

# Builds a clean test image (given).
mkimg: mkimg.c xv6fs.h
	$(CC) $(CFLAGS) -o $@ mkimg.c

# Damages a copy of an image in named ways (given).
corrupt: corrupt.c xv6fs.h
	$(CC) $(CFLAGS) -o $@ corrupt.c

clean:
	rm -f xfsck mkimg corrupt *.img *.o

.PHONY: all clean
