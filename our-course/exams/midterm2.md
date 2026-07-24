# Midterm 2 — Operating Systems

**Coverage:** weeks 1–20, weighted toward weeks 11–20 — concurrency
(OSTEP ch. 24–33), persistence (ch. 36–41), and security (ch. 52–56) — with
one deliberate reach into midterm-1 material.
**Time:** 90 minutes. **Closed book.** No calculator — none of the arithmetic
needs one.
**Answer THREE of the four questions.** Each question is worth 20 marks; marks
for each part are shown in brackets. Where a calculation is requested, show
your working: a correct method with an arithmetic slip earns most of the
marks; a bare number earns few. Where a trace depends on a convention, use the
one stated.

---

## Question 1 — Concurrency

*(a)* The Lu et al. study of real-world concurrency bugs classifies most
non-deadlock bugs as **atomicity violations** or **order violations**. Define
each, give a two-line code sketch of each, and state roughly what fraction of
the study's non-deadlock bugs the two classes covered together — and what that
implies for where a bug-finding tool should spend its effort. **[4 marks]**

*(b)* A bounded buffer is shared by several producer and several consumer
threads (`put`/`get` update `count` correctly when called with the lock held):

```c
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cv = PTHREAD_COND_INITIALIZER;

void *producer(void *arg) {                void *consumer(void *arg) {
  for (;;) {                                 for (;;) {
    int item = make_item();                    pthread_mutex_lock(&m);
    pthread_mutex_lock(&m);                    if (count == 0)            // C1
    if (count == MAX)            // P1           pthread_cond_wait(&cv, &m);
      pthread_cond_wait(&cv, &m);             int item = get();           // C2
    put(item);                   // P2         pthread_cond_signal(&cv);   // C3
    pthread_cond_signal(&cv);    // P3         pthread_mutex_unlock(&m);
    pthread_mutex_unlock(&m);                  consume(item);
  }                                          }
}                                          }
```

  (i) This code contains **two** distinct design defects. Identify both.
  **[2 marks]**

  (ii) For one of them, give a concrete interleaving (label the steps by
  thread and line) with one producer and two consumers that drives `get()` to
  run on an **empty** buffer. Mark the exact moment the woken thread's view of
  the world becomes stale. **[3 marks]**

  (iii) Fix the code, and state the general rule (and its semantics — Mesa,
  not Hoare) that makes your fix necessary rather than stylistic. **[3 marks]**

*(c)* Give the semaphore rendering of the same bounded buffer: name the
semaphores, their **initial values**, and the order of operations in `put` and
`get`. Then show, with a two-thread interleaving, what goes wrong if a
consumer acquires the mutex **before** waiting on the full-buffer semaphore.
**[4 marks]**

*(d)* A fixed-priority preemptive system has three threads: **H** (high),
**M** (medium, CPU-bound, never blocks), **L** (low). L acquires a mutex; H
preempts L and blocks on that mutex; M then runs, preempting L — and keeps
running. H waits for as long as M cares to compute.

  (i) Name this pathology, and explain why the scheduler, looking only at
  priorities and run-states, is behaving "correctly" while the system fails —
  what fact is invisible to it? **[2 marks]**

  (ii) Give two distinct remedies, one sentence of mechanism and one drawback
  each. (Lampson and Redell met exactly this in Mesa — their fix is an
  acceptable answer to name, but the mechanism is what earns the mark.)
  **[2 marks]**

---

## Question 2 — I/O and RAID

*(a)* (i) Name the three components of the service time of a single disk
request, and state which of them dominates for small random requests.
**[2 marks]**

  (ii) A driver can learn of request completion by **polling** or by
  **interrupt**. Give one regime where each is clearly the right choice, with
  the reason. **[2 marks]**

*(b)* A disk spins at 12,000 RPM, has a 3.5 ms average seek, and transfers at
100 MB/s.

  (i) Compute the time for one rotation and the average rotational delay.
  **[1 mark]**

  (ii) Compute the service time and the effective throughput for a workload of
  random 4 KB reads. Which term dominates? **[3 marks]**

  (iii) Compute the effective throughput for sequential 100 MB reads (one seek
  and one rotational delay, then pure transfer), and give the
  sequential-to-random throughput ratio to the nearest power of ten. State one
  file-system design decision from this course that this ratio justifies.
  **[2 marks]**

*(c)* An array holds **N = 6** such disks. Let R be one disk's random 4 KB
bandwidth (from (b)). For **RAID-0**, **RAID-1** (mirrored pairs), and
**RAID-5** (rotated parity), derive — with a one-line reason per entry, not
from memory — the capacity (in disks' worth) and the steady-state throughput
for random reads and random writes, in multiples of R. Then explain why
**RAID-4** random writes are stuck at R/2 *no matter how large N grows*, and
what exactly RAID-5 changes to escape that. **[6 marks]**

*(d)* Your service gets this six-bay server. Disks are 4 TB each, and the
service needs **at least 8 TB of usable space**. Operations offers two
configurations: **P** — all six disks in one RAID-5 array; **Q** — four
disks as two RAID-1 mirrored pairs, the remaining two kept as hot spares.
Using your (c) results, give each configuration's usable capacity and its
random-read and random-write throughput in multiples of R (derive the
four-disk figures for Q). Then name the **single measured property of the
write traffic** that decides between them, and state which configuration
wins on each side of it. **[4 marks]**

---

## Question 3 — File systems

*(a)* (i) Name four things an inode stores, and the one obvious thing about a
file that it does **not** store — and where that lives instead. **[2 marks]**

  (ii) Distinguish a **hard link** from a **symbolic link**: what each one is
  on disk, and one failure or restriction each has that the other does not.
  **[2 marks]**

*(b)* A file system has 4 KB blocks and **8-byte** block pointers. An inode
holds **12 direct** pointers, one **single-indirect**, one **double-indirect**
and one **triple-indirect** pointer.

  (i) How many pointers fit in one 4 KB block? Compute the maximum file size
  reachable through (1) the direct pointers, (2) adding the single indirect,
  (3) adding the double indirect, (4) adding the triple indirect. Give exact
  byte counts or exact KiB/MiB/GiB. **[4 marks]**

  (ii) With the file's inode already in memory and no other caching, how many
  disk reads fetch the byte at offset (1) 10,000, (2) 5,000,000,
  (3) 2³¹ (= 2 GiB)? Show which pointer chain each offset resolves through.
  **[3 marks]**

  (iii) What is the largest file for which *every* byte is reachable in at
  most two disk reads (inode in memory)? **[1 mark]**

  (iv) With cold caches (only the superblock in memory), count the disk reads
  performed by `open("/usr/share/dict/words")`, assuming each directory's
  entries fit in one data block. Now suppose every directory has grown to
  span **two** data blocks, and a lookup must in the worst case scan both:
  recount, and give the general formula for a path with `d` directories below
  the root. **[2 marks]**

*(c)* FFS placed inodes with their data and files with their directories,
in cylinder groups — McKusick and colleagues reported throughput an order of
magnitude better than the old file system's few percent of disk bandwidth.

  (i) Explain the two seek costs of the old layout that cylinder-group
  placement removes, and why FFS deliberately **breaks** its own locality rule
  for large files. **[3 marks]**

  (ii) "Our disk stores a mail spool: millions of files, nearly all under
  4 KB, each written once and later read whole. Files that small never
  trigger the large-file exception, so FFS's placement machinery is idle
  complexity — a simple layout that hands out the first free inode and the
  first free blocks would serve just as well." Evaluate. Identify which
  placement decisions still earn their keep on this workload, say what the
  first-free layout comes to cost as the disk fills and ages, and give your
  verdict with the conditions under which the simple layout would in fact be
  fine. **[3 marks]**

---

## Question 4 — Security

*(a)* State the three classic security goals (CIA), one phrase each, and
explain the course's distinction between a security **policy** and a security
**mechanism**, with one OS example of each. **[4 marks]**

*(b)* A build server has subjects **dana** (a developer), **omar** (an
operator), and an unattended monitoring daemon **mon**. Objects are the
release archive `rel` (a file), the tape drive `tape` (a device), and
`deploy` — a setuid program owned by dana that copies the release archive to
tape. Policy: dana may read and write `rel`, may write `tape`, and may run
`deploy`; omar may not access `rel` directly, but may read and write `tape`
and may run `deploy`; mon may read `rel` and nothing else.

  (i) Draw the access matrix, and write out both projections: the per-object
  **ACLs** and the per-subject **capability lists**. **[3 marks]**

  (ii) State which projection makes each of these cheap, and why the asymmetry
  is structural: (1) "revoke omar's right to run `deploy`, now"; (2) "list
  everything omar can touch, for an audit" — and add one sentence on why
  `deploy` being setuid means the cheap answer to (2) still understates
  omar's reach. **[2 marks]**

  (iii) Unix compresses ACLs to owner/group/other × rwx, and checks the
  classes in order — owner, then group, then other — with the **first match
  final**: a user who matches an earlier class never falls through to a later
  class's bits. Express each of the following as Unix permissions plus a
  group definition, or argue it cannot be done: (1) *every user on the
  system may write the shared file `scratch` — except mon, which must have no
  access at all*; (2) *every user on the system may read and write the shared
  log `worklog` — except members of the group `probation`, who may read it
  but must never write it* (the owner, dana, is not on probation).
  **[3 marks]**

*(c)* A site's password file stores one **salted** hash per user (a distinct
random salt each), `N = 2,000` users. An attacker steals it, has a dictionary
of `D = 5 × 10⁷` likely passwords, and computes 10⁹ hashes per second — so
the full attack, every word against every user, costs `N × D = 10¹¹` hashes,
about 100 seconds. Site policy will force every password to be rotated
**30 days** after the theft is discovered. The site switches to a
password-hashing function that iterates the hash `k` times per evaluation.

  (i) Find the smallest `k` — a round figure will do — that pushes the full
  `N × D` attack past the 30-day deadline. **[1 mark]**

  (ii) The rotation deadline protects the *file*; a targeted attacker wants
  **one particular** account. With your `k` from (i), how long does the whole
  dictionary take against that single user — does it beat the deadline? Find
  the `k′` that pushes even the single-user attack past 30 days, and state
  the general relationship between `k′` and `k`. **[2 marks]**

  (iii) What does one legitimate login cost at `k`, and at `k′` — and is
  each still tolerable? Name the *property* of the hashing function that `k`
  embodies, and state the asymmetry that makes the scheme work at all.
  **[1 mark]**

*(d)* A print-accounting daemon on a shared machine: it runs as **root** so it
can read any user's spool files; if its quota-policy file fails to parse it
logs a warning and **allows all jobs**; and it checks a user's permission to
print **once, at job submission**, though jobs may sit queued for hours before
printing. Name the Saltzer–Schroeder-tradition design principle each of the
three behaviours violates, give the concrete attack or failure each invites,
and propose the minimal fix for each. **[4 marks]**

---

*End of Midterm 2.*
