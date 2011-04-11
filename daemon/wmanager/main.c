/*
 * Copyright (C) 2011 Jaagup Repan
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

#include "wmanager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <driver.h>
#include <exec.h>
#include <proc.h>
#include <page.h>
#include <mutex.h>

uint64_t vgafd, mousefd, kbdfd;
bool winkey;
int next_index = 1;

char *wmanager_rcall(uint64_t source, struct vfs_obj *file, const char *args) {
	struct window_t *window;
	size_t width, height;
	int flags, x, y;
	char buffer[32];

	if (strcmp(args, "listmodes") == 0) {
		return strdup("any");
	}
	if (strcmp(args, "createwindow") == 0) {
		sprintf(buffer, "/sys/wmanager/%i", next_index);
		io_cons(buffer, RP_TYPE_FILE);
		find_window(next_index - 1, 0)->owner = RP_PID(source);
		sprintf(buffer, "%i", next_index - 1);
		return strdup(buffer);
	}

	window = find_window(file->index, RP_PID(source));
	if (!window) {
		return strdup("");
	}

	if (strncmp(args, "setmode ", 8) == 0) {
		if (sscanf(args + 8, "%i %i", &width, &height) != 2) {
			return strdup("");
		}
		resize_window(window, window->width, window->height, false);
		if (!(window->flags & CONSTANT_SIZE) && !(window->flags & FLOATING)) {
			// window must be CONSTNAT_SIZE or FLOATING for resizing to make sense
			window->flags |= FLOATING;
			update_tiling();
		}
		return strdup("T");
	}
	if (strcmp(args, "getmode") == 0) {
		sprintf(buffer, "%i %i 32", window->width, window->height);
		return strdup(buffer);
	}
	if (strncmp(args, "syncrect ", 9) == 0) {
		if (sscanf(args + 9, "%i %i %i %i", &x, &y, &width, &height) != 4) {
			return strdup("");
		}
		update_screen(window->x + x, window->y + y, window->x + x + width, window->y + y + height);
		return strdup("T");
	}
	if (strcmp(args, "unshare") == 0) {
		if (!window->bitmap) {
			return strdup("");
		}
		mutex_spin(&window->mutex);
		page_free(window->bitmap, window->width * window->height * 4);
		window->bitmap = NULL;
		mutex_free(&window->mutex);
		return strdup("T");
	}

	if (strcmp(args, "register") == 0) {
		window->flags |= LISTEN_EVENTS;
		return strdup("T");
	}
	if (strcmp(args, "deregister") == 0) {
		window->flags &= ~LISTEN_EVENTS;
		return strdup("T");
	}

	if (strcmp(args, "getwindowflags") == 0) {
		sprintf(buffer, "%i", window->flags);
		return strdup(buffer);
	}
	if (strncmp(args, "setwindowflags ", 15) == 0) {
		if (sscanf(args + 15, "%i", &flags) != 1) {
			return strdup("");
		}
		window->flags = flags;
		return strdup("T");
	}

	return NULL;
}

int wmanager_share(uint64_t source, struct vfs_obj *file, uint8_t *buffer, size_t size, uint64_t off) {
	struct window_t *window = find_window(file->index, RP_PID(source));
	if (off != 0) {
		return -1;
	}
	if (!window) {
		return -1;
	}
	if (size != window->width * window->height * 4) {
		return -1;
	}
	mutex_spin(&window->mutex);
	if (window->bitmap) {
		page_free(window->bitmap, window->width * window->height * 4);
	}
	window->bitmap = buffer;
	mutex_free(&window->mutex);
	return 0;
}

int wmanager_sync(uint64_t source, struct vfs_obj *file) {
	struct window_t *window = find_window(file->index, RP_PID(source));
	if (!window) {
		return -1;
	}
	update_screen(window->x - 1, window->y - 1, window->x + window->width + 1, window->y + window->height + 1);
	return 0;
}

struct vfs_obj *wmanager_cons(uint64_t source, int type) {
	struct vfs_obj *fobj = NULL;

	if (RP_PID(source) != getpid()) {
		return NULL;
	}

	switch (type) {
	case RP_TYPE_FILE:
		fobj        = calloc(sizeof(struct vfs_obj), 1);
		fobj->type  = type;
		fobj->size  = 0;
		fobj->link  = 0;
		fobj->data  = NULL;
		fobj->index = next_index++;
		fobj->acl   = acl_set_default(fobj->acl, PERM_READ | PERM_WRITE);
		break;
	}
	
	return fobj;
}

int wmanager_push(uint64_t source, struct vfs_obj *file) {
	if (RP_PID(source) != getpid()) {
		return -1;
	}

	return add_window(file->index);
}

int wmanager_pull(uint64_t source, struct vfs_obj *file) {
	return remove_window(file->index, RP_PID(source));
}

void wmanager_event(uint64_t source, uint64_t value) {
	int type = value >> 62;
	int data = value & ~(0x3LL << 62);
	bool released;

	if (type == 0x0 && source == kbdfd) { // keyboard
		released = data & 0x00400000 ? true : false;
		data &= ~0x00400000;
		if (data == 0x00800004) {
			winkey = !released;
		}
	}
	if (type == 0x1 && source == mousefd) { 
		mouse_move((data >> 16) & 0xffff, data & 0xffff);
	}
	if (type == 0x2 && source == mousefd) {
		mouse_buttons(data & ~(0x3LL << 62));
	}
	if (type == 0x3 && source == vgafd) {
		resize_screen((data >> 16) & 0xffff, data & 0xffff);
		return; // don't forward
	}

	if (active_window && (active_window->flags & LISTEN_EVENTS)) {
		event(RP_CONS(active_window->owner, 0), value);
	}
}

int main(int argc, char **argv) {
	struct vfs_obj *root;
	size_t width, height;

	stdout = stderr = fopen("/dev/serial", "w");

	if (fork() < 0) {
		exec("/sbin/mouse");
	}
	mwait(PORT_CHILD, 0);

	root        = calloc(sizeof(struct vfs_obj), 1);
	root->type  = RP_TYPE_DIR;
	root->size  = 0;
	root->acl   = acl_set_default(root->acl, PERM_READ | PERM_WRITE);
	vfs_set_index(0, root);

	di_wrap_rcall(wmanager_rcall);
	di_wrap_share(wmanager_share);
	di_wrap_sync (wmanager_sync);
	vfs_wrap_cons(wmanager_cons);
	vfs_wrap_push(wmanager_push);
	vfs_wrap_pull(wmanager_pull);
	vfs_wrap_init();

	io_link("/sys/wmanager", RP_CONS(getpid(), 0));

	vgafd = io_find("/dev/svga0");
	sscanf(rcall(vgafd, "getmode"), "%i %i", &width, &height);
	resize_screen(width, height);

	mousefd = io_find("/dev/mouse");
	kbdfd = io_find("/dev/kbd");
	event_register(mousefd, wmanager_event);
	event_register(kbdfd, wmanager_event);
	event_register(vgafd, wmanager_event);

	if (fork() < 0) {
		exec("/sbin/fbterm");
	}

	_done();

	return 0;
}
