# Exercise Sheet 17 — Files and directories

**Attempt after Week 17.** Budget ~3 hours. Work closed-book first, then
self-mark against [`solutions/exercise17-solutions.md`](solutions/exercise17-solutions.md).

**This sheet leans on:** OSTEP ch. 39; Bishop & Dilger (1996), the TOCTTOU
paper. TLPI ch. 18 is the reference if an edge-case contract is in doubt.

**You will need:** pen and paper. Every question is answerable by reasoning
from the chapter's model; a Linux shell is handy for *checking* your
answers to §B1–B2 (`strace`, `stat`, `ln`) but is not required. (Ch. 39's
code-writing homework — your own `stat`, `ls`, `tail` — lives in lab 7,
not here.)

---

## A. Warm-ups

*True or false? Justify in one or two sentences. A bare verdict earns
nothing.*

**A1.** `unlink("f")` deletes the file `f` and frees its blocks.

**A2.** `lseek(fd, 0, SEEK_END)` positions the disk arm at the file's last
block, so the next read is fast.

**A3.** After `fork()`, a read by the child advances the file offset seen
by the parent.

**A4.** Likewise, if two processes each `open()` the same file, a read by
one advances the offset seen by the other.

**A5.** When `write()` returns successfully, the data will survive a power
failure.

**A6.** Hard links to directories are forbidden; symbolic links to
directories are allowed.

**A7.** Removing the original file breaks a hard link but not a symbolic
link.

**A8.** To read the file `/a/b/f`, a process needs read permission on `f`
and execute permission on `/`, `/a`, and `/a/b`.

---

## B. The interface, traced

**B1. Descriptors and offsets.**
The file `data` holds exactly 1,000 bytes. Trace this program, giving each
call's return value and, after each call, the offset in every open-file-table
entry in play (label them OFT-a, OFT-b, …):

```c
int fd  = open("data", O_RDONLY);      // (1)
int fd2 = dup(fd);                     // (2)
read(fd, buf, 100);                    // (3)
lseek(fd2, 500, SEEK_SET);             // (4)
read(fd, buf, 100);                    // (5)
int fd3 = open("data", O_RDONLY);      // (6)
read(fd3, buf, 100);                   // (7)
int rc = fork();                       // (8)
// child only:
lseek(fd, 0, SEEK_END);                // (9)
// parent, after wait():
off_t o1 = lseek(fd, 0, SEEK_CUR);     // (10)
off_t o2 = lseek(fd3, 0, SEEK_CUR);    // (11)
```

State the value of `o1` and `o2`, and explain in one sentence each why they
differ in kind: what is shared at (2), what at (8), and what is not shared
at (6)?

**B2. Links and counts.**
Starting in an empty directory `d0`, predict the link count of every inode
touched, after each step:

```
1  touch a
2  ln a b
3  ln -s a c
4  mkdir sub
5  ln b sub/f
6  rm a
7  cat c        # what happens?
8  cat sub/f    # and here?
9  mkdir sub/deeper
```

  (a) Give the link counts after each numbered step (files *and*
      directories).
  (b) Explain steps 7 and 8: which one fails, and why does the other still
      work?
  (c) Derive the general rule for a directory's link count, and justify it
      from the `.` and `..` entries.
  (d) `du` on `d0` counts `a`/`b`/`sub/f`'s bytes once, but a naive
      script summing `ls -l` sizes counts them three times. What field
      from `stat()` lets a tool detect the sharing?

**B3. Permission bits, worked.**
Users and groups: **alice** ∈ {staff, web}, **bob** ∈ {staff},
**carol** ∈ {web}. Files:

```
-rw-rw-r--  alice web    /srv/www/index.html
-rw-r-----  bob   staff  /home/bob/notes.txt
-rwxr-x---  alice web    /srv/tools/deploy.sh
```

  (a) For each user × file: may they read? write? execute? (Nine short
      verdicts with one-clause reasons.)
  (b) `/home/bob` has mode `drwx------`. Does that change any verdict from
      (a)? State the rule about directory execute bits it illustrates.
  (c) Bob is to be able to *run* `deploy.sh` but not read it. Can the owner
      arrange this with `chmod`/`chgrp` alone? If yes give the commands; if
      no, prove the obstruction. Would the arrangement actually stop a
      determined bob from learning the script's contents? (It is a shell
      script — think about what "execute" means for one.)
  (d) Give the octal mode for: owner read/write, group read, others
      nothing, on a regular file — and the `chmod` invocation that sets
      index.html back to it.

**B4. TOCTTOU, step by step.**
A mail daemon runs as root. To append a message to `/var/mail/bob` — which
must be a regular file owned by bob, not a symlink — it does:

```
1  lstat("/var/mail/bob", &sb);        // check: regular file, owner bob
2  if (checks pass)
3      fd = open("/var/mail/bob", O_WRONLY|O_APPEND);
4      write(fd, msg, len);
```

  (a) Bob is malicious. Give the exact sequence of bob's actions,
      interleaved with the daemon's lines 1–4, that gets the message
      appended to `/etc/passwd`. Which of bob's operations are permitted
      by his own privileges, and why does each succeed?
  (b) In week 11–14 vocabulary: what is the shared mutable state, what is
      the "check", what is the "use", and why can't the daemon just take a
      lock around lines 1–3?
  (c) The daemon is changed to: `fd = open(path, O_WRONLY|O_APPEND|O_NOFOLLOW)`
      followed by `fstat(fd, &sb)` and the same checks *on the open
      descriptor*, aborting if they fail. Explain, using the chapter's
      descriptor/open-file-table/inode model, why the race is now closed —
      what exactly does bob's `rename()` no longer affect?
  (d) Bishop and Dilger frame TOCTTOU as a *binding* problem: the check
      and the use must refer to the same object. State in one sentence why
      pathnames make that hard and descriptors make it easy.

---

## C. Diagnose the failure

**C1. The vanishing file.**
A logging service creates one file per hour: it `open()`s
`/logs/2026-07-20-14.log` with `O_CREAT`, writes records for an hour,
calls `fsync(fd)` after every batch, and `close()`s the file. Operations
confirms via monitoring that the final `fsync` of each file returns 0.
After a power failure at 14:59, the machine reboots and
`/logs/2026-07-20-14.log` **does not exist at all** — yet the previous
hours' files are intact and complete.

  (a) Explain the mechanism. What, precisely, was durable at the moment of
      the crash, and what was not? (Ch. 39's durability discussion names
      this exact trap.)
  (b) Give the minimal fix in terms of the calls the service must add, and
      when.
  (c) A colleague proposes instead: "write each hour's log to
      `name.tmp`, then `rename()` it into place at the end of the hour —
      rename is atomic, problem solved." Diagnose what this fixes, what it
      quietly changes about the service's guarantees, and the case in
      which it loses strictly more data than the current design.

**C2. The backup that wasn't.**
A nightly "snapshot" tool conserves space by hard-linking: for each file,
`snap/N/path` is created with `ln` against yesterday's copy unless the file
changed that day. Months later, a user restores `report.txt` from
`snap/40/` — three weeks pre-dating an accidental edit — and finds the
restored file **contains the accidental edit**. Meanwhile another user, who
"corrupted" `config.yaml` the same way, restores it from an old snapshot
and gets the pristine version, as advertised.

  (a) Diagnose the mechanism: why did `report.txt`'s history vanish from
      every snapshot at once, while `config.yaml`'s survived? What must be
      true about how each file was edited? (Name the two classes of editor
      behaviour involved, in terms of the syscalls from this week.)
  (b) State the invariant a hard-link-based snapshotter needs from every
      program that modifies files, and why the tool cannot enforce it.
  (c) Propose the fix within the tool itself, and state its cost.

**C3. Checked at open.**
An administrator discovers that a former employee's still-running analysis
job is reading a confidential dataset. She revokes access:
`chmod 000 /data/secret.db` — and verifies with `ls -l`. The job keeps
reading the file successfully for six more hours.
  (a) Diagnose: why does revocation not take effect? Be precise about
      *when* Unix checks permissions and what the process holds.
  (b) The chapter calls a file descriptor "a capability". In one paragraph,
      connect that phrase to this incident: what property of
      capability-style access does the administrator have to live with,
      and what would she have to do to actually stop the job?
  (c) Checking permissions at every `read()` instead would make revocation
      immediate. Give two reasons Unix doesn't do that, and a judgement:
      for which kind of system is check-at-open the wrong default?
