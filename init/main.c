/* 
 * Copyright 2009, 2010 Nick Johnson 
 * ISC Licensed, see LICENSE for details
 */

#include <flux/arch.h>
#include <flux/signal.h>
#include <flux/request.h>
#include <flux/proc.h>
#include <flux/driver.h>
#include <flux/exec.h>

#include <driver/terminal.h>
#include <driver/keyboard.h>
#include <driver/ata.h>
#include <driver/pci.h>

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "inc/tar.h"

FILE *disk;

void driver_start(int fd, struct driver_interface *driver, device_t dev) {
	int32_t pid;

	signal_policy(SIG_REPLY, POLICY_QUEUE);

	pid = fork();

	if (pid < 0) {
		driver->init(dev);
		fire(-pid, SIG_REPLY, NULL);
		for(;;);
	}

	signal_waits(SIG_REPLY, pid, true);

	fdset(fd, pid, 0);
}

void daemon_start(int fd, void *image, size_t image_size) {
	int32_t pid;

	signal_policy(SIG_REPLY, POLICY_QUEUE);

	pid = fork();

	if (pid < 0) {
		exec(image, image_size);
		for(;;);
	}

	fdset(fd, pid, 0);
}

int main() {
	device_t nulldev;
	struct tar_file *boot_image, *file;
	int i;

	nulldev.type = -1;

	driver_start(FD_STDOUT, &terminal, nulldev);

	printf("Flux Operating System 0.4a\n");
	printf("Copyright 2010 Nick Johnson\n\n");

	printf("Reading Boot Image...\n");
	boot_image = tar_parse((uint8_t*) BOOT_IMAGE);

	printf("contents: ");
	for (i = 0; boot_image[i].name; i++) {
		printf("%s ", boot_image[i].name);
	}
	printf("\n");

	file = tar_find(boot_image, "vfsd");
	printf("Launching Virtual Filesystem Daemon...\n");
	daemon_start(FD_STDVFS, file->start, file->size);

	for(;;);
}
