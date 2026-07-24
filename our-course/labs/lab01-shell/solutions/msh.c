/* msh.c -- reference solution for Lab 1, Parts 1-6.
 *
 * A small Unix shell: builtins, fork/exec/wait, < and > redirection,
 * pipelines of arbitrary length, background jobs with asynchronous reaping,
 * and process groups with terminal control.
 *
 * Scope, as specified: no quoting, no globbing, no variable expansion, no
 * subshells, no comments, no && || ; , no here-documents, no >>, no 2>.
 *
 * SPOILER. This is the answer. See solutions/README.md.
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

static int   last_status = 0;      /* status of the last command run       */
static int   interactive = 0;      /* stdin is a terminal                  */
static pid_t shell_pgid;           /* our own process group                */

/* ------------------------------------------------------------------ jobs */

struct job {
	int   used;
	int   id;                       /* job number, as printed by jobs   */
	pid_t pgid;
	char  cmd[MAX_LINE];
};

static struct job jobs[MAX_JOBS];
static int next_job_id = 1;

static struct job *
job_of_pgid(pid_t pgid)
{
	for (int i = 0; i < MAX_JOBS; i++)
		if (jobs[i].used && jobs[i].pgid == pgid)
			return &jobs[i];
	return NULL;
}

static void
job_add(pid_t pgid, const char *cmd)
{
	for (int i = 0; i < MAX_JOBS; i++) {
		if (!jobs[i].used) {
			jobs[i].used = 1;
			jobs[i].id   = next_job_id++;
			jobs[i].pgid = pgid;
			snprintf(jobs[i].cmd, sizeof jobs[i].cmd, "%s", cmd);
			printf("[%d] %d\n", jobs[i].id, (int)pgid);
			fflush(stdout);
			return;
		}
	}
	fprintf(stderr, "msh: too many jobs\n");
}

/* Reap whatever has finished, without blocking. Called at the top of every
 * prompt and from the `jobs` builtin.
 *
 * This is the alternative the spec allows to a SIGCHLD handler, and it is the
 * one the reference takes: a handler runs between any two instructions of the
 * main loop, so everything it touches has to be async-signal-safe, and the
 * blocking waitpid() that collects the *foreground* job then has to be written
 * to tolerate the handler having got there first. Sweeping at the prompt has
 * neither problem -- and no child of ours can outlive its own job entry,
 * because the entry is created before the prompt that sweeps it.
 */
static void
reap_finished(void)
{
	int status;
	pid_t pid;

	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		/* Jobs are process groups; the job is over when its leader is.
		 * (Every stage of a background pipeline shares the leader's
		 * pgid, so the other stages are reaped here and ignored.) */
		struct job *j = job_of_pgid(pid);
		if (j != NULL) {
			if (interactive) {
				printf("[%d] done   %s\n", j->id, j->cmd);
				fflush(stdout);
			}
			j->used = 0;
		}
	}
}

static void
builtin_jobs(void)
{
	reap_finished();
	for (int i = 0; i < MAX_JOBS; i++)
		if (jobs[i].used)
			printf("[%d] running   %s\n", jobs[i].id, jobs[i].cmd);
	fflush(stdout);
}

/* ------------------------------------------------------------- tokeniser */

/* Split a line into tokens. Whitespace separates; < > | & are self-delimiting
 * one-character tokens, so `echo hi>out` is four tokens and not two.
 *
 * Tokens are copied into `scratch` rather than carved out of `line`, because
 * "hi>out" needs three NUL terminators in six bytes and there is no room to
 * do that in place.
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

/* ---------------------------------------------------------------- parser */

struct stage {
	char *argv[MAX_TOKENS];
	int   argc;
	char *infile;
	char *outfile;
};

/* Turn a token list into an array of pipeline stages.
 * Returns the number of stages, or -1 on a syntax error (already reported). */
static int
parse(char *tok[], int ntok, struct stage st[], int maxstages, int *bg)
{
	int ns = 0;
	struct stage *cur;

	*bg = 0;
	memset(&st[0], 0, sizeof st[0]);
	cur = &st[0];
	ns = 1;

	for (int i = 0; i < ntok; i++) {
		char *t = tok[i];

		if (strcmp(t, "|") == 0) {
			if (cur->argc == 0) {
				fprintf(stderr, "msh: syntax error near '|'\n");
				return -1;
			}
			if (ns >= maxstages) {
				fprintf(stderr, "msh: pipeline too long\n");
				return -1;
			}
			memset(&st[ns], 0, sizeof st[ns]);
			cur = &st[ns++];
		} else if (strcmp(t, "<") == 0 || strcmp(t, ">") == 0) {
			if (i + 1 >= ntok || strchr("<>|&", tok[i + 1][0]) != NULL) {
				fprintf(stderr,
				        "msh: syntax error: %s needs a filename\n", t);
				return -1;
			}
			if (t[0] == '<')
				cur->infile = tok[++i];
			else
				cur->outfile = tok[++i];
		} else if (strcmp(t, "&") == 0) {
			if (i != ntok - 1) {
				fprintf(stderr, "msh: syntax error: & must end the line\n");
				return -1;
			}
			*bg = 1;
		} else {
			if (cur->argc >= MAX_TOKENS - 1) {
				fprintf(stderr, "msh: too many arguments\n");
				return -1;
			}
			cur->argv[cur->argc++] = t;
		}
	}

	if (cur->argc == 0) {
		if (ns > 1 || *bg) {
			fprintf(stderr, "msh: syntax error: empty command\n");
			return -1;
		}
		return 0;                       /* blank line */
	}
	for (int i = 0; i < ns; i++)
		st[i].argv[st[i].argc] = NULL;
	return ns;
}

/* -------------------------------------------------------------- builtins */

/* `cd` cannot be an external program.
 *
 * An external program runs in a child created by fork(2), and chdir(2) changes
 * only the calling process's working directory -- it is a per-process
 * attribute held in the kernel's task structure, not a shared one. The child
 * would change its own directory and then exit, and the shell, which never
 * moved, would carry on exactly where it was. There is no system call that
 * lets one process set another's working directory, and that is not an
 * oversight: it is the same isolation that stops a program you run from
 * rearranging your shell. So the change has to happen *in the shell's own
 * process*, which means the shell must implement it itself.
 *
 * `exit` is a builtin for the same shape of reason: only the shell can end the
 * shell. `jobs` is a builtin because the job table is the shell's own memory
 * and a child could not see it.
 */
static int
builtin_cd(char *argv[], int argc)
{
	const char *dir;

	if (argc > 2) {
		fprintf(stderr, "msh: cd: too many arguments\n");
		return 1;
	}
	if (argc == 2) {
		dir = argv[1];
	} else {
		dir = getenv("HOME");
		if (dir == NULL) {
			fprintf(stderr, "msh: cd: HOME not set\n");
			return 1;
		}
	}
	if (chdir(dir) != 0) {
		fprintf(stderr, "msh: cd: %s: %s\n", dir, strerror(errno));
		return 1;
	}
	return 0;
}

/* Returns 1 if the (single-stage, foreground) command was a builtin and has
 * been run; 0 if it is an ordinary command. */
static int
run_builtin(struct stage *s)
{
	if (strcmp(s->argv[0], "exit") == 0) {
		int code = last_status;
		if (s->argc > 1) {
			char *end;
			long v;
			errno = 0;
			v = strtol(s->argv[1], &end, 10);
			/* strtol reports failure by leaving *end unconsumed, not
			 * by returning 0 -- "abc" and "0" both give 0. bash
			 * exits 2 with a diagnostic here; silently treating a
			 * bad argument as zero is the tempting wrong answer. */
			if (end == s->argv[1] || *end != '\0' || errno == ERANGE) {
				fprintf(stderr,
					"msh: exit: %s: numeric argument required\n",
					s->argv[1]);
				exit(2);
			}
			code = (int)v;
		}
		exit(code & 0xff);
	}
	if (strcmp(s->argv[0], "cd") == 0) {
		last_status = builtin_cd(s->argv, s->argc);
		return 1;
	}
	if (strcmp(s->argv[0], "jobs") == 0) {
		builtin_jobs();
		last_status = 0;
		return 1;
	}
	return 0;
}

/* -------------------------------------------------------------- children */

/* Everything below happens in the child, between fork and exec: the whole
 * point of the two calls being separate. */
static void
child_redirect(struct stage *s)
{
	int fd;

	if (s->infile != NULL) {
		fd = open(s->infile, O_RDONLY);
		if (fd < 0) {
			fprintf(stderr, "msh: %s: %s\n", s->infile, strerror(errno));
			_exit(1);
		}
		if (dup2(fd, STDIN_FILENO) < 0) {
			fprintf(stderr, "msh: dup2: %s\n", strerror(errno));
			_exit(1);
		}
		close(fd);
	}
	if (s->outfile != NULL) {
		fd = open(s->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
		if (fd < 0) {
			fprintf(stderr, "msh: %s: %s\n", s->outfile, strerror(errno));
			_exit(1);
		}
		if (dup2(fd, STDOUT_FILENO) < 0) {
			fprintf(stderr, "msh: dup2: %s\n", strerror(errno));
			_exit(1);
		}
		close(fd);
	}
}

static void
child_default_signals(void)
{
	signal(SIGINT,  SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTSTP, SIG_DFL);
	signal(SIGTTIN, SIG_DFL);
	signal(SIGTTOU, SIG_DFL);
}

/* ------------------------------------------------------------- pipelines */

/*
 * Run a pipeline of `ns` stages.
 *
 *   pipes[i] joins stage i (writes to pipes[i][1]) to stage i+1 (reads from
 *   pipes[i][0]). Stage i's stdin is pipes[i-1][0], its stdout pipes[i][1].
 *
 * The rule that makes it terminate: a read end sees EOF only when EVERY copy
 * of the matching write end is closed. The parent gets a copy of both ends of
 * every pipe it creates, and every child forked after a pipe was created
 * inherits a copy too. So each child closes every pipe descriptor it did not
 * dup, and the parent closes both ends of pipe i-1 as soon as the child that
 * needed it has been forked.
 */
static void
run_pipeline(struct stage st[], int ns, int bg, const char *cmdline)
{
	int   pipes[MAX_STAGES][2];
	pid_t pids[MAX_STAGES];
	pid_t pgid = 0;
	int   status = 0;

	/* stdio buffers are copied by fork(); flush before, or a buffered
	 * prompt or job notice comes out once per child. */
	fflush(NULL);

	for (int i = 0; i < ns; i++) {
		if (i < ns - 1 && pipe(pipes[i]) < 0) {
			fprintf(stderr, "msh: pipe: %s\n", strerror(errno));
			last_status = 1;
			return;
		}

		pids[i] = fork();
		if (pids[i] < 0) {
			fprintf(stderr, "msh: fork: %s\n", strerror(errno));
			last_status = 1;
			return;
		}

		if (pids[i] == 0) {
			/* ---- child ---- */
			child_default_signals();

			/* Part 6: every job is its own process group, so that a
			 * Ctrl-C at the terminal reaches the whole pipeline and
			 * nothing else. Done in the child AND in the parent
			 * because either may run first. */
			setpgid(0, pgid);

			if (i > 0) {
				if (dup2(pipes[i - 1][0], STDIN_FILENO) < 0)
					_exit(1);
			}
			if (i < ns - 1) {
				if (dup2(pipes[i][1], STDOUT_FILENO) < 0)
					_exit(1);
			}
			/* close every pipe descriptor still open here: the two
			 * ends of our own pipe, and the ends of the previous
			 * one that we either dup'd or never wanted. */
			if (i > 0) {
				close(pipes[i - 1][0]);
				close(pipes[i - 1][1]);
			}
			if (i < ns - 1) {
				close(pipes[i][0]);
				close(pipes[i][1]);
			}

			/* File redirection last, so an explicit < or > on the
			 * first or last stage overrides the pipe. */
			child_redirect(&st[i]);

			execvp(st[i].argv[0], st[i].argv);
			fprintf(stderr, "msh: %s: %s\n", st[i].argv[0],
			        strerror(errno));
			_exit(errno == ENOENT ? 127 : 126);
		}

		/* ---- parent ---- */
		if (pgid == 0)
			pgid = pids[i];
		setpgid(pids[i], pgid);

		/* Pipe i-1 is now wired into both the child that writes it and
		 * the child that reads it. The parent's copies are what would
		 * keep the reader from ever seeing EOF. */
		if (i > 0) {
			close(pipes[i - 1][0]);
			close(pipes[i - 1][1]);
		}
	}
	/* Nothing left to close here: the last iteration closed pipe ns-2, and
	 * every earlier pipe was closed at the end of the iteration after the
	 * one that created it. If you are unsure yours is as tidy, run
	 *     ls -l /proc/self/fd
	 * as a command inside your own shell: `ls` inherits whatever the shell
	 * had open, so the listing should be 0, 1, 2 and the one `ls` opened to
	 * read the directory -- no pipes -- however many pipelines have run. */

	if (bg) {
		job_add(pgid, cmdline);
		last_status = 0;
		return;
	}

	/* Foreground: hand the terminal to the job, wait for every stage, take
	 * the status of the last one, take the terminal back. */
	if (interactive)
		tcsetpgrp(STDIN_FILENO, pgid);

	for (int i = 0; i < ns; i++) {
		int st_i;
		while (waitpid(pids[i], &st_i, 0) < 0) {
			if (errno == EINTR)
				continue;
			st_i = 0;
			break;
		}
		if (i == ns - 1)
			status = st_i;
	}

	if (interactive) {
		tcsetpgrp(STDIN_FILENO, shell_pgid);
		if (WIFSIGNALED(status))
			printf("\n");
	}

	if (WIFEXITED(status))
		last_status = WEXITSTATUS(status);
	else if (WIFSIGNALED(status))
		last_status = 128 + WTERMSIG(status);
	else
		last_status = 1;
}

/* ------------------------------------------------------------------ main */

static void
shell_init(void)
{
	interactive = isatty(STDIN_FILENO);

	if (!interactive) {
		shell_pgid = getpgrp();
		return;
	}

	/* If we were started in the background, stop until someone brings us
	 * to the foreground; otherwise our tcsetpgrp below would SIGTTOU us. */
	while (tcgetpgrp(STDIN_FILENO) != (shell_pgid = getpgrp()))
		kill(-shell_pgid, SIGTTIN);

	/* Part 6: the shell must ignore the terminal's signals. Ctrl-C is
	 * delivered to the foreground process *group*; if the shell stayed in
	 * that group, or listened, it would die with its own job. */
	signal(SIGINT,  SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);

	shell_pgid = getpid();
	if (setpgid(shell_pgid, shell_pgid) < 0 && errno != EPERM) {
		fprintf(stderr, "msh: cannot put the shell in its own group: %s\n",
		        strerror(errno));
		exit(1);
	}
	tcsetpgrp(STDIN_FILENO, shell_pgid);
}

int
main(void)
{
	char line[MAX_LINE];
	char scratch[2 * MAX_LINE + 2];
	char *tok[MAX_TOKENS];
	struct stage st[MAX_STAGES];

	shell_init();

	for (;;) {
		int ntok, ns, bg;
		char cmdline[MAX_LINE];

		reap_finished();

		if (interactive) {
			printf("msh> ");
			fflush(stdout);
		}

		if (fgets(line, sizeof line, stdin) == NULL) {
			if (interactive)
				printf("\n");
			break;                          /* EOF: Ctrl-D */
		}

		snprintf(cmdline, sizeof cmdline, "%s", line);
		cmdline[strcspn(cmdline, "\n")] = '\0';

		ntok = tokenise(line, scratch, sizeof scratch, tok, MAX_TOKENS);
		if (ntok < 0) {
			fprintf(stderr, "msh: line too complex\n");
			last_status = 2;
			continue;
		}
		if (ntok == 0)
			continue;                       /* blank line */

		ns = parse(tok, ntok, st, MAX_STAGES, &bg);
		if (ns < 0) {
			last_status = 2;
			continue;
		}
		if (ns == 0)
			continue;

		/* Builtins run in the shell itself, and only as a whole
		 * foreground command -- `cd x | wc` would have to run in a
		 * child, where the chdir would be useless. */
		if (ns == 1 && !bg && run_builtin(&st[0]))
			continue;

		run_pipeline(st, ns, bg, cmdline);
	}

	return last_status;
}
