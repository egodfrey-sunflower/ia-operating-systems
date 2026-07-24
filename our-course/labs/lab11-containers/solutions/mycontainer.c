/* mycontainer.c -- a container runtime in miniature. REFERENCE SOLUTION.
 *
 * Assembles a container from primitives: a process placed in its own user,
 * UTS, PID, mount and network namespaces (Parts 1, 2, 4), given a private root
 * with pivot_root (Part 3), and -- when asked -- confined by a cgroup v2 with
 * memory and pid limits (Part 5). There is no kernel "container" object here;
 * there is a process with several independent restrictions applied to it.
 *
 *   mycontainer [--mem BYTES] [--pids N] [--no-userns] ROOTFS CMD [ARGS...]
 *
 *   --mem BYTES   create a cgroup and set memory.max (+ memory.swap.max=0)
 *   --pids N      create a cgroup and set pids.max
 *   --no-userns   skip the user namespace (needs real root; for the privileged
 *                 tier / TIERS.md comparison)
 *
 * Build:  cc -Wall -Wextra -O2 -o mycontainer mycontainer.c
 *
 * The ordering here is not arbitrary -- several steps only work in one place:
 *   - clone() gets a real child stack (a NULL stack segfaults instantly);
 *   - the uid/gid maps are written by the PARENT, and setgroups must be denied
 *     before gid_map will accept a write;
 *   - the child waits for the parent to finish maps+cgroup before it execs;
 *   - pivot_root needs the new root to be a mount point and needs mount
 *     propagation made private first;
 *   - the PID namespace only renumbers the child, so the isolated process is
 *     the clone()d child, not this process.
 */
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>

#define STACK_SIZE (1024 * 1024)

/* glibc has no pivot_root wrapper; call it directly. */
static int pivot_root(const char *new_root, const char *put_old) {
	return syscall(SYS_pivot_root, new_root, put_old);
}

struct config {
	char  *rootfs;
	char **cmd;        /* NULL-terminated argv for the container command */
	long   mem_max;    /* bytes, or -1 for unset */
	long   pids_max;   /* count, or -1 for unset */
	int    userns;     /* 1 = create a user namespace */
	int    syncfd;     /* child reads this; parent closes it to release child */
};

static void die(const char *msg) { perror(msg); exit(1); }

/* For clean teardown if mycontainer itself is interrupted. */
static volatile pid_t g_child;
static char           g_cg[512];

static void on_signal(int sig) {
	if (g_child > 0) {
		int s;
		kill(g_child, SIGKILL);
		waitpid(g_child, &s, 0);   /* reap it: the pid namespace, and every
		                              process in it, is gone once this returns */
	}
	if (g_cg[0]) rmdir(g_cg);      /* now empty */
	_exit(128 + sig);
}

static void write_file(const char *path, const char *val) {
	int fd = open(path, O_WRONLY);
	if (fd < 0) { fprintf(stderr, "open %s: %s\n", path, strerror(errno)); exit(1); }
	if (write(fd, val, strlen(val)) < 0) {
		fprintf(stderr, "write %s <- %s: %s\n", path, val, strerror(errno));
		exit(1);
	}
	close(fd);
}

/* ---- the child: runs inside the new namespaces, becomes the container ---- */
static int child_fn(void *arg) {
	struct config *c = arg;
	char buf[1];

	/* Block until the parent has written our uid/gid maps and (if asked)
	 * moved us into the cgroup. The parent writes one byte to release us.
	 * (We wait for a byte rather than EOF because the child inherits its own
	 * copy of the pipe's write end, so EOF would never arrive.) Without this
	 * barrier we would exec as the overflow uid, before any map exists. */
	if (read(c->syncfd, buf, 1) != 1)
		die("child: sync read");
	close(c->syncfd);

	/* Die if the launcher dies, so a killed mycontainer never leaves orphaned
	 * container processes running (and, being pid 1 here, taking the whole
	 * namespace's processes down with us). Preserved across a normal exec. */
	prctl(PR_SET_PDEATHSIG, SIGKILL);

	/* UTS: our own hostname, invisible to the host. */
	if (sethostname("container", 9) < 0) die("sethostname");

	/* --- Part 3: install the private root with pivot_root --- *
	 * pivot_root has two preconditions almost everyone trips over:
	 *  1. mount propagation must be private, or the umount of the old root
	 *     propagates back to the host and pivot_root refuses (EINVAL);
	 *  2. new_root must itself be a mount point -- so we bind rootfs onto
	 *     itself first. */
	if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) < 0)
		die("mount --make-rprivate /");
	if (mount(c->rootfs, c->rootfs, NULL, MS_BIND | MS_REC, NULL) < 0)
		die("bind rootfs onto itself");

	/* A minimal /dev, bind-mounted from the host. We cannot mknod device
	 * nodes in an unprivileged user namespace (that needs CAP_MKNOD in the
	 * initial userns), but we CAN bind the host's nodes onto pre-made regular
	 * files -- enough for /dev/null, shell redirections and background jobs. */
	static const char *devs[] = { "null", "zero", "full", "random", "urandom", "tty" };
	for (size_t d = 0; d < sizeof devs / sizeof devs[0]; d++) {
		char src[64], dst[PATH_MAX];
		snprintf(src, sizeof src, "/dev/%s", devs[d]);
		snprintf(dst, sizeof dst, "%s/dev/%s", c->rootfs, devs[d]);
		mount(src, dst, NULL, MS_BIND, NULL);   /* best-effort */
	}

	/* Optional shared dir: bind a host directory to <rootfs>/share so the
	 * Part 4 demo can show that a file the container creates as "root" is
	 * owned by the real unprivileged uid on the host. Bound before pivot, so
	 * it rides along under the new root. */
	const char *share = getenv("LAB11_SHARE");
	if (share && *share) {
		char dst[PATH_MAX];
		snprintf(dst, sizeof dst, "%s/share", c->rootfs);
		mkdir(dst, 0777);
		if (mount(share, dst, NULL, MS_BIND, NULL) < 0)
			die("bind LAB11_SHARE");
	}

	if (chdir(c->rootfs) < 0) die("chdir rootfs");

	/* put_old must exist under the new root; build-rootfs.sh made /oldroot. */
	if (pivot_root(".", "oldroot") < 0) die("pivot_root");
	if (chdir("/") < 0) die("chdir /");

	/* --- Part 2: a fresh /proc so ps/id/etc read THIS pid namespace --- *
	 * Mount it before detaching the old root -- /proc is a fresh mount, not
	 * inherited from the host, so it reflects our pid namespace. */
	if (mount("proc", "/proc", "proc", 0, NULL) < 0) die("mount /proc");

	/* Detach the old root entirely: after this the host tree is unreachable.
	 * MNT_DETACH does a lazy unmount so busy references do not block us.
	 * We deliberately do NOT rmdir /oldroot: because the new root is the
	 * rootfs bind-mounted onto itself, an rmdir here would delete the real
	 * template directory on disk, breaking the next run. Leaving an empty,
	 * detached /oldroot mountpoint behind is harmless. */
	if (umount2("/oldroot", MNT_DETACH) < 0) die("umount2 /oldroot");

	execvp(c->cmd[0], c->cmd);
	die("execvp");          /* only reached on failure */
	return 1;
}

/* ---- cgroup v2 setup (Part 5), done by the parent on the delegated tree ---- */
static char *cgroup_setup(struct config *c) {
	char *cg = g_cg;
	char fallback[192];
	const char *base = getenv("LAB11_CGROUP_BASE");
	if (!base) {
		/* systemd's per-user slice is named after the uid, not "1000". */
		unsigned u = (unsigned)getuid();
		snprintf(fallback, sizeof fallback,
		         "/sys/fs/cgroup/user.slice/user-%u.slice/user@%u.service", u, u);
		base = fallback;
	}
	snprintf(cg, sizeof g_cg, "%s/mycontainer.%d", base, getpid());

	if (mkdir(cg, 0755) < 0 && errno != EEXIST) {
		fprintf(stderr, "mkdir %s: %s\n"
		        "  (is cgroup v2 delegated here? run ./probe.sh)\n",
		        cg, strerror(errno));
		exit(1);
	}
	/* This leaf holds the process, so it must NOT enable subtree_control --
	 * cgroup v2's "no internal processes" rule. The memory/pids controllers
	 * are available here because the PARENT delegates them in its
	 * subtree_control; we rely on that, we do not set it on the leaf. */
	char path[600], val[64];
	if (c->mem_max >= 0) {
		snprintf(path, sizeof path, "%s/memory.max", cg);
		snprintf(val, sizeof val, "%ld", c->mem_max);
		write_file(path, val);
		snprintf(path, sizeof path, "%s/memory.swap.max", cg);
		write_file(path, "0");   /* else it swaps instead of being OOM-killed */
	}
	if (c->pids_max >= 0) {
		snprintf(path, sizeof path, "%s/pids.max", cg);
		snprintf(val, sizeof val, "%ld", c->pids_max);
		write_file(path, val);
	}
	return cg;
}

static void cgroup_admit(const char *cg, pid_t pid) {
	char path[600], val[64];
	snprintf(path, sizeof path, "%s/cgroup.procs", cg);
	snprintf(val, sizeof val, "%d", pid);
	write_file(path, val);   /* the child and all its future forks land here */
}

int main(int argc, char **argv) {
	struct config c = { .mem_max = -1, .pids_max = -1, .userns = 1 };

	int i = 1;
	for (; i < argc && argv[i][0] == '-'; i++) {
		if (!strcmp(argv[i], "--mem")  && i + 1 < argc) c.mem_max  = atol(argv[++i]);
		else if (!strcmp(argv[i], "--pids") && i + 1 < argc) c.pids_max = atol(argv[++i]);
		else if (!strcmp(argv[i], "--no-userns")) c.userns = 0;
		else { fprintf(stderr, "unknown option %s\n", argv[i]); return 2; }
	}
	if (argc - i < 2) {
		fprintf(stderr,
		    "usage: %s [--mem BYTES] [--pids N] [--no-userns] ROOTFS CMD [ARGS...]\n",
		    argv[0]);
		return 2;
	}
	c.rootfs = argv[i++];
	c.cmd    = &argv[i];

	/* Sync pipe: child blocks on the read end until the parent closes the
	 * write end after finishing maps + cgroup admission. */
	int syncpipe[2];
	if (pipe(syncpipe) < 0) die("pipe");
	c.syncfd = syncpipe[0];

	int flags = SIGCHLD | CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWNET;
	if (c.userns) flags |= CLONE_NEWUSER;

	/* A real child stack. Passing NULL here is the classic first crash. */
	char *stack = malloc(STACK_SIZE);
	if (!stack) die("malloc stack");
	pid_t pid = clone(child_fn, stack + STACK_SIZE, flags, &c);
	if (pid < 0) die("clone");
	g_child = pid;
	signal(SIGINT,  on_signal);
	signal(SIGTERM, on_signal);

	close(syncpipe[0]);      /* parent keeps only the write end */

	/* --- Part 4: write the uid/gid maps from the parent --- *
	 * Map container uid/gid 0 to our real uid/gid. setgroups must be denied
	 * before the kernel will accept gid_map (a security requirement, and the
	 * step the man pages bury). Written here in the parent, targeting the
	 * child by pid, because a process cannot write its own map once it holds
	 * capabilities in the new userns. */
	if (c.userns) {
		char path[64], line[64];
		uid_t uid = getuid();
		gid_t gid = getgid();

		snprintf(path, sizeof path, "/proc/%d/setgroups", pid);
		write_file(path, "deny");
		snprintf(path, sizeof path, "/proc/%d/uid_map", pid);
		snprintf(line, sizeof line, "0 %d 1", uid);
		write_file(path, line);
		snprintf(path, sizeof path, "/proc/%d/gid_map", pid);
		snprintf(line, sizeof line, "0 %d 1", gid);
		write_file(path, line);
	}

	/* Part 5: create the cgroup and drop the child in before it execs. */
	char *cg = NULL;
	if (c.mem_max >= 0 || c.pids_max >= 0) {
		cg = cgroup_setup(&c);
		cgroup_admit(cg, pid);
		setenv("LAB11_CGROUP", cg, 1);   /* so a wrapper can read accounting */
		fprintf(stderr, "mycontainer: cgroup %s\n", cg);
	}

	if (write(syncpipe[1], "1", 1) != 1) die("release child");
	close(syncpipe[1]);      /* child is now unblocked and execs the command */

	int status;
	if (waitpid(pid, &status, 0) < 0) die("waitpid");

	/* Tidy the cgroup: the pid namespace's death took every process with it,
	 * so the leaf is empty and rmdir succeeds. Read accounting first if you
	 * want it: the files vanish with the directory, and the write-up wants
	 * the numbers (README.md, Part 5, "read the accounting back out"). */
	if (cg) rmdir(cg);

	if (WIFEXITED(status))   return WEXITSTATUS(status);
	if (WIFSIGNALED(status)) {
		fprintf(stderr, "mycontainer: child killed by signal %d\n", WTERMSIG(status));
		return 128 + WTERMSIG(status);
	}
	return 0;
}
