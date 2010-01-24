/* Copyright 2010 Nick Johnson */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <mmap.h>
#include <flux.h>
#include <driver.h>
#include <signal.h>
#include <stdio.h>

#include <driver/terminal.h>
#include <driver/keyboard.h>

static volatile struct request *reply = NULL;

void console_hand(uint32_t caller, struct request *req) {
	if (reply) req_free((void*) reply);
	reply = req;
}

void console_init() {
	int32_t pid;

	sigregister(SIG_REPLY, console_hand);

	pid = fork();
	if (pid < 0) {
		terminal.init(0);
		fire(-pid, SIG_REPLY, NULL);
		block();
		for(;;);
	}
	stdout = malloc(sizeof(FILE));
	stdout->target = pid;
	stdout->wport = SIG_WRITE;
	wait(SIG_REPLY);

	pid = fork();
	if (pid < 0) {
		keyboard.init(0);
		fire(-pid, SIG_REPLY, NULL);
		block();
		for(;;);
	}
	stdin = malloc(sizeof(FILE));
	stdin->target = pid;
	stdin->rport = SIG_READ;
	wait(SIG_REPLY);
}

size_t console_write(char *buffer, size_t size) {
	return fwrite(buffer, sizeof(char), size, stdout);
}

size_t console_read(char *buffer, size_t size) {
	struct request *r;
	size_t oldsize = size;

	while (size) {

		r = req_alloc();
		r->datasize = size;
		r->dataoff	= 0;
		r->format	= REQ_READ;
		r = req_checksum(r);

		reply = NULL;
		fire(stdin->target, stdin->rport, r);
		wait(SIG_REPLY);
		r = (void*) reply;

		memcpy(buffer, r->reqdata, r->datasize);

		buffer += r->datasize;
		size -= r->datasize;
		req_free(r);

	}

	return oldsize;
}
