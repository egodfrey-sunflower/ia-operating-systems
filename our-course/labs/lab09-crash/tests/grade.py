#!/usr/bin/env python3
# grade.py <workdir> -- autograder for Lab 9, Parts 1-5.
#
# The lab is driven entirely through its tools' command lines (blockdev.py,
# crashfs, journal), so the harness never imports the submission.  Its oracles
# are its OWN, not the submission's:
#
#   * structure  -- Lab 7's REFERENCE xfsck, compiled HERE from tests/oracle/
#                   with -Wall -Wextra -Werror (never the student's checker);
#   * atomicity  -- an independent image reader in this file: the root must
#                   hold exactly f1..fm for some m, each byte-exact;
#   * durability -- m must cover (a) the pending committed transaction the
#                   crashed image's own log header shows, and (b) every op
#                   whose `committed` line the no-crash run printed at or
#                   below the crash's install count.
#
# The three are deliberately overlapping and each is load-bearing: a recovery
# that rolls back instead of replaying leaves a structurally clean image at
# every crash point (only durability fires); a leak in the bitmap loses no
# data (only xfsck fires); a half-replayed transaction can pass neither.
# solutions/README.md carries the mutation table proving each case fires.
#
# Every asserted number is derivable from the handout: the Part 2 table from
# its five stated writes and xfsck's invariants; the install counts from the
# stated protocol (2n+2 per data-mode transaction, 8+d per ordered create);
# the commit points from summing those; the broken-mode corrupting point from
# what recovery does with a durable commit record and no durable log.
#
# CRASH-TABLE.md / CAMPAIGN.md / JOURNAL-COST.md prose is rubric-marked
# (solutions/README.md), not checked here.  Exits 0 only if every case passes.

import os
import shutil
import struct
import subprocess
import sys
import tempfile

TIMEOUT = 30

# the fixed geometry and workload (must match the handout; independent of the
# submission's xv6fs.py)
BSIZE, FSSIZE = 1024, 64
LOGSTART, INODESTART, BMAPSTART, FIRSTDATA = 2, 10, 12, 13
LOGMAGIC, LOGCAP = 0x6A726E6C, 7
T_FILE = 2
WORKLOAD = [(1, 1), (2, 2), (3, 1), (4, 3)]

results = []
counts = {"PASS": 0, "FAIL": 0}


def record(name, verdict):
    results.append((verdict, name))
    counts[verdict] = counts.get(verdict, 0) + 1


def fail(name, msg, out=None):
    record(name, "FAIL")
    sys.stderr.write("  [%s] %s\n" % (name, msg))
    if out:
        sys.stderr.write("    --- output ---\n")
        for line in out.splitlines()[:15]:
            sys.stderr.write("    " + line + "\n")


def run(cmd, stdin_data=None):
    try:
        p = subprocess.run(cmd, input=stdin_data, capture_output=True,
                           text=True, timeout=TIMEOUT)
        return p.returncode, p.stdout, p.stderr
    except subprocess.TimeoutExpired:
        return 124, "", "timeout"


def resolve_tool(workdir, name):
    direct = os.path.join(workdir, name)
    if os.path.isfile(direct) and os.access(direct, os.X_OK):
        return [direct]
    if os.path.isfile(direct):
        return [sys.executable, direct]
    return None


# ---------------------------------------------------------------------------
# the harness's own image reader (independent of the submission's xv6fs.py)
# ---------------------------------------------------------------------------

def fill_pattern(t, j):
    return bytes([(64 + 8 * t + j) & 0xFF]) * BSIZE


class Img:
    def __init__(self, path):
        with open(path, "rb") as f:
            self.data = f.read()

    def block(self, bno):
        return self.data[bno * BSIZE:(bno + 1) * BSIZE]

    def inode(self, inum):
        raw = self.block(INODESTART + inum // 16)
        off = (inum % 16) * 64
        f = struct.unpack("<4h14I", raw[off:off + 64])
        return {"type": f[0], "nlink": f[3], "size": f[4], "addrs": f[5:18]}

    def root_entries(self):
        root = self.inode(1)
        out = []
        for fbn in range(min(root["size"] // BSIZE, 11)):
            bno = root["addrs"][fbn]
            if bno == 0:
                continue
            raw = self.block(bno)
            for off in range(0, BSIZE, 16):
                inum, name = struct.unpack_from("<H14s", raw, off)
                if inum != 0:
                    out.append((name.rstrip(b"\0").decode("ascii", "replace"),
                                inum))
        return out

    def pending_seq(self):
        """seq of a committed-but-uncheckpointed txn per the log header, or 0."""
        magic, seq, n = struct.unpack_from("<3I", self.block(LOGSTART), 0)
        return seq if (magic == LOGMAGIC and 0 < n <= LOGCAP) else 0

    def prefix(self):
        """-> (m, why): m complete byte-exact files f1..fm, or (-1, why)."""
        names = {n: i for n, i in self.root_entries() if n not in (".", "..")}
        m = 0
        for t, d in WORKLOAD:
            nm = "f%d" % t
            if nm not in names:
                break
            ino = self.inode(names[nm])
            if ino["type"] != T_FILE or ino["nlink"] != 1:
                return -1, "%s type=%d nlink=%d" % (nm, ino["type"], ino["nlink"])
            if ino["size"] != d * BSIZE:
                return -1, "%s size=%d want %d" % (nm, ino["size"], d * BSIZE)
            for j in range(d):
                bno = ino["addrs"][j]
                blk = self.block(bno) if 0 < bno < FSSIZE else b""
                if blk != fill_pattern(t, j):
                    return -1, "%s block %d wrong contents" % (nm, j)
            m += 1
        extra = set(names) - {"f%d" % t for t, _ in WORKLOAD[:m]}
        if extra:
            return -1, "unexpected root entries %s" % sorted(extra)
        return m, ""


def parse_kv(out, prefix_word):
    """Fields of the first line starting with `prefix_word `."""
    for line in out.splitlines():
        if line.startswith(prefix_word + " "):
            return dict(x.split("=", 1) for x in line.split()[1:] if "=" in x)
    return None


def committed_map(out):
    d = {}
    for line in out.splitlines():
        if line.startswith("committed "):
            kv = dict(x.split("=", 1) for x in line.split()[1:])
            try:
                d[int(kv["op"])] = int(kv["installs"])
            except (KeyError, ValueError):
                pass
    return d


# ---------------------------------------------------------------------------
# Part 1 -- the block device
# ---------------------------------------------------------------------------

def bdev(BD, tmp, script):
    img = os.path.join(tmp, "bd.img")
    if os.path.exists(img):
        os.remove(img)
    rc, out, _ = run(BD + [img, "-"], stdin_data=script)
    return rc, out, img


def dump_blocks(BD, img):
    """{bno: byte} of the durable image, via a fresh process."""
    rc, out, _ = run(BD + [img, "-"], stdin_data="dump\n")
    d = {}
    for line in out.splitlines():
        if line.startswith("dump bno="):
            kv = dict(x.split("=", 1) for x in line.split()[1:])
            d[int(kv["bno"])] = int(kv["byte"], 16)
    return d


def part1(BD, tmp):
    # 1.1 fifo: writes install immediately, in issue order; counters count.
    rc, out, img = bdev(BD, tmp, "init 16\nwrite 1 aa\nwrite 2 bb\nstats\n")
    want = dump_blocks(BD, img) == {1: 0xAA, 2: 0xBB}
    name = "Part 1: fifo installs immediately (stats installs=2)"
    if rc == 0 and "stats installs=2 barriers=0 queued=0" in out and want:
        record(name, "PASS")
    else:
        fail(name, "want both blocks durable and 'stats installs=2 barriers=0 "
                   "queued=0', got rc=%d durable=%s" % (rc, dump_blocks(BD, img)), out)

    # 1.2 fifo + crash_after: the durable prefix is exact, CRASH line, exit 3.
    rc, out, img = bdev(BD, tmp, "init 16\ncrashafter 3\n"
                        "write 1 11\nwrite 2 22\nwrite 3 33\nwrite 4 44\n")
    d = dump_blocks(BD, img)
    name = "Part 1: crash after N loses exactly the writes past N"
    if rc == 3 and "CRASH installs=3" in out and d == {1: 0x11, 2: 0x22, 3: 0x33}:
        record(name, "PASS")
    else:
        fail(name, "want exit 3, 'CRASH installs=3', durable {1,2,3} only; "
                   "got rc=%d durable=%s" % (rc, d), out)

    # 1.3 reorder: nothing durable before the barrier, though reads see the
    # queued write; everything durable after it.
    rc, out, img = bdev(BD, tmp, "init 16\nmode reorder\n"
                        "write 5 55\nread 5\nstats\ncrashnow\n")
    d = dump_blocks(BD, img)
    name = "Part 1: reorder queues until a barrier (reads still see the write)"
    if rc == 3 and "read bno=5 byte=55" in out and \
       "stats installs=0 barriers=0 queued=1" in out and d == {}:
        record(name, "PASS")
    else:
        fail(name, "want 'read bno=5 byte=55', 'stats installs=0 barriers=0 "
                   "queued=1', nothing durable, exit 3; got rc=%d durable=%s"
                   % (rc, d), out)

    # 1.4 reorder drains in REVERSE issue order: crash mid-drain keeps only
    # the LAST-issued write.
    rc, out, img = bdev(BD, tmp, "init 16\nmode reorder\ncrashafter 1\n"
                        "write 1 aa\nwrite 2 bb\nwrite 3 cc\nbarrier\n")
    d = dump_blocks(BD, img)
    name = "Part 1: the reorder drain is reverse issue order"
    if rc == 3 and "CRASH installs=1" in out and d == {3: 0xCC}:
        record(name, "PASS")
    else:
        fail(name, "crash 1 install into a 3-write drain must leave only the "
                   "last-issued block (3); got rc=%d durable=%s" % (rc, d), out)

    # 1.5 a full barrier drains everything queued.
    rc, out, img = bdev(BD, tmp, "init 16\nmode reorder\n"
                        "write 1 aa\nwrite 2 bb\nbarrier\nstats\n")
    d = dump_blocks(BD, img)
    name = "Part 1: a barrier makes the whole queue durable"
    if rc == 0 and "stats installs=2 barriers=1 queued=0" in out and \
       d == {1: 0xAA, 2: 0xBB}:
        record(name, "PASS")
    else:
        fail(name, "want both blocks durable and 'stats installs=2 barriers=1 "
                   "queued=0'; got rc=%d durable=%s" % (rc, d), out)

    # 1.6 a second write to a queued block coalesces IN PLACE: the entry keeps
    # its queue position.  Derivable from the stated model: after the third
    # write the queue is [1:cc, 2:bb] (block 1's entry replaced where it sat),
    # the drain is reverse issue order, so install 1 is block 2 -- one install
    # then a crash leaves exactly {2: bb}.  A coalesce that moved the entry to
    # the tail would drain block 1 first and leave {1: cc} instead.
    rc, out, img = bdev(BD, tmp, "init 16\nmode reorder\ncrashafter 1\n"
                        "write 1 aa\nwrite 2 bb\nwrite 1 cc\nbarrier\n")
    d = dump_blocks(BD, img)
    name = "Part 1: a coalesced write keeps its queue position"
    if rc == 3 and "CRASH installs=1" in out and d == {2: 0xBB}:
        record(name, "PASS")
    else:
        fail(name, "rewriting queued block 1 must not move its entry: the "
                   "reverse drain installs block 2 first, so one install then "
                   "the crash leaves exactly {2: bb}; got rc=%d durable=%s"
             % (rc, d), out)


# ---------------------------------------------------------------------------
# Part 2 -- the unjournaled crash table
# ---------------------------------------------------------------------------

def xfsck_classes(XFSCK, img):
    rc, out, _ = run([XFSCK, img])
    classes = sorted({ln.split()[1].rstrip(":") for ln in out.splitlines()
                      if ln.startswith("FAIL ")})
    clean = (rc == 0 and out.splitlines() and out.splitlines()[0] == "xfsck: clean")
    return clean, classes, out


def part2(CF, XFSCK, tmp):
    base = os.path.join(tmp, "p2base.img")
    img = os.path.join(tmp, "p2.img")
    run(CF + ["mkfs", base])

    # 2.1 the uncrashed workload: exactly 5 installs, clean, contents exact.
    shutil.copy(base, img)
    rc, out, _ = run(CF + ["workload", img])
    clean, _, xout = xfsck_classes(XFSCK, img)
    rd = Img(img)
    names = dict(rd.root_entries())
    content_ok = False
    if "f" in names:
        ino = rd.inode(names["f"])
        content_ok = (ino["type"] == T_FILE and ino["size"] == 2 * BSIZE and
                      rd.block(ino["addrs"][0]) == fill_pattern(1, 0) and
                      rd.block(ino["addrs"][1]) == fill_pattern(1, 1))
    name = "Part 2: the create is exactly 5 writes and the result is clean"
    if rc == 0 and "workload done installs=5" in out and clean and content_ok:
        record(name, "PASS")
    else:
        fail(name, "want 'workload done installs=5', xfsck clean, and f's two "
                   "blocks byte-exact; got rc=%d clean=%s content=%s"
             % (rc, clean, content_ok), out + xout)

    # 2.2 the crash table, point by point.  Derivation: after W1 the entry
    # names a free inode; after W2 the inode claims blocks the bitmap says are
    # free; after W3 the structures agree but the file's contents are not yet
    # written -- the case a structural checker cannot see, so xfsck must be
    # CLEAN there while the data is still the zeroes mkfs left.
    expect = {
        0: ("clean", None),
        1: ("classes", ["dangling-entry"]),
        2: ("classes", ["block-free-but-used"]),
        3: ("clean", (0, 0)),        # data blocks still zero, zero
        4: ("clean", (1, 0)),        # first data block written, second not
        5: ("clean", (1, 1)),        # both written: rc 0, no crash
    }
    for k in range(6):
        shutil.copy(base, img)
        rc, out, _ = run(CF + ["workload", img, "crash=%d" % k])
        clean, classes, xout = xfsck_classes(XFSCK, img)
        kind, detail = expect[k]
        crash_ok = (rc == 3 and ("CRASH installs=%d" % k) in out) if k < 5 \
            else (rc == 0 and "workload done installs=5" in out)
        name = "Part 2: crash after %d write%s -> %s" % (
            k, "" if k == 1 else "s",
            "consistent" if kind == "clean" else " + ".join(detail))
        if not crash_ok:
            fail(name, "workload did not stop at exactly %d installs (rc=%d)"
                 % (k, rc), out)
            continue
        if kind == "classes":
            if not clean and classes == detail:
                record(name, "PASS")
            else:
                fail(name, "want xfsck classes %s, got clean=%s classes=%s"
                     % (detail, clean, classes), xout)
        else:
            ok = clean
            if ok and detail is not None:
                rd = Img(img)
                names = dict(rd.root_entries())
                ino = rd.inode(names.get("f", 0)) if "f" in names else None
                if ino is None:
                    ok = False
                else:
                    for j, written in enumerate(detail):
                        want = fill_pattern(1, j) if written else bytes(BSIZE)
                        if rd.block(ino["addrs"][j]) != want:
                            ok = False
            elif clean and detail is None:
                rd = Img(img)
                ok = "f" not in dict(rd.root_entries())
            if ok:
                record(name, "PASS")
            else:
                fail(name, "want a clean image whose data blocks hold exactly "
                           "what had been written by point %d" % k, xout)


# ---------------------------------------------------------------------------
# Parts 3-5 -- the journal, the campaign, the cost comparison
# ---------------------------------------------------------------------------

def workload_ref(JN, tmp, flags):
    """No-crash reference run -> (ok, done-fields, committed-map, out)."""
    img = os.path.join(tmp, "ref.img")
    run(JN + ["mkfs", img])
    rc, out, _ = run(JN + ["workload", img] + flags)
    done = parse_kv(out, "done")
    return rc == 0 and done is not None, done, committed_map(out), out, img


def part3(JN, XFSCK, tmp):
    # 3.1 data mode, no crash: the protocol's install count (2n+2 per op:
    # 10+12+10+14 = 46), 4 barriers per op, commits acknowledged in order.
    ok, done, commits, out, img = workload_ref(JN, tmp, ["mode=data"])
    clean, _, xout = xfsck_classes(XFSCK, img)
    m, _why = Img(img).prefix()
    name = "Part 3: data journaling costs 2n+2 installs per op (46 total)"
    if ok and done.get("installs") == "46" and done.get("barriers") == "16" \
       and commits == {1: 10, 2: 22, 3: 32, 4: 46} and clean and m == 4:
        record(name, "PASS")
    else:
        fail(name, "want done installs=46 barriers=16, committed at "
                   "10/22/32/46, clean, all 4 files exact; got done=%s "
                   "commits=%s clean=%s files=%s" % (done, commits, clean, m),
             out + xout)

    # 3.2 recover on an intact image touches nothing.
    rc, out, _ = run(JN + ["recover", img])
    clean, _, _ = xfsck_classes(XFSCK, img)
    m, _why = Img(img).prefix()
    name = "Part 3: recovery of an intact image is empty"
    if rc == 0 and "recover empty" in out and clean and m == 4:
        record(name, "PASS")
    else:
        fail(name, "want 'recover empty' and an unchanged image; got rc=%d "
                   "files=%s" % (rc, m), out)

    # 3.3/3.4/3.5 three pinned points of op 1 (commit installs at 5 = 4 slots
    # + the record): mid-journal (2) -> nothing; at the commit (5) -> f1
    # survives; mid-checkpoint (7) -> replay over a partial checkpoint.
    base = os.path.join(tmp, "p3base.img")
    run(JN + ["mkfs", base])
    for i, wantm, label in ((2, 0, "an uncommitted op leaves no trace"),
                            (5, 1, "a committed op survives (durability)"),
                            (7, 1, "replay over a partial checkpoint")):
        img2 = os.path.join(tmp, "p3.img")
        shutil.copy(base, img2)
        rc, out, _ = run(JN + ["workload", img2, "mode=data", "crash=%d" % i])
        rc2, rout, _ = run(JN + ["recover", img2])
        clean, _, xout = xfsck_classes(XFSCK, img2)
        m, why = Img(img2).prefix()
        name = "Part 3: crash at i=%d -- %s" % (i, label)
        if rc == 3 and rc2 == 0 and clean and m == wantm:
            record(name, "PASS")
        else:
            fail(name, "want clean image with exactly %d file(s); got rc=%d/%d "
                       "clean=%s files=%s %s" % (wantm, rc, rc2, clean, m, why),
                 out + rout + xout)


def campaign(JN, XFSCK, tmp, flags, done, commits, label):
    """The sweep: for every crash point, crash -> recover -> three oracles.
    Returns (points, bad, first_bad_msg)."""
    total = int(done["installs"])
    base = os.path.join(tmp, "cbase.img")
    img = os.path.join(tmp, "c.img")
    run(JN + ["mkfs", base])
    bad, first = 0, ""
    for i in range(total):
        shutil.copy(base, img)
        rc1, wout, _ = run(JN + ["workload", img] + flags + ["crash=%d" % i])
        pend = Img(img).pending_seq()
        rc2, rout, _ = run(JN + ["recover", img])
        clean, _, xout = xfsck_classes(XFSCK, img)
        m, why = Img(img).prefix()
        need = max(pend, max([t for t, inst in commits.items() if inst <= i],
                             default=0))
        msg = ""
        if rc1 != 3 or ("CRASH installs=%d" % i) not in wout:
            # a device that ignores its crash point would sail through every
            # sweep below with a quietly completed image -- refuse that here
            msg = "workload did not crash at install %d (rc=%d)" % (i, rc1)
        elif rc2 != 0:
            msg = "recover failed (rc=%d)" % rc2
        elif not clean:
            msg = "structure: %s" % (xout.splitlines()[0] if xout else "?")
        elif m < 0:
            msg = "atomicity: %s" % why
        elif m < need:
            msg = "durability: %d op(s) committed, %d present" % (need, m)
        if msg:
            bad += 1
            if not first:
                first = "i=%d: %s" % (i, msg)
    return total, bad, first


def part4(JN, XFSCK, tmp):
    # 4.1 the full campaign, fifo device: every crash point recovers to a
    # consistent AND durable image.  The sweep's length comes from the
    # no-crash run's own install count, so `bad == 0` alone would let a
    # journal that installs NOTHING pass a zero-point sweep vacuously --
    # the point count is asserted too.  46 is handout-derivable: 2n+2
    # installs per data-mode transaction, n = 3+d, d = 1,2,1,3 =>
    # 10+12+10+14.
    ok, done, commits, out, _ = workload_ref(JN, tmp, ["mode=data"])
    name = "Part 4: campaign, data mode, fifo -- every crash point recovers"
    if not ok:
        fail(name, "no-crash run failed", out)
    else:
        pts, bad, first = campaign(JN, XFSCK, tmp, ["mode=data"], done,
                                   commits, "fifo")
        if bad == 0 and pts == 46:
            record(name, "PASS")
            sys.stderr.write("  [info] campaign(fifo): %d crash points, all "
                             "consistent+durable\n" % pts)
        elif pts != 46:
            fail(name, "campaign ran %d crash point(s), expected 46 (2n+2 "
                       "per op, n = 4,5,4,6) -- is the journal installing "
                       "anything?" % pts)
        else:
            fail(name, "%d of %d crash points bad; first: %s" % (bad, pts, first))

    # 4.2 the same campaign on the reordering device: this is where every
    # missing barrier surfaces, since the drain is free to install the commit
    # record first.
    ok, done, commits, out, _ = workload_ref(JN, tmp, ["mode=data", "dev=reorder"])
    name = "Part 4: campaign, data mode, reordering device -- barriers hold"
    if not ok:
        fail(name, "no-crash run failed under dev=reorder", out)
    else:
        pts, bad, first = campaign(JN, XFSCK, tmp, ["mode=data", "dev=reorder"],
                                   done, commits, "reorder")
        # same 46 as 4.1 -- reordering changes when installs land, never how
        # many.  A journal that never barriers installs 0 blocks on this
        # device, and a 0-point sweep must not pass.
        if bad == 0 and pts == 46:
            record(name, "PASS")
            sys.stderr.write("  [info] campaign(reorder): %d crash points, all "
                             "consistent+durable\n" % pts)
        elif pts != 46:
            fail(name, "campaign ran %d crash point(s), expected 46 -- did "
                       "the journal barrier at all? (an undrained reordering "
                       "device installs nothing)" % pts)
        else:
            fail(name, "%d of %d crash points bad; first: %s" % (bad, pts, first))

    # 4.3 recovery itself crashes at every install and is simply run again.
    base = os.path.join(tmp, "rbase.img")
    img = os.path.join(tmp, "r.img")
    run(JN + ["mkfs", base])
    istar = None
    for i in range(60):
        shutil.copy(base, img)
        run(JN + ["workload", img, "mode=data", "crash=%d" % i])
        if Img(img).pending_seq() == 2:
            istar = i
            break
    name = "Part 4: recovery is idempotent under its own crashes"
    if istar is None:
        fail(name, "no crash point leaves op 2 committed-but-uncheckpointed "
                   "in the log header -- is the commit record being written?")
    else:
        _, rout, _ = run(JN + ["recover", img])
        rdone = parse_kv(rout, "recover done")
        rtotal = int(rdone["installs"]) if rdone else 0
        bad, first = 0, ""
        for j in range(rtotal):
            shutil.copy(base, img)
            run(JN + ["workload", img, "mode=data", "crash=%d" % istar])
            rcj, _, _ = run(JN + ["recover", img, "crash=%d" % j])
            rc2, _, _ = run(JN + ["recover", img])
            clean, _, xout = xfsck_classes(XFSCK, img)
            m, why = Img(img).prefix()
            if not (rcj == 3 and rc2 == 0 and clean and m >= 2):
                bad += 1
                if not first:
                    first = "j=%d clean=%s files=%s %s" % (j, clean, m, why)
        if rtotal >= 4 and bad == 0:
            record(name, "PASS")
            sys.stderr.write("  [info] recovery sweep: workload crash i*=%d, "
                             "%d recovery crash points, all fine\n"
                             % (istar, rtotal))
        else:
            fail(name, "recovery has %d installs; %d crash point(s) lost op 2 "
                       "or corrupted; first: %s" % (rtotal, bad, first))

    # 4.4 the deliberate violation: commit issued before the log blocks.
    # Crash at install 1 leaves a durable commit record naming log slots that
    # were never written; replaying them destroys the metadata they cover.
    shutil.copy(base, img)
    rc, out, _ = run(JN + ["workload", img, "mode=data",
                           "misorder=commit-first", "crash=1"])
    # under commit-first the ONE install performed is the commit record
    # itself (op 1, seq=1), so the crashed image's header must show a
    # committed transaction whose log slots were never written.  Without
    # this check any broken journal whose crash=1 image happens to be
    # unclean -- e.g. a null pass-through mid-create -- would pass.
    pend = Img(img).pending_seq()
    run(JN + ["recover", img])
    clean, _, xout = xfsck_classes(XFSCK, img)
    name = "Part 4: commit-before-log corrupts at crash point 1"
    if rc == 3 and pend == 1 and not clean:
        record(name, "PASS")
    else:
        fail(name, "with misorder=commit-first, install 1 IS the commit "
                   "record: crash=1 must leave a durable header (seq=1, "
                   "n>0) naming never-written slots, and replaying them "
                   "must leave an image xfsck objects to; got rc=%d "
                   "pending_seq=%d clean=%s" % (rc, pend, clean), out + xout)


def part5(JN, XFSCK, tmp):
    # 5.1 ordered mode, no crash: 8+d installs per create (9+10+9+11 = 39),
    # 5 barriers per op, and the same final tree.
    ok, done, commits, out, img = workload_ref(JN, tmp, ["mode=ordered"])
    clean, _, xout = xfsck_classes(XFSCK, img)
    m, _why = Img(img).prefix()
    name = "Part 5: ordered journaling costs 8+d installs per op (39 total)"
    if ok and done.get("installs") == "39" and done.get("barriers") == "20" \
       and commits == {1: 9, 2: 19, 3: 28, 4: 39} and clean and m == 4:
        record(name, "PASS")
    else:
        fail(name, "want done installs=39 barriers=20, committed at "
                   "9/19/28/39, clean, 4 files exact; got done=%s commits=%s "
                   "clean=%s files=%s" % (done, commits, clean, m), out + xout)

    # 5.2 ordered mode passes the same campaign (its guarantee for THIS
    # workload -- fresh blocks only -- is the same; what it gives up is the
    # subject of JOURNAL-COST.md, which the rubric marks).
    name = "Part 5: campaign, ordered mode -- every crash point recovers"
    if not ok:
        fail(name, "no-crash run failed", out)
        return
    pts, bad, first = campaign(JN, XFSCK, tmp, ["mode=ordered"], done,
                               commits, "ordered")
    # 39 is handout-derivable (8+d installs per create, d = 1,2,1,3 =>
    # 9+10+9+11); a zero-point sweep must not pass -- see 4.1/4.2.
    if bad == 0 and pts == 39:
        record(name, "PASS")
        sys.stderr.write("  [info] campaign(ordered): %d crash points, all "
                         "consistent+durable\n" % pts)
    elif pts != 39:
        fail(name, "campaign ran %d crash point(s), expected 39 (8+d per "
                   "op, d = 1,2,1,3) -- is the journal installing "
                   "anything?" % pts)
    else:
        fail(name, "%d of %d crash points bad; first: %s" % (bad, pts, first))


# ---------------------------------------------------------------------------

def main(argv):
    if len(argv) != 2:
        sys.stderr.write("usage: grade.py <workdir>\n")
        return 2
    workdir = os.path.abspath(argv[1])
    if not os.path.isdir(workdir):
        sys.stderr.write("grade.py: '%s' is not a directory\n" % workdir)
        return 2
    here = os.path.dirname(os.path.abspath(__file__))

    print("== building the oracle (Lab 7's reference xfsck) ==")
    tmp_top = tempfile.mkdtemp(prefix="lab9-grade.")
    xfsck = os.path.join(tmp_top, "xfsck")
    rc = subprocess.run(["gcc", "-Wall", "-Wextra", "-Werror", "-std=gnu11",
                         "-O2", "-o", xfsck,
                         os.path.join(here, "oracle", "xfsck.c"),
                         os.path.join(here, "oracle", "xv6img.c")],
                        capture_output=True, text=True)
    if rc.returncode != 0:
        sys.stderr.write("internal error: oracle failed to build:\n" + rc.stderr)
        return 2
    print("oracle OK\n")

    print("== locating tools in %s ==" % workdir)
    missing = False
    tools = {}
    for name in ("blockdev.py", "crashfs", "journal"):
        cmd = resolve_tool(workdir, name)
        if cmd is None:
            sys.stderr.write("  no runnable '%s' in workdir\n" % name)
            missing = True
        tools[name] = cmd
    for name in ("wal.py", "xv6fs.py"):
        if not os.path.isfile(os.path.join(workdir, name)):
            sys.stderr.write("  no '%s' in workdir (journal imports it)\n" % name)
            missing = True
    if missing:
        sys.stderr.write("RESULT: tools missing\n")
        shutil.rmtree(tmp_top, ignore_errors=True)
        return 1
    print("tools OK\n")

    BD, CF, JN = tools["blockdev.py"], tools["crashfs"], tools["journal"]
    part1(BD, tmp_top)
    part2(CF, xfsck, tmp_top)
    part3(JN, xfsck, tmp_top)
    part4(JN, xfsck, tmp_top)
    part5(JN, xfsck, tmp_top)
    shutil.rmtree(tmp_top, ignore_errors=True)

    print("\n== results ==")
    for verdict, name in results:
        print("  %-6s %s" % (verdict, name))
    print()
    print("%d passed, %d failed" % (counts["PASS"], counts["FAIL"]))
    print("(CRASH-TABLE.md, CAMPAIGN.md and JOURNAL-COST.md are rubric-marked")
    print(" against solutions/README.md and not checked here.)")
    return 1 if counts["FAIL"] else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
