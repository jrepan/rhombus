/*
 * Copyright (C) 2009, 2010 Nick Johnson <nickbjohnson4224 at gmail.com>
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
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

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pack.h>

/****************************************************************************
 * __fload
 *
 * Load a file descriptor from exec-peristent memory. Returns a pointer to
 * the loaded file descriptor on success, NULL on failure.
 */

FILE *__fload(int id) {
	FILE *new;
	const FILE *saved;
	size_t length;

	/* allocate space for new file */
	new = malloc(sizeof(FILE));

	/* check for allocation errors */
	if (!new) {
		return NULL;
	}

	/* unpack file */
	saved = __pack_load(PACK_KEY_FILE | id, &length);

	/* existence/sanity check */
	if (!saved || length != sizeof(FILE)) {
		return NULL;
	}
	
	/* copy file */
	memcpy(new, saved, sizeof(FILE));

	return new;
}
