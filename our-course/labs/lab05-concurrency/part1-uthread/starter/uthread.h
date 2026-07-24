/*
 * uthread.h -- the contract for Lab 5 Part 1.
 *
 * The harness includes THIS header and links against YOUR uthread.c, so
 * nothing declared here may change: not a name, not a type, not a constant.
 * Everything the harness asks about your thread package it asks through the
 * five functions below. It never looks at your thread table, your ready
 * queue, or your stacks.
 *
 * x86-64 System V only. The context switch is machine code and does not
 * pretend to be portable.
 */
#ifndef UTHREAD_H
#define UTHREAD_H

/* The most threads that may exist at one time, counting threads that have
 * been created but have not yet run. uthread_create() returns -1 rather than
 * exceeding it. */
#define UTHREAD_MAX 64

/* Bytes of stack per thread. 64 KiB is generous for anything in this lab and
 * small enough that 64 of them fit comfortably. */
#define UTHREAD_STACK 65536

/*
 * A saved thread context: the seven registers that survive a call under the
 * System V AMD64 ABI. rax, rcx, rdx, rsi, rdi and r8-r11 are caller-saved --
 * whoever called uthread_yield() has already spilled anything it cared about,
 * so the switch does not have to.
 *
 * The order of these fields is the order uthread_swtch() stores them in, so
 * the field offsets and the assembly's displacements have to agree. They are
 * both written out longhand, deliberately.
 */
struct uthread_ctx {
	unsigned long rsp;      /*  0 */
	unsigned long rbx;      /*  8 */
	unsigned long rbp;      /* 16 */
	unsigned long r12;      /* 24 */
	unsigned long r13;      /* 32 */
	unsigned long r14;      /* 40 */
	unsigned long r15;      /* 48 */
};

/*
 * Save the current registers into *save, load the registers from *load, and
 * carry on running wherever *load's stack pointer says to. Defined in
 * swtch.S. It returns -- eventually -- to whoever saved *save, which may be a
 * very long time and several threads later.
 */
void uthread_swtch(struct uthread_ctx *save, struct uthread_ctx *load);

/* Prepare the package. Call once, before anything else. */
void uthread_init(void);

/*
 * Create a runnable thread that will call fn(arg), and append it to the tail
 * of the ready queue. Returns the new thread's id, which is >= 1 and never
 * reused within a run, or -1 if there is no free slot or no memory.
 *
 * May be called from the main context or from inside a running thread.
 */
int uthread_create(void (*fn)(void *arg), void *arg);

/*
 * Give up the processor. The caller goes to the TAIL of the ready queue and
 * the thread at the head runs next. Returns when the caller is scheduled
 * again. Called from the main context, it does nothing.
 */
void uthread_yield(void);

/*
 * Terminate the calling thread. Does not return. A thread whose function
 * returns normally must behave exactly as if it had called this.
 */
void uthread_exit(void);

/*
 * The calling thread's id, or 0 in the main context (before uthread_run(),
 * after it returns, or inside uthread_run()'s own loop).
 */
int uthread_self(void);

/*
 * Run threads until none is runnable, then return. Called from the main
 * context. Every thread created before or during the run gets to finish.
 */
void uthread_run(void);

#endif /* UTHREAD_H */
