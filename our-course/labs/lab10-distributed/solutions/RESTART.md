# RESTART.md — killing the server mid-workload (model report)

## The experiment

```sh
./fileserver /tmp/exp                                   # port=P
./fileclient P workload name=r n=200 timeout=100 retries=80 delay=5
# ... at "progress phase=write i=103":  kill -9 <server pid>
# ... ~0.4 s later:                     ./fileserver /tmp/exp port=P
```

Observed: the client stalled at write 104, retried into the outage
(`retrans=5` for the whole run), and continued the moment the fresh
server answered. Final line:

```
workload done n=200 wrote=200 verified=200 size=6400 rpcs=402 retrans=5
```

(`retrans` varies 0–5 with where the kill lands relative to the outage.)
exit 0, and the on-disk file is byte-identical to the 200 expected
records. The client cannot tell a dead-and-replaced server from a slow
network; that is the whole demonstration.

## Why it works — three properties, each load-bearing

1. **The offset lives in the request.** Every write says *where*:
   `write(fh, off=3328, 32 bytes)`. The new server process needs no
   memory of how far the workload had got — request 104 is as
   self-contained as request 0.
2. **The handle names the file, not the process.** fh is derived from
   the file itself (the reference uses the inode number, re-resolved
   from the export directory on every request), so the handle issued by
   the dead server is equally meaningful to the new one. Nothing was
   "open".
3. **Every operation is idempotent.** The kill can land *after* a write
   executed but *before* its reply escaped. The client retries; the new
   server executes the same write again; the same 32 bytes land at the
   same offset. Executed once or twice, the file is the same.

One mechanism — retry — then covers all three failures ch. 49 lists: a
lost request (nothing happened; retry does it), a lost reply (it
happened; retry harmlessly re-does it), and a crashed server (both of
the above, plus a new process that properties 1–2 make interchangeable
with the old one).

## The counterfactual

Keep a server-side cursor instead of per-request offsets and the restart
is no longer invisible: the new process's cursor is 0, the remaining 96
writes land on top of records 0–95, and the client "succeeds" its way to
a corrupt file — the failure is silent until the read-back. Make the
handle a table index and it is *loud* instead: every post-restart
request answers ESTALE, because the handle referred to the process, not
the file. And make one operation non-idempotent — `append` is the
classic — and the lost-reply case alone breaks it, no restart needed:
the retried append doubles its record. Statelessness is not an
implementation nicety; it is what makes "retry" a complete answer.
