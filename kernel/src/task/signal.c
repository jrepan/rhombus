// Copyright 2009 Nick Johnson

#include <lib.h>
#include <task.h>
#include <trap.h>

image_t *signal(u32int task, u32int sig, u32int arg0, u32int arg1, u32int arg2, u32int flags) {
	task_t *t, *src_t;
	u32int src = curr_pid;

	// Check existence of target
	src_t = get_task(src);
	t = get_task(task);
	if (t->magic != 0x4224) {
		printk("no such task\n");
		src_t->image->eax = ENOTASK;
		return src_t->image;
	}

	// Check permissions
//	if (t->user.id != src_t->user.id && t->user.ring >= src_t->user.ring) {
//		src_t->image->eax = EPERMIT;
//		return src_t->image;
//	}

	// Check existence of signal handler
	if (!signal_table[sig]) {
		printk("no signal handler\n");
		src_t->image->eax = ENOSIG;
		return src_t->image;
	}

	// Block if set to block
	if (flags & TF_BLOCK) src_t->flags |= TF_BLOCK;
	
	// Create new image to return to
	task_switch(t);
	memcpy((u8int*) ((u32int) t->image - sizeof(image_t)), t->image, sizeof(image_t));
	t->tss_esp = (u32int) t->image; // Will now create images above old image
	t->image = (void*) ((u32int) t->image - sizeof(image_t));
	if ((u32int) t->image < 0xF3FFE100) 
		panic("task state stack overflow");

	// Set registers to describe signal
	t->image->caller = curr_pid;
	t->image->eax = src;
	t->image->ebx = arg0;
	t->image->ecx = arg1;
	t->image->edx = arg2;
	t->image->esi = sig;

	// Locate entry point
	t->image->eip = signal_table[sig];

	tss_set_esp(t->tss_esp);
	return t->image;
}

image_t *sret(image_t *image) {
	task_t *t, *src_t;

	src_t = get_task(image->caller);

	// Reset image stack
	t = get_task(curr_pid);
	t->image = (void*) t->tss_esp;
	t->tss_esp += sizeof(image_t); 

	// Bounds check image
	if ((u32int) t->tss_esp >= 0xF3FFF000) 
		panic("task state stack underflow");
		//return exit_call(t->image);

	// Make sure the caller is now unblocked
	src_t->flags &= ~TF_BLOCK;

	return t->image;
}
