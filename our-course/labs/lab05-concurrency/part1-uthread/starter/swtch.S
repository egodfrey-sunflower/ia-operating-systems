/*
 * swtch.S -- the context switch. x86-64 System V.
 *
 *      void uthread_swtch(struct uthread_ctx *save, struct uthread_ctx *load);
 *
 * save is in %rdi, load is in %rsi (argument registers 1 and 2).
 *
 * There are exactly seven registers to move, and the reason there are seven
 * is the ABI: %rbx, %rbp and %r12-%r15 are callee-saved, so the compiled code
 * on either side of this call is entitled to assume they survive; and %rsp,
 * because swapping the stack pointer is the whole trick.
 *
 * Everything else -- %rax, %rcx, %rdx, %rsi, %rdi, %r8-%r11, and the whole
 * floating-point and vector file -- is caller-saved. The compiler has already
 * spilled anything live across this call into the caller's stack frame, and
 * that frame is part of the stack we are about to swap away. It comes back
 * when the stack comes back.
 *
 * The return address is not in a register: `call` pushed it onto the stack we
 * are leaving. That is why this works at all. `ret` at the bottom pops from
 * whichever stack the second half loaded, and so returns into a different
 * thread's uthread_swtch() call, the one that saved that context.
 */

	.text
	.globl	uthread_swtch
	.type	uthread_swtch, @function
	.align	16
uthread_swtch:
	/* Save into *%rdi. Offsets match struct uthread_ctx, in order. */
	movq	%rsp,  0(%rdi)
	movq	%rbx,  8(%rdi)
	movq	%rbp, 16(%rdi)
	movq	%r12, 24(%rdi)
	movq	%r13, 32(%rdi)
	movq	%r14, 40(%rdi)
	movq	%r15, 48(%rdi)

	/*
	 * TODO: the restore half. It is the mirror image of the block above:
	 * seven movq instructions, same offsets, %rsi as the base, the register
	 * as the destination instead of the source.
	 *
	 * Load %rsp FIRST or last, it does not matter which -- but be aware
	 * that every instruction after it runs on the incoming thread's stack.
	 *
	 * Get one of the seven wrong and the harness tells you which:
	 * 'the callee-saved registers survive a switch' names the register.
	 */

	ret
	.size	uthread_swtch, .-uthread_swtch

/* Tell the linker this object needs no executable stack. Without it the
 * linker warns, and the graded build is -Werror. */
	.section	.note.GNU-stack,"",@progbits
