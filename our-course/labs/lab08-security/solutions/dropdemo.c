/* dropdemo.c -- Lab 8 Part 4: acquire, use, drop privilege permanently, and
 * show it cannot be regained.  REFERENCE / MODEL (Part 4 is rubric-marked).
 *
 * Build:  cc -Wall -Wextra -o dropdemo dropdemo.c
 * Run privileged to see the real drop:  sudo chown root dropdemo && sudo chmod u+s dropdemo && ./dropdemo
 * Run as yourself to see the MODEL path (no root needed).
 *
 * The lesson (ch. 55): dropping privilege is an ORDERED sequence, and setuid()
 * alone is not it.  A process has real / effective / SAVED set-user-IDs; while
 * the effective ID is still 0 the saved ID can be restored, so a permanent drop
 * must clear the saved ID too, and must drop group privilege and supplementary
 * groups FIRST -- once uid 0 is gone you can no longer call setgroups().
 *
 * The correct order:
 *   1. setgroups()  -- drop supplementary groups   (needs privilege; do it first)
 *   2. setgid(rgid) -- drop the group id, saved gid included
 *   3. setuid(ruid) -- drop the user id, saved uid included  (do it LAST)
 * then PROVE it: an attempt to seteuid(0) must fail with EPERM.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <grp.h>

static void report(const char *when)
{
	uid_t r, e, s;
	gid_t gr, ge, gs;
	getresuid(&r, &e, &s);
	getresgid(&gr, &ge, &gs);
	printf("  %-8s uid(r=%d e=%d saved=%d)  gid(r=%d e=%d saved=%d)\n",
	       when, (int)r, (int)e, (int)s, (int)gr, (int)ge, (int)gs);
}

int main(void)
{
	uid_t ruid = getuid();
	gid_t rgid = getgid();

	report("start");

	if (geteuid() != 0) {
		/* MODEL path: not privileged, so narrate the sequence instead. */
		printf("model: not running as root (euid=%d).\n", (int)geteuid());
		printf("model: with privilege I would, in this order:\n");
		printf("model:   setgroups(0, NULL);   drop supplementary groups\n");
		printf("model:   setgid(%d);           drop gid incl. saved-set-gid\n", (int)rgid);
		printf("model:   setuid(%d);           drop uid incl. saved-set-uid\n", (int)ruid);
		printf("model: then seteuid(0) would fail with EPERM -- saved-set-uid "
		       "is gone, so the privilege is unrecoverable.\n");
		printf("RESULT: modelled (run setuid-root to execute the real drop)\n");
		return 0;
	}

	/* PRIVILEGED path: acquire (we hold euid 0), use it, then drop for good. */
	printf("privileged: euid=0; (pretend we did the one root-only task here)\n");

	if (setgroups(0, NULL) != 0) { perror("setgroups"); return 1; }
	if (setgid(rgid) != 0)       { perror("setgid");    return 1; }
	if (setuid(ruid) != 0)       { perror("setuid");    return 1; }
	report("dropped");

	/* PROVE the drop is permanent: regaining root must fail. */
	if (seteuid(0) == 0) {
		printf("RESULT: FAIL -- regained euid 0; the drop did not stick\n");
		return 1;
	}
	printf("  regain   seteuid(0) -> failed (%s), as required\n", strerror(errno));
	report("final");
	printf("RESULT: dropped permanently; privilege cannot be regained\n");
	return 0;
}
