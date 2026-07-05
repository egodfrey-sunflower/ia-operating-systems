#!/usr/bin/env python3
"""Shared xv6-riscv QEMU test harness for Labs 2-6.

This module drives a booted xv6 kernel under QEMU (`make qemu`, -nographic
mode) as a child process, lets a test script send shell commands and match
their output with regexes, and guarantees the whole QEMU process group is
torn down on exit. Stdlib only (Python 3.8+); no pexpect dependency.

--------------------------------------------------------------------------
QUICK START (for Lab 3-6 authors)
--------------------------------------------------------------------------

    from qemuharness import Xv6Session, build, HarnessError

    build(workdir)                      # run `make`, raise on failure
    with Xv6Session(workdir, cpus=1, timeout=120) as sess:
        out = sess.run_cmd("echo hi")   # -> "hi"
        sess.wait_for(r"ALL TESTS PASSED", timeout=300)

The context manager (or close()) ALWAYS kills QEMU, even on exception.

--------------------------------------------------------------------------
API
--------------------------------------------------------------------------

class Xv6Session(workdir, cpus=1, timeout=120, make_target="qemu",
                 quiet=False)
    Boot xv6 from `workdir` (a built-or-buildable xv6 tree). Launches
    `make <make_target> CPUS=<cpus>` in its own process group and spawns a
    background reader thread that accumulates stdout. The constructor blocks
    until the shell prompt is ready (it calls wait_for_boot()); on failure
    it tears QEMU down and raises. `timeout` is the DEFAULT per-operation
    timeout in seconds (each method can override it).

    .wait_for(pattern, timeout=None) -> re.Match
        Block until `pattern` (str or compiled regex, searched with
        re.search over MULTILINE text) appears anywhere in the accumulated
        output. Returns the match object. Raises HarnessTimeout (with the
        tail of output in the message) if the deadline passes, or
        HarnessError if QEMU has died.

    .run_cmd(cmd, timeout=None) -> str
        Send one shell command line, wait for the next `$ ` prompt, and
        return everything the command printed -- with the shell's echo of
        the typed command and the trailing prompt stripped. Carriage
        returns are removed. NOTE: match on command *output*, not the echo.

    .wait_for_boot(timeout=None)
        Wait for `init: starting sh` and the first `$ ` prompt. Called
        automatically by the constructor; safe to call again (no-op once
        the prompt has been seen).

    .send_line(line)     Send raw text + newline (no waiting).
    .send_raw(data)      Send raw bytes/str verbatim (e.g. control chars).
    .output              Property: all accumulated console text so far.
    .close()             Kill the QEMU process group (SIGTERM, then SIGKILL
                         after a short grace). Idempotent -- safe to call
                         twice. Called by __exit__.

build(workdir, timeout=300)
    Run `make` in `workdir`. On non-zero exit raise HarnessError carrying
    the tail of the build output. Returns nothing on success.

--------------------------------------------------------------------------
IMPORTANT xv6 CONSOLE QUIRKS (learned empirically on this course's setup)
--------------------------------------------------------------------------

  * Input sent BEFORE the shell prompt appears is silently EATEN. Always
    wait for the prompt (the constructor does this for you) before sending.
  * The shell ECHOES what you type. run_cmd() strips that echo line, but if
    you use wait_for()/send_line() directly, remember your command text
    will show up in the output -- match on the command's OUTPUT instead.
  * The QEMU console uses `\n` line endings; stray `\r` may appear and is
    stripped by run_cmd().
  * The default prompt is the two characters `$ ` (dollar, space).

--------------------------------------------------------------------------
SELF-TEST
--------------------------------------------------------------------------

    python3 qemuharness.py <xv6-workdir>

Boots the tree, runs `echo harness-ok`, asserts the output, prints OK and
exits 0 (nonzero on any failure). Use it to smoke-test a freshly built
tree.
"""

import os
import re
import signal
import subprocess
import sys
import threading
import time

# The xv6 shell prompt: dollar-space.
PROMPT = "$ "
# Marker printed by init just before it exec's the shell.
SH_BANNER = "init: starting sh"

DEFAULT_TIMEOUT = 120


class HarnessError(Exception):
    """Base class for harness failures (build error, QEMU died, etc.)."""


class HarnessTimeout(HarnessError):
    """Raised when a wait_for/run_cmd deadline elapses."""


def _tail(text, n=2000):
    """Return the last n characters of text, for error messages."""
    if len(text) <= n:
        return text
    return "...(truncated)...\n" + text[-n:]


class Xv6Session:
    """A booted xv6-under-QEMU instance you can drive from Python."""

    def __init__(self, workdir, cpus=1, timeout=DEFAULT_TIMEOUT,
                 make_target="qemu", quiet=False):
        self.workdir = os.path.abspath(workdir)
        self.cpus = cpus
        self.timeout = timeout
        self.quiet = quiet
        self._buf = []            # list of str chunks
        self._buflen = 0
        self._lock = threading.Lock()
        self._closed = False
        self._booted = False
        self._proc = None
        self._reader = None

        cmd = ["make", make_target, "CPUS=%d" % cpus]
        try:
            # start_new_session=True => child is a process-group / session
            # leader, so we can signal the whole group (QEMU + any make shell)
            # with os.killpg and never leak strays.
            self._proc = subprocess.Popen(
                cmd,
                cwd=self.workdir,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                start_new_session=True,
                bufsize=0,
            )
        except OSError as e:
            raise HarnessError("failed to launch %r in %s: %s"
                               % (cmd, self.workdir, e))

        self._reader = threading.Thread(target=self._read_loop, daemon=True)
        self._reader.start()

        try:
            self.wait_for_boot(timeout=timeout)
        except BaseException:
            self.close()
            raise

    # ----- internals -------------------------------------------------------

    def _read_loop(self):
        """Background thread: drain QEMU stdout into the buffer."""
        stream = self._proc.stdout
        try:
            while True:
                chunk = stream.read(1)
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

    # ----- public API ------------------------------------------------------

    @property
    def output(self):
        """All console text accumulated so far."""
        return self._snapshot()

    def wait_for(self, pattern, timeout=None):
        """Block until `pattern` appears in the output; return the match."""
        if timeout is None:
            timeout = self.timeout
        if isinstance(pattern, str):
            rx = re.compile(pattern, re.MULTILINE)
        else:
            rx = pattern
        deadline = time.time() + timeout
        while True:
            data = self._snapshot()
            m = rx.search(data)
            if m:
                return m
            if not self._alive():
                # drain whatever is left, check once more
                time.sleep(0.1)
                data = self._snapshot()
                m = rx.search(data)
                if m:
                    return m
                raise HarnessError(
                    "QEMU exited (code %r) before matching %r.\n"
                    "--- output tail ---\n%s"
                    % (self._proc.poll(), rx.pattern, _tail(data)))
            if time.time() > deadline:
                raise HarnessTimeout(
                    "timed out after %gs waiting for %r.\n"
                    "--- output tail ---\n%s"
                    % (timeout, rx.pattern, _tail(data)))
            time.sleep(0.05)

    def wait_for_boot(self, timeout=None):
        """Wait for init to start the shell and the first prompt to appear."""
        if self._booted:
            return
        if timeout is None:
            timeout = self.timeout
        deadline = time.time() + timeout
        self.wait_for(re.escape(SH_BANNER), timeout=timeout)
        # then the prompt
        remaining = max(1.0, deadline - time.time())
        self.wait_for(re.escape(PROMPT), timeout=remaining)
        self._booted = True

    def send_raw(self, data):
        """Write raw data to QEMU stdin (str encoded utf-8, or bytes)."""
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
        """Send a line of text followed by newline (no waiting)."""
        self.send_raw(line + "\n")

    def run_cmd(self, cmd, timeout=None):
        """Send `cmd`, capture output up to the next prompt, return it.

        The shell's echo of the typed command and the trailing prompt are
        stripped; carriage returns are removed.
        """
        if timeout is None:
            timeout = self.timeout
        if not self._booted:
            self.wait_for_boot(timeout=timeout)
        with self._lock:
            mark = self._buflen
        self.send_line(cmd)
        # Wait for the next prompt to appear in the newly produced output.
        # The shell prints "$ " after the command completes.
        deadline = time.time() + timeout
        while True:
            data = self._snapshot()
            tail = data[mark:]
            # Find a prompt that comes after at least the echoed command.
            # We look for the LAST "$ " in the tail; xv6 prints exactly one
            # prompt per command completion.
            idx = tail.rfind(PROMPT)
            # require a newline before the prompt so we don't catch a "$ "
            # that is part of the echoed command line itself.
            if idx > 0 and ("\n" in tail[:idx]):
                captured = tail[:idx]
                return self._clean_output(captured, cmd)
            if not self._alive():
                raise HarnessError(
                    "QEMU exited (code %r) while running %r.\n"
                    "--- output tail ---\n%s"
                    % (self._proc.poll(), cmd, _tail(data)))
            if time.time() > deadline:
                raise HarnessTimeout(
                    "timed out after %gs running %r.\n"
                    "--- output tail ---\n%s"
                    % (timeout, cmd, _tail(data)))
            time.sleep(0.05)

    @staticmethod
    def _clean_output(captured, cmd):
        """Strip the echoed command line and normalize newlines."""
        text = captured.replace("\r", "")
        lines = text.split("\n")
        # Drop the first line if it is the echo of the command we typed.
        if lines and lines[0].strip() == cmd.strip():
            lines = lines[1:]
        # Also tolerate the echo being prefixed by a prompt fragment.
        elif lines and cmd.strip() and lines[0].strip().endswith(cmd.strip()):
            lines = lines[1:]
        return "\n".join(lines).strip("\n")

    def close(self):
        """Kill the QEMU process group. Idempotent."""
        if self._closed:
            return
        self._closed = True
        proc = self._proc
        if proc is None:
            return
        # Close stdin so QEMU/make notice EOF.
        try:
            if proc.stdin:
                proc.stdin.close()
        except OSError:
            pass
        if proc.poll() is None:
            # Signal the whole process group.
            self._killpg(signal.SIGTERM)
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self._killpg(signal.SIGKILL)
                try:
                    proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    pass
        # Join reader thread.
        if self._reader is not None:
            self._reader.join(timeout=2)

    def _killpg(self, sig):
        try:
            pgid = os.getpgid(self._proc.pid)
            os.killpg(pgid, sig)
        except (ProcessLookupError, OSError):
            # Fall back to killing the process directly.
            try:
                self._proc.send_signal(sig)
            except (ProcessLookupError, OSError):
                pass

    # ----- context manager -------------------------------------------------

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        self.close()
        return False


def build(workdir, timeout=300):
    """Run `make` in workdir; raise HarnessError (with output tail) on fail."""
    workdir = os.path.abspath(workdir)
    try:
        proc = subprocess.run(
            ["make"],
            cwd=workdir,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=timeout,
        )
    except subprocess.TimeoutExpired as e:
        out = e.output.decode("utf-8", "replace") if e.output else ""
        raise HarnessError("build timed out after %gs in %s\n%s"
                           % (timeout, workdir, _tail(out)))
    except OSError as e:
        raise HarnessError("failed to run make in %s: %s" % (workdir, e))
    out = proc.stdout.decode("utf-8", "replace")
    if proc.returncode != 0:
        raise HarnessError(
            "build failed (make exit %d) in %s\n--- build tail ---\n%s"
            % (proc.returncode, workdir, _tail(out)))
    return out


def _self_test(workdir):
    """Boot, run `echo harness-ok`, assert output, return 0/1."""
    print("[selftest] building %s ..." % workdir)
    try:
        build(workdir)
    except HarnessError as e:
        print("[selftest] BUILD FAILED:\n%s" % e)
        return 1
    print("[selftest] booting ...")
    try:
        with Xv6Session(workdir, cpus=1, timeout=90) as sess:
            out = sess.run_cmd("echo harness-ok", timeout=30)
            print("\n[selftest] run_cmd returned: %r" % out)
            if "harness-ok" not in out:
                print("[selftest] FAIL: expected 'harness-ok' in output")
                return 1
    except HarnessError as e:
        print("[selftest] FAIL: %s" % e)
        return 1
    print("[selftest] OK")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.stderr.write("usage: %s <xv6-workdir>\n" % sys.argv[0])
        sys.exit(2)
    sys.exit(_self_test(sys.argv[1]))
