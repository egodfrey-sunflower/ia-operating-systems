#!/usr/bin/env bash
#
# genfixtures.sh [dir] -- build the fixture tree Lab 1's tests run against.
#
# Deterministic: the same bytes on every machine, which is what makes a
# transcript diff against bash meaningful. tests/run.sh regenerates this into a
# temporary directory on every run; you never have to call it by hand, but it
# is worth doing once so you can see what you are being tested against.

set -eu
export LC_ALL=C

DIR=${1:-./tree}
mkdir -p "$DIR"
DIR=$(cd "$DIR" && pwd -P)

# The three subdirectories are rebuilt from scratch; the script's own top-level
# files are overwritten. Anything ELSE already in $DIR is left alone -- this
# will not delete files it did not create. If you point it at a directory you
# have used before, expect to see leftovers from last time. tests/run.sh always
# generates into a fresh mktemp -d, so the graded run is never affected.
rm -rf "$DIR/sub" "$DIR/home" "$DIR/out"
mkdir -p "$DIR/sub" "$DIR/home" "$DIR/out"

# words.txt -- unsorted, with duplicates, so `sort` and `uniq -c` both have
# something to do and the output of a 4-stage pipeline is not the input.
cat > "$DIR/words.txt" <<'EOF'
pear
apple
fig
apple
cherry
fig
apple
damson
cherry
elderberry
fig
banana
EOF

# lines.txt -- 200 numbered lines. Big enough that `wc -l` is a real answer and
# small enough to stay in a pipe buffer, so a pipeline that hangs is hanging on
# a missing EOF and not on a full pipe.
: > "$DIR/lines.txt"
i=1
while [ "$i" -le 200 ]; do
	printf 'line %03d\n' "$i" >> "$DIR/lines.txt"
	i=$((i + 1))
done

# Markers for the `cd` cases: cd must change the *shell's own* directory, which
# you can only see by then naming a file relatively.
printf 'marker in the fixture root\n' > "$DIR/marker.txt"
printf 'marker in sub\n'              > "$DIR/sub/marker.txt"
printf 'marker in home\n'             > "$DIR/home/marker.txt"

# slowprint.sh -- sleeps, then prints. A shell that forks without waiting
# prints the *next* command's output first, which is how the wait test sees the
# missing wait() without needing a stopwatch.
cat > "$DIR/slowprint.sh" <<'EOF'
#!/bin/sh
sleep 0.4
echo A
EOF
chmod +x "$DIR/slowprint.sh"

# latewrite.sh -- sleeps, then writes a marker FILE (not stdout). Used as the
# FIRST stage of a pipeline whose last stage exits at once. A shell that waits
# only for the last stage returns to the prompt before the marker exists, so
# the next command cannot read it; a shell that waits for every stage can.
# Writing to a file rather than stdout keeps SIGPIPE out of the experiment.
cat > "$DIR/latewrite.sh" <<'EOF'
#!/bin/sh
sleep 0.6
echo LATE > out/late.txt
EOF
chmod +x "$DIR/latewrite.sh"

# bgjob.sh -- sleeps, then leaves a marker behind. Backgrounded by the Part 5
# zombie test, which needs to know the job really ran (and then really died)
# and not merely that the shell has no children.
cat > "$DIR/bgjob.sh" <<'EOF'
#!/bin/sh
sleep 0.2
: > out/bg.done
EOF
chmod +x "$DIR/bgjob.sh"

# noexec.txt -- exists, is not executable. Exec failure must be reported by the
# *child*, with the shell surviving.
printf 'not a program\n' > "$DIR/noexec.txt"
chmod 644 "$DIR/noexec.txt"

echo "fixtures in $DIR"
