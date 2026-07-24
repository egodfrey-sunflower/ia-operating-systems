#!/usr/bin/env bash
#
# run.sh <workdir> -- autograder for Lab 6, Parts 1-5.
#
# The lab is three command-line simulators -- iobuf, disksim, raidsim -- driven
# entirely through a text interface (argv in, stdout out).  That is deliberate:
# it lets a C submission and a Python submission be graded by the same harness.
#
# For each tool the harness looks for <tool>.c in <workdir> and, if it finds it,
# does the GRADED BUILD ITSELF -- its own gcc line, its own -Wall -Wextra
# -Werror, into its own temporary directory -- so a Makefile that quietly drops
# -Werror cannot change the grade.  If there is no <tool>.c it looks for an
# executable named <tool> in <workdir> (the Python path: a shebang script made
# executable, or a one-line wrapper).  Either way, the tool is then exercised
# only through its command line.
#
# A student Makefile, if present, is run once as a separate smoke gate: 'make'
# is the documented C workflow and has to work, but it is never the graded
# build.
#
# Every verdict is a property of the tool's OUTPUT under a fixed input, never a
# number the harness lets the tool choose for itself.  Exits 0 only if every
# case passes.  No root, no large allocations, a timeout on every case.

set -u
export LC_ALL=C

if [ $# -ne 1 ]; then
	echo "usage: run.sh <workdir>" >&2
	exit 2
fi
WORKDIR=$1
[ -d "$WORKDIR" ] || { echo "run.sh: '$WORKDIR' is not a directory" >&2; exit 2; }
WORKDIR=$(cd "$WORKDIR" && pwd -P)
LABDIR=$(cd "$(dirname "$0")/.." && pwd -P)
FIXDIR="$LABDIR/fixtures"

TMP=$(mktemp -d)
TMP=$(cd "$TMP" && pwd -P)
trap 'rm -rf "$TMP"' EXIT

TIMEOUT=30      # per case; every case here runs in well under a second

pass=0; fail=0; skip=0
declare -a RESULTS
record() { # <name> <PASS|FAIL|SKIP>
	RESULTS+=("$2|$1")
	case $2 in
		PASS) pass=$((pass+1)) ;;
		SKIP) skip=$((skip+1)) ;;
		*)    fail=$((fail+1)) ;;
	esac
}

GRADE_CFLAGS='-Wall -Wextra -Werror -std=gnu11 -g'

# ---------------------------------------------------------------------------
# build / locate each tool
# ---------------------------------------------------------------------------

echo "== building/locating tools in $WORKDIR =="

# Resolve one tool to a command.  Prints the command on stdout; returns 1 and
# prints a reason on stderr if it cannot be built or found.
resolve_tool() { # <name>
	local name=$1
	if [ -f "$WORKDIR/$name.c" ]; then
		if ! gcc $GRADE_CFLAGS -o "$TMP/$name" "$WORKDIR/$name.c" \
		     >"$TMP/$name.build" 2>&1; then
			echo "  $name: graded build FAILED (gcc $GRADE_CFLAGS $name.c):" >&2
			cat "$TMP/$name.build" >&2
			return 1
		fi
		if grep -q -F "warning:" "$TMP/$name.build"; then
			echo "  $name: build produced warnings (the spec is -Werror clean):" >&2
			grep -F "warning:" "$TMP/$name.build" >&2
			return 1
		fi
		echo "$TMP/$name"
		return 0
	fi
	if [ -x "$WORKDIR/$name" ]; then
		echo "$WORKDIR/$name"
		return 0
	fi
	echo "  $name: no $name.c to compile and no executable '$name' in $WORKDIR." >&2
	return 1
}

build_ok=1
if ! IOBUF=$(resolve_tool iobuf);   then build_ok=0; fi
if ! DISKSIM=$(resolve_tool disksim); then build_ok=0; fi
if ! RAIDSIM=$(resolve_tool raidsim); then build_ok=0; fi
if [ "$build_ok" != 1 ]; then
	echo "RESULT: build failure" >&2
	exit 1
fi
echo "tools OK"

# The student Makefile, as a separate smoke gate only (C submissions).
if [ -f "$WORKDIR/Makefile" ] && ls "$WORKDIR"/*.c >/dev/null 2>&1; then
	if ! (make -C "$WORKDIR" clean >/dev/null 2>&1 && \
	      make -C "$WORKDIR" all >"$TMP/make.log" 2>&1); then
		echo "note: 'make' did not build cleanly (this is a smoke gate, not the" >&2
		echo "graded build, but 'make' is the documented workflow):" >&2
		cat "$TMP/make.log" >&2
	fi
fi
echo

# ---------------------------------------------------------------------------
# helpers.  Each runs a tool under a timeout and captures stdout in $TMP/out.
# ---------------------------------------------------------------------------

run_capture() { # <cmd...> ; stdout -> $TMP/out, returns the tool's exit code
	timeout -k 5 "$TIMEOUT" "$@" >"$TMP/out" 2>"$TMP/err"
}

raid() { # feed a script (stdin) to raidsim, capture -> $TMP/out
	timeout -k 5 "$TIMEOUT" "$RAIDSIM" - >"$TMP/out" 2>"$TMP/err"
}

# Report a FAIL with a captured excerpt.
fail_with() { # <label> <message...>
	record "$1" FAIL
	{ echo "  [$1] $2"
	  if [ -s "$TMP/out" ]; then echo "    --- output ---"; head -20 "$TMP/out"; fi
	} >&2
}

# grep the captured output for a fixed string.
outhas() { grep -q -F "$1" "$TMP/out"; }

# ===========================================================================
# Part 1 -- the buffered device interface
# ===========================================================================

# Unbuffered is fully serial: N units at (tcpu + tdev) each.
# n=20 tcpu=3 tdev=10 -> 20*(3+10) = 260.  A mutant that overlaps the device
# with the CPU (any K>1 masquerading as unbuffered) produces less than 260.
run_capture "$IOBUF" buf unbuffered n=20 tdev=10 tcpu=3
if outhas "time=260"; then record "Part 1: unbuffered transfer is fully serial (260)" PASS
else fail_with "Part 1: unbuffered transfer is fully serial (260)" "expected time=260"; fi

# Double buffering overlaps CPU and device, so it must beat unbuffered.
u=$("$IOBUF" buf unbuffered n=40 tdev=10 tcpu=3 | sed -n 's/.*time=\([0-9]*\).*/\1/p')
d=$("$IOBUF" buf double     n=40 tdev=10 tcpu=3 | sed -n 's/.*time=\([0-9]*\).*/\1/p')
if [ -n "$u" ] && [ -n "$d" ] && [ "$d" -lt "$u" ]; then
	record "Part 1: double buffering beats unbuffered" PASS
else fail_with "Part 1: double buffering beats unbuffered" "double=$d not < unbuffered=$u"; fi

# Exact overlap floor: for a STEADY producer two buffers already keep the 10-tick
# device fed by a 3-tick producer, so double and circular hit the same completion
# time and it is a hard number, not just "less than unbuffered".  n=60 tdev=10
# tcpu=3 -> 603 for BOTH (the recurrence's fixed point; extra depth sits idle).
# This pins the recurrence directly rather than by relative triangulation, and
# also asserts the "depth buys nothing for a steady producer" property.
ed=$("$IOBUF" buf double   n=60 tdev=10 tcpu=3 | sed -n 's/.*time=\([0-9]*\).*/\1/p')
ec=$("$IOBUF" buf circular n=60 tdev=10 tcpu=3 | sed -n 's/.*time=\([0-9]*\).*/\1/p')
if [ "$ed" = 603 ] && [ "$ec" = 603 ]; then
	record "Part 1: double and circular hit the exact overlap floor for a steady producer (603)" PASS
else fail_with "Part 1: double and circular hit the exact overlap floor for a steady producer (603)" \
	"expected 603 and 603, got double=$ed circular=$ec"; fi

# The separating case: a BURSTY producer.  A deep circular buffer lets the
# producer run ahead during the cheap run so the device never idles; a double
# buffer (2 slots) cannot, and stalls.  circular < double < unbuffered, strict.
# A circular buffer secretly capped at depth 2 comes out EQUAL to double -- the
# "silently a weaker strategy" mutant this case exists to catch.
uu=$("$IOBUF" buf unbuffered n=40 tdev=10 tcpu=3 depth=8 burst=6 | sed -n 's/.*time=\([0-9]*\).*/\1/p')
dd=$("$IOBUF" buf double     n=40 tdev=10 tcpu=3 depth=8 burst=6 | sed -n 's/.*time=\([0-9]*\).*/\1/p')
cc=$("$IOBUF" buf circular   n=40 tdev=10 tcpu=3 depth=8 burst=6 | sed -n 's/.*time=\([0-9]*\).*/\1/p')
if [ -n "$cc" ] && [ -n "$dd" ] && [ -n "$uu" ] && [ "$cc" -lt "$dd" ] && [ "$dd" -lt "$uu" ]; then
	record "Part 1: circular absorbs a burst that stalls double buffering" PASS
else fail_with "Part 1: circular absorbs a burst that stalls double buffering" \
	"expected circular($cc) < double($dd) < unbuffered($uu)"; fi

# Polling busy-waits: wasted cycles scale with the device time. n=16 D=10 -> 160.
# A model that ignores the device time cannot produce this.
run_capture "$IOBUF" io polling n=16 tdev=10 hoverhead=50 setup=100
if outhas "wasted=160"; then record "Part 1: polling waste scales with device time (160)" PASS
else fail_with "Part 1: polling waste scales with device time (160)" "expected wasted=160"; fi

# Interrupt overhead is per-unit handler cost, independent of device time.
# n=16 H=50 -> 800, and it stays 800 when the device gets slower (D=10 vs D=100).
i1=$("$IOBUF" io interrupt n=16 tdev=10  hoverhead=50 | sed -n 's/.*wasted=\([0-9]*\).*/\1/p')
i2=$("$IOBUF" io interrupt n=16 tdev=100 hoverhead=50 | sed -n 's/.*wasted=\([0-9]*\).*/\1/p')
if [ "$i1" = 800 ] && [ "$i2" = 800 ]; then
	record "Part 1: interrupt cost is per-unit, not per-device-tick (800)" PASS
else fail_with "Part 1: interrupt cost is per-unit, not per-device-tick (800)" \
	"expected 800 and 800, got $i1 and $i2"; fi

# The chapter-36 crossover: polling wins for a fast device, interrupts win for a
# slow one.  D=5 (< H=50): polling cheaper.  D=100 (> H=50): interrupts cheaper.
pf=$("$IOBUF" io polling   n=16 tdev=5   hoverhead=50 | sed -n 's/.*wasted=\([0-9]*\).*/\1/p')
inf=$("$IOBUF" io interrupt n=16 tdev=5  hoverhead=50 | sed -n 's/.*wasted=\([0-9]*\).*/\1/p')
ps=$("$IOBUF" io polling   n=16 tdev=100 hoverhead=50 | sed -n 's/.*wasted=\([0-9]*\).*/\1/p')
ins=$("$IOBUF" io interrupt n=16 tdev=100 hoverhead=50 | sed -n 's/.*wasted=\([0-9]*\).*/\1/p')
if [ -n "$pf" ] && [ "$pf" -lt "$inf" ] && [ "$ps" -gt "$ins" ]; then
	record "Part 1: polling wins fast, interrupts win slow (the crossover)" PASS
else fail_with "Part 1: polling wins fast, interrupts win slow (the crossover)" \
	"fast: polling=$pf interrupt=$inf (want <); slow: polling=$ps interrupt=$ins (want >)"; fi

# DMA programs the transfer once and takes a single completion interrupt for the
# WHOLE burst, so its wasted work is S+H, independent of both n and the device
# time.  n=16 S=100 H=50 -> 150, and it STAYS 150 at n=64 -- a per-unit DMA cost
# (garbage like n*S, or S+n*H) balloons with n and is caught by the second point.
# This pins the chapter-36 lesson that DMA pays its overhead once, not per unit.
dm1=$("$IOBUF" io dma n=16 tdev=100 hoverhead=50 setup=100 | sed -n 's/.*wasted=\([0-9]*\).*/\1/p')
dm2=$("$IOBUF" io dma n=64 tdev=100 hoverhead=50 setup=100 | sed -n 's/.*wasted=\([0-9]*\).*/\1/p')
if [ "$dm1" = 150 ] && [ "$dm2" = 150 ]; then
	record "Part 1: DMA overhead is paid once per burst, not per unit (150)" PASS
else fail_with "Part 1: DMA overhead is paid once per burst, not per unit (150)" \
	"expected 150 and 150, got $dm1 and $dm2"; fi

# ===========================================================================
# Part 2 -- the disk cost model
# ===========================================================================
# Oracle: default geometry C=500 spt=100 seek=1 rot=1 xfer=1, start cyl 0.
# The stream (250, 3070, 30) hand-computes to:
#   lba 250  -> cyl 2  sec 50: seek 2,  after seek t=2,  rot=(50-2)=48, done=51
#   lba 3070 -> cyl 30 sec 70: seek 28, t=79, head_sec=79, rot=(70-79+100)%100=91, t=170, done=171
#   lba 30   -> cyl 0  sec 30: seek 30, t=201, head_sec=1, rot=(30-1)=29, t=230, done=231
# A model that ignores rotation (rot=0) yields done=3,31,61 -- caught here.
printf '250 1\n3070 1\n30 1\n' > "$TMP/model.stream"
run_capture "$DISKSIM" run fifo "$TMP/model.stream"
if outhas "seek=2 rot=48 xfer=1 done=51" && \
   outhas "seek=28 rot=91 xfer=1 done=171" && \
   outhas "seek=30 rot=29 xfer=1 done=231"; then
	record "Part 2: seek+rotation+transfer match the analytic oracle" PASS
else fail_with "Part 2: seek+rotation+transfer match the analytic oracle" \
	"per-request breakdown does not match the hand-computed values"; fi

# The transfer term must scale with the request size, not be a fixed +1.  lba 250
# reaches its sector at t=50 as above; a 5-sector transfer then costs 5, not 1,
# so done=55.  A model that hardcodes xfer=1 (ignoring the count) still says 51.
printf '250 5\n' > "$TMP/xfer.stream"
run_capture "$DISKSIM" run fifo "$TMP/xfer.stream"
if outhas "seek=2 rot=48 xfer=5 done=55"; then
	record "Part 2: the transfer term scales with the sector count" PASS
else fail_with "Part 2: the transfer term scales with the sector count" \
	"a 5-sector transfer at lba 250 must give xfer=5 done=55"; fi

# Rotation must actually contribute: the same request served when the head is at
# a different rotational position has a different latency.  Serving lba 50 first
# (cyl 0 sec 50) gives rot=50; serving it after lba 5 (transfer advances time)
# gives a different rot.  A rot=0 model makes both zero.
run_capture "$DISKSIM" run fifo "$TMP/model.stream"
r1=$(sed -n 's/.*lba=250 .*rot=\([0-9]*\).*/\1/p' "$TMP/out")
if [ -n "$r1" ] && [ "$r1" -gt 0 ]; then
	record "Part 2: rotational latency is nonzero when it should be" PASS
else fail_with "Part 2: rotational latency is nonzero when it should be" "rot for lba 250 was '$r1'"; fi

# ===========================================================================
# Part 3 -- scheduling policies
# ===========================================================================
# The separation fixture: head starts at cylinder 50, requests at cylinders
# {10,90,20,80,55,45} (lba = cyl*100).  Hand-computed seek totals (cylinders):
#   FIFO 285, SSTF 130, SCAN 120, C-SCAN 155  -- all four DIFFERENT -- and the
# service orders are all distinct too.  These numbers are the evidence that the
# four policies are four policies and not one under four names.
SEP="$FIXDIR/disk-sep.stream"

check_order() { # <label> <policy> <expected order_cyl>
	run_capture "$DISKSIM" run "$2" "$SEP" start=50
	if grep -q -F "order_cyl=$3 " "$TMP/out"; then record "$1" PASS
	else fail_with "$1" "expected order_cyl=$3"; fi
}
check_order "Part 3: FIFO serves in arrival order"        fifo  10,90,20,80,55,45
check_order "Part 3: SSTF serves nearest-first"           sstf  45,55,80,90,20,10
check_order "Part 3: SCAN sweeps up then reverses"        scan  55,80,90,45,20,10
check_order "Part 3: C-SCAN sweeps up then wraps to low"  cscan 55,80,90,10,20,45

# The policy-separation check proper: the four seek totals must all differ.  A
# policy that silently behaves like FIFO collides with 285 and is caught here.
declare -A seekt
for p in fifo sstf scan cscan; do
	run_capture "$DISKSIM" run "$p" "$SEP" start=50
	seekt[$p]=$(sed -n 's/.*seek_total=\([0-9]*\).*/\1/p' "$TMP/out")
done
if [ "${seekt[fifo]}" = 285 ] && [ "${seekt[sstf]}" = 130 ] && \
   [ "${seekt[scan]}" = 120 ] && [ "${seekt[cscan]}" = 155 ] && \
   [ "${seekt[fifo]}" != "${seekt[sstf]}" ] && \
   [ "${seekt[sstf]}" != "${seekt[scan]}" ] && \
   [ "${seekt[scan]}" != "${seekt[cscan]}" ] && \
   [ "${seekt[fifo]}" != "${seekt[cscan]}" ]; then
	record "Part 3: the four policies produce four different seek totals" PASS
else fail_with "Part 3: the four policies produce four different seek totals" \
	"fifo=${seekt[fifo]} sstf=${seekt[sstf]} scan=${seekt[scan]} cscan=${seekt[cscan]} (want 285/130/120/155, all distinct)"; fi

# Starvation: a cluster near the start plus one lone request far away.  Under
# SSTF the outlier is served LAST (it waits for the whole cluster); under SCAN
# the sweep reaches it in one pass.  We compare the outlier's service position.
# The outlier is lba 49000 (cyl 490); the head starts at cyl 15.
STARVE="$FIXDIR/disk-starve.stream"
run_capture "$DISKSIM" run sstf "$STARVE" start=15
sstf_idx=$(sed -n 's/.*idx=\([0-9]*\) lba=49000 .*/\1/p' "$TMP/out")
run_capture "$DISKSIM" run scan "$STARVE" start=15
scan_idx=$(sed -n 's/.*idx=\([0-9]*\) lba=49000 .*/\1/p' "$TMP/out")
if [ -n "$sstf_idx" ] && [ -n "$scan_idx" ] && [ "$scan_idx" -lt "$sstf_idx" ]; then
	record "Part 3: SCAN serves the starved request far earlier than SSTF" PASS
else fail_with "Part 3: SCAN serves the starved request far earlier than SSTF" \
	"outlier served at SCAN idx=$scan_idx, SSTF idx=$sstf_idx (want SCAN earlier)"; fi

# ===========================================================================
# Part 4 -- the RAID engine
# ===========================================================================

# Capacity per level: RAID0 keeps all N disks, RAID1 keeps one, RAID4/5 keep
# N-1.  With rows=4 and 4 disks: 0->16, 1->4, 4->12, 5->12.
cap_case() { # <label> <level> <expected blocks>
	printf "init %s 4 1 4 16\ncapacity\n" "$2" | raid
	if grep -q -F "capacity blocks=$3 " "$TMP/out"; then record "$1" PASS
	else fail_with "$1" "expected capacity blocks=$3"; fi
}
cap_case "Part 4: RAID0 capacity is all N disks"     0 16
cap_case "Part 4: RAID1 capacity is one disk"        1 4
cap_case "Part 4: RAID4 capacity is N-1 disks"       4 12
cap_case "Part 4: RAID5 capacity is N-1 disks"       5 12

# RAID5 rotated parity: the parity disk must depend on the stripe number.
# stripe 0 -> parity disk 3, stripe 1 -> parity disk 2; and lba 5 (stripe 1,
# third data column) lands on disk 3 because it skips the parity disk.  A RAID5
# that pins parity to one disk (i.e. behaves like RAID4) fails the second line.
printf 'init 5 4 1 4 16\nplace 0\nplace 3\nplace 5\n' | raid
if grep -q -F "place lba=0 data_disk=0 row=0 stripe=0 parity_disk=3" "$TMP/out" && \
   grep -q -F "place lba=3 data_disk=0 row=1 stripe=1 parity_disk=2" "$TMP/out" && \
   grep -q -F "place lba=5 data_disk=3 row=1 stripe=1 parity_disk=2" "$TMP/out"; then
	record "Part 4: RAID5 parity rotates with the stripe number" PASS
else fail_with "Part 4: RAID5 parity rotates with the stripe number" "placement wrong"; fi

# Parity is the XOR of the stripe's data blocks.  Write 0xAA,0xCC,0x0F into the
# three data disks of stripe 0; the parity block on disk 3 must read 0x69
# (0xAA^0xCC^0x0F).  A wrong parity computation produces a different byte.
printf 'init 5 4 1 4 16\nwrite 0 170\nwrite 1 204\nwrite 2 15\nreadraw 3 0\n' | raid
if grep -q -F "readraw disk=3 row=0 bytes=6969696969696969" "$TMP/out"; then
	record "Part 4: RAID5 parity block is the XOR of the stripe" PASS
else fail_with "Part 4: RAID5 parity block is the XOR of the stripe" "parity byte not 0x69"; fi

# Chunk versus block: with chunk=2, logical blocks 0 and 1 sit on the SAME disk
# in consecutive rows before striping moves on; block 2 starts the next column.
# Conflating chunk and block size breaks this.
printf 'init 5 4 2 4 16\nplace 0\nplace 1\nplace 2\n' | raid
if grep -q -F "place lba=0 data_disk=0 row=0" "$TMP/out" && \
   grep -q -F "place lba=1 data_disk=0 row=1" "$TMP/out" && \
   grep -q -F "place lba=2 data_disk=1 row=0" "$TMP/out"; then
	record "Part 4: the stripe unit (chunk) is not the block" PASS
else fail_with "Part 4: the stripe unit (chunk) is not the block" "chunk mapping wrong"; fi

# Subtractive update reads the OLD data and OLD parity: exactly 2 reads + 2
# writes, whatever the array width.  Additive reads every OTHER data block.  On
# a 6-disk RAID5 (5 data disks) they differ: subtractive 2R, additive 4R.  A
# write labelled subtractive that actually reads 4 blocks is caught here.
printf 'init 5 6 1 4 16\nfill 1\nparity subtractive\nreset\nwrite 0 99\niostat\n' | raid
if grep -q -F "iostat reads=2 writes=2" "$TMP/out"; then
	record "Part 4: subtractive parity update is 2 reads + 2 writes" PASS
else fail_with "Part 4: subtractive parity update is 2 reads + 2 writes" "expected reads=2 writes=2"; fi

printf 'init 5 6 1 4 16\nfill 1\nparity additive\nreset\nwrite 0 99\niostat\n' | raid
if grep -q -F "iostat reads=4 writes=2" "$TMP/out"; then
	record "Part 4: additive parity update reads every other data block" PASS
else fail_with "Part 4: additive parity update reads every other data block" "expected reads=4 writes=2"; fi

# auto must pick the cheaper of the two for a wide array: subtractive (2 reads).
printf 'init 5 6 1 4 16\nfill 1\nparity auto\nreset\nwrite 0 99\niostat\n' | raid
if grep -q -F "iostat reads=2 writes=2" "$TMP/out"; then
	record "Part 4: auto picks the cheaper parity update on a wide array" PASS
else fail_with "Part 4: auto picks the cheaper parity update on a wide array" "expected reads=2 writes=2 (subtractive)"; fi

# ===========================================================================
# Part 5 -- degraded mode and rebuild
# ===========================================================================

# The strongest check in the lab, once per redundant level: fill with a known
# pattern, checksum it, fail EACH disk in turn, verify the reads still return the
# pattern (checksum unchanged while degraded), rebuild, and verify byte-identity
# (checksum unchanged after rebuild).  A rebuild from the wrong parity, or a
# degraded read that returns the zeroed block, changes the checksum.
survive_each() { # <label> <level> <ndisks>
	local label=$1 level=$2 nd=$3
	local base d ok=1
	base=$(printf "init %s %s 1 4 16\nfill 4\nchecksum\n" "$level" "$nd" | \
	       { timeout -k 5 "$TIMEOUT" "$RAIDSIM" -; } | sed -n 's/checksum \(.*\)/\1/p')
	for (( d=0; d<nd; d++ )); do
		# degraded checksum (after fail, before rebuild) and rebuilt checksum
		printf "init %s %s 1 4 16\nfill 4\nfail %d\nchecksum\nrebuild %d\nchecksum\n" \
		       "$level" "$nd" "$d" "$d" > "$TMP/rs"
		mapfile -t sums < <(timeout -k 5 "$TIMEOUT" "$RAIDSIM" - < "$TMP/rs" | sed -n 's/checksum \(.*\)/\1/p')
		if [ "${sums[0]:-x}" != "$base" ] || [ "${sums[1]:-x}" != "$base" ]; then
			ok=0
			echo "    disk $d: degraded=${sums[0]:-none} rebuilt=${sums[1]:-none} original=$base" >&2
		fi
	done
	if [ "$ok" = 1 ] && [ -n "$base" ]; then record "$label" PASS
	else record "$label" FAIL; echo "  [$label] a failure was not survived byte-identically" >&2; fi
}
survive_each "Part 5: RAID5 survives each disk, rebuilds byte-identical" 5 4
survive_each "Part 5: RAID4 survives each disk, rebuilds byte-identical" 4 4
survive_each "Part 5: RAID1 survives each disk, rebuilds byte-identical" 1 3

# Degraded reads must RECONSTRUCT from the surviving DATA columns and parity, not
# return zeros/stale data and not lean on a stripe that is mostly zero.  Write
# distinct nonzero bytes into the whole stripe first: lba 3,4,5 are the three data
# blocks of stripe 1 (disks 0,1,3), so parity on disk 2 is 0x11^0x22^0xDB=0xE8.
# Fail the disk holding lba 5 and read it back: a correct reconstruction XORs the
# two surviving data blocks with parity to recover 0xDB.  A reconstruction that
# omits the data columns (XORs only parity) would return 0xE8 and is caught here,
# not just downstream by survive_each.
printf 'init 5 4 1 4 16\nwrite 3 17\nwrite 4 34\nwrite 5 219\nread 5\nfail 3\nread 5\n' | raid
# lba 5 is on disk 3 (see placement above); 0xDB = 219.
n=$(grep -c -F "read lba=5 bytes=dbdbdbdbdbdbdbdb" "$TMP/out")
if [ "$n" = 2 ]; then
	record "Part 5: a degraded read reconstructs the real data" PASS
else fail_with "Part 5: a degraded read reconstructs the real data" \
	"the block did not read back identically before and after the failure"; fi

# The negative case: RAID0 has no redundancy, so a failure loses data.  The read
# must report LOST and the checksum must change.  A RAID0 that claims to survive
# a failure (e.g. by reconstructing from nothing) is caught here.
before=$(printf 'init 0 4 1 4 16\nfill 3\nchecksum\n' | { timeout -k 5 "$TIMEOUT" "$RAIDSIM" -; } | sed -n 's/checksum \(.*\)/\1/p')
printf 'init 0 4 1 4 16\nfill 3\nfail 1\nread 1\nchecksum\n' | raid
after=$(sed -n 's/checksum \(.*\)/\1/p' "$TMP/out")
if outhas "read lba=1 LOST" && [ -n "$before" ] && [ "$after" != "$before" ]; then
	record "Part 5: RAID0 loses data on a disk failure (negative case)" PASS
else fail_with "Part 5: RAID0 loses data on a disk failure (negative case)" \
	"expected a LOST read and a changed checksum (before=$before after=$after)"; fi

# Rebuild cost: reconstructing one disk of a 4-disk RAID5 with 4 rows reads the
# 3 surviving blocks per row = 12 blocks.  A rebuild that reads the wrong number
# is doing something other than one-stripe-at-a-time reconstruction.
printf 'init 5 4 1 4 16\nfill 1\nfail 2\nrebuild 2\n' | raid
if outhas "rebuild disk=2 blocks_read=12"; then
	record "Part 5: rebuild reads exactly the surviving blocks (12)" PASS
else fail_with "Part 5: rebuild reads exactly the surviving blocks (12)" "expected blocks_read=12"; fi

# ---------------------------------------------------------------------------
# report
# ---------------------------------------------------------------------------
echo
echo "== results =="
for r in "${RESULTS[@]}"; do
	printf '  %-6s %s\n' "${r%%|*}" "${r#*|}"
done
echo
if [ "$skip" -gt 0 ]; then
	echo "$pass passed, $fail failed, $skip skipped"
else
	echo "$pass passed, $fail failed"
fi
echo "(RESULTS.md -- the buffering, scheduling and RAID tables and their"
echo " interpretation -- is marked against the rubric in solutions/README.md,"
echo " not by this script.)"
[ "$fail" -eq 0 ]
