#
# user/uthread_switch.S  --  reference solution (Lab 5, Part A).
#
#   void thread_switch(struct context *old, struct context *new);
#
# Save the current callee-saved registers (ra, sp, s0..s11) into *old (a0),
# then load them from *new (a1), then ret. This is byte-for-byte the same idea
# as the kernel's swtch.S: because ra is restored from *new, the final `ret`
# resumes execution wherever the incoming thread last called thread_switch()
# -- or, for a brand-new thread, at the ra we planted in thread_create().
#
# Offsets match struct context in user/uthread.c:
#   0 ra, 8 sp, 16 s0, 24 s1, 32 s2, ... 104 s11.

.globl thread_switch
thread_switch:
	sd ra, 0(a0)
	sd sp, 8(a0)
	sd s0, 16(a0)
	sd s1, 24(a0)
	sd s2, 32(a0)
	sd s3, 40(a0)
	sd s4, 48(a0)
	sd s5, 56(a0)
	sd s6, 64(a0)
	sd s7, 72(a0)
	sd s8, 80(a0)
	sd s9, 88(a0)
	sd s10, 96(a0)
	sd s11, 104(a0)

	ld ra, 0(a1)
	ld sp, 8(a1)
	ld s0, 16(a1)
	ld s1, 24(a1)
	ld s2, 32(a1)
	ld s3, 40(a1)
	ld s4, 48(a1)
	ld s5, 56(a1)
	ld s6, 64(a1)
	ld s7, 72(a1)
	ld s8, 80(a1)
	ld s9, 88(a1)
	ld s10, 96(a1)
	ld s11, 104(a1)

	ret
