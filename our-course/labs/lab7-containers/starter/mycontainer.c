/*
 * mycontainer - a teaching container runtime (STARTER SKELETON)
 *
 * Lab 7, IA Operating Systems.  Your job is to fill in the four graded TODO-marked
 * functions so that `mycontainer run` builds a real container out of Linux
 * namespaces, pivot_root and cgroups v2 (plus, as an unweighted stretch, a
 * veth pair).
 *
 * The plumbing is done for you: argument parsing, the clone(2) stack, the
 * parent/child synchronisation pipe, and a pivot_root(2) wrapper (glibc has
 * none).  Search for "TODO" - four graded functions plus the stretch:
 *
 *   setup_namespaces()  Task 1  - hostname in the new UTS namespace
 *   setup_rootfs()      Task 2  - pivot_root into the image, mount /proc
 *   setup_cgroup()      Task 3  - apply the memory/cpu caps, move child in
 *   setup_userns()      Task 4  - uid_map / gid_map (+ setgroups deny)
 *   setup_net()         stretch - veth pair across the network namespace
 *
 * This is a teaching tool, NOT a security boundary.  Never run untrusted
 * images.  Most tasks require root (or --userns for the unprivileged subset).
 *
 * Usage:
 *   mycontainer run [options] <rootfs> <cmd> [args...]
 * Options:
 *   -m, --mem SIZE    memory cap (e.g. 100M)
 *   -c, --cpus N      cpu cap as fraction of one CPU (e.g. 0.5)
 *   -u, --userns      new user namespace (map you -> root)
 *   -n, --net         new network namespace with a veth pair
 *   -h, --help        show this help
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

/* glibc ships no wrapper for pivot_root(2); call it raw.  (Provided.) */
static int __attribute__((unused))
pivot_root(const char *new_root, const char *put_old)
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

/* Write a whole string to a file; handy for uid_map and the cgroup knobs. */
static int __attribute__((unused))
write_file(const char *path, const char *data)
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

/* Run a command, wait, return its exit status (or -1 on spawn failure).
 * Useful for shelling out to `ip` in setup_net(). */
static int __attribute__((unused))
run_cmd(char *const argv[])
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

/* Parse "100M" / "512K" / "1G" / "1048576" into bytes.  (Provided.)
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

/* ================================================================= TODOs == */

/*
 * Task 1: give the container its own hostname.  The UTS namespace itself is
 * created by the CLONE_NEWUTS flag in main(); here, from inside it, call
 * sethostname("container", ...).  Return 0 on success, -1 on failure.
 */
static int setup_namespaces(void)
{
	fprintf(stderr, "setup_namespaces: TODO (Task 1) not implemented\n");
	return -1;
}

/*
 * Task 2: pivot_root into `rootfs` and mount a fresh /proc.
 *
 * The NOTES section of pivot_root(2) walks through the standard sequence;
 * work the calls and their flags out from there.  Two things to get right:
 *
 *   - Make the mount table private BEFORE touching anything, so that nothing
 *     you do here propagates back to the host (mount_namespaces(7), "Shared
 *     and slave subtrees" -- getting this wrong can unmount host filesystems).
 *   - When must /proc be mounted, relative to the pivot?  Decide
 *     deliberately: the wrong answer only shows up later, under --userns
 *     (the handout's Task 4 "proc-mount ordering" pitfall).
 *
 * Explain in your handout answer WHY pivot_root beats chroot here.
 */
static int setup_rootfs(const char *rootfs)
{
	(void)rootfs;
	fprintf(stderr, "setup_rootfs: TODO (Task 2) not implemented\n");
	return -1;
}

/*
 * Task 3: create a cgroup v2 under /sys/fs/cgroup, apply the memory and/or
 * cpu caps, and move `child` into it.  The control-file interface is
 * documented in cgroups(7) ("Cgroups version 2"); the handout states the
 * directory name the grader expects.
 * Called by the PARENT (real root) before the child is released.
 */
static int setup_cgroup(pid_t child, long mem_bytes, double cpus)
{
	(void)child; (void)mem_bytes; (void)cpus;
	fprintf(stderr, "setup_cgroup: TODO (Task 3) not implemented\n");
	return -1;
}

/*
 * Task 4: write /proc/<child>/uid_map and gid_map to map your uid/gid to 0
 * inside the container.  You MUST write "deny" to /proc/<child>/setgroups
 * before writing gid_map (the setgroups/gid_map security rule).  Called by
 * the PARENT.
 */
static int setup_userns(pid_t child)
{
	(void)child;
	fprintf(stderr, "setup_userns: TODO (Task 4) not implemented\n");
	return -1;
}

/*
 * Stretch (unweighted): create a veth pair, keep veth0 on the host (10.0.7.1/24) and push
 * veth1 into the child's netns (addressed 10.0.7.2/24 by the child in
 * child_fn below).  Shelling out to `ip` via run_cmd() is fine and expected.
 * Called by the PARENT with the child's PID.
 */
static int setup_net(pid_t child)
{
	(void)child;
	fprintf(stderr, "setup_net: TODO (stretch) not implemented\n");
	return -1;
}

static void cleanup_cgroup(void) { /* TODO: rmdir the cgroup you created */ }
static void cleanup_net(void)    { /* TODO: delete the veth pair */ }

/* -------------------------------------------------- Task 1 + stretch: child entry */

/* Runs inside all the new namespaces.  (Plumbing provided; extend for net.) */
static int child_fn(void *arg)
{
	struct child_args *ca = arg;
	char buf[4];

	/* clone() gave us a COPY of the fd table, so we also hold the pipe's
	 * write end.  Close it, or the pipe never reaches EOF and we hang. */
	close(ca->pipe_wr);

	/* Block until the parent has set up uid_map / cgroup / veth. */
	if (read(ca->pipe_rd, buf, sizeof(buf)) < 0)
		die("child: read pipe");
	close(ca->pipe_rd);

	/* Task 1: hostname. */
	if (setup_namespaces() < 0)
		die("setup_namespaces");

	/* Stretch: bring up lo + veth1 here (still using the host's `ip`,
	 * before we pivot away from it).  TODO:
	 *   ip link set lo up
	 *   ip addr add 10.0.7.2/24 dev veth1
	 *   ip link set veth1 up
	 *   ip route add default via 10.0.7.1
	 */
	if (ca->use_net) {
		/* TODO (stretch): configure the container end of the veth. */
	}

	/* Task 2: pivot into the image. */
	if (setup_rootfs(ca->rootfs) < 0)
		die("setup_rootfs");

	/* We are PID 1 in the new PID namespace. */
	execvp(ca->argv[0], ca->argv);
	die("execvp");
	return 1; /* unreachable */
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

	if (argc < 2 || strcmp(argv[1], "run") != 0) {
		usage(argv[0]);
		return 2;
	}
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

	/* Task 1: the always-on namespaces.  (Add NEWUSER/NEWNET below.) */
	int flags = CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | CLONE_NEWIPC
		    | SIGCHLD;
	if (use_userns)
		flags |= CLONE_NEWUSER;   /* Task 4 */
	if (use_net)
		flags |= CLONE_NEWNET;    /* stretch */

	pid_t child = clone(child_fn, child_stack + STACK_SIZE, flags, &ca);
	if (child < 0)
		die("clone");
	close(pipefd[0]);

	int rc = 1, released = 0;

	if (use_userns && setup_userns(child) < 0) {
		perror("setup_userns");
		goto out;
	}
	if ((mem_bytes > 0 || cpus > 0) &&
	    setup_cgroup(child, mem_bytes, cpus) < 0) {
		perror("setup_cgroup");
		goto out;
	}
	if (use_net && setup_net(child) < 0) {
		fprintf(stderr, "setup_net failed\n");
		goto out;
	}

	/* Release the child. */
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
