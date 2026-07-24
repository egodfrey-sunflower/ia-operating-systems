#!/usr/bin/env bash
#
# genfixtures.sh [DEST] -- build the Lab 7 Part 1 fixture tree.
#
# The tree is *generated*, not committed: it contains symlinks (one of them
# dangling), a hard-linked pair, files with set-user-ID / set-group-ID / sticky
# bits, an empty file, an empty directory and a multi-megabyte file, none of
# which survive a git round-trip faithfully. Regenerating is deterministic --
# same bytes and same modes every time -- which is what lets tests/run.sh diff
# against the system tools (stat, tail, find).
#
# DEST defaults to ./tree next to this script. It is removed and rebuilt.
#
# See README.md in this directory for what each fixture is for.

set -eu
export LC_ALL=C
umask 022

DEST=${1:-"$(cd "$(dirname "$0")" && pwd)/tree"}

rm -rf "$DEST"
mkdir -p "$DEST"

# ===========================================================================
# meta/ -- for mystat and myls: a spread of types, link counts and modes.
# ===========================================================================

M="$DEST/meta"
mkdir -p "$M"

printf 'hello\n'                 > "$M/reg644";  chmod 0644 "$M/reg644"
printf '#!/bin/sh\necho hi\n'    > "$M/exec755";  chmod 0755 "$M/exec755"
printf 'secret\n'                > "$M/priv600";  chmod 0600 "$M/priv600"
printf 'set-user-id\n'           > "$M/setuid";   chmod 4755 "$M/setuid"
printf 'set-group-id\n'          > "$M/setgid";   chmod 2755 "$M/setgid"
printf 'sticky\n'                > "$M/sticky";   chmod 1755 "$M/sticky"

# Zero bytes: stat calls this a "regular empty file", not a "regular file".
: > "$M/empty"; chmod 0644 "$M/empty"

# A fixed-size ordinary file (exactly 1234 bytes).
yes 'padding padding padding' | head -c 1234 > "$M/big"; chmod 0644 "$M/big"

# A subdirectory: type "directory", link count 2 (itself + its own ".").
mkdir -p "$M/subdir"

# A hard-linked pair: both names share one inode, link count 2.
printf 'shared inode\n' > "$M/hard_a"; chmod 0644 "$M/hard_a"
ln "$M/hard_a" "$M/hard_b"

# A symlink to a real file, and a dangling symlink to a name that is not there.
# lstat succeeds on both; the dangling one still reports as a symbolic link.
ln -s reg644           "$M/link_to_reg"
ln -s no_such_target   "$M/dangling"

# A dotfile (myls must hide it) and an uppercase name (sorts before lowercase
# in byte order, which readdir will not do for you).
printf 'hidden\n' > "$M/.hidden"; chmod 0644 "$M/.hidden"
printf 'U\n'      > "$M/Upper";   chmod 0644 "$M/Upper"

# ===========================================================================
# tail/ -- for mytail.
# ===========================================================================

T="$DEST/tail"
mkdir -p "$T"

# 20 numbered lines, trailing newline.
i=1
: > "$T/lines20"
while [ "$i" -le 20 ]; do
	printf 'line %02d\n' "$i" >> "$T/lines20"
	i=$((i + 1))
done

# Three lines, the last with no trailing newline.
printf 'alpha\nbeta\ngamma' > "$T/nonl"

# A single line, no trailing newline.
printf 'just one line, no newline' > "$T/oneline_nonl"

# Zero bytes.
: > "$T/empty"

# ~4 MiB of numbered lines, trimmed mid-line to exactly 4194304 bytes (so it
# does NOT end in a newline). Large enough that reading it whole to find the
# last few lines is visibly different from seeking to the end.
awk 'BEGIN{for(i=0;i<80000;i++) printf "line %06d the quick brown fox jumps over the lazy dog\n", i}' \
	| head -c 4194304 > "$T/big"

# ===========================================================================
# find/ -- for myfind.
# ===========================================================================

F="$DEST/find"
mkdir -p "$F/a/b/c" "$F/emptysub"

# "foo" appears at three depths. "foobar" and "barfoo" contain "foo" but are
# not equal to it -- a whole-name match reports only the three foo's.
printf 'x\n' > "$F/foo"
printf 'x\n' > "$F/a/foo"
printf 'x\n' > "$F/a/b/foo"
printf 'x\n' > "$F/foobar"
printf 'x\n' > "$F/barfoo"

# The name "foo" spelled in capitals -- a different name, byte for byte.
# `find -name foo` does not report it.
printf 'x\n' > "$F/FOO"

# A deep match, four levels down.
printf 'x\n' > "$F/a/b/c/deep_target"

# A directory whose name is the search target: a match is not only files.
mkdir -p "$F/a/needle_dir"
printf 'x\n' > "$F/a/needle_dir/inside"

# A symlink whose target is a file (matched by its own name, "linkfoo", which
# is not "foo"), and a symlink that points back at its own directory. Neither
# is followed: the loop link would send a following walk round forever.
ln -s foo "$F/linkfoo"
ln -s .   "$F/loop"

echo "fixtures built in $DEST"
