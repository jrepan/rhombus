// Copyright 2009 Nick Johnson

#include <lib.h>
#include <init.h>
#include <elf.h>
#include <mem.h>
#include <task.h>
#include <trap.h>

__attribute__ ((section(".tdata")))
struct tar_header *header[256];

__attribute__ ((section(".tdata")))
struct tar_header *initrd;

__attribute__ ((section(".tdata")))
u32int initrd_size;

__attribute__ ((section(".ttext")))
static int tar_header_check(struct tar_header *t) {
	u32int i, sum = 0;
	u8int *header_byte = (u8int*) t;
	for (i = 0;   i < 512; i++) sum += header_byte[i];
	for (i = 148; i < 156; i++) sum -= header_byte[i]; 	// Discount checksum itself
	for (i = 148; i < 156; i++) sum += (u8int) ' ';		// Count checksum as spaces
	if (atoi(t->chksum, 8) == sum) return sum;
	return -1;
}

__attribute__ ((section(".ttext")))
static u8int *header_contents(struct tar_header *t) {
	return (u8int*) ((u32int) t + sizeof(struct tar_header));
}

__attribute__ ((section(".ttext")))
void init_kload() {
	u32int i, n;

	// Check for initrd (it's really a tape archive)
	if (!mboot->mods_count) panic("No initrd found!");
	initrd = (void*) *(u32int*) (mboot->mods_addr + KSPACE) + KSPACE;
	initrd_size = *(u32int*) (mboot->mods_addr + KSPACE + 4) + KSPACE;
	initrd_size -= (u32int) initrd;
	initrd_size /= 512; // In 512 byte blocks
	printk("%d blocks", initrd_size);

	// Check validity of tarball
	if (tar_header_check(initrd) == -1) panic("Tar checksum error");

	// Index initrd for later use
	for (i = 0, n = 0; i < initrd_size; i++) if (tar_header_check(&initrd[i]) != -1)
		header[n++] = &initrd[i];
	header[n] = NULL;
}

__attribute__ ((section(".ttext")))
void init_libsys() {
	u32int i, n;
	task_t *t;

	// Check for libsys header
	for (n = 0; n < 256; n++) if (header[n] && !strcmp(header[n]->name, "libsys")) break;
	if (n == 256) panic("No system library found");

	// Set up a stack for the process image
	t = get_task(curr_pid);

	for (i = USTACK_BSE; i < USTACK_TOP; i += 0x1000) p_alloc(i, (PF_USER | PF_RW));
	for (i = SSTACK_BSE; i < SSTACK_TOP; i += 0x1000) p_alloc(i, (PF_USER | PF_RW));
	t->image = (void*) (SSTACK_INI - sizeof(image_t));

	// Set up space for the signal handler table
	p_alloc(SIG_TBL, (PF_USER | PF_RW));
	p_alloc(SOV_TBL, (PF_USER | PF_RW));

	// Load libsys image
	if (elf_check(header_contents(header[n]))) panic("libsys is not valid ELF");

	// Setup process image
	t->tss_esp = SSTACK_INI;
	t->image->useresp = USTACK_INI;
	t->image->esp = USTACK_INI;
	t->image->ebp = USTACK_INI;
	t->image->ss = 0x23;
	t->image->ds = 0x23;
	t->image->eip = elf_load(header_contents(header[n]));
	for (i = 0; i < 1024; i++) sigovr_table[i] = 0;
	for (i = 0; i < 1024; i++) signal_table[i] = 0;
	t->image->cs = 0x1B;
	extern u32int get_eflags();
	t->image->eflags = get_eflags() | 0x0200; // Turns on interrupts in eflags
}

// Note - this function breaks on all GCC optimizations and normal TCC - try and fix ASAP
__attribute__ ((section(".ttext")))
void init_initrd_rmap() {
	u32int i, base, limit;
	base = (u32int) initrd;
	limit = base + (initrd_size * 512);
#define	new_base 0x10000000

	for (i = base; i < limit; i += 0x1000) {
		page_set(((i - base) + new_base), (page_get(i) | 0x7));
		page_set(i, 0x00000000);
	}
}