# Lab 5 Part C — classic synchronization problems in POSIX threads.
#
# Warnings are errors: your code must be clean under -Wall -Wextra.
CC      = cc
CFLAGS  = -Wall -Wextra -Werror -std=c11 -pthread -g
LDFLAGS = -pthread

PROGS = bbuffer barrier rwlock

.PHONY: all clean
all: $(PROGS)

bbuffer: bbuffer.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

barrier: barrier.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

rwlock: rwlock.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(PROGS)
