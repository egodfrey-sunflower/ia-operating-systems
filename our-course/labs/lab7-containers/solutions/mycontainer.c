/*
 * mycontainer - a teaching container runtime (SOLUTION)
 *
 * Lab 7, IA Operating Systems.  ~500 lines of C that build a container the
 * way Docker/runc do underneath: Linux namespaces + pivot_root + cgroups v2
 * + a veth pair.  This is a teaching tool, NOT a security boundary.  Run it
 * only on rootfs images you trust, and understand that a real runtime layers
 * seccomp, capability dropping and much more on top of what is here.
 *
 * Usage:
 *   mycontainer run [options] <rootfs> <cmd> [args...]
 *
 * Options:
 *   -m, --mem SIZE    memory.max for the container (e.g. 100M, 512K, 1G)
 *   -c, --cpus N      cpu.max as a fraction of one CPU (e.g. 0.5)
 *   -u, --userns      new user namespace; map your uid/gid to root inside
 *   -n, --net         new network namespace with a veth pair to the host
 *   -h, --help        this message
 *
 * Most tasks require root (or --userns for the unprivileged subset).
 */

#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>

/* glibc ships no wrapper for pivot_root(2); call it raw. */
static int pivot_root(const char *new_root, const char *put_old)
{
	return (int)syscall(SYS_pivot_root, new_root, put_old);
}

#define STACK_SIZE (1024 * 1024)

/* Everything the child needs, passed through clone()'s arg pointer. */
struct child_args {
	char  *rootfs;   /* path to the extracted root filesystem */
	char **argv;     /* command + args to exec inside the container */
	int    use_net;  /* configure loopback + veth1 inside the netns */
	int    pipe_rd;  /* read end: parent closes write end to release us */
	int    pipe_wr;  /* our inherited copy of the write end - must close */
};

static char child_stack[STACK_SIZE];

/* ------------------------------------------------------------------ helpers */

static void die(const char *msg)
{
	perror(msg);
	exit(1);
}

/* Write a whole string to a file; used for uid_map, cgroup knobs, etc. */
static int write_file(const char *path, const char *data)
{
	int fd = open(path, O_WRONLY | O_TRUNC);
	if (fd < 0)
		return -1;
	ssize_t n = write(fd, data, strlen(data));
	int saved = errno;
	close(fd);
	if (n != (ssize_t)strlen(data)) {
		errno = saved;
		return -1;
	}
	return 0;
}

/* Run a command, wait, return its exit status (or -1 on spawn failure). */
static int run_cmd(char *const argv[])
{
	pid_t pid = fork();
	if (pid < 0)
		return -1;
	if (pid == 0) {
		execvp(argv[0], argv);
		perror(argv[0]);
		_exit(127);
	}
	int status;
	if (waitpid(pid, &status, 0) < 0)
		return -1;
	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* Parse "100M" / "512K" / "1G" / "1048576" into bytes.
 * Rejects empty input, negative values, trailing junk ("100Mfoo") and
 * anything that overflows a long, either in strtol or in the multiply. */
static long parse_mem(const char *s)
{
	char *end;
	long mult = 1;

	errno = 0;
	long v = strtol(s, &end, 10);
	if (errno == ERANGE || end == s || v < 0)
		return -1;
	switch (*end) {
	case 'g': case 'G': mult = 1024L * 1024 * 1024; end++; break;
	case 'm': case 'M': mult = 1024L * 1024;        end++; break;
	case 'k': case 'K': mult = 1024L;               end++; break;
	case '\0': break;
	default: return -1;
	}
	if (*end != '\0')
		return -1;
	if (v > LONG_MAX / mult)
		return -1;
	return v * mult;
}

/* ------------------------------------------------------- Task 2: filesystem */

/*
 * Why pivot_root and not chroot?  chroot only moves the "/" lookup for path
 * resolution; the process keeps its old root reachable (via an open fd, a
 * relative ".." walk from a passed directory fd, or simply by chroot-ing
 * again) and can escape.  pivot_root actually swaps the mount that is the
 * root of the mount namespace, then we unmount the old root entirely so there
 * is nothing left to escape to.
 *
 * The "put_old" dance: pivot_root needs somewhere to park the old root while
 * it installs the new one, and put_old must be underneath new_root.  We bind
 * new_root onto itself (pivot_root requires new_root to be a mount point),
 * chdir into it, pivot with "." / "oldroot", then umount2(oldroot,
 * MNT_DETACH) and rmdir it.
 */
static int setup_rootfs(const char *rootfs)
{
	/* 1. Stop our mounts propagating back to the host (and vice versa). */
	if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) < 0) {
		perror("setup_rootfs: make / rprivate");
		return -1;
	}

	/* 2. pivot_root needs new_root to be a mount point: bind it to itself. */
	if (mount(rootfs, rootfs, NULL, MS_BIND | MS_REC, NULL) < 0) {
		perror("setup_rootfs: bind rootfs to itself");
		return -1;
	}

	if (chdir(rootfs) < 0) {
		perror("setup_rootfs: chdir rootfs");
		return -1;
	}

	/*
	 * 3. Mount the fresh /proc NOW, while the host's proc instance is
	 * still visible in this mount namespace.  In a user namespace the
	 * kernel only permits a new procfs mount if a fully visible proc
	 * mount still exists to compare against (fs_fully_visible); once we
	 * pivot and detach the old root there is none, and this mount would
	 * fail with EPERM.  Pre-pivot, "proc" here is <rootfs>/proc, and the
	 * mount rides along into the new root.
	 */
	if (mkdir("proc", 0555) < 0 && errno != EEXIST) {
		perror("setup_rootfs: mkdir proc");
		return -1;
	}
	if (mount("proc", "proc", "proc",
		  MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL) < 0) {
		perror("setup_rootfs: mount /proc");
		return -1;
	}

	/* 4. Somewhere under new_root to stash the old root. */
	if (mkdir("oldroot", 0777) < 0 && errno != EEXIST) {
		perror("setup_rootfs: mkdir oldroot");
		return -1;
	}

	if (pivot_root(".", "oldroot") < 0) {
		perror("setup_rootfs: pivot_root");
		return -1;
	}

	if (chdir("/") < 0) {
		perror("setup_rootfs: chdir /");
		return -1;
	}

	/* 5. Detach the old root so the container cannot reach the host FS. */
	if (umount2("/oldroot", MNT_DETACH) < 0) {
		perror("setup_rootfs: umount old root");
		return -1;
	}
	rmdir("/oldroot");

	/* Best-effort /dev bits so shells behave; ignore failures. */
	mount("tmpfs", "/dev", "tmpfs", MS_NOSUID, "mode=755");
	return 0;
}

/* ------------------------------------------------ Task 1: UTS / hostname */

/*
 * The namespaces themselves are requested via clone(2) flags in main().  What
 * is left to do from inside the new namespaces is to give the container its
 * own identity: set the hostname in our fresh UTS namespace.
 */
static int setup_namespaces(void)
{
	if (sethostname("container", strlen("container")) < 0)
		return -1;
	return 0;
}

/* -------------------------------------------------- Task 1/5: child entry */

static int child_fn(void *arg)
{
	struct child_args *ca = arg;
	char buf[4];

	/*
	 * clone() without CLONE_FILES gives us a COPY of the parent's fd
	 * table, so we hold the pipe's write end too.  Close it, or the pipe
	 * never reaches EOF and we deadlock waiting for the parent.
	 */
	close(ca->pipe_wr);

	/*
	 * Block until the parent has written our uid_map, put us in the
	 * cgroup, and moved the veth into our netns.  The parent signals by
	 * closing its end of the pipe, so read() returns 0 (EOF).
	 */
	if (read(ca->pipe_rd, buf, sizeof(buf)) < 0)
		die("child: read pipe");
	close(ca->pipe_rd);

	/* Task 1: our own UTS namespace -> our own hostname. */
	if (setup_namespaces() < 0)
		die("setup_namespaces");

	/* Stretch: bring up loopback + the container end of the veth. */
	if (ca->use_net) {
		char *lo_up[]  = { "ip", "link", "set", "lo", "up", NULL };
		char *v_addr[] = { "ip", "addr", "add", "10.0.7.2/24",
				   "dev", "veth1", NULL };
		char *v_up[]   = { "ip", "link", "set", "veth1", "up", NULL };
		char *route[]  = { "ip", "route", "add", "default",
				   "via", "10.0.7.1", NULL };
		/* These run in the host's mount ns still (before pivot), so
		 * the host 'ip' binary is used - the container image need not
		 * ship one. */
		run_cmd(lo_up);
		run_cmd(v_addr);
		run_cmd(v_up);
		run_cmd(route);
	}

	/* Task 2: pivot into the image and mount a private /proc.
	 * setup_rootfs reports the exact failing step itself. */
	if (setup_rootfs(ca->rootfs) < 0)
		exit(1);

	/* Become the requested command.  We are PID 1 in the new PID ns. */
	execvp(ca->argv[0], ca->argv);
	die("execvp");
	return 1; /* unreachable */
}

/* --------------------------------------------------- Task 4: user namespace */

/*
 * With CLONE_NEWUSER the child starts with no valid mappings.  Only a process
 * in the PARENT user ns may write the child's uid_map/gid_map, and to write
 * gid_map at all we must first write "deny" to setgroups (otherwise an
 * unprivileged user could drop supplementary groups to bypass file
 * permissions - the classic setgroups/gid_map vulnerability).
 */
static int setup_userns(pid_t child)
{
	char path[64], line[64];

	snprintf(path, sizeof(path), "/proc/%d/setgroups", child);
	if (write_file(path, "deny") < 0)
		return -1;

	/*
	 * Mapping choice matters more than it looks.  An unprivileged user
	 * may only map their own uid, so "0 <uid> 1" is all we can have.
	 * Root COULD write the same map - but then every file owned by any
	 * other uid (say, a rootfs extracted by a normal user) has NO mapping
	 * in the namespace, and the kernel disables capability checks against
	 * unmapped inodes (capable_wrt_inode_uidgid), so even "root" in the
	 * container gets EPERM touching them.  Privileged runtimes therefore
	 * map the full range; we do the same when we can.
	 */
	snprintf(path, sizeof(path), "/proc/%d/uid_map", child);
	if (getuid() == 0)
		snprintf(line, sizeof(line), "0 0 4294967295");
	else
		snprintf(line, sizeof(line), "0 %d 1", getuid());
	if (write_file(path, line) < 0)
		return -1;

	snprintf(path, sizeof(path), "/proc/%d/gid_map", child);
	if (getgid() == 0)
		snprintf(line, sizeof(line), "0 0 4294967295");
	else
		snprintf(line, sizeof(line), "0 %d 1", getgid());
	if (write_file(path, line) < 0)
		return -1;
	return 0;
}

/* --------------------------------------------------------- Task 3: cgroups */

#define CG_ROOT "/sys/fs/cgroup"

/* Ensure cpu+memory controllers are delegated to our sub-tree. */
static void enable_controllers(void)
{
	/* Best effort: on stock Ubuntu the root already has them available. */
	write_file(CG_ROOT "/cgroup.subtree_control", "+memory +cpu");
}

static char cg_path[128];

static int setup_cgroup(pid_t child, long mem_bytes, double cpus)
{
	enable_controllers();

	snprintf(cg_path, sizeof(cg_path), CG_ROOT "/mycontainer_%d", getpid());
	if (mkdir(cg_path, 0755) < 0 && errno != EEXIST)
		return -1;

	char path[192], val[64];

	if (mem_bytes > 0) {
		snprintf(path, sizeof(path), "%s/memory.max", cg_path);
		snprintf(val, sizeof(val), "%ld", mem_bytes);
		if (write_file(path, val) < 0)
			return -1;
		/* Disable swap so the hog is actually OOM-killed at the cap. */
		snprintf(path, sizeof(path), "%s/memory.swap.max", cg_path);
		write_file(path, "0");
	}

	if (cpus > 0) {
		/* cpu.max is "quota period" in microseconds; period 100000. */
		snprintf(path, sizeof(path), "%s/cpu.max", cg_path);
		snprintf(val, sizeof(val), "%d 100000", (int)(cpus * 100000));
		if (write_file(path, val) < 0)
			return -1;
	}

	/* Move the child in BEFORE it execs, so limits bind from the start. */
	snprintf(path, sizeof(path), "%s/cgroup.procs", cg_path);
	snprintf(val, sizeof(val), "%d", child);
	if (write_file(path, val) < 0)
		return -1;
	return 0;
}

static void cleanup_cgroup(void)
{
	if (cg_path[0])
		rmdir(cg_path);
}

/* --------------------------------------------------- Stretch: veth plumbing */

/*
 * Create a veth pair, keep veth0 on the host at 10.0.7.1/24, and push veth1
 * into the child's network namespace (addressed 10.0.7.2/24 by the child).
 * Shelling out to iproute2 is entirely standard and keeps this readable; a
 * production runtime would use rtnetlink directly.
 */
static int setup_net(pid_t child)
{
	char pid_s[16];
	snprintf(pid_s, sizeof(pid_s), "%d", child);

	char *add[]  = { "ip", "link", "add", "veth0", "type", "veth",
			 "peer", "name", "veth1", NULL };
	char *mv[]   = { "ip", "link", "set", "veth1", "netns", pid_s, NULL };
	char *addr[] = { "ip", "addr", "add", "10.0.7.1/24", "dev", "veth0",
			 NULL };
	char *up[]   = { "ip", "link", "set", "veth0", "up", NULL };

	if (run_cmd(add) != 0)
		return -1;
	if (run_cmd(mv) != 0)
		return -1;
	if (run_cmd(addr) != 0)
		return -1;
	if (run_cmd(up) != 0)
		return -1;
	return 0;
}

static void cleanup_net(void)
{
	char *del[] = { "ip", "link", "del", "veth0", NULL };
	run_cmd(del); /* deleting one end removes the pair; ignore errors */
}

/* ------------------------------------------------------------------- main */

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s run [options] <rootfs> <cmd> [args...]\n"
		"  -m, --mem SIZE    memory cap (e.g. 100M)\n"
		"  -c, --cpus N      cpu cap as fraction of one CPU (e.g. 0.5)\n"
		"  -u, --userns      new user namespace (map you -> root)\n"
		"  -n, --net         new network namespace with a veth pair\n"
		"  -h, --help        show this help\n",
		prog);
}

int main(int argc, char **argv)
{
	long   mem_bytes = 0;
	double cpus = 0;
	int    use_userns = 0, use_net = 0;

	static struct option opts[] = {
		{ "mem",    required_argument, 0, 'm' },
		{ "cpus",   required_argument, 0, 'c' },
		{ "userns", no_argument,       0, 'u' },
		{ "net",    no_argument,       0, 'n' },
		{ "help",   no_argument,       0, 'h' },
		{ 0, 0, 0, 0 }
	};

	/* Require the "run" subcommand first, then parse its options. */
	if (argc < 2 || strcmp(argv[1], "run") != 0) {
		usage(argv[0]);
		return 2;
	}
	/* Shift past "run" for getopt. */
	int sub_argc = argc - 1;
	char **sub_argv = argv + 1;

	int c;
	while ((c = getopt_long(sub_argc, sub_argv, "+m:c:unh", opts, NULL))
	       != -1) {
		switch (c) {
		case 'm':
			mem_bytes = parse_mem(optarg);
			if (mem_bytes < 0) {
				fprintf(stderr, "bad --mem: %s\n", optarg);
				return 2;
			}
			break;
		case 'c':
			cpus = atof(optarg);
			if (cpus <= 0) {
				fprintf(stderr, "bad --cpus: %s\n", optarg);
				return 2;
			}
			break;
		case 'u': use_userns = 1; break;
		case 'n': use_net = 1; break;
		case 'h': usage(argv[0]); return 0;
		default:  usage(argv[0]); return 2;
		}
	}

	/* After options: <rootfs> <cmd> [args...] */
	if (optind + 1 >= sub_argc) {
		usage(argv[0]);
		return 2;
	}
	char *rootfs = sub_argv[optind];
	char **cmd = &sub_argv[optind + 1];

	struct stat st;
	if (stat(rootfs, &st) < 0 || !S_ISDIR(st.st_mode)) {
		fprintf(stderr, "rootfs '%s' is not a directory\n", rootfs);
		return 1;
	}

	/* Pipe used to hold the child until parent setup is done. */
	int pipefd[2];
	if (pipe(pipefd) < 0)
		die("pipe");

	struct child_args ca = {
		.rootfs  = rootfs,
		.argv    = cmd,
		.use_net = use_net,
		.pipe_rd = pipefd[0],
		.pipe_wr = pipefd[1],
	};

	/* Task 1: the always-on namespaces. */
	int flags = CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | CLONE_NEWIPC
		    | SIGCHLD;
	if (use_userns)
		flags |= CLONE_NEWUSER;   /* Task 4 */
	if (use_net)
		flags |= CLONE_NEWNET;    /* stretch */

	pid_t child = clone(child_fn, child_stack + STACK_SIZE, flags, &ca);
	if (child < 0)
		die("clone");
	close(pipefd[0]); /* child holds the read end */

	int rc = 1, released = 0;

	/* Task 4: map uid/gid while the child blocks on the pipe. */
	if (use_userns && setup_userns(child) < 0) {
		perror("setup_userns");
		goto out;
	}

	/* Task 3: create the cgroup and move the child in. */
	if ((mem_bytes > 0 || cpus > 0) &&
	    setup_cgroup(child, mem_bytes, cpus) < 0) {
		perror("setup_cgroup");
		goto out;
	}

	/* Stretch: build the veth pair and hand one end to the child. */
	if (use_net && setup_net(child) < 0) {
		fprintf(stderr, "setup_net failed\n");
		goto out;
	}

	/* Release the child: closing our write end is the EOF it waits for. */
	close(pipefd[1]);
	pipefd[1] = -1;
	released = 1;

	int status;
	if (waitpid(child, &status, 0) < 0) {
		perror("waitpid");
		goto out;
	}
	if (WIFEXITED(status))
		rc = WEXITSTATUS(status);
	else if (WIFSIGNALED(status))
		rc = 128 + WTERMSIG(status);

out:
	if (!released) {
		/* We bailed before letting the child run: kill and reap it so
		 * it never execs into a half-configured container. */
		kill(child, SIGKILL);
		if (pipefd[1] >= 0)
			close(pipefd[1]);
		waitpid(child, NULL, 0);
	}
	if (use_net)
		cleanup_net();
	cleanup_cgroup();
	return rc;
}
