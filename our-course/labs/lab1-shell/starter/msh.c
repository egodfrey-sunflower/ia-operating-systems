/*
 * msh - a minimal Unix shell (STARTER SKELETON)
 *
 * Your job is to grow this into a working shell across Tasks 1-5 (see the
 * lab README).  What you get for free:
 *   - a REPL loop that reads a line with getline(),
 *   - a working tokenize() that also splits the metacharacters
 *     < > >> 2> | & into their own tokens (so you don't have to fight
 *     the lexer),
 *   - a data model sketch (struct redir/cmd/pipeline) to fill in,
 *   - a run_pipeline() stub that currently just says "not implemented".
 *
 * This file compiles clean under -Wall -Wextra -Werror and runs, but it
 * executes nothing yet - every real test fails until you implement it.
 *
 * Suggested order of attack:
 *   Task 1  write parse() + fork/execvp for a single command; add the
 *           `exit` and `cd` builtins.  (Ask yourself in the writeup: why
 *           must cd and exit run in the shell process, not a child?)
 *   Task 2  handle struct redir in the child with open()/dup2()/close().
 *   Task 3  loop over pipeline stages creating pipes; be ruthless about
 *           closing pipe fds in the parent (the classic hang).
 *   Task 4  background jobs: don't wait(); reap with SIGCHLD or WNOHANG;
 *           add a `wait` builtin.
 *   Task 5  catch SIGINT in the shell and forward it to the foreground
 *           job's process group; put children in their own group.
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

#define MAXARGS 64
#define MAXCMDS 32
#define MAXTOK  256

/* ------------------------------------------------------------------ */
/* Data model.  TODO: you will fill these structs in as you go.       */
/* ------------------------------------------------------------------ */

struct redir {
    char *infile;   /* <  file      (NULL if none)                    */
    char *outfile;  /* >  or >> file (NULL if none)                   */
    char *errfile;  /* 2> file      (NULL if none)                    */
    int   append;   /* 1 if the redirection was >>                    */
};

struct cmd {
    char *argv[MAXARGS];
    int   argc;
    struct redir redir;
};

struct pipeline {
    struct cmd cmds[MAXCMDS];
    int  ncmds;
    int  background;
};

/* ------------------------------------------------------------------ */
/* Tokenizer - PROVIDED, do not change (unless you want to).          */
/*                                                                    */
/* Splits `line` on whitespace and turns < > >> 2> | & into their own */
/* tokens even without surrounding spaces.  Each token is heap-       */
/* allocated; the caller frees them (see free_tokens()).              */
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
        if (*p == '2' && p[1] == '>') { tok[n++] = strdup("2>"); p += 2; continue; }

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
/* TODO: struct pipeline parse(char *tok[], int ntok, ...)            */
/* TODO: builtins - int is_builtin(const char *); run them in-process */
/* TODO: void apply_redir(struct redir *) in the child                */
/* ------------------------------------------------------------------ */

/*
 * run_pipeline: the heart of the shell.  Right now it does nothing.
 *
 * Replace this stub: parse `tok` into a struct pipeline, then fork/exec
 * each stage, wiring pipes between them and applying redirections at the
 * ends.  Handle the `&` (background) and builtin cases too.
 */
static void run_pipeline(char *tok[], int ntok)
{
    (void)tok;
    (void)ntok;
    fprintf(stderr, "msh: not implemented\n");
}

int main(void)
{
    /* TODO (Task 5): install signal handlers here. */

    char *line = NULL;
    size_t cap = 0;

    for (;;) {
        /* Prompt to stderr so stdout carries only program output. */
        fprintf(stderr, "msh> ");

        ssize_t r = getline(&line, &cap, stdin);
        if (r < 0)
            break;   /* EOF (TODO Task 5: also handle EINTR here) */

        char *tok[MAXTOK];
        int ntok = tokenize(line, tok, MAXTOK);
        if (ntok > 0)
            run_pipeline(tok, ntok);
        free_tokens(tok, ntok);
    }

    free(line);
    return 0;
}
