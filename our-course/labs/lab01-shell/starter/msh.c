/* msh.c -- a small Unix shell. Lab 1.
 *
 * What is already here, and works:
 *   - the REPL: prompt (only when stdin is a terminal), read a line, tokenise
 *     it, hand the tokens to execute(), repeat until EOF;
 *   - tokenise(), which splits a line into words and treats < > | & as
 *     one-character tokens of their own.
 *
 * String parsing is not what this lab teaches, so it is given to you. Do not
 * spend the budget improving it.
 *
 * What you write is execute() and everything under it: builtins, fork/exec/
 * wait, redirection, pipelines, background jobs, process groups. Each TODO
 * below names the Part it belongs to.
 *
 * SCOPE -- read this before you start writing a parser:
 *   NO quoting, NO globbing, NO variable expansion, NO subshells, NO comments,
 *   NO `;` or `&&` or `||`, NO `>>` or `2>`, NO here-documents.
 *   A command line is: words, `<file`, `>file`, `|` between stages, and an
 *   optional trailing `&`. That is the whole language. Every hour spent on the
 *   parser is an hour not spent on Part 6, which is the part that teaches most.
 *
 * Build:  make            (-Wall -Wextra -Werror: a warning is a build failure)
 * Test:   make test
 */

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#define MAX_LINE    1024
#define MAX_TOKENS  128
#define MAX_STAGES  32
#define MAX_JOBS    32

/* The status of the last command run. The shell exits with it -- that is how
 * the tests observe a command's exit status, since there is no $? here. */
static int last_status = 0;

/* Non-zero when stdin is a terminal. The prompt, and every terminal operation
 * in Part 6, must be conditional on this: the tests drive the shell down a
 * pipe, and a prompt written into that pipe would land in the diff. */
static int interactive = 0;

/* ------------------------------------------------------------- tokeniser */
/* GIVEN. Do not rewrite this.                                              */

/* Split a line into tokens. Whitespace separates; < > | & are self-delimiting
 * one-character tokens, so `echo hi>out` is four tokens and not two.
 *
 * Tokens are copied into `scratch` rather than carved out of `line`, because
 * "hi>out" needs three NUL terminators in six bytes and there is no room to do
 * that in place. tok[] is left NULL-terminated.
 *
 * Returns the token count, or -1 if the line has too many tokens.
 */
static int
tokenise(const char *line, char *scratch, size_t scratchsz,
         char *tok[], int maxtok)
{
	static const char SPACE[] = " \t\n\r\f\v";
	static const char META[]  = "<>|&";
	const char *p = line;
	size_t o = 0;
	int n = 0;

	while (*p != '\0') {
		while (*p != '\0' && strchr(SPACE, *p) != NULL)
			p++;
		if (*p == '\0')
			break;
		if (n >= maxtok - 1 || o + 2 > scratchsz)
			return -1;

		tok[n++] = &scratch[o];
		if (strchr(META, *p) != NULL) {
			scratch[o++] = *p++;
		} else {
			while (*p != '\0' && strchr(SPACE, *p) == NULL &&
			       strchr(META, *p) == NULL) {
				if (o + 2 > scratchsz)
					return -1;
				scratch[o++] = *p++;
			}
		}
		scratch[o++] = '\0';
	}
	tok[n] = NULL;
	return n;
}

/* ------------------------------------------------------------ your parser */

/* A suggested shape for one stage of a pipeline. `a < in | b | c > out` is
 * three of these. You are not obliged to use it, but Part 4 is much easier if
 * the parse is finished before any forking starts. */
struct stage {
	char *argv[MAX_TOKENS];   /* NULL-terminated, ready for execvp   */
	int   argc;
	char *infile;             /* from `< file`, or NULL              */
	char *outfile;            /* from `> file`, or NULL              */
};

/* --------------------------------------------------------------- execute */

static void
execute(char *tok[], int ntok, const char *cmdline)
{
	(void)tok;
	(void)ntok;
	(void)cmdline;

	/* TODO Part 1 -- builtins.
	 *
	 *   exit [N]   end the shell. With no argument, exit with last_status;
	 *              with one, with N.
	 *   cd [DIR]   chdir(2). No argument means $HOME. On failure, report to
	 *              stderr and set last_status = 1.
	 *   jobs       (Part 5) list the background jobs still running.
	 *
	 * Builtins run in the shell's own process, and only when the whole
	 * command is one stage in the foreground.
	 *
	 * Write a comment here saying why `cd` CANNOT be an external program.
	 * The answer is a fact about a system call and about what fork(2)
	 * copies; two or three sentences. This is graded -- see
	 * solutions/README.md once you have written yours.
	 */

	/* TODO Part 2 -- fork, exec, wait.
	 *
	 *   fork(); in the child execvp(); in the parent waitpid().
	 *   Record the child's exit status in last_status -- WIFEXITED /
	 *   WEXITSTATUS, and 128 + WTERMSIG if it died of a signal.
	 *   If execvp fails, the CHILD reports it and calls _exit(127) for
	 *   "not found" (ENOENT) or _exit(126) for anything else. Use _exit,
	 *   not exit: a forked child shares the parent's stdio buffers.
	 *   Call fflush(NULL) before you fork, for the same reason.
	 *
	 * Before you write the waitpid, leave it out on purpose, run
	 *     ./slowprint.sh
	 *     echo B
	 * and then, from another terminal,
	 *     ps --ppid $(pgrep -n msh) -o pid,ppid,stat,comm
	 * Look at the process in state Z. Then put the wait back.
	 * (`--ppid` matters: a bare `ps` lists only processes on your own
	 * terminal, so from a second terminal it prints nothing at all.)
	 */

	/* TODO Part 3 -- redirection.
	 *
	 *   `< file` and `> file`, anywhere in the command's tokens.
	 *   Do it in the CHILD, between fork and exec: open the file, dup2 it
	 *   onto 0 or 1, close the original. The program you launch must need
	 *   no cooperation and must not be able to tell.
	 *   `>` creates with O_CREAT|O_TRUNC and mode 0666.
	 *   If the open fails, the child reports it and _exit(1) -- the command
	 *   must not run.
	 */

	/* TODO Part 4 -- pipelines of arbitrary length.
	 *
	 *   n stages, n-1 pipes. pipe i joins stage i's stdout to stage i+1's
	 *   stdin. Fork every stage, then wait for all of them; the pipeline's
	 *   status is the LAST stage's.
	 *
	 *   The part everyone gets wrong is closing: a read end sees EOF only
	 *   when EVERY copy of the matching write end is closed, and the parent
	 *   holds a copy of both ends of every pipe it creates, and every child
	 *   forked afterwards inherits a copy too. Get the descriptor bookkeeping
	 *   wrong and the shell does not print a wrong answer, it never returns
	 *   -- and the test harness reports a timeout.
	 */

	/* TODO Part 5 -- background jobs.
	 *
	 *   A trailing `&`: do not wait. Print `[n] pid` and record the job in
	 *   a table of your own. Add the `jobs` builtin.
	 *   Reap asynchronously -- either a SIGCHLD handler, or a
	 *   waitpid(-1, &st, WNOHANG) sweep at the top of the loop. If you use
	 *   a handler, only async-signal-safe calls inside it, and it must not
	 *   swallow the foreground job's status.
	 */

	/* TODO Part 6 -- signals and process groups.
	 *
	 *   setpgid() every job into a process group of its own -- in the child
	 *   AND in the parent, because either may run first.
	 *   The shell ignores SIGINT, SIGQUIT, SIGTSTP, SIGTTIN, SIGTTOU;
	 *   every child puts them back to SIG_DFL before exec.
	 *   For a foreground job, tcsetpgrp() the terminal to the job's group
	 *   and take it back afterwards -- but only when `interactive`.
	 *   Part 6 is checked by the manual checklist in README.md, not here.
	 */
}

/* ------------------------------------------------------------------ main */
/* GIVEN. The REPL is written for you.                                      */

int
main(void)
{
	char line[MAX_LINE];
	char scratch[2 * MAX_LINE + 2];
	char *tok[MAX_TOKENS];
	char cmdline[MAX_LINE];

	interactive = isatty(STDIN_FILENO);

	/* TODO Part 6: put the shell in its own process group, give it the
	 * terminal, and make it ignore the terminal's signals -- when
	 * interactive, and only then. */

	for (;;) {
		int ntok;

		/* TODO Part 5: sweep up any background job that has finished,
		 * here, before the prompt. */

		if (interactive) {
			printf("msh> ");
			fflush(stdout);
		}

		if (fgets(line, sizeof line, stdin) == NULL) {
			if (interactive)
				printf("\n");
			break;                   /* EOF: Ctrl-D, or end of script */
		}

		/* keep a copy of the line as typed, for the jobs listing */
		snprintf(cmdline, sizeof cmdline, "%s", line);
		cmdline[strcspn(cmdline, "\n")] = '\0';

		ntok = tokenise(line, scratch, sizeof scratch, tok, MAX_TOKENS);
		if (ntok < 0) {
			fprintf(stderr, "msh: line too complex\n");
			last_status = 2;
			continue;
		}
		if (ntok == 0)
			continue;                /* blank or whitespace-only line */

		execute(tok, ntok, cmdline);
	}

	/* The shell's own exit status is the last command's. The tests rely on
	 * this: it is the only way to observe a command's status from outside. */
	return last_status;
}
