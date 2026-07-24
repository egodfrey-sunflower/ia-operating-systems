/*
 * regprobe.S -- does a context switch really preserve the callee-saved
 * registers?
 *
 *      unsigned long uthread_regprobe(void);
 *
 * Loads a distinct constant into each of the six callee-saved registers,
 * calls uthread_yield(), and returns a bitmask of the ones that came back
 * changed: bit 0 %rbx, 1 %rbp, 2 %r12, 3 %r13, 4 %r14, 5 %r15. Zero means
 * all six survived.
 *
 * This has to be assembly. In C the compiler decides which registers hold
 * which values, and it would happily keep all six locals in memory, in which
 * case the test would pass against a switch that saves nothing at all.
 *
 * Part of the test harness, not of the lab. You are not asked to read it,
 * though it is short and it is the only place that says precisely what
 * "callee-saved" means in a form a machine can check.
 */

	.text
	.globl	uthread_regprobe
	.type	uthread_regprobe, @function
	.align	16
uthread_regprobe:
	pushq	%rbx
	pushq	%rbp
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	pushq	%rax            /* padding, so %rsp is 16-byte aligned below */

	movabsq	$0x1111111111111111, %rbx
	movabsq	$0x2222222222222222, %rbp
	movabsq	$0x3333333333333333, %r12
	movabsq	$0x4444444444444444, %r13
	movabsq	$0x5555555555555555, %r14
	movabsq	$0x6666666666666666, %r15

	call	uthread_yield

	xorl	%eax, %eax

	movabsq	$0x1111111111111111, %rcx
	cmpq	%rcx, %rbx
	je	1f
	orq	$1, %rax
1:
	movabsq	$0x2222222222222222, %rcx
	cmpq	%rcx, %rbp
	je	2f
	orq	$2, %rax
2:
	movabsq	$0x3333333333333333, %rcx
	cmpq	%rcx, %r12
	je	3f
	orq	$4, %rax
3:
	movabsq	$0x4444444444444444, %rcx
	cmpq	%rcx, %r13
	je	4f
	orq	$8, %rax
4:
	movabsq	$0x5555555555555555, %rcx
	cmpq	%rcx, %r14
	je	5f
	orq	$16, %rax
5:
	movabsq	$0x6666666666666666, %rcx
	cmpq	%rcx, %r15
	je	6f
	orq	$32, %rax
6:
	popq	%rcx            /* the padding */
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbp
	popq	%rbx
	ret
	.size	uthread_regprobe, .-uthread_regprobe

	.section	.note.GNU-stack,"",@progbits
