/* hog.c -- a workload for demonstrating cgroup limits (Part 5). GIVEN.
 *
 * Build it STATIC so it runs inside the busybox rootfs, and copy it in:
 *   cc -static -O2 -o hog hog.c && cp hog rootfs/bin/hog
 * (build-rootfs.sh copies it automatically if a ./hog is present.)
 *
 *   hog mem       allocate and TOUCH memory forever, 8 MiB at a time. Under a
 *                 memory.max limit with swap disabled, the kernel OOM-kills it.
 *   hog fork      fork children that sleep, forever. Under a pids.max limit,
 *                 fork() starts failing at the cap; hog then waits so you can
 *                 read pids.current from outside while it is still stuck.
 *   hog fork MAX  the same, but stop after MAX successful forks (or at the
 *                 first failure), print how far it got, and exit 0. Bounded,
 *                 so it is safe to run with no limit set at all -- which is
 *                 what assert.sh needs, since its pids case has to be able to
 *                 observe an UNLIMITED container without forking the machine
 *                 to death. The count it prints is the observable.
 *
 * Nothing here knows about cgroups -- that is the point. It is an ordinary
 * greedy process; the limit is imposed from outside, by the cgroup it was
 * placed in. Touching the pages (not just malloc) matters: unwritten pages
 * are not charged to the cgroup, so a malloc-only hog would never be killed.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
	if (argc < 2) { fprintf(stderr, "usage: hog mem|fork\n"); return 2; }

	if (!strcmp(argv[1], "mem")) {
		size_t chunk = 8u * 1024 * 1024;
		unsigned long total = 0;
		for (;;) {
			char *p = malloc(chunk);
			if (!p) { fprintf(stderr, "malloc failed at %lu MiB\n", total >> 20); return 1; }
			memset(p, 1, chunk);          /* TOUCH: force the pages resident */
			total += chunk;
			if ((total & (64u*1024*1024 - 1)) == 0)
				fprintf(stderr, "hog: %lu MiB resident\n", total >> 20);
		}
	}

	if (!strcmp(argv[1], "fork")) {
		long max = (argc > 2) ? atol(argv[2]) : -1;   /* -1 = unbounded */
		long n = 0;
		while (max < 0 || n < max) {
			pid_t pid = fork();
			if (pid < 0) {
				fprintf(stderr, "hog: fork failed after %ld children\n", n);
				if (max < 0) pause();   /* stay alive to be inspected */
				return 0;               /* bounded run: report and go */
			}
			if (pid == 0) { pause(); _exit(0); }   /* child: sleep forever */
			n++;
		}
		fprintf(stderr, "hog: forked %ld children\n", n);
		return 0;   /* exiting kills the pid namespace, and with it the children */
	}

	fprintf(stderr, "hog: unknown mode %s\n", argv[1]);
	return 2;
}
