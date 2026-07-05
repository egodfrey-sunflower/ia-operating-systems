#
# user/uthread_switch.S  --  YOU WRITE THIS (Lab 5, Part A).
#
#   void thread_switch(struct context *old, struct context *new);
#
# The contract: save the callee-saved registers plus ra and sp into *old
# (a0), load the same set from *new (a1), then `ret`. The offsets you use
# MUST match the field order of `struct context` in user/uthread.c.
#
# This is the user-space twin of kernel/swtch.S -- read that first. Where
# that final `ret` ends up, especially the very first time a brand-new
# thread is switched to, is handout written question 2; answer it before
# you write the code.
#
# Delete this comment block and write the real code below.

.globl thread_switch
thread_switch:
	# TODO: implement the contract described above.
	ret
