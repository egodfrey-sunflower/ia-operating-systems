/* mycontainer.c -- SKELETON. This is where you work if you take the C route.
 *
 * Target interface (assert.sh drives exactly this; the shell skeleton takes
 * the same arguments, on purpose):
 *
 *     mycontainer [--mem BYTES] [--pids N] ROOTFS CMD [ARGS...]
 *
 * Build:  cc -Wall -Wextra -O2 -o mycontainer mycontainer.c
 *
 * ============================================================================
 * THE MECHANICS YOU ARE GIVEN
 * ============================================================================
 * None of the following is discoverable by reasoning, and none of it is what
 * the lab is testing. Trial and error on any of them costs an hour and teaches
 * nothing, so they are stated once, here, as requirements. What each primitive
 * actually RESTRICTS is not given -- working that out is the lab.
 *
 *  1. clone() needs a real child stack: allocate it, and pass the pointer to
 *     the TOP of it (stacks grow down). A NULL stack segfaults immediately.
 *
 *  2. The uid/gid maps are written by the PARENT, into /proc/<child>/uid_map
 *     and /proc/<child>/gid_map, after clone() returns. A process cannot write
 *     its own map once it holds capabilities in the new namespace.
 *
 *  3. Before gid_map will accept a write, /proc/<child>/setgroups must be
 *     written with the string "deny". In that order. The kernel refuses the
 *     gid_map write otherwise, with EPERM and no explanation.
 *
 *  4. The child must not exec until the parent has finished writing the maps
 *     (and, for Part 5, has put it in the cgroup). Otherwise it execs as the
 *     overflow uid, 65534. A pipe the parent closes, or writes one byte to, is
 *     the usual barrier.
 *
 *  5. pivot_root() has no glibc wrapper: syscall(SYS_pivot_root, new, old).
 *
 *  6. pivot_root() has two preconditions, and both are checked before anything
 *     useful happens:
 *       - mount propagation must be private first:
 *             mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL)
 *         (this is `mount --make-rprivate /`);
 *       - new_root must itself be a MOUNT POINT, so bind the rootfs directory
 *         onto itself: mount(rootfs, rootfs, NULL, MS_BIND | MS_REC, NULL).
 *     put_old must be a directory under new_root; build-rootfs.sh makes
 *     /oldroot for you.
 *
 *  7. A PID namespace only renumbers the CHILD. The process that calls clone()
 *     is not renumbered, and neither is a process that calls unshare(). If you
 *     test getpid() in the wrong process you will conclude, wrongly, that the
 *     namespace did not work.
 *
 *  8. /proc is what ps, id and everything else actually read. A fresh
 *     mount("proc", "/proc", "proc", 0, NULL) inside the container is what
 *     makes it show the container's pid namespace. Mount it AFTER pivot_root
 *     and BEFORE detaching the old root.
 *
 *  9. Detach the old root with umount2("/oldroot", MNT_DETACH). Do not rmdir
 *     /oldroot afterwards if your new root is a self-bind of a directory on
 *     disk -- you would delete the real directory.
 *
 * 10. cgroup v2: the controller must be listed in the PARENT cgroup's
 *     cgroup.subtree_control before memory.max or pids.max exist in your leaf,
 *     and a cgroup that holds processes may not itself enable controllers
 *     ("no internal processes"). So: make a leaf under a delegated parent,
 *     write the limits there, and put the process in that leaf. ./probe.sh
 *     prints the parent to use, as LAB11_CGROUP_BASE.
 *
 * 11. Set memory.swap.max to 0 as well as memory.max, or the hog swaps instead
 *     of being killed and Part 5 looks like it does not work.
 *
 * ============================================================================
 * WHAT YOU WRITE
 * ============================================================================
 * The five parts, in the order the plan gives them. Each is a few lines.
 * Delete the TODOs as you go; run ./assert.sh after each part.
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
#include <signal.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>

#define STACK_SIZE (1024 * 1024)

/* (warns as unused until Part 3 calls it) */
static int pivot_root(const char *new_root, const char *put_old) {
	return syscall(SYS_pivot_root, new_root, put_old);
}

static void die(const char *msg) { perror(msg); exit(1); }

struct config {
	char  *rootfs;
	char **cmd;        /* NULL-terminated argv for the container command */
	long   mem_max;    /* bytes, or -1 for unset */
	long   pids_max;   /* count, or -1 for unset */
	int    userns;     /* 1 = create a user namespace */
	int    syncfd;     /* the child's end of the barrier from mechanic 4 */
};

/* ---- the child: everything below runs INSIDE the new namespaces ---- */
static int child_fn(void *arg)
{
	struct config *c = arg;
	char buf[1];

	/* Mechanic 4: wait for the parent before doing anything. */
	if (read(c->syncfd, buf, 1) != 1) die("child: sync read");
	close(c->syncfd);

	/* Optional but kind: die if the launcher dies, so a killed mycontainer
	 * never leaves a container running. Survives exec. */
	prctl(PR_SET_PDEATHSIG, SIGKILL);

	/* --- Part 1: UTS --- */
	/* TODO: give the container its own hostname. One call. */

	/* --- Part 3: the private root --- */
	/* TODO: mechanic 6's two preconditions, then pivot_root(), then chdir("/"). */

	/* --- Part 2: a /proc that describes THIS pid namespace --- */
	/* TODO: mechanic 8. */

	/* --- Part 3, finishing: make the host tree unreachable --- */
	/* TODO: mechanic 9. */

	execvp(c->cmd[0], c->cmd);
	die("execvp");
	return 1;
}

int main(int argc, char **argv)
{
	struct config c = { .mem_max = -1, .pids_max = -1, .userns = 1 };

	int i = 1;
	for (; i < argc && argv[i][0] == '-'; i++) {
		if (!strcmp(argv[i], "--mem")  && i + 1 < argc)      c.mem_max  = atol(argv[++i]);
		else if (!strcmp(argv[i], "--pids") && i + 1 < argc) c.pids_max = atol(argv[++i]);
		else if (!strcmp(argv[i], "--no-userns"))            c.userns   = 0;
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

	/* Mechanic 4: the barrier. */
	int syncpipe[2];
	if (pipe(syncpipe) < 0) die("pipe");
	c.syncfd = syncpipe[0];

	/* --- Part 1: the namespace flags, one bit each. ---
	 * Part 1 asks you to demonstrate them ONE AT A TIME first: build with a
	 * single flag, show what changes, then add the next. They are independent
	 * bits, not a bundle, and that independence is the point of the part.
	 *
	 *   CLONE_NEWUTS   hostname and domain name
	 *   CLONE_NEWPID   process ids            (mechanic 7)
	 *   CLONE_NEWNS    mount table            ("NEWNS" is historical: it was
	 *                                          the first namespace Linux had)
	 *   CLONE_NEWNET   network interfaces, routes, ports
	 *   CLONE_NEWUSER  uids, gids, capabilities   -- Part 4
	 *   CLONE_NEWIPC, CLONE_NEWCGROUP, CLONE_NEWTIME  exist too; not needed here
	 *
	 * SIGCHLD in the flags is not a namespace -- it is the signal the child
	 * sends the parent on exit, which is what makes waitpid() below work.
	 */
	int flags = SIGCHLD;
	/* TODO: | the namespace flags you want. Start with one. */
	if (c.userns) flags |= CLONE_NEWUSER;

	/* Mechanic 1: a real stack, and the pointer to its top. */
	char *stack = malloc(STACK_SIZE);
	if (!stack) die("malloc stack");
	pid_t pid = clone(child_fn, stack + STACK_SIZE, flags, &c);
	if (pid < 0) die("clone");

	close(syncpipe[0]);      /* the parent keeps only the write end */

	/* --- Part 4: the uid and gid maps, written from HERE (mechanics 2, 3) --- */
	if (c.userns) {
		/* TODO: /proc/<pid>/setgroups <- "deny"
		 *       /proc/<pid>/uid_map   <- "0 <getuid()> 1"
		 *       /proc/<pid>/gid_map   <- "0 <getgid()> 1"
		 * A map line is "inner outer count". */
	}

	/* --- Part 5: the cgroup (mechanics 10, 11) --- */
	if (c.mem_max >= 0 || c.pids_max >= 0) {
		/* TODO: mkdir a leaf under LAB11_CGROUP_BASE (./probe.sh prints it),
		 *       write memory.max / memory.swap.max / pids.max,
		 *       then write the CHILD's pid to that leaf's cgroup.procs.
		 * Only the container goes in. Putting this process in as well means
		 * the limit is shared between the launcher and the container, which
		 * is a different, and wrong, experiment. */
	}

	/* Release the child (mechanic 4). */
	if (write(syncpipe[1], "1", 1) != 1) die("release child");
	close(syncpipe[1]);

	int status;
	if (waitpid(pid, &status, 0) < 0) die("waitpid");

	/* TODO (Part 5): rmdir the leaf. The pid namespace died with its pid 1,
	 * taking every process in it, so the leaf is empty and rmdir succeeds. */

	if (WIFEXITED(status)) return WEXITSTATUS(status);
	if (WIFSIGNALED(status)) {
		fprintf(stderr, "mycontainer: child killed by signal %d\n", WTERMSIG(status));
		return 128 + WTERMSIG(status);
	}
	return 0;
}
