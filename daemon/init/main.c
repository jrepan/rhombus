/*
 * Copyright (C) 2009-2011 Nick Johnson <nickbjohnson4224 at gmail.com>
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <natio.h>
#include <errno.h>
#include <proc.h>
#include <exec.h>
#include <ipc.h>

#include "inc/tar.h"
#include "initrd.h"

const char *splash ="\
Rhombus Operating System 0.8 Alpha\n\
\n";

void panic(const char *message) {
	printf("INIT PANIC: %s\n", message);
	for(;;);
}

static uint64_t start(struct tar_file *file, char const **argv) {
	int32_t pid;

	pid = fork();

	if (pid < 0) {
		execiv(file->start, file->size, argv);
		for(;;);
	}

	mwait(PORT_CHILD, RP_CONS(pid, 0));

	return RP_CONS(pid, 0);
}

int main() {
	struct tar_file *boot_image, *file;
	char const **argv;
	uint64_t temp, temp1, temp2;

	argv = malloc(sizeof(char*) * 4);

	/* Boot Image */
	boot_image = tar_parse((void*) BOOT_IMAGE);

	/* Dynamic Linker */
	file = tar_find(boot_image, "lib/dl.so");
	dl_load(file->start);

	/* Initial Root Filesystem / Device Filesystem / System Filesystem (tmpfs) */
	argv[0] = "tmpfs";
	argv[1] = NULL;
	file = tar_find(boot_image, "sbin/tmpfs");
	fs_root = start(file, argv);
	fs_cons("/dev", FS_TYPE_DIR);
	fs_cons("/sys", FS_TYPE_DIR);

	/* Logfile */
	fs_cons("/dev/stderr", FS_TYPE_FILE);

	/* Serial Driver */
	argv[0] = "serial";
	argv[1] = NULL;
	file = tar_find(boot_image, "sbin/serial");
	fs_plink("/dev/serial", start(file, argv), NULL);
	stdout = stderr = fopen("/dev/serial", "w");

	/* Keyboard Driver */
	argv[0] = "kbd";
	argv[1] = NULL;
	file = tar_find(boot_image, "sbin/kbd");
	temp = start(file, argv);

	/* Graphics Driver */
	argv[0] = "svga";
	argv[1] = NULL;
	file = tar_find(boot_image, "sbin/svga");
	start(file, argv);

	/* Init control file */
	fs_plink("/sys/init", RP_CONS(getpid(), 1), NULL);

	/* Initrd */
	initrd_init();
	fs_plink("/dev/initrd", RP_CONS(getpid(), 0), NULL);

	/* Root filesystem (tarfs) */
	argv[0] = "tarfs";
	argv[1] = "/dev/initrd";
	argv[2] = NULL;
	file = tar_find(boot_image, "sbin/tarfs");
	temp = start(file, argv);
	
	/* Link /dev and /sys and change root */
	temp1 = fs_find("/dev");
	temp2 = fs_find("/sys");
	fs_root = temp;
	fs_plink("/dev", temp1, NULL);
	fs_plink("/sys", temp2, NULL);

	/* Terminal Driver */
	argv[0] = "fbterm";
	argv[1] = "/dev/kbd";
	argv[2] = "/dev/svga0";
	argv[3] = NULL;
	file = tar_find(boot_image, "sbin/fbterm");
	fs_plink("/dev/tty", start(file, argv), NULL);

	/* Splash */
	stdout = stderr = stdin = fopen("/dev/tty", "w");
	printf(splash);

	/* Temporary filesystem */
	argv[0] = "tmpfs";
	argv[1] = NULL;
	file = tar_find(boot_image, "sbin/tmpfs");
	fs_plink("/tmp", start(file, argv), NULL);

	/* Time Driver */
	argv[0] = "time";
	argv[1] = NULL;
	file = tar_find(boot_image, "sbin/time");
	fs_plink("/dev/time", start(file, argv), NULL);

	setname("init");
	
	mwait(PORT_CHILD, 0);

	printf("INIT PANIC: system daemon died\n");
	for(;;);
	return 0;
}
