/*
 * msh - a minimal Unix shell (reference solution for Lab 1)
 *
 * Implements: simple commands with PATH search, the `exit`/`cd`/`wait`
 * builtins, I/O redirection (< > >> 2>), pipelines of arbitrary length,
 * background jobs (&) with SIGCHLD reaping, and SIGINT handling that kills
 * the foreground job rather than the shell.
 *
 * The interesting parts are the "why" comments, not the "what":
 *   - why builtins run in the shell process (cd, exit, wait),
 *   - why the parent must close every pipe fd it does not need,
 *   - why we block SIGCHLD around the foreground wait,
 *   - how SIGINT is forwarded to the foreground process group.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define MAXARGS 64      /* max argv entries per command                  */
#define MAXCMDS 32      /* max stages in a pipeline                      */
#define MAXTOK  256     /* max tokens on one input line                  */
#define MAXBG   128     /* max tracked background child pids             */

/* ------------------------------------------------------------------ */
/* Data model: a line parses into a pipeline of commands.             */
/* ------------------------------------------------------------------ */

struct redir {
    char *infile;   /* <  file      (NULL if none)                     */
    char *outfile;  /* >  or >> file (NULL if none)                    */
    char *errfile;  /* 2> file      (NULL if none)                     */
    int   append;   /* 1 if the output redirection was >>              */
};

struct cmd {
    char *argv[MAXARGS];
    int   argc;
    struct redir redir;
};

struct pipeline {
    struct cmd cmds[MAXCMDS];
    int  ncmds;
    int  background;    /* trailing & ?                                */
};

/* ------------------------------------------------------------------ */
/* Global signal-visible state.                                       */
/* ------------------------------------------------------------------ */

/* Process group of the running foreground job (0 = none). The SIGINT
 * handler reads this to forward the signal to the job. */
static volatile sig_atomic_t fg_pgid = 0;

/* Background children still running. Mutated by the SIGCHLD handler and
 * by main-line code; main-line code blocks SIGCHLD before touching it. */
static volatile sig_atomic_t bg_count = 0;
static pid_t bg_pids[MAXBG];

static int last_status = 0;   /* exit status of the last foreground job */

/* ------------------------------------------------------------------ */
/* Tokenizer (provided to students unchanged in the starter).         */
/*                                                                    */
/* Splits a line on whitespace, and recognizes the metacharacters     */
/* < > >> 2> | & as standalone tokens even without surrounding        */
/* spaces. Every token is heap-allocated; the caller frees them.      */
/* ------------------------------------------------------------------ */
static int tokenize(char *line, char *tok[], int maxtok)
{
    int n = 0;
    char *p = line;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\n')
            p++;
        if (!*p)
            break;
        if (n >= maxtok - 1) {
            fprintf(stderr, "msh: too many tokens\n");
            break;
        }
        if (*p == '|') { tok[n++] = strdup("|"); p++; continue; }
        if (*p == '&') { tok[n++] = strdup("&"); p++; continue; }
        if (*p == '<') { tok[n++] = strdup("<"); p++; continue; }
        if (*p == '>') {
            if (p[1] == '>') { tok[n++] = strdup(">>"); p += 2; }
            else             { tok[n++] = strdup(">");  p += 1; }
            continue;
        }
        /* "2>" only when the 2 sits at a token boundary (so "file2"
         * and a bare "2" argument keep the digit). */
        if (*p == '2' && p[1] == '>') { tok[n++] = strdup("2>"); p += 2; continue; }

        /* ordinary word: run to the next delimiter/metacharacter */
        char *start = p;
        while (*p && !strchr(" \t\n<>|&", *p))
            p++;
        int len = (int)(p - start);
        char *w = malloc((size_t)len + 1);
        memcpy(w, start, (size_t)len);
        w[len] = '\0';
        tok[n++] = w;
    }
    tok[n] = NULL;
    return n;
}

static void free_tokens(char *tok[], int n)
{
    for (int i = 0; i < n; i++)
        free(tok[i]);
}

/* ------------------------------------------------------------------ */
/* Parser: tokens -> struct pipeline.                                 */
/* Returns 0 on a runnable pipeline, 1 on an empty line, -1 on a      */
/* syntax error (message already printed).                            */
/* ------------------------------------------------------------------ */
static int parse(char *tok[], int ntok, struct pipeline *pl)
{
    memset(pl, 0, sizeof *pl);
    pl->ncmds = 1;
    struct cmd *c = &pl->cmds[0];

    for (int i = 0; i < ntok; i++) {
        char *t = tok[i];

        if (!strcmp(t, "|")) {
            if (c->argc == 0) {
                fprintf(stderr, "msh: syntax error near '|'\n");
                return -1;
            }
            if (pl->ncmds >= MAXCMDS) {
                fprintf(stderr, "msh: too many pipeline stages\n");
                return -1;
            }
            c = &pl->cmds[pl->ncmds++];
            continue;
        }
        if (!strcmp(t, "&")) {
            if (i != ntok - 1) {
                fprintf(stderr, "msh: syntax error near '&'\n");
                return -1;
            }
            pl->background = 1;
            continue;
        }
        if (!strcmp(t, "<") || !strcmp(t, ">") ||
            !strcmp(t, ">>") || !strcmp(t, "2>")) {
            if (i + 1 >= ntok) {
                fprintf(stderr, "msh: syntax error: expected filename after '%s'\n", t);
                return -1;
            }
            char *fn = tok[++i];
            if      (!strcmp(t, "<"))  c->redir.infile  = fn;
            else if (!strcmp(t, "2>")) c->redir.errfile = fn;
            else { c->redir.outfile = fn; c->redir.append = !strcmp(t, ">>"); }
            continue;
        }
        if (c->argc >= MAXARGS - 1) {
            fprintf(stderr, "msh: too many arguments\n");
            return -1;
        }
        c->argv[c->argc++] = t;
    }

    for (int k = 0; k < pl->ncmds; k++)
        pl->cmds[k].argv[pl->cmds[k].argc] = NULL;

    if (pl->ncmds == 1 && pl->cmds[0].argc == 0)
        return 1;   /* blank line */

    for (int k = 0; k < pl->ncmds; k++)
        if (pl->cmds[k].argc == 0) {
            fprintf(stderr, "msh: syntax error: empty command\n");
            return -1;
        }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Signal handlers.                                                   */
/* ------------------------------------------------------------------ */

/* SIGINT: the shell catches it (so it never dies) and forwards it to
 * the foreground job's process group. Interactively the terminal driver
 * would deliver Ctrl-C to the whole foreground group directly; here we
 * emulate that semantic for a signal delivered to the shell process. */
static void sigint_handler(int sig)
{
    (void)sig;
    if (fg_pgid > 0)
        kill(-(pid_t)fg_pgid, SIGINT);
}

/* SIGCHLD: reap finished *background* children. Foreground waits run
 * with SIGCHLD blocked, so this only ever fires for background jobs. */
static void sigchld_handler(int sig)
{
    (void)sig;
    int saved = errno;
    pid_t p;
    int st;
    while ((p = waitpid(-1, &st, WNOHANG)) > 0) {
        for (int i = 0; i < bg_count; i++) {
            if (bg_pids[i] == p) {
                bg_pids[i] = bg_pids[bg_count - 1];
                bg_count--;
                break;
            }
        }
    }
    errno = saved;
}

static void install_handlers(void)
{
    struct sigaction sa;

    /* No SA_RESTART: a SIGINT delivered while we block in getline() or
     * waitpid() must interrupt that call (EINTR) so we can print a fresh
     * prompt / notice the killed job. */
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);
}

/* ------------------------------------------------------------------ */
/* Builtins.                                                          */
/*                                                                    */
/* cd and exit MUST run in the shell process itself: a forked child   */
/* changing directory or exiting would affect only that child, leaving */
/* the parent shell unchanged. `wait` is a builtin for the same       */
/* reason - only the shell knows its background children.             */
/* ------------------------------------------------------------------ */

/* Wait for every background job to finish. Uses the block/sigsuspend
 * idiom so there is no lost-wakeup race with the SIGCHLD handler. */
static void builtin_wait(void)
{
    sigset_t chld, old;
    sigemptyset(&chld);
    sigaddset(&chld, SIGCHLD);
    sigprocmask(SIG_BLOCK, &chld, &old);
    while (bg_count > 0)
        sigsuspend(&old);           /* atomically unblock + wait      */
    sigprocmask(SIG_SETMASK, &old, NULL);
}

static int is_builtin(const char *cmd)
{
    return !strcmp(cmd, "exit") || !strcmp(cmd, "cd") || !strcmp(cmd, "wait");
}

static void run_builtin(struct cmd *c)
{
    if (!strcmp(c->argv[0], "exit")) {
        int code = (c->argc > 1) ? atoi(c->argv[1]) : last_status;
        exit(code);
    } else if (!strcmp(c->argv[0], "cd")) {
        const char *dir = (c->argc > 1) ? c->argv[1] : getenv("HOME");
        if (!dir)
            dir = "/";
        if (chdir(dir) < 0) {
            fprintf(stderr, "msh: cd: %s: %s\n", dir, strerror(errno));
            last_status = 1;
        } else {
            last_status = 0;
        }
    } else { /* wait */
        builtin_wait();
        last_status = 0;
    }
}

/* ------------------------------------------------------------------ */
/* Redirection, applied in the child after the pipe fds are wired.    */
/* Applying redirection last means an explicit < > 2> at a pipeline    */
/* end overrides the pipe wiring for that fd.                          */
/* ------------------------------------------------------------------ */
static void apply_redir(struct redir *r)
{
    if (r->infile) {
        int fd = open(r->infile, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "msh: %s: %s\n", r->infile, strerror(errno));
            _exit(1);
        }
        dup2(fd, 0);
        close(fd);
    }
    if (r->outfile) {
        int flags = O_WRONLY | O_CREAT | (r->append ? O_APPEND : O_TRUNC);
        int fd = open(r->outfile, flags, 0644);
        if (fd < 0) {
            fprintf(stderr, "msh: %s: %s\n", r->outfile, strerror(errno));
            _exit(1);
        }
        dup2(fd, 1);
        close(fd);
    }
    if (r->errfile) {
        int fd = open(r->errfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            fprintf(stderr, "msh: %s: %s\n", r->errfile, strerror(errno));
            _exit(1);
        }
        dup2(fd, 2);
        close(fd);
    }
}

/* Record background pids so the SIGCHLD handler and `wait` can find
 * them. Caller holds SIGCHLD blocked. */
static void add_bg(pid_t *pids, int n)
{
    for (int i = 0; i < n && bg_count < MAXBG; i++)
        bg_pids[bg_count++] = pids[i];
}

/* ------------------------------------------------------------------ */
/* The core: fork/exec a whole pipeline, wiring pipes between stages. */
/* ------------------------------------------------------------------ */
static void run_pipeline(struct pipeline *pl)
{
    int n = pl->ncmds;

    /* A lone foreground builtin runs in the shell (see run_builtin). */
    if (n == 1 && is_builtin(pl->cmds[0].argv[0]) && !pl->background) {
        run_builtin(&pl->cmds[0]);
        return;
    }

    /* Block SIGCHLD across fork + wait. This keeps the async reaper
     * from stealing a foreground child (whose status we need) and makes
     * the background bookkeeping race-free. */
    sigset_t chld, old;
    sigemptyset(&chld);
    sigaddset(&chld, SIGCHLD);
    sigprocmask(SIG_BLOCK, &chld, &old);

    int   prev_read = -1;    /* read end of the pipe from the last stage */
    pid_t pgid = 0;          /* process group for the whole job          */
    pid_t pids[MAXCMDS];
    int   nforked = 0;

    for (int i = 0; i < n; i++) {
        int pfd[2] = { -1, -1 };
        if (i < n - 1 && pipe(pfd) < 0) {
            fprintf(stderr, "msh: pipe: %s\n", strerror(errno));
            break;
        }

        pid_t pid = fork();
        if (pid < 0) {
            fprintf(stderr, "msh: fork: %s\n", strerror(errno));
            if (pfd[0] != -1) close(pfd[0]);
            if (pfd[1] != -1) close(pfd[1]);
            break;
        }

        if (pid == 0) {
            /* ---------------- child ---------------- */
            /* Restore default signal behaviour and unblock SIGCHLD so
             * the exec'd program starts from a clean slate. */
            signal(SIGINT, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);
            sigprocmask(SIG_SETMASK, &old, NULL);

            /* Join the job's process group (pgid==0 => become leader). */
            setpgid(0, pgid);

            /* Wire this stage's stdin/stdout to the pipes. */
            if (prev_read != -1) dup2(prev_read, 0);
            if (pfd[1]    != -1) dup2(pfd[1], 1);

            /* Close every pipe fd: after dup2 the originals are extra
             * references. If we leaked the write end here, the next
             * stage would never see EOF. */
            if (prev_read != -1) close(prev_read);
            if (pfd[0]    != -1) close(pfd[0]);
            if (pfd[1]    != -1) close(pfd[1]);

            apply_redir(&pl->cmds[i].redir);

            execvp(pl->cmds[i].argv[0], pl->cmds[i].argv);
            fprintf(stderr, "msh: %s: %s\n",
                    pl->cmds[i].argv[0], strerror(errno));
            _exit(127);
        }

        /* ---------------- parent ---------------- */
        if (pgid == 0)
            pgid = pid;             /* first child leads the group      */
        setpgid(pid, pgid);         /* set from both sides: race-free   */
        pids[nforked++] = pid;

        /* The parent must close both ends it just handed to children.
         * The classic hang: if the parent keeps a pipe's write end open,
         * the reader never sees EOF and blocks forever. */
        if (prev_read != -1) close(prev_read);
        if (pfd[1]    != -1) close(pfd[1]);
        prev_read = pfd[0];         /* keep read end for the next stage */
    }
    if (prev_read != -1)
        close(prev_read);

    if (pl->background) {
        add_bg(pids, nforked);
        fprintf(stderr, "[%d]\n", (int)pgid);
        sigprocmask(SIG_SETMASK, &old, NULL);   /* now reaper may run   */
    } else {
        fg_pgid = pgid;
        for (int i = 0; i < nforked; i++) {
            int st = 0;
            while (waitpid(pids[i], &st, 0) < 0 && errno == EINTR)
                ;                    /* SIGINT can interrupt: retry     */
            if (i == nforked - 1) {  /* pipeline status = last stage    */
                if (WIFEXITED(st))        last_status = WEXITSTATUS(st);
                else if (WIFSIGNALED(st)) last_status = 128 + WTERMSIG(st);
            }
        }
        fg_pgid = 0;
        sigprocmask(SIG_SETMASK, &old, NULL);
    }
}

/* ------------------------------------------------------------------ */
/* REPL.                                                              */
/* ------------------------------------------------------------------ */
int main(void)
{
    install_handlers();

    char *line = NULL;
    size_t cap = 0;

    for (;;) {
        /* Prompt goes to stderr so a test capturing stdout sees only
         * program output, never the prompt. */
        fprintf(stderr, "msh> ");

        errno = 0;
        ssize_t r = getline(&line, &cap, stdin);
        if (r < 0) {
            if (errno == EINTR) {   /* interrupted by SIGINT: reprompt  */
                clearerr(stdin);
                continue;
            }
            break;                   /* EOF                             */
        }

        char *tok[MAXTOK];
        int ntok = tokenize(line, tok, MAXTOK);
        if (ntok > 0) {
            struct pipeline pl;
            if (parse(tok, ntok, &pl) == 0)
                run_pipeline(&pl);
        }
        free_tokens(tok, ntok);
    }

    free(line);
    return last_status;
}
