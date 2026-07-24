#!/usr/bin/env python3
"""Shared xv6-riscv QEMU driver for the kernel labs (Lab 2 onward).

Boots a built xv6 tree under QEMU in -nographic mode as a child process,
lets a test script type shell commands and scrape the console, and
guarantees the emulator is torn down. Standard library only.

    from qemuharness import Xv6Session, build, clean, HarnessError, sweep

    build(workdir, ["POLICY=lottery"])
    with Xv6Session(workdir, cpus=1, policy="lottery") as s:
        print(s.run_cmd("echo hi"))          # -> "hi"
    sweep()                                   # assert nothing survived

Three things this adds over "just run QEMU":

  ONE AT A TIME. Opening a second Xv6Session while one is live raises.
  Each emulator holds 128 MB; two of them on a small machine is how a test
  run takes the machine down rather than the other way round.

  A PANIC IS A RESULT. A kernel that panics prints "panic: ..." and then
  spins forever, so a harness that only waits for a prompt reports a
  timeout minutes later. Every wait watches for panic text and fails
  immediately with the console tail. The same mechanism catches one thing
  a kernel would otherwise fail at silently: a scheduler that selects no
  process executes wfi and goes quiet, so the Lab 2 kernel's scheduler()
  looks for RUNNABLE processes before idling and prints "sched: nothing
  chosen but N runnable" if it finds any. That is FATAL_PATTERNS' second
  entry and it raises SchedulerStall. A kernel without the watchdog never
  matches it and behaves exactly as before.

  NOTHING SURVIVES. QEMU is started in its own process group and the group
  is signalled on close, on exception, at interpreter exit, and on SIGINT,
  SIGTERM and SIGHUP -- that last set matters because Python does NOT run
  atexit handlers for a process killed by a signal, so a run that is
  Ctrl-C'd or wrapped in `timeout` would otherwise leave the emulator, and
  its 128 MB, behind. sweep() is the belt to those braces: it reports any
  qemu-system-riscv64 left anywhere on the machine and kills the ones this
  process started.

CONSOLE QUIRKS, learned the hard way:

  * Input sent before the shell prompt appears is silently eaten. The
    constructor waits for the prompt; do not send before it returns.
  * The shell echoes what you type. run_cmd() strips the echo, but if you
    use send_line()/wait_for() directly, match on the command's OUTPUT --
    matching on text you just typed always succeeds.
  * Line endings are '\\n' with stray '\\r'; run_cmd() strips them.
  * The prompt is the two characters "$ ".
"""

import atexit
import os
import re
import signal
import subprocess
import sys
import threading
import time

PROMPT = "$ "
SH_BANNER = "init: starting sh"

DEFAULT_TIMEOUT = 120

# Console text that means "this kernel is not going to answer". Checked on
# every wait so a broken kernel fails in seconds with a legible message
# rather than at the far end of a timeout.
#
# Deliberately just the one pattern, and deliberately NOT anchored to a line
# start: a panic raised while a user program is halfway through a printf
# appears mid-line, and ^panic: with MULTILINE would miss it. Nothing else in
# the console stream contains the text "panic: ", so this cannot fire falsely.
#
_live_session = None          # at most one Xv6Session at a time
_our_pgids = set()            # process groups we created, for sweep()


class HarnessError(Exception):
    """Build failure, QEMU died, kernel panicked, ..."""


class HarnessTimeout(HarnessError):
    """A wait_for/run_cmd deadline elapsed."""


class KernelPanic(HarnessError):
    """The kernel printed panic text."""


class SchedulerStall(HarnessError):
    """The kernel said it had runnable processes and chose none of them."""


# The second pattern is not something stock xv6 prints. It comes from the
# watchdog at the bottom of scheduler()'s idle branch in the Lab 2 kernel,
# which looks for RUNNABLE processes before it executes wfi and says so if it
# finds any. Without it a scheduler that chooses nobody is SILENT -- the core
# executes wfi and the console goes quiet -- and the only thing that notices
# is a deadline minutes later. A kernel without that watchdog simply never
# matches this, and nothing else here changes.
FATAL_PATTERNS = [
    (re.compile(r"panic: .*"), "the kernel panicked", KernelPanic),
    (re.compile(r"sched: nothing chosen but \d+ runnable"),
     "the scheduler chose no process while processes were runnable",
     SchedulerStall),
]


def _tail(text, n=2500):
    if len(text) <= n:
        return text
    return "...(truncated)...\n" + text[-n:]


# ---------------------------------------------------------------------------
# building
# ---------------------------------------------------------------------------

def _run_make(workdir, args, timeout):
    try:
        proc = subprocess.run(
            ["make"] + list(args),
            cwd=os.path.abspath(workdir),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=timeout,
        )
    except subprocess.TimeoutExpired as e:
        out = e.output.decode("utf-8", "replace") if e.output else ""
        raise HarnessError("make %s timed out after %gs in %s\n%s"
                           % (" ".join(args), timeout, workdir, _tail(out)))
    except OSError as e:
        raise HarnessError("failed to run make in %s: %s" % (workdir, e))
    return proc.returncode, proc.stdout.decode("utf-8", "replace")


def clean(workdir, timeout=120):
    """`make clean`. Ignores failure -- a tree that never built is clean."""
    _run_make(workdir, ["clean"], timeout)


def build(workdir, makeargs=(), timeout=300):
    """`make <makeargs>` in workdir. Raise HarnessError with the tail on fail.

    NOTE the deliberate absence of -j. These labs are built on a two-core
    machine that also has to run QEMU; a parallel make buys seconds and
    costs the whole run when it swaps.
    """
    rc, out = _run_make(workdir, list(makeargs), timeout)
    if rc != 0:
        raise HarnessError(
            "build failed (make %s exited %d) in %s\n--- build tail ---\n%s"
            % (" ".join(makeargs), rc, workdir, _tail(out)))
    return out


# ---------------------------------------------------------------------------
# the session
# ---------------------------------------------------------------------------

class Xv6Session:
    """A booted xv6-under-QEMU instance you can type at."""

    def __init__(self, workdir, cpus=1, timeout=DEFAULT_TIMEOUT,
                 policy=None, makeargs=(), quiet=True, echo_prefix="    | "):
        global _live_session
        if _live_session is not None:
            raise HarnessError(
                "an Xv6Session is already open (%s). One QEMU at a time: "
                "close the first one before booting another."
                % _live_session.workdir)

        self.workdir = os.path.abspath(workdir)
        self.cpus = cpus
        self.timeout = timeout
        self.policy = policy
        self.quiet = quiet
        self.echo_prefix = echo_prefix
        self._buf = []
        self._buflen = 0
        self._lock = threading.Lock()
        self._closed = False
        self._booted = False
        self._proc = None
        self._reader = None
        self._pgid = None

        cmd = ["make", "qemu", "CPUS=%d" % cpus] + list(makeargs)
        try:
            # start_new_session => the child leads its own process group, so
            # killpg reaches QEMU and the make shell that spawned it.
            self._proc = subprocess.Popen(
                cmd, cwd=self.workdir,
                stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                start_new_session=True, bufsize=0,
            )
        except OSError as e:
            _live_session = None
            raise HarnessError("failed to launch %r in %s: %s"
                               % (cmd, self.workdir, e))

        _live_session = self
        try:
            self._pgid = os.getpgid(self._proc.pid)
            _our_pgids.add(self._pgid)
        except OSError:
            pass

        self._reader = threading.Thread(target=self._read_loop, daemon=True)
        self._reader.start()

        try:
            self.wait_for_boot(timeout=timeout)
            if policy is not None:
                # Guard against grading the wrong kernel: this line comes
                # from the compile-time flag, so it proves the build, not
                # the student's scheduler.
                self.wait_for(r"^sched: policy=%s$" % re.escape(policy),
                              timeout=5)
        except BaseException:
            self.close()
            raise

    # ----- internals -------------------------------------------------------

    def _read_loop(self):
        # os.read on the raw fd, not stream.read(n): a buffered read of n
        # blocks until it has n bytes, which would hide a prompt that is the
        # last thing the kernel says. os.read returns whatever has arrived.
        fd = self._proc.stdout.fileno()
        try:
            while True:
                chunk = os.read(fd, 4096)
                if not chunk:
                    break
                text = chunk.decode("utf-8", "replace")
                with self._lock:
                    self._buf.append(text)
                    self._buflen += len(text)
                if not self.quiet:
                    sys.stdout.write(text)
                    sys.stdout.flush()
        except (OSError, ValueError):
            pass

    def _snapshot(self):
        with self._lock:
            return "".join(self._buf)

    def _alive(self):
        return self._proc is not None and self._proc.poll() is None

    def _check_fatal(self, data):
        for rx, why, exc in FATAL_PATTERNS:
            m = rx.search(data)
            if m:
                raise exc("%s: %s\n--- console tail ---\n%s"
                          % (why, m.group(0).strip(), _tail(data)))

    # ----- public API ------------------------------------------------------

    @property
    def output(self):
        return self._snapshot()

    def wait_for(self, pattern, timeout=None):
        """Block until `pattern` appears in the console text; return the match.

        Raises KernelPanic the moment panic text appears, HarnessError if
        QEMU exits, HarnessTimeout at the deadline. All three carry the tail
        of the console so a failure report says what the kernel was doing.
        """
        if timeout is None:
            timeout = self.timeout
        rx = re.compile(pattern, re.MULTILINE) if isinstance(pattern, str) \
            else pattern
        deadline = time.time() + timeout
        while True:
            data = self._snapshot()
            m = rx.search(data)
            if m:
                return m
            self._check_fatal(data)
            if not self._alive():
                time.sleep(0.15)
                data = self._snapshot()
                m = rx.search(data)
                if m:
                    return m
                self._check_fatal(data)
                raise HarnessError(
                    "QEMU exited (code %r) before matching %r\n"
                    "--- console tail ---\n%s"
                    % (self._proc.poll(), rx.pattern, _tail(data)))
            if time.time() > deadline:
                raise HarnessTimeout(
                    "timed out after %gs waiting for %r\n"
                    "--- console tail ---\n%s"
                    % (timeout, rx.pattern, _tail(data)))
            time.sleep(0.05)

    def wait_for_boot(self, timeout=None):
        if self._booted:
            return
        if timeout is None:
            timeout = self.timeout
        deadline = time.time() + timeout
        self.wait_for(re.escape(SH_BANNER), timeout=timeout)
        self.wait_for(re.escape(PROMPT),
                      timeout=max(1.0, deadline - time.time()))
        self._booted = True

    def send_raw(self, data):
        if self._closed or not self._alive():
            raise HarnessError("cannot send: QEMU is not running")
        if isinstance(data, str):
            data = data.encode("utf-8")
        try:
            self._proc.stdin.write(data)
            self._proc.stdin.flush()
        except (OSError, ValueError) as e:
            raise HarnessError("failed to send to QEMU: %s" % e)

    def send_line(self, line):
        self.send_raw(line + "\n")

    def run_cmd(self, cmd, timeout=None):
        """Type one command line; return everything it printed.

        The shell's echo of the command and the trailing prompt are removed;
        carriage returns are stripped.
        """
        if timeout is None:
            timeout = self.timeout
        if not self._booted:
            self.wait_for_boot(timeout=timeout)
        with self._lock:
            mark = self._buflen
        self.send_line(cmd)
        deadline = time.time() + timeout
        while True:
            data = self._snapshot()
            tail = data[mark:]
            idx = tail.rfind(PROMPT)
            # Require a newline before the prompt, so the "$ " inside the
            # echoed command line is not mistaken for the next prompt.
            if idx > 0 and "\n" in tail[:idx]:
                return self._clean_output(tail[:idx], cmd)
            self._check_fatal(data)
            if not self._alive():
                raise HarnessError(
                    "QEMU exited (code %r) while running %r\n"
                    "--- console tail ---\n%s"
                    % (self._proc.poll(), cmd, _tail(data)))
            if time.time() > deadline:
                raise HarnessTimeout(
                    "timed out after %gs running %r\n"
                    "--- console tail ---\n%s"
                    % (timeout, cmd, _tail(data)))
            time.sleep(0.05)

    @staticmethod
    def _clean_output(captured, cmd):
        text = captured.replace("\r", "")
        lines = text.split("\n")
        if lines and lines[0].strip() == cmd.strip():
            lines = lines[1:]
        elif lines and cmd.strip() and lines[0].strip().endswith(cmd.strip()):
            lines = lines[1:]
        return "\n".join(lines).strip("\n")

    def close(self):
        """Kill the QEMU process group. Idempotent; safe from __del__ paths."""
        global _live_session
        if self._closed:
            return
        self._closed = True
        if _live_session is self:
            _live_session = None
        proc = self._proc
        if proc is None:
            return
        try:
            if proc.stdin:
                proc.stdin.close()
        except OSError:
            pass
        if proc.poll() is None:
            self._killpg(signal.SIGTERM)
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self._killpg(signal.SIGKILL)
                try:
                    proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    pass
        # Killing the process group kills `make` for certain and QEMU almost
        # always. Almost is not good enough at 128 MB a copy, so look: any
        # qemu-system-riscv64 still in our process group gets SIGKILL by pid.
        _kill_qemus_in_pgid(self._pgid)
        if self._reader is not None:
            self._reader.join(timeout=2)
        # _our_pgids deliberately keeps this pgid for the life of the
        # process: sweep() uses it to tell a QEMU this run started from one
        # that belonged to the machine before we got here.

    def _killpg(self, sig):
        try:
            os.killpg(self._pgid if self._pgid is not None
                      else os.getpgid(self._proc.pid), sig)
        except (ProcessLookupError, OSError):
            try:
                self._proc.send_signal(sig)
            except (ProcessLookupError, OSError):
                pass

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        self.close()
        return False


# ---------------------------------------------------------------------------
# stray control
# ---------------------------------------------------------------------------

def running_qemus():
    """pids of every qemu-system-riscv64 on the machine, ours or not.

    Matched on argv[0] only. Matching anywhere in the command line finds
    every shell that merely MENTIONS qemu -- including the one that invoked
    this harness -- and a stray check that fires on itself is worse than no
    stray check at all.
    """
    pids = []
    for name in os.listdir("/proc"):
        if not name.isdigit():
            continue
        try:
            with open("/proc/%s/cmdline" % name, "rb") as f:
                argv0 = f.read().split(b"\0")[0].decode("utf-8", "replace")
        except OSError:
            continue
        if os.path.basename(argv0) == "qemu-system-riscv64":
            pids.append(int(name))
    return pids


def _kill_qemus_in_pgid(pgid, tries=20, delay=0.1):
    """SIGKILL every qemu-system-riscv64 in `pgid`, and wait for them to go."""
    if pgid is None:
        return
    for _ in range(tries):
        left = []
        for pid in running_qemus():
            try:
                if os.getpgid(pid) == pgid:
                    left.append(pid)
            except OSError:
                continue
        if not left:
            return
        for pid in left:
            try:
                os.kill(pid, signal.SIGKILL)
            except OSError:
                pass
        time.sleep(delay)


def sweep(verbose=True):
    """Kill any QEMU still in a process group we created; report the rest.

    Returns the list of pids that were still running and are NOT ours --
    those belong to something else on the machine and are not killed here.
    """
    if _live_session is not None:
        _live_session.close()
    ours, theirs = [], []
    for pid in running_qemus():
        try:
            pgid = os.getpgid(pid)
        except OSError:
            continue
        (ours if pgid in _our_pgids else theirs).append(pid)
    for pid in ours:
        try:
            os.kill(pid, signal.SIGKILL)
        except OSError:
            pass
    if verbose and ours:
        print("qemuharness: killed %d stray QEMU process(es): %s"
              % (len(ours), " ".join(str(p) for p in ours)), file=sys.stderr)
    if verbose and theirs:
        print("qemuharness: WARNING, %d qemu-system-riscv64 process(es) not "
              "started by this run are alive: %s"
              % (len(theirs), " ".join(str(p) for p in theirs)),
              file=sys.stderr)
    return theirs


atexit.register(lambda: sweep(verbose=False))


def _install_signal_sweep():
    """Sweep on the signals that would otherwise leave QEMU running.

    atexit is not enough, and finding that out costs a machine. Python runs
    atexit handlers on a normal exit and on an uncaught exception; it does
    NOT run them when the process is killed by a signal. So a grader run
    that is Ctrl-C'd, or wrapped in `timeout`, or killed by a supervising
    script, dies without tearing down the emulator -- which then holds its
    128 MB until somebody notices.

    Each handler tears down and then re-raises the signal through the
    default disposition, so the exit status still says what killed us.
    """
    def handler(signum, frame):
        try:
            sweep(verbose=False)
        finally:
            signal.signal(signum, signal.SIG_DFL)
            os.kill(os.getpid(), signum)

    for sig in (signal.SIGTERM, signal.SIGINT, signal.SIGHUP):
        try:
            if signal.getsignal(sig) in (signal.SIG_DFL, signal.default_int_handler):
                signal.signal(sig, handler)
        except (ValueError, OSError, AttributeError):
            # Not the main thread, or the platform has no such signal.
            pass


_install_signal_sweep()


# ---------------------------------------------------------------------------
# self-test
# ---------------------------------------------------------------------------

def _self_test(workdir):
    print("[selftest] building %s ..." % workdir)
    try:
        build(workdir)
    except HarnessError as e:
        print("[selftest] BUILD FAILED:\n%s" % e)
        return 1
    print("[selftest] booting ...")
    try:
        with Xv6Session(workdir, cpus=1, timeout=90) as s:
            out = s.run_cmd("echo harness-ok", timeout=30)
            print("[selftest] run_cmd returned: %r" % out)
            if "harness-ok" not in out:
                print("[selftest] FAIL: expected 'harness-ok'")
                return 1
    except HarnessError as e:
        print("[selftest] FAIL: %s" % e)
        return 1
    finally:
        sweep()
    print("[selftest] OK")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.stderr.write("usage: %s <xv6-workdir>\n" % sys.argv[0])
        sys.exit(2)
    sys.exit(_self_test(sys.argv[1]))
