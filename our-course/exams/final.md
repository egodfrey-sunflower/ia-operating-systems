# Operating Systems — Final Examination

**Time allowed: THREE hours.**

**Question 1 is COMPULSORY.** Answer Question 1 **and THREE of Questions
2–7** — four answers in total. All questions carry the same number of marks
(20). Question 1 is a single transplanted Tripos question, sat under the
proper IA time allocation (about 35 minutes).

Closed book. Write your answers by hand. Where a calculation is asked for,
show your working — a correct number with no derivation earns little credit.
Diagrams are encouraged wherever they make an argument shorter.

The topic coverage, by design, spans the whole course: (i) Unix & shell
internals, (ii) crash consistency, (iii) concurrency, (iv) virtualization &
kernel architecture, (v) design synthesis, (vi) memory & I/O. CPU scheduling
is assessed by the two midterms (and possibly by the compulsory Question 1),
not by this paper's own optional pool; likewise protection theory reaches this
paper only through Question 1 or the essay prompts. The compulsory Question 1
anchors every script to the IA core; Question 7 tests memory and I/O directly.

---

## Question 1 — Tripos transplant *(COMPULSORY, 20 marks)*

Answer the following past-paper question from `../../cambridge-course/exam_questions/` under
exam conditions:

- **y2026p2q3**

Attempt it exactly as printed, and allocate your time as for any 20-mark
question (about 35 minutes — the proper Tripos allocation). This question is
marked against the official Part IA standard.

> Its sealed partner **y2026p2q4** is **not** part of this paper; it is sat
> separately as the week-17 timed standalone mock (see `README.md`).

---

## Question 2 — Unix case study: the shell, and versioned snapshots

*(a)* The Unix process model splits process creation into two system calls,
`fork()` and `execve()`, rather than providing a single "spawn a program" call.

  *(i)* Describe what `fork()` and `execve()` each do, and explain what the
  `wait()`/`waitpid()` call adds that makes the three usable together as the
  shell's core loop.

  *(ii)* When you type `Ctrl-C` at an interactive shell, the foreground program
  dies but the shell survives and prompts again. Explain, in terms of
  **process groups** and the **controlling terminal**, how the signal reaches
  exactly the right processes.                                     **[5 marks]**

*(b)* A user runs the following command line at an interactive shell:

```
grep -v '^#' conf.txt | sort | uniq -c > out.txt
```

  *(i)* List, **in order**, the system calls the shell makes to set this up and
  run it, giving the **file-descriptor numbers** involved at each step. State
  clearly the convention you assume for descriptor allocation, and identify the
  three children and what each has on descriptors 0, 1 and 2 at the moment it
  calls `execve()`. Note explicitly what the shell must do with the pipe
  descriptors in the *parent* after forking, and say what goes wrong — and for
  **which** process — if any pipe descriptor is left open where it should not
  be.                                                              **[6 marks]**

  *(ii)* A colleague wants both output streams of a command in one file and
  cannot decide between `cmd 2>&1 > log.txt` and `cmd > log.txt 2>&1`. Exactly
  one does what she wants. Using the same fd-level operations as in *(i)*, show
  what each ordering leaves on descriptors 1 and 2, and state where each
  variant's stderr actually ends up.                               **[2 marks]**

*(c)* A system administrator builds a "versioned" file system. Every night at
01:00 a script recursively "copies" the live tree to a new directory
`/snap-YYYY-MM-DD`, but to save space it makes **hard links** to the existing
files instead of real copies; the newest snapshot is then used as the working
root. The aim is to let users recover from mistaken edits, accidental
deletions, and disk failure.

  *(i)* Identify **three** distinct ways in which this scheme fails to deliver
  what the administrator wants. For each, say which of the three goals
  (mistaken edits / deletions / disk failure) it undermines, and give the
  precise **inode-level mechanism** by which it bites — what exactly is shared
  between the snapshot's name and the live name, and which operation therefore
  propagates where it should not. Naming a flaw without its mechanism earns
  only partial credit.

  *(ii)* For **each** flaw you identified, explain how a modern
  copy-on-write file system (e.g. ZFS or btrfs) avoids it. Your answer should
  make clear *what mechanism* does the work — which structures are copied,
  which are shared, and at what moment — not merely that "CoW fixes it".
                                                                   **[7 marks]**

---

## Question 3 — Crash consistency: journaling and the log-structured alternative

*(a)* *(i)* Distinguish **data journaling** (`data=journal`) from **ordered
metadata journaling** (`data=ordered`, the ext3/ext4 default) in one or two
sentences each.

  *(ii)* After a crash, `fsck` (without a journal) scans the whole file system.
  Give **two** classes of inconsistency it *can* repair, and **two** kinds of
  damage it *cannot* repair, and say in one sentence why journaling makes
  recovery both faster and stronger.                               **[5 marks]**

*(b)* A process appends **8 new data blocks** to an existing file. Doing so
also dirties the file's **inode** (new size and block pointers) and **one data
bitmap block** (marking the 8 blocks allocated). Assume every journal
transaction is bracketed by a **descriptor block** and a **commit block**, and
that a disk barrier is issued where correctness requires ordering.

  *(i)* List, **in order**, the disk block writes performed under **data
  journaling**, and separately under **ordered metadata journaling**. Group
  them into (journal writes) and (checkpoint / in-place writes) and mark where
  a barrier is required.

  *(ii)* Give the **total number of block writes** for each mode, and hence the
  **write amplification** (physical blocks written per useful data block). By
  how many blocks do the two totals differ, and what single fact explains that
  difference?                                                      **[8 marks]**

*(c)* A log-structured file system (LFS) writes *all* new data and metadata by
appending to a sequential log.

  *(i)* Explain how LFS performs a **read** of an existing file (how does it
  find an inode that keeps moving?), how it performs a **write**, and what the
  **segment cleaner** must do and why it is necessary.

  *(ii)* Name one workload on which LFS clearly **beats** FFS and one on which
  it clearly **loses**, and justify each in terms of the mechanics from
  *(c)(i)*. In your losing case, explain what the disk's **utilisation** has to
  do with the cost.                                                **[7 marks]**

---

## Question 4 — Concurrency II: condition variables, deadlock, RCU

*(a)* State the **four** conditions that must all hold for deadlock to be
possible. A kernel then adopts a **global lock-ordering discipline**: every
lock is assigned a fixed rank, and a thread may acquire a lock only if its rank
exceeds that of every lock the thread already holds. Which **single** condition
does this discipline deny, and why exactly can it then never be satisfied?
Explain why the **other three** conditions still hold in such a kernel — and
why that does not matter.                                          **[4 marks]**

*(b)* The following implements a **frame allocator** for a paging system:
`alloc(n)` blocks until it can claim `n` free frames, and `release(n)` returns
`n` frames. Many threads call `alloc` concurrently, with **different** values
of `n`. `cond_wait(cv, m)` atomically releases mutex `m`, blocks on `cv`, and
re-acquires `m` before returning (**Mesa** semantics).

```c
int    avail = 64;                  /* free frames */
mutex  m;
cond   cv;

void alloc(int n) {                 /* 1 <= n <= 64 */
    lock(&m);
    while (avail < n)               /* line A1 */
        cond_wait(&cv, &m);
    avail -= n;
    unlock(&m);
}

void release(int n) {
    lock(&m);
    avail += n;
    cond_signal(&cv);               /* line R1 */
    unlock(&m);
}
```

  *(i)* This code is subtly wrong: it can leave threads blocked **forever**
  even though enough frames are free to satisfy at least one of them. Identify
  the bug, and give a concrete interleaving — with specific values of `n` —
  that wedges the system in that state. **[4 marks]**

  *(ii)* Give the one-word fix, and explain why it is correct even though it
  wakes threads whose predicate is still false — which line makes that
  harmless? Would the **original** code be correct if every caller used the
  same fixed `n`? Justify. **[4 marks]**

*(c)* A system has three resource types **A, B, C** with totals **(12, 6, 8)**.
The current state is:

| Process | Allocation (A B C) | Maximum (A B C) |
|:-------:|:------------------:|:---------------:|
| P0      | 1 0 2              | 5 2 4           |
| P1      | 2 1 0              | 4 3 3           |
| P2      | 4 0 3              | 9 1 5           |
| P3      | 0 2 1              | 1 4 2           |
| P4      | 1 1 0              | 3 3 6           |

  *(i)* Compute the **Available** vector and each process's **Need**, and show
  that the current state is **safe** by giving a safe sequence.

  *(ii)* Two requests arrive (consider each **independently**, from the state
  above): request **X** is P3 asking for **(1, 2, 1)**; request **Y** is P1
  asking for **(1, 1, 1)**. Using the banker's algorithm, determine for each
  whether it can be granted. Exactly one is safe — grant it and show the safe
  sequence; for the other, show why no safe sequence exists.       **[5 marks]**

*(d)* Read-copy-update (RCU) lets readers traverse a shared structure with **no
locks and no atomic writes** on the read side. Explain how it achieves this —
what a *writer* does to publish an update, and what a **grace period** is for —
and hence what RCU trades away to buy cheap reads.                 **[3 marks]**

---

## Question 5 — Virtualization: trap-and-emulate, paging, containers

*(a)* State the **Popek–Goldberg** criterion for a machine to be classically
virtualizable by trap-and-emulate, defining *sensitive* and *privileged*
instructions. Give one concrete reason the 32-bit x86 architecture failed this
test, and name one hardware feature later added to fix it.         **[5 marks]**

*(b)* A guest runs under a hypervisor. The guest uses a radix (multi-level)
page table of depth **d = 4**; the host's table has depth **h = 3**. Assume
the TLB is empty (every translation is a full walk).

  *(i)* Under **shadow paging**, the VMM maintains a single page table mapping
  guest-virtual directly to host-physical. How many memory accesses does the
  hardware make to service one TLB miss? Explain.

  *(ii)* Under **nested paging** (two-dimensional walk), every guest-physical
  address encountered during the guest walk must itself be translated through
  the host table. Derive a general formula in terms of `d` and `h` for the
  number of memory accesses per TLB miss, and evaluate it for the machine
  above (`d = 4`, `h = 3`). Then state, from your formula, which is more
  expensive at the margin on this machine — adding one level of **guest**
  depth or one level of **host** depth — and explain structurally why (what
  work does each extra level of each kind add to the walk?).

  *(iii)* State, with reasons, the cost that **shadow** paging pays in exchange
  for its cheaper walk — i.e. why nested paging exists at all despite the number
  in *(ii)*.                                                       **[8 marks]**

*(c)* A container (as built in the containers lab) isolates a process using
several independent Linux kernel mechanisms.

  *(i)* Name the mechanism responsible for **each** of: (1) isolating the set of
  visible processes and the hostname; (2) giving the container its own root
  filesystem image; (3) capping its memory and CPU. One sentence each.

  *(ii)* Give **two** things a container **cannot** isolate that a full virtual
  machine can, and explain the underlying reason (what is *shared* that makes it
  impossible).                                                     **[7 marks]**

---

## Question 6 — Design / synthesis essay

Answer **ONE** of the following. A good answer takes a position and defends it
with specific evidence from named systems and their trade-offs; a list of
facts without judgement will not reach the upper band.

**Either (A):**
> "The history of operating-system structure is a pendulum: functionality
> swings **into** the kernel for performance and **out** of it for safety —
> and it never stops moving." Trace the pendulum through **monolithic Unix**,
> **microkernels** (Mach → L4 → seL4), **exokernels**, **unikernels**, and
> **eBPF**, identifying for each what moved, in which direction, and what
> force (performance cost or safety/TCB argument) pushed it. Then argue where
> the pendulum will come to rest — or make the case that it never will. Your
> answer should engage with at least one development that complicates the
> simple in/out story.

**Or (B):**
> Design an operating system for **one** of the following unusual targets, and
> justify each significant departure from the conventional Unix design:
> **(i)** a machine with **thousands of cores**; **(ii)** a microcontroller with
> **no MMU**; or **(iii)** a **safety-critical controller that must be formally
> verified**. Be specific about which Unix assumptions you keep, which you
> discard, and why — and name at least one real system or research project that
> informs your choices.

**[20 marks]**

---

## Question 7 — Memory & I/O

*(a)* A 64-bit machine implements a **48-bit** byte-addressed virtual address
space with **4 KiB** pages and **8-byte** page-table entries; each page table
occupies exactly one page.

  *(i)* Explain why a single-level (linear) page table is untenable for this
  address space, and why a multi-level (radix) table fixes the problem. What
  does the TLB contribute — i.e. what would the design below cost without one?

  *(ii)* Show that the 48-bit address splits into a 12-bit offset and **four**
  9-bit indices, i.e. that the table must have exactly four levels.

  *(iii)* A memory access takes **80 ns**. On a TLB hit, translation is free
  and a reference costs one memory access. On a TLB miss, the hardware first
  walks the four-level table (**four additional memory accesses**) and then
  makes the reference. With a TLB hit rate of **95%**, compute the **effective
  memory-access time**. What hit rate would be needed to bring the effective
  access time down to at most **90 ns**?                           **[7 marks]**

*(b)* A process with **3** allocated frames (initially empty; every first touch
faults) issues the page-reference string:

```
1  2  3  4  2  1  5  2  4  1
```

  *(i)* Trace the string under **LRU** and under **OPT** (Belady's optimal),
  showing the frame contents after every reference and marking each fault.
  Give both fault counts.

  *(ii)* Define the **working set** W(t, Δ) and compute it, with **Δ = 4**,
  immediately after the **5th** and after the **7th** reference. What does the
  change in its size tell the OS about this process's frame demand, given its
  **3** allocated frames?

  *(iii)* Define **thrashing**, explain how too high a degree of
  multiprogramming causes it, and give one action the OS can take in response.
                                                                   **[7 marks]**

*(c)* Two storage devices are available. The **hard disk** has average seek
time **5 ms**, spins at **6000 RPM**, and transfers at a sustained
**128 MB/s** (1 MB = 10⁶ bytes). The **SSD** services a random **4 KiB** read
in **50 µs** end-to-end.

  *(i)* Compute the average service time for one random 4 KiB read on the hard
  disk (seek + rotational latency + transfer — state why the rotational term
  is half a revolution), and hence each device's random-read **IOPS** and the
  ratio between them.

  *(ii)* A database needs a sustained **5000 random 4 KiB reads per second**.
  Using your figures, how many hard disks would be required, versus how many
  SSDs? Then explain **(1)** why an elevator (SCAN) I/O scheduler improves the
  hard disk's throughput but buys essentially nothing on the SSD, and
  **(2)** one benefit and one risk of **write-behind** buffering in the I/O
  path serving this database.                                      **[6 marks]**

---

*End of paper.*
