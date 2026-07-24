#!/usr/bin/env python3
# grade.py <workdir> -- autograder for Lab 8, Parts 1-3.
#
# Lab 8 is three command-line tools -- pwstore, crack, accessmatrix -- driven
# entirely through their text interface (argv/stdin in, stdout out).  That is
# deliberate, exactly as in Lab 6: the harness never imports the submission, so a
# Python submission and (in principle) any other language that produces the same
# lines grade identically.  The reference is Python because Part 1 needs a real
# key-derivation function and Python's standard-library hashlib supplies PBKDF2
# with nothing to link.
#
# Every verdict is a property of a tool's OUTPUT under a fixed input, never a
# number the harness lets the tool choose for itself.  Each asserted number is
# derivable from the handout:
#   * salt working  -- two identical passwords MUST give two different hashes;
#   * crack count    -- a store with K wordlist-member passwords recovers exactly K,
#                       at full-scan cost W*N (no early exit at the first hit);
#   * hash_ops       -- W against an unsalted store, W*N against a salted one
#                       (the salting lesson; W = wordlist size, N = user count);
#   * storage        -- granted = distinct granted (s,o,r) triples,
#                       acl_entries = cap_entries = granted, matrix_cells = S*O*R;
#   * revoke costs   -- ACL subject-revocation is O(objects), capability
#                       object-revocation is O(subjects); indirection makes the
#                       latter O(1).
#
# Parts 1 (timing note), 4 (setuid audit) and the threat-model note are
# rubric-marked and NOT touched here -- see solutions/README.md.
#
# What's checked:  functional tests for Part 1, a PROPERTY test for Part 2 (the
# core), outcome+counter tests for Part 3.  Exits 0 only if every case passes.

import os
import random
import subprocess
import sys
import tempfile

TIMEOUT = 30
PART2_MATRICES = 8          # random matrices per run; 8 is the first count at
                            # which every (s,o,r) cell is granted in >=1 matrix
                            # AND ungranted in >=1, so even a single-cell
                            # projection bug disagrees with the oracle somewhere
PART2_S = 6
PART2_O = 6
PART2_RIGHTS = "rwx"
PART2_SEED = 20250721       # fixed base seed -> reproducible, never flakes

results = []                # list of (verdict, name)
counts = {"PASS": 0, "FAIL": 0, "SKIP": 0}


def record(name, verdict):
    results.append((verdict, name))
    counts[verdict] = counts.get(verdict, 0) + 1


def fail(name, msg, out=None):
    record(name, "FAIL")
    sys.stderr.write("  [%s] %s\n" % (name, msg))
    if out:
        sys.stderr.write("    --- output ---\n")
        for line in out.splitlines()[:20]:
            sys.stderr.write("    " + line + "\n")


# ---------------------------------------------------------------------------
# locate the three tools in WORKDIR (language-agnostic, like Lab 6)
# ---------------------------------------------------------------------------

def resolve_tool(workdir, name):
    """Return a command-prefix list for a tool, or None if it cannot be found."""
    direct = os.path.join(workdir, name)
    dotpy = os.path.join(workdir, name + ".py")
    if os.path.isfile(direct) and os.access(direct, os.X_OK):
        return [direct]                     # executable script or compiled binary
    if os.path.isfile(dotpy):
        return [sys.executable, dotpy]      # a <name>.py file
    if os.path.isfile(direct):
        return [sys.executable, direct]     # present but not +x: run with python3
    return None


def run(cmd, stdin_data=None):
    """Run a command under a timeout; return (rc, stdout, stderr)."""
    try:
        p = subprocess.run(cmd, input=stdin_data, capture_output=True,
                           text=True, timeout=TIMEOUT)
        return p.returncode, p.stdout, p.stderr
    except subprocess.TimeoutExpired:
        return 124, "", "timeout"


def crack_field(out, key):
    """Pull one key=value field off crack's 'crack ...' summary line."""
    for line in out.splitlines():
        if line.startswith("crack "):
            for kv in line.split():
                if kv.startswith(key + "="):
                    return kv.split("=", 1)[1]
    return None


def check_results(out):
    """Parse every 'check s=.. o=.. r=.. = yes|no' line into a dict."""
    d = {}
    for line in out.splitlines():
        if line.startswith("check "):
            head, sep, res = line.partition(" = ")
            if not sep:
                continue
            p = dict(kv.split("=", 1) for kv in head.split()[1:])
            d[(int(p["s"]), int(p["o"]), p["r"])] = res.strip()
    return d


# ===========================================================================
# Part 1 -- password storage and the dictionary attack
# ===========================================================================

def part1(tmp, PW, CRACK):
    # 1. register then authenticate with the right password -> OK.
    store = os.path.join(tmp, "s1")
    run(PW + ["register", store, "alice", "correcthorse", "salted"])
    _, out, _ = run(PW + ["auth", store, "alice", "correcthorse"])
    if "result=OK" in out:
        record("Part 1: register then authenticate succeeds", "PASS")
    else:
        fail("Part 1: register then authenticate succeeds", "expected result=OK", out)

    # 2. wrong passwords are rejected: a wholly different one (catches an auth
    #    that accepts anything) AND a near-miss differing only in case (catches
    #    a kdf/auth that normalises -- e.g. lowercases -- before hashing).
    _, out_a, _ = run(PW + ["auth", store, "alice", "wrongpassword"])
    _, out_b, _ = run(PW + ["auth", store, "alice", "CorrectHorse"])
    if "result=FAIL" in out_a and "result=FAIL" in out_b:
        record("Part 1: wrong passwords are rejected (even a case near-miss)", "PASS")
    else:
        fail("Part 1: wrong passwords are rejected (even a case near-miss)",
             "expected result=FAIL for both 'wrongpassword' and 'CorrectHorse'",
             out_a + out_b)

    # 3. THE SALT: two users, same password, salted -> DIFFERENT stored hashes.
    #    Catches a store that does not salt (same password -> same hash).
    store = os.path.join(tmp, "s2")
    run(PW + ["register", store, "bob", "hunter2", "salted"])
    run(PW + ["register", store, "carol", "hunter2", "salted"])
    _, out, _ = run(PW + ["dump", store])
    hashes = [line.split("hash=", 1)[1].strip()
              for line in out.splitlines() if "hash=" in line]
    if len(hashes) == 2 and hashes[0] != hashes[1]:
        record("Part 1: same password, salted, gives different hashes", "PASS")
    else:
        fail("Part 1: same password, salted, gives different hashes",
             "the two hashes were not distinct: %r" % hashes, out)

    # 4. control: WITHOUT salt, the same password gives the SAME hash (so the
    #    difference in case 3 is the salt, not nondeterminism elsewhere).
    store = os.path.join(tmp, "s3")
    run(PW + ["register", store, "d", "hunter2", "unsalted"])
    run(PW + ["register", store, "e", "hunter2", "unsalted"])
    _, out, _ = run(PW + ["dump", store])
    hashes = [line.split("hash=", 1)[1].strip()
              for line in out.splitlines() if "hash=" in line]
    if len(hashes) == 2 and hashes[0] == hashes[1]:
        record("Part 1: same password, unsalted, gives the same hash (control)", "PASS")
    else:
        fail("Part 1: same password, unsalted, gives the same hash (control)",
             "unsalted hashes should be identical: %r" % hashes, out)

    # 5. the dictionary attack recovers EXACTLY the wordlist-member passwords,
    #    at FULL-SCAN cost.  A controlled wordlist; three users are in it, two
    #    are not -> recover 3.  All 5 users are salted, so the handout's "scan
    #    the whole wordlist for each salted user -- do not stop at the first
    #    hit" pins hash_ops = W*5 here too: an early-exit cracker recovers the
    #    same set but reports fewer ops (its count would depend on which
    #    passwords are present and where they sit in the list).
    wl = os.path.join(tmp, "wl")
    words = ["dragon", "monkey", "letmein", "sunshine", "shadow", "qwerty"]
    with open(wl, "w") as f:
        f.write("\n".join(words) + "\n")
    store = os.path.join(tmp, "s4")
    in_list = {"u0": "dragon", "u1": "monkey", "u2": "letmein"}
    not_in = {"u3": "Tr0ub4dor&3xy", "u4": "9f3aQ-zznope"}
    for u, pw in list(in_list.items()) + list(not_in.items()):
        run(PW + ["register", store, u, pw, "salted"])
    _, out, _ = run(CRACK + [store, wl])
    got = {}
    for line in out.splitlines():
        if line.startswith("cracked "):
            parts = dict(kv.split("=", 1) for kv in line.split()[1:])
            got[parts["user"]] = parts["password"]
    recovered_ok = ("recovered=%d" % len(in_list)) in out
    n_users = len(in_list) + len(not_in)
    want_ops = len(words) * n_users          # W * N: every salted user, full scan
    ops = crack_field(out, "hash_ops")
    name5 = "Part 1: the dictionary attack recovers the weak passwords, full-scan"
    if got == in_list and recovered_ok and ops == str(want_ops):
        record(name5, "PASS")
    else:
        fail(name5,
             "expected %r recovered=%d hash_ops=%d, got %r hash_ops=%s"
             % (in_list, len(in_list), want_ops, got, ops), out)

    # 6. the SALTING LESSON, as a deterministic counter.  With passwords that are
    #    NOT in the wordlist, the cracker scans all W words for every user.
    #    Against an UNSALTED store it hashes each word once -> hash_ops = W.
    #    Against a SALTED store it re-hashes per user      -> hash_ops = W*N.
    #    W = 6 (the wordlist above), N = 4.
    N = 4
    uns = os.path.join(tmp, "uns")
    sal = os.path.join(tmp, "sal")
    for i in range(N):
        run(PW + ["register", uns, "u%d" % i, "absent-pw-%d" % i, "unsalted"])
        run(PW + ["register", sal, "u%d" % i, "absent-pw-%d" % i, "salted"])
    _, out_u, _ = run(CRACK + [uns, wl])
    _, out_s, _ = run(CRACK + [sal, wl])

    hu, hs = crack_field(out_u, "hash_ops"), crack_field(out_s, "hash_ops")
    W = len(words)
    if hu == str(W) and hs == str(W * N):
        record("Part 1: salting turns W hashes into W*N (the cost asymmetry)", "PASS")
    else:
        fail("Part 1: salting turns W hashes into W*N (the cost asymmetry)",
             "unsalted hash_ops=%s (want %d), salted hash_ops=%s (want %d)"
             % (hu, W, hs, W * N))


# ===========================================================================
# Part 2 -- the access matrix and its projections  (THE PROPERTY TEST)
# ===========================================================================

def parse_projection(body):
    """Parse the ' : ' body of an acl/caplist line into {index: rights}."""
    d = {}
    body = body.strip()
    if not body:
        return d
    for tok in body.split():
        idx, rs = tok.split(":", 1)
        d[int(idx)] = rs
    return d


def part2(AM):
    S, O, rights = PART2_S, PART2_O, PART2_RIGHTS
    total_triples = 0
    granted_seen = 0
    ungranted_seen = 0
    ok = True
    detail = ""

    for mi in range(PART2_MATRICES):
        rng = random.Random(PART2_SEED + mi)
        # oracle: the set of granted (s,o,r) triples
        oracle = set()
        lines = ["init %d %d %s" % (S, O, rights)]
        for s in range(S):
            for o in range(O):
                for r in rights:
                    if rng.random() < 0.5:
                        oracle.add((s, o, r))
                        lines.append("grant %d %d %s" % (s, o, r))
        # query every triple three ways, plus every projection
        for s in range(S):
            for o in range(O):
                for r in rights:
                    lines.append("check %d %d %s" % (s, o, r))
        for o in range(O):
            lines.append("acl %d" % o)
        for s in range(S):
            lines.append("caplist %d" % s)
        script = "\n".join(lines) + "\n"
        rc, out, err = run(AM + ["-"], stdin_data=script)

        checks = {}
        acls = {}
        caps = {}
        for line in out.splitlines():
            if line.startswith("check "):
                p = dict(kv.split("=", 1) for kv in line.replace(" = ", " res=").split()[1:])
                checks[(int(p["s"]), int(p["o"]), p["r"])] = (p["res"] == "yes")
            elif line.startswith("acl o="):
                head, body = line.split(" : ", 1) if " : " in line else (line, "")
                o = int(head.split("o=", 1)[1])
                acls[o] = parse_projection(body)
            elif line.startswith("caplist s="):
                head, body = line.split(" : ", 1) if " : " in line else (line, "")
                s = int(head.split("s=", 1)[1])
                caps[s] = parse_projection(body)

        for s in range(S):
            for o in range(O):
                for r in rights:
                    total_triples += 1
                    truth = (s, o, r) in oracle
                    if truth:
                        granted_seen += 1
                    else:
                        ungranted_seen += 1
                    c = checks.get((s, o, r))
                    a = r in acls.get(o, {}).get(s, "")
                    p = r in caps.get(s, {}).get(o, "")
                    if not (c == truth and a == truth and p == truth):
                        ok = False
                        detail = ("matrix %d triple (s=%d,o=%d,r=%s): oracle=%s "
                                  "check=%s acl=%s caplist=%s"
                                  % (mi, s, o, r, truth, c, a, p))
                        break
                if not ok:
                    break
            if not ok:
                break
        if not ok:
            break

    # coverage guard: the generator must produce a genuinely MIXED matrix, or the
    # property is vacuous (all-empty or all-full would let a broken projection pass)
    covered = granted_seen > 0 and ungranted_seen > 0
    name = ("Part 2: check, ACL and capability projections all agree "
            "(%d matrices x %d triples)" % (PART2_MATRICES, S * O * len(rights)))
    if ok and covered:
        record(name, "PASS")
        sys.stderr.write("  [info] Part 2 property: %d triples checked over %d matrices, "
                         "seed base=%d, density granted=%d ungranted=%d\n"
                         % (total_triples, PART2_MATRICES, PART2_SEED,
                            granted_seen, ungranted_seen))
    elif not covered:
        fail(name, "generator did not produce a mixed matrix "
                   "(granted=%d ungranted=%d) -- property would be vacuous"
                   % (granted_seen, ungranted_seen))
    else:
        fail(name, detail)

    # STORAGE.  Every number is the handout's formula applied to a matrix built
    # right here: init 3 4 rw -> S=3, O=4, R=2, matrix_cells = 3*4*2 = 24; four
    # DISTINCT (s,o,r) triples are granted (a cell is a set of rights, so the
    # duplicate grant must not double-count) -> granted = 4, and a list
    # representation stores one entry per triple -> acl_entries = cap_entries
    # = 4.  Nothing here is a grader constant a student could only copy.
    script = ("init 3 4 rw\n"
              "grant 0 0 r\ngrant 0 0 w\ngrant 1 2 r\ngrant 2 3 w\n"
              "grant 0 0 r\n"
              "storage\n")
    _, out, _ = run(AM + ["-"], stdin_data=script)
    want = ("storage subjects=3 objects=4 rights=2 granted=4 "
            "acl_entries=4 cap_entries=4 matrix_cells=24")
    line = next((l for l in out.splitlines() if l.startswith("storage")), "")
    sname = "Part 2: storage figures follow the granted-triple formula"
    if line.strip() == want:
        record(sname, "PASS")
    else:
        fail(sname, "want '%s'\n    got  '%s'" % (want, line.strip()), out)


# ===========================================================================
# Part 3 -- revocation both ways
# ===========================================================================

def part3(AM):
    # outcome: after revoking a subject's right across all objects, checks fail
    # for every affected cell -- AND nothing else is lost.  Subject 1 also holds
    # a SECOND right (w on object 2) and a bystander (subject 2) holds the
    # revoked right on the same object; both must survive.  This catches the
    # over-revocations: wiping the whole cell instead of discarding one right,
    # or discarding the right from every subject instead of just subject 1.
    script = ("init 4 5 rwx\n"
              "grant 1 2 r\ngrant 1 3 r\ngrant 1 4 r\n"
              "grant 1 2 w\ngrant 2 2 r\n"
              "revoke_subject 1 r\n"
              "check 1 2 r\ncheck 1 3 r\ncheck 1 4 r\n"
              "check 1 2 w\ncheck 2 2 r\n")
    _, out, _ = run(AM + ["-"], stdin_data=script)
    got = check_results(out)
    want = {(1, 2, "r"): "no", (1, 3, "r"): "no", (1, 4, "r"): "no",
            (1, 2, "w"): "yes", (2, 2, "r"): "yes"}
    name = "Part 3: revoking a subject's right removes it everywhere, and only it"
    if got == want:
        record(name, "PASS")
    else:
        fail(name, "want revoked cells 'no' AND the untouched right (1,2,w) and "
                   "bystander (2,2,r) still 'yes'; got %r" % got, out)

    # outcome: after revoking an object's right across all subjects, checks fail
    # for every affected cell -- AND nothing else is lost.  Subject 0 also holds
    # a SECOND right (r) on object 2, and subject 1 holds the revoked right on a
    # bystander object (3); both must survive.  Mirror of the case above.
    script = ("init 4 5 rwx\n"
              "grant 0 2 w\ngrant 1 2 w\ngrant 3 2 w\n"
              "grant 0 2 r\ngrant 1 3 w\n"
              "revoke_object 2 w\n"
              "check 0 2 w\ncheck 1 2 w\ncheck 3 2 w\n"
              "check 0 2 r\ncheck 1 3 w\n")
    _, out, _ = run(AM + ["-"], stdin_data=script)
    got = check_results(out)
    want = {(0, 2, "w"): "no", (1, 2, "w"): "no", (3, 2, "w"): "no",
            (0, 2, "r"): "yes", (1, 3, "w"): "yes"}
    name = "Part 3: revoking an object's right removes it for everyone, and only them"
    if got == want:
        record(name, "PASS")
    else:
        fail(name, "want revoked cells 'no' AND the untouched right (0,2,r) and "
                   "bystander object (1,3,w) still 'yes'; got %r" % got, out)

    # THE ASYMMETRY.  init 4 5: 4 subjects, 5 objects.
    #  revoke_subject: ACL must visit all 5 objects (acl_ops=5), capability 1.
    #  revoke_object:  ACL visits 1 object (acl_ops=1), capability all 4 (cap_ops=4).
    # A model that revokes correctly but reports SYMMETRIC costs fails here.
    script = ("init 4 5 rwx\n"
              "grant 1 2 r\ngrant 0 3 w\n"
              "revoke_subject 1 r\n"
              "revoke_object 3 w\n")
    _, out, _ = run(AM + ["-"], stdin_data=script)
    rs = next((l for l in out.splitlines() if l.startswith("revoke_subject ")), "")
    ro = next((l for l in out.splitlines() if l.startswith("revoke_object ")), "")
    rs_ok = "acl_ops=5" in rs and "cap_ops=1" in rs
    ro_ok = "acl_ops=1" in ro and "cap_ops=4" in ro
    if rs_ok and ro_ok:
        record("Part 3: revocation costs are asymmetric (ACL O(objects), cap O(subjects))", "PASS")
    else:
        fail("Part 3: revocation costs are asymmetric (ACL O(objects), cap O(subjects))",
             "want subject 'acl_ops=5 cap_ops=1', object 'acl_ops=1 cap_ops=4'\n"
             "    got: %s | %s" % (rs.strip(), ro.strip()), out)

    # INDIRECTION makes capability object-revocation tractable: cap_ops drops
    # from S(=4) to 1, and the checks still fail.  A model that fakes the cheap
    # count without actually revoking is caught by the checks; one that ignores
    # indirection is caught by cap_ops still being 4.
    script = ("init 4 5 rwx\n"
              "grant 0 2 w\ngrant 1 2 w\ngrant 3 2 w\n"
              "indirect on\n"
              "revoke_object 2 w\n"
              "check 0 2 w\ncheck 3 2 w\n")
    _, out, _ = run(AM + ["-"], stdin_data=script)
    ro = next((l for l in out.splitlines() if l.startswith("revoke_object ")), "")
    no_count = sum(1 for line in out.splitlines()
                   if line.startswith("check ") and line.endswith("= no"))
    if "cap_ops=1" in ro and no_count == 2:
        record("Part 3: indirection makes capability revocation O(1)", "PASS")
    else:
        fail("Part 3: indirection makes capability revocation O(1)",
             "want revoke_object cap_ops=1 and both checks 'no'; got '%s', no_count=%d"
             % (ro.strip(), no_count), out)


# ===========================================================================

def main(argv):
    if len(argv) != 2:
        sys.stderr.write("usage: grade.py <workdir>\n")
        return 2
    workdir = os.path.abspath(argv[1])
    if not os.path.isdir(workdir):
        sys.stderr.write("grade.py: '%s' is not a directory\n" % workdir)
        return 2

    print("== locating tools in %s ==" % workdir)
    tools = {}
    missing = False
    for name in ("pwstore", "crack", "accessmatrix"):
        cmd = resolve_tool(workdir, name)
        if cmd is None:
            sys.stderr.write("  %s: no executable '%s' or '%s.py' in workdir\n"
                             % (name, name, name))
            missing = True
        tools[name] = cmd
    if missing:
        sys.stderr.write("RESULT: tools missing\n")
        return 1
    print("tools OK\n")

    PW, CRACK, AM = tools["pwstore"], tools["crack"], tools["accessmatrix"]
    with tempfile.TemporaryDirectory() as tmp:
        part1(tmp, PW, CRACK)
    part2(AM)
    part3(AM)

    print("\n== results ==")
    for verdict, name in results:
        print("  %-6s %s" % (verdict, name))
    print()
    if counts["SKIP"]:
        print("%d passed, %d failed, %d skipped"
              % (counts["PASS"], counts["FAIL"], counts["SKIP"]))
    else:
        print("%d passed, %d failed" % (counts["PASS"], counts["FAIL"]))
    print("(Part 1's timing note, Part 4's setuid audit and THREAT-MODEL.md are")
    print(" rubric-marked -- see solutions/README.md -- and not checked here.)")
    return 1 if counts["FAIL"] else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
