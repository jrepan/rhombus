#ifndef ELF_H
#define ELF_H

#include <lib.h>

// ELF Header
typedef struct {
	u8int e_ident[16];
	u16int e_type, e_machine;
	u32int e_version;
	u32int e_entry, e_phoff, e_shoff, e_flags;
	u16int e_ehsize, e_phentsize, e_phnum;
	u16int e_shentsize, e_shnum, e_shstrndx;
} __attribute__ ((packed)) elf_t;

#define ET_EXEC 2
#define EM_386 3

// ELF Program Header
typedef struct {
	u32int p_type, p_offset, p_vaddr, p_paddr;
	u32int p_filesz, p_memsz, p_flags, p_align;
} __attribute__ ((packed)) elf_ph_t;

#define PT_NULL 	0
#define PT_LOAD 	1
#define PT_DYNAMIC 	2
#define PT_INTERP 	3
#define PT_NOTE 	4
#define PT_SHLIB 	5
#define PT_PHDR		6

#define PF_R	0x1
#define PF_W	0x2
#define PF_X	0x4

void elf_load_segment(u8int *src, elf_ph_t *seg);
u32int elf_load(u8int *src); // Returns entry point
int elf_check(u8int *src);

#endif /*ELF_H*/