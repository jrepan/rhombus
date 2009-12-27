#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <khaos/kernel.h>
#include <khaos/driver.h>
#include <khaos/signal.h>
#include <khaos/config.h>

#include <driver/console.h>

#include "../libc/libc.h"

void swrite(const char *message) {
	console.write(0, strlen(message), (void*) message);
}

static void print_bootsplash() {
swrite("\n\
\t\t\t\t\t\t\t           '^a,\n\
\t\t\t\t\t\t\t        ,.    'b.\n\
\t\t\t\t\t\t\t      .d'       b.\n\
\t\t\t\t\t\t\t      S:        a:\n\
\t\t\t\t\t\t\t      'q,       p'\n\
\t\t\t\t\t\t\t        \"'    .p'\n\
\t\t\t\t\t\t\t           .,a'\n\
\t\t\t\t\t _  _   _   _   _____   _____   _____ \n\
\t\t\t\t\t| |/ / | |_| | |___  | |  _  | |  ___|\n\
\t\t\t\t\t|   <  |  _  | |  _  | | |_| | |___  |\n\
\t\t\t\t\t|_|\\_\\ |_| |_| |_____| |_____| |_____|\n\
\t\t\t\t\t          -= Version 0.1a =-\n\n\n");
}

void xwrite(uint32_t addr) {
	char m[9];
	const char *d = "0123456789ABCDEF";
	size_t i;

	for (i = 7; (int) i >= 0; i--) {
		m[i] = d[addr & 0xF];
		addr >>= 4;
	}

	m[8] = '\0';
	swrite((const char*) m);
}

void (*irq_table[16])(void);
void irq_handler(uint32_t source, uint32_t args[4]) {
	if (irq_table[args[0] & 0xF]) irq_table[args[0] & 0xF]();
}

void rirq(int irq, uint32_t handler) {
	irq_table[irq] = (void (*)(void)) handler;
	_ctrl(CTRL_IRQRD | CTRL_IRQ(irq), CTRL_IRQRD | CTRL_IRQMASK);
}

void stuff(uint32_t source, uint32_t args[4]) {
	swrite("ZOMG 42!!!");
}

void segfault(uint32_t source, uint32_t args[4]) {
	swrite("\n");

	if (args[3] & ~0xFFF) {
		swrite("Segmentation Fault\n");
		swrite("Address: ");
		xwrite(args[3]);
		swrite("\n");
	}
	else {
		swrite("Null Pointer Exception\n");
	}

	swrite("Instruction: ");
	xwrite(args[0]);
	swrite("\n");

	for(;;);
	exit(1);
}

void death(uint32_t source, uint32_t args[4]) {
}

int main() {
	size_t i, j;
	int32_t pid;
	static uint32_t *mb[1024];

	khsig_register(0, segfault);
	khsig_register(2, segfault);
	khsig_register(3, irq_handler);
	khsig_register(7, death);
	khsig_register(42, stuff);

	console.init(0);
	rirq(console.interrupt, (uint32_t) console.handler);
	print_bootsplash();

	swrite("Fork test:\n");
	swrite("\tspawning 16 tasks:\t");
	for (i = 0; i < 16; i++) {
		pid = fork();
		if (pid < 0) {
			khsig_wait(16);
			exit(0);
		}
		else {
			mb[i] = (void*) pid;
			swrite(".");
			if (i % 2 == 0) {
				mb[i] = NULL;
				khsig_asend(pid, 16, (uint32_t*) mb);
				khsig_wait(7);
			}
		}
	}
	swrite("Done.\n");

	swrite("\tkilling 16 tasks:\t");
	for (i = 0; i < 16; i++) {
		if (mb[i]) {
			khsig_asend((uint32_t) mb[i], 16, (uint32_t*) mb);
			swrite("..");
		}
	}
	swrite("Done.\n");

	swrite("\n");

	swrite("Signals test:\n");
	khsig_wreset(16);
	khsig_block();
	if (khsig_asend(info(0), 16, (uint32_t*) mb)) {
		swrite("\tblocked\n");
		khsig_unblock();
		khsig_asend(info(0), 16, (uint32_t*) mb);
	}
	khsig_wait(16);
	swrite("\tunblocked\n");

	swrite("\n");

	swrite("Allocator test:\n");
	for (i = 0; i < 1; i++) {
		swrite("\tAllocing 64 Blocks\t");
		for (j = 0; j < 64; j++) {
			mb[j] = malloc(0x1000);
			if (j % 4 == 0) swrite(".");
		}
		swrite("Done. \n");

		swrite("\tTesting 64 Blocks\t");
		for (j = 0; j < 64; j++) {
			mb[j][1023] = 0;
			if (j % 4 == 0) swrite(".");
		}
		swrite("Done. \n");

		swrite("\tFreeing 64 Blocks\t");
		for (j = 0; j < 64; j++) {
			free(mb[j]);
			if (j % 4 == 0) swrite(".");
		}
		swrite("Done. \n");
	}
	
	for (i = 0; i < 1; i++) {
		swrite("\tAllocing 1024 Cells\t");
		for (j = 0; j < 1024; j++) {
			mb[j] = malloc(13);
			if (j % 64 == 0) swrite(".");
		}
		swrite("Done. \n");

		swrite("\tTesting 1024 Cells\t");
		for (j = 0; j < 1024; j++) {
			memclr(mb[j], 13);
			if (j % 64 == 0) swrite(".");
		}
		swrite("Done. \n");

		swrite("\tFreeing 1024 Cells\t");
		for (j = 0; j < 1024; j++) {
			free(mb[j]);
			if (j % 64 == 0) swrite(".");
		}
		swrite("Done. \n");
	}

	swrite("\tCycling 2^16 Cells\t");
	for (i = 0; i < 256; i++) {
		for (j = 0; j < 256; j++) {
			mb[j] = malloc(8);
		}
		for (j = 0; j < 256; j++) {
			free(mb[j]);
		}
		if (i % 16 == 0) swrite(".");
	}
	swrite("Done. \n");

	swrite("\tCycling 2^12 Blocks\t");
	for (i = 0; i < 16; i++) {
		for (j = 0; j < 256; j++) {
			mb[j] = malloc(0x1000);
		}
		for (j = 0; j < 256; j++) {
			free(mb[j]);
		}
		swrite(".");
	}
	swrite("Done. \n");

	swrite("\nAll tests passed.\n");

	for(;;);
	return 0;
}
