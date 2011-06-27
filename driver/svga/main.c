/*
 * Copyright (C) 2011 Nick Johnson <nickbjohnson4224 at gmail.com>
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <mutex.h>
#include <page.h>
#include <proc.h>
#include <ipc.h>

#include <rdi/core.h>
#include <rdi/vfs.h>
#include <rdi/arch.h>
#include <rdi/io.h>

#include "svga.h"

struct event_list *event_list;
uint32_t *buffer;
char *modesstr;

char *svga_rcall_register(uint64_t source, uint32_t index, int argc, char **argv) {
	struct resource *file;

	file = index_get(index);

	mutex_spin(&file->mutex);
	event_list = event_list_add(event_list, source);
	mutex_free(&file->mutex);

	return strdup("T");
}

char *svga_rcall_deregister(uint64_t source, uint32_t index, int argc, char **argv) {
	struct resource *file;

	file = index_get(index);

	mutex_spin(&file->mutex);
	event_list = event_list_del(event_list, source);
	mutex_free(&file->mutex);

	return strdup("T");
}

char *svga_rcall_getmode(uint64_t source, uint32_t index, int argc, char **argv) {
	char *rets = NULL;

	rets = malloc(16);
	sprintf(rets, "%d %d %d", svga.w, svga.h, svga.d);
	return rets;
}

char *svga_rcall_listmodes(uint64_t source, uint32_t index, int argc, char **argv) {
	return strdup(modesstr);
}

char *svga_rcall_unshare(uint64_t source, uint32_t index, int argc, char **argv) {
	struct resource *file;

	file = index_get(index);
	if (!file) return NULL;

	mutex_spin(&file->mutex);
	page_free(buffer, msize(buffer));
	free(buffer);
	buffer = valloc(svga.w * svga.h * 4);
	mutex_free(&file->mutex);
	return strdup("T");
}

char *svga_rcall_setmode(uint64_t source, uint32_t index, int argc, char **argv) {
	struct resource *file;
	int x, y, d;
	int mode;

	file = index_get(index);
	if (!file) return NULL;

	if (argc != 4) return NULL;

	x = atoi(argv[1]);
	y = atoi(argv[2]);
	d = atoi(argv[3]);

	mutex_spin(&file->mutex);
	mode = svga_find_mode(x, y, d);
	if (svga_set_mode(mode)) return NULL;
	page_free(buffer, msize(buffer));
	free(buffer);
	buffer = valloc(svga.w * svga.h * 4);
	mutex_free(&file->mutex);

	return strdup("T");
}

char *svga_rcall_syncrect(uint64_t source, uint32_t index, int argc, char **argv) {
	struct resource *file;
	int x, y, w, h;

	file = index_get(index);
	if (!file) return NULL;

	if (argc != 5) return NULL;

	x = atoi(argv[1]);
	y = atoi(argv[2]);
	w = atoi(argv[3]);
	h = atoi(argv[4]);

	mutex_spin(&file->mutex);
	svga_fliprect(buffer, x, y, w, h);
	mutex_free(&file->mutex);

	return strdup("T");
}

void svga_sync(uint64_t source, uint32_t index) {
	svga_flip(buffer);
}

int svga_share(uint64_t source, uint32_t index, uint8_t *_buffer, size_t size, uint64_t off) {

	if (size != svga.w * svga.h * 4) {
		return -1;
	}
	if (off != 0) {
		return -1;
	}

	if (buffer) {
		page_free(buffer, msize(buffer));
		free(buffer);
	}

	buffer = (uint32_t*) _buffer;

	return 0;
}

int main(int argc, char **argv) {
	char *modesstr0;
	char *modestr;
	int i;

	index_set(0, resource_cons(FS_TYPE_FILE | FS_TYPE_GRAPH, PERM_READ | PERM_WRITE));

	svga_init();

	// generate list of modes
	modesstr = strdup("");
	for (i = 0; i < modelist_count; i++) {
		modesstr0 = modesstr;
		modestr = malloc(16);
		sprintf(modestr, "%d:%d:%d ", modelist[i].w, modelist[i].h, modelist[i].d);
		modesstr = strvcat(modesstr, modestr, NULL);
		free(modesstr0);
	}

	svga_set_mode(svga_find_mode(640, 480, 24));
	buffer = malloc(svga.w * svga.h * 4);

	/* set up driver interface */
	rcall_set("getmode",    svga_rcall_getmode);
	rcall_set("listmodes",  svga_rcall_listmodes);
	rcall_set("unshare",    svga_rcall_unshare);
	rcall_set("setmode",    svga_rcall_setmode);
	rcall_set("register",   svga_rcall_register);
	rcall_set("deregister", svga_rcall_deregister);
	rcall_set("syncrect",   svga_rcall_syncrect);
	rdi_set_sync (svga_sync);
	rdi_set_share(svga_share);
	rdi_init_all();

	/* register the driver as /dev/svga0 */
	fs_plink("/dev/svga0", RP_CONS(getpid(), 0), NULL);
	msendb(RP_CONS(getppid(), 0), PORT_CHILD);
	_done();

	return 0;
}
