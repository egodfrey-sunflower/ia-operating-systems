#!/usr/bin/env python3
"""Mutation driver for Lab 10 (build-time verification tool; lives in
solutions/ because its patches name the bugs).

For each named mutant: copy the reference sources into a scratch dir,
apply one deliberate bug, run tests/run.sh against it, and report which
cases flipped to FAIL.  Each mutant declares the case(s) that MUST fail;
the driver exits non-zero if any mutant survives (i.e. its targeted case
still passes).
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

SOL = os.path.dirname(os.path.abspath(__file__))
LAB = os.path.dirname(SOL)
RUN = os.path.join(LAB, "tests", "run.sh")

SRCS = ["udpecho.c", "reliable.c", "rpc.c", "rpcdemo.c",
        "fileserver.c", "fileclient.c"]


def patch(path, old, new, count=1):
    s = open(path).read()
    assert old in s, f"pattern not found in {path}:\n{old[:120]}"
    s = s.replace(old, new, count)
    open(path, "w").write(s)


# --- the mutants -----------------------------------------------------------

def m1_no_retransmit(d):
    """reliability: drop-on-loss -- never retransmit, give up on timeout"""
    patch(os.path.join(d, "reliable.c"),
          """		if (retrans >= retries)
			return -1;
		net_send(nt, pkt, len, srv);
		retrans++;""",
          """		(void)retries;
		return -1;      /* MUTANT: no retransmission */""")


def m2_deliver_dups(d):
    """reliability: receiver delivers every DATA arrival (no dedup)"""
    patch(os.path.join(d, "reliable.c"),
          """		if (type == T_DATA) {
			if (seq == expected) {""",
          """		if (type == T_DATA) {
			expected = seq; /* MUTANT: no dedup */
			if (seq == expected) {""")


def m3_no_fin_drain(d):
    """reliability: server exits the moment it first sees FIN"""
    patch(os.path.join(d, "reliable.c"),
          """		} else if (type == T_FIN) {
			if (!fin_seen) {
				fin_seen = 1;
				fin_seq = seq;
			}
			uint8_t ack[MAXPKT];
			size_t alen = pack(ack, T_ACK, fin_seq, NULL, 0);
			net_send(nt, ack, alen, &peer);
		}""",
          """		} else if (type == T_FIN) {
			uint8_t ack[MAXPKT];
			size_t alen = pack(ack, T_ACK, seq, NULL, 0);
			net_send(nt, ack, alen, &peer);
			(void)fin_seen; (void)fin_seq;
			break;  /* MUTANT: no drain; first FIN ends us */
		}""")


def m4_no_reply_cache(d):
    """rpc: no duplicate suppression -- every request executes the handler"""
    patch(os.path.join(d, "rpc.c"),
          """		struct cache_ent *e = cache_find(s, client);
		if (e != NULL && e->seq == seq) {
			net_send(s->nt, e->reply, e->len, &from);
			s->dups++;
			continue;
		}
		if (e != NULL && seq < e->seq)
			continue;       /* older than the cached call: a
			                   ghost; nothing useful to say */""",
          """		struct cache_ent *e = cache_find(s, client);
		/* MUTANT: no duplicate suppression -- always execute */""")


def m5_seq_only_cache(d):
    """rpc: reply cache keyed on seq alone (client ignored)"""
    patch(os.path.join(d, "rpc.c"),
          """static struct cache_ent *cache_find(rpc_server *s, uint32_t client)
{
	for (int i = 0; i < CACHE_SLOTS; i++)
		if (s->cache[i].used && s->cache[i].client == client)
			return &s->cache[i];
	return NULL;
}""",
          """static struct cache_ent *cache_find(rpc_server *s, uint32_t client)
{
	(void)client;   /* MUTANT: one global entry, keyed on seq alone */
	return s->cache[0].used ? &s->cache[0] : NULL;
}""")
    patch(os.path.join(d, "rpc.c"),
          """static struct cache_ent *cache_take(rpc_server *s, uint32_t client)
{
	for (int i = 0; i < CACHE_SLOTS; i++)
		if (!s->cache[i].used) {
			s->cache[i].used = 1;
			s->cache[i].client = client;
			return &s->cache[i];
		}""",
          """static struct cache_ent *cache_take(rpc_server *s, uint32_t client)
{
	s->cache[0].used = 1;   /* MUTANT */
	s->cache[0].client = client;
	if (1)
		return &s->cache[0];
	for (int i = 0; i < CACHE_SLOTS; i++)
		if (!s->cache[i].used) {
			s->cache[i].used = 1;
			s->cache[i].client = client;
			return &s->cache[i];
		}""")


def m6_any_reply(d):
    """rpc: client takes any reply as the answer to the call in flight"""
    patch(os.path.join(d, "rpc.c"),
          """		if (magic != RPC_MAGIC || rid != c->id || rseq != c->seq)
			continue;       /* not the call in flight: discard */""",
          """		if (magic != RPC_MAGIC)
			continue;       /* MUTANT: any reply will do */
		(void)rid; (void)rseq;""")


def m7_table_handles(d):
    """fileserver: fh = index into an in-memory table (dies with process)"""
    patch(os.path.join(d, "fileserver.c"),
          "static const char *exportdir;",
          """static const char *exportdir;

/* MUTANT: per-process open-file table; handles are table indices */
static char fh_table[64][FS_MAXNAME + 1];
static int fh_next;""")
    patch(os.path.join(d, "fileserver.c"),
          """	struct stat st;
	if (stat(path, &st) != 0)
		return RPC_ENOENT;
	mb_put_u64(rm, (uint64_t)st.st_ino);
	return RPC_OK;
}""",
          """	struct stat st;
	if (stat(path, &st) != 0)
		return RPC_ENOENT;
	if (fh_next >= 64)
		return RPC_EIO;
	snprintf(fh_table[fh_next], sizeof fh_table[0], "%s", name);
	mb_put_u64(rm, (uint64_t)(1000 + fh_next++));    /* MUTANT */
	return RPC_OK;
}""")
    patch(os.path.join(d, "fileserver.c"),
          """	DIR *d = opendir(exportdir);
	if (d == NULL)
		return -1;
	struct dirent *de;
	while ((de = readdir(d)) != NULL) {
		if (strcmp(de->d_name, ".") == 0 ||
		    strcmp(de->d_name, "..") == 0)
			continue;
		char path[4096];
		snprintf(path, sizeof path, "%s/%s", exportdir, de->d_name);
		struct stat st;
		if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
			continue;
		if ((uint64_t)st.st_ino == fh) {
			snprintf(out, max, "%s", path);
			closedir(d);
			return 0;
		}
	}
	closedir(d);
	return -1;""",
          """	/* MUTANT: resolve through the in-memory table */
	if (fh < 1000 || fh >= (uint64_t)(1000 + fh_next))
		return -1;      /* stale: this process never issued it */
	snprintf(out, max, "%s/%s", exportdir, fh_table[fh - 1000]);
	return 0;""")


def m8_write_cursor(d):
    """fileserver: writes land at a server-side cursor, not the offset"""
    patch(os.path.join(d, "fileserver.c"),
          "static const char *exportdir;",
          """static const char *exportdir;
static uint64_t wcursor;        /* MUTANT: server-side file position */""")
    patch(os.path.join(d, "fileserver.c"),
          """	ssize_t n = pwrite(fd, buf, len, (off_t)off);
	close(fd);
	if (n < 0 || (size_t)n != len)
		return RPC_EIO;""",
          """	(void)off;      /* MUTANT: ignore the offset, use the cursor */
	ssize_t n = pwrite(fd, buf, len, (off_t)wcursor);
	wcursor += len;
	close(fd);
	if (n < 0 || (size_t)n != len)
		return RPC_EIO;""")


def m9_no_cache(d):
    """fileclient: ac ignored -- every read goes to the server"""
    patch(os.path.join(d, "fileclient.c"),
          """	*hit = 0;
	if (f->ac_ms <= 0) {    /* cache off: straight through */
		return fs_read(f, f->fh, off, len, out, outlen);
	}""",
          """	*hit = 0;
	if (1) {        /* MUTANT: cache disabled */
		return fs_read(f, f->fh, off, len, out, outlen);
	}""")


def m10_never_expire(d):
    """fileclient: cached attributes never expire"""
    patch(os.path.join(d, "fileclient.c"),
          """	return f->avalid && (now_ms() - f->afetched) < f->ac_ms;""",
          """	return f->avalid;       /* MUTANT: the window never closes */""")


def m11_blind_echo(d):
    """udpecho: server echoes without verifying the checksum"""
    patch(os.path.join(d, "udpecho.c"),
          """		if (sum != cksum(payload, plen)) {""",
          """		if (0 && sum != cksum(payload, plen)) {  /* MUTANT */""")


def m12_write_back(d):
    """fileclient: writes buffered locally, flushed only at quit"""
    patch(os.path.join(d, "fileclient.c"),
          "#define RECSIZE 32",
          """#define RECSIZE 32

/* MUTANT: deferred-write state */
static char wb_buf[FS_MAXDATA + 64];
static long wb_off = -1;
static size_t wb_len;""")
    patch(os.path.join(d, "fileclient.c"),
          """			} else {
				size_t len = strlen(b);
				int st = fs_write(f, f->fh, (uint64_t)x, b,
				                  len);
				if (st != RPC_OK) {
					printf("write ERR status=%d\\n", st);
				} else {
					cache_drop(f);  /* write-through;
					                   revalidate next */
					printf("write off=%ld len=%zu ok\\n",
					       x, len);
				}
			}""",
          """			} else {
				/* MUTANT: write-back -- defer to quit */
				size_t len = strlen(b);
				snprintf(wb_buf, sizeof wb_buf, "%s", b);
				wb_off = x;
				wb_len = len;
				cache_drop(f);
				printf("write off=%ld len=%zu ok\\n", x, len);
			}""")
    patch(os.path.join(d, "fileclient.c"),
          """		} else if (strncmp(line, "quit", 4) == 0) {
			return 0;""",
          """		} else if (strncmp(line, "quit", 4) == 0) {
			if (wb_off >= 0 && f->have)     /* MUTANT: flush */
				fs_write(f, f->fh, (uint64_t)wb_off,
				         wb_buf, wb_len);
			return 0;""")


def m13_blocking_recv(d):
    """udpecho: client waits forever for a reply (no timeout)"""
    patch(os.path.join(d, "udpecho.c"),
          """		long r = net_recv(nt, rp, sizeof rp, NULL, (int)timeout);""",
          """		(void)timeout;
		long r = net_recv(nt, rp, sizeof rp, NULL, -1);  /* MUTANT */""")


def m14_new_seq_per_retry(d):
    """rpc: each retransmission gets a fresh seq (retries look like new calls)"""
    patch(os.path.join(d, "rpc.c"),
          """		if (r == -1) {                          /* timeout */
			if (tries >= c->max_retries)
				return -1;
			net_send(c->nt, req, reqlen, &c->srv);
			tries++;
			c->retrans++;
			continue;
		}""",
          """		if (r == -1) {                          /* timeout */
			if (tries >= c->max_retries)
				return -1;
			c->seq++;       /* MUTANT: retry = a new call */
			mbuf m2;
			mb_winit(&m2, req, sizeof req);
			mb_put_u32(&m2, RPC_MAGIC);
			mb_put_u32(&m2, c->id);
			mb_put_u32(&m2, c->seq);
			mb_put_u32(&m2, proc);
			mb_put_blob(&m2, args, alen);
			reqlen = m2.len;
			net_send(c->nt, req, reqlen, &c->srv);
			tries++;
			c->retrans++;
			continue;
		}""")


def m15_size_only_revalidate(d):
    """fileclient: revalidation compares size alone (ignores mtime)"""
    patch(os.path.join(d, "fileclient.c"),
          """	if (f->avalid &&
	    (size != f->asize || mt_s != f->amt_s || mt_ns != f->amt_ns))
		f->dvalid = 0;  /* someone changed the file under us */""",
          """	(void)mt_s; (void)mt_ns;
	if (f->avalid && size != f->asize)      /* MUTANT: size only */
		f->dvalid = 0;""")


MUTANTS = [
    # (name, patch fn, [cases that MUST fail (fixed substrings)])
    ("M1 reliability drops on loss (no retransmit)", m1_no_retransmit,
     ["Part 2: 30% loss, seed 31", "Part 2: 30% loss, seed 32",
      "Part 2: 30% loss, seed 33"]),
    ("M2 reliability delivers duplicates (no dedup)", m2_deliver_dups,
     ["Part 2: 30% loss, seed 31"]),
    ("M3 reliability server skips the close drain", m3_no_fin_drain,
     ["Part 2: 30% loss"]),          # any 30% seed counts; checked specially
    ("M4 RPC without duplicate suppression (re-executes)", m4_no_reply_cache,
     ["Part 3: CENTREPIECE"]),
    ("M5 RPC reply cache keyed on seq alone", m5_seq_only_cache,
     ["Part 3: two clients' calls are never confused",
      "Part 3: two clients interleaved under loss"]),
    ("M6 RPC client accepts any reply", m6_any_reply,
     ["Part 3: a late duplicate reply"]),
    ("M7 file handles are a per-process table", m7_table_handles,
     ["Part 4: RESTART -- kill -9 mid-writes",
      "Part 4: RESTART -- kill -9 mid-reads"]),
    ("M8 writes land at a server-side cursor", m8_write_cursor,
     ["Part 4: RESTART -- kill -9 mid-writes"]),
    ("M9 cache disabled (every read refetches)", m9_no_cache,
     ["Part 5: the cache turns 40 reads",
      "Part 5: inside the window the cache serves"]),
    ("M10 cache never revalidates", m10_never_expire,
     ["Part 5: past the window the staleness ends"]),
    ("M11 echo server skips checksum verification", m11_blind_echo,
     ["Part 1: injected corruption is detected"]),
    ("M12 client write-back (flush at quit)", m12_write_back,
     ["Part 5: a write goes through immediately"]),
    ("M13 client blocks forever on a lost reply", m13_blocking_recv,
     ["Part 1: a lossy link loses datagrams"]),
    ("M14 a retry gets a new seq (looks like a new call)", m14_new_seq_per_retry,
     ["Part 3: CENTREPIECE"]),
    ("M15 revalidation compares size alone", m15_size_only_revalidate,
     ["Part 5: past the window the staleness ends"]),
]


def run_suite(workdir):
    p = subprocess.run(["bash", RUN, workdir], capture_output=True,
                       text=True, timeout=900)
    fails = re.findall(r"^  FAIL   (.*)$", p.stdout, re.M)
    passes = re.findall(r"^  PASS   (.*)$", p.stdout, re.M)
    return p.returncode, passes, fails


def main():
    only = sys.argv[1:] or None
    bad = 0
    for name, fn, must_fail in MUTANTS:
        if only and not any(o in name for o in only):
            continue
        d = tempfile.mkdtemp(prefix="lab10mut-")
        for f in SRCS:
            shutil.copy(os.path.join(SOL, f), d)
        fn(d)
        rc, passes, fails = run_suite(d)
        caught = [c for c in must_fail if any(c in f for f in fails)]
        missed = [c for c in must_fail if not any(c in f for f in fails)]
        verdict = "CAUGHT" if (not missed and rc != 0) else "SURVIVED"
        if verdict == "SURVIVED":
            bad += 1
        print(f"{verdict}: {name}")
        print(f"    suite exit={rc}; {len(fails)} case(s) failed")
        for f in fails:
            print(f"      FAIL {f}")
        if missed:
            print(f"    !! targeted case did NOT fail: {missed}")
        shutil.rmtree(d, ignore_errors=True)
        sys.stdout.flush()
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
