# Lab 7 - mycontainer (starter)
#
# gnu11, not c11: clone(2), pivot_root, CLONE_* and the syscall() wrapper all
# depend on _GNU_SOURCE / GNU extensions.  -Werror keeps us honest.

CC      := gcc
CFLAGS  := -Wall -Wextra -Werror -std=gnu11 -g

# --- Alpine minirootfs (pinned) -------------------------------------------
ALPINE_VER    := 3.21.3
ALPINE_MAJOR  := 3.21
ALPINE_FILE   := alpine-minirootfs-$(ALPINE_VER)-x86_64.tar.gz
ALPINE_URL    := https://dl-cdn.alpinelinux.org/alpine/v$(ALPINE_MAJOR)/releases/x86_64/$(ALPINE_FILE)
ALPINE_SHA256 := 1a694899e406ce55d32334c47ac0b2efb6c06d7e878102d1840892ad44cd5239

.PHONY: all clean clean-rootfs rootfs rootfs-busybox

all: mycontainer

mycontainer: mycontainer.c
	$(CC) $(CFLAGS) -o $@ $<

# Download + verify + extract an Alpine rootfs into ./rootfs/
# One shell block so the "already populated" guard actually short-circuits.
rootfs:
	@set -e; \
	if [ -f rootfs/bin/busybox ] || [ -f rootfs/bin/sh ]; then \
		echo "rootfs/ already populated (make clean-rootfs to rebuild)"; \
		exit 0; \
	fi; \
	echo "Downloading $(ALPINE_FILE) ..."; \
	curl -fsSL -o $(ALPINE_FILE) "$(ALPINE_URL)"; \
	echo "$(ALPINE_SHA256)  $(ALPINE_FILE)" | sha256sum -c -; \
	mkdir -p rootfs; \
	tar -xzf $(ALPINE_FILE) -C rootfs; \
	echo "nameserver 1.1.1.1" > rootfs/etc/resolv.conf; \
	echo "rootfs/ ready (Alpine $(ALPINE_VER))"

# Offline fallback: build a minimal tree around the busybox-static binary.
# Requires the busybox-static package (/bin/busybox or /usr/bin/busybox).
rootfs-busybox:
	@BB=; for p in /bin/busybox /usr/bin/busybox; do \
		[ -x $$p ] && BB=$$p && break; done; \
	if [ -z "$$BB" ]; then \
		echo "busybox not found; install the 'busybox-static' package"; \
		exit 1; fi; \
	echo "Using $$BB"; \
	mkdir -p rootfs/bin rootfs/proc rootfs/dev rootfs/etc rootfs/tmp; \
	cp $$BB rootfs/bin/busybox; \
	for applet in sh ls cat ps mount echo sleep hostname ip ping cp mkdir; do \
		ln -sf busybox rootfs/bin/$$applet; done; \
	echo "nameserver 1.1.1.1" > rootfs/etc/resolv.conf; \
	echo "rootfs/ ready (busybox-static)"

clean:
	rm -f mycontainer $(ALPINE_FILE)

# rootfs is not removed by 'clean' (it is expensive to rebuild); this is opt-in.
clean-rootfs:
	rm -rf rootfs $(ALPINE_FILE)
