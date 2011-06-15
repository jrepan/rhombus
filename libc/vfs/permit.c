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

#include <natio.h>
#include <mutex.h>
#include <proc.h>
#include <vfs.h>

/*****************************************************************************
 * vfs_permit
 *
 * Decide whether a particular operation is permitted upon a resource. The
 * valid arguments for <operation> are PERM_READ, PERM_WRITE, PERM_ALTER and
 * PERM_LOCK.
 *
 * Returns zero if the given operation is illegal, nonzero if it is legal.
 *
 * This function does not acquire a lock on <r>, but does acquire a lock on
 * <r->lock>. <r> must not be modified while this function is running.
 */

int vfs_permit(struct resource *r, uint64_t source, int operation) {
	uint32_t pid = RP_PID(source);
	
	/* check permissions */
	if ((acl_get(r->acl, getuser(pid)) & operation) == 0) {
		/* permission denied */
		return 0;
	}

	/* check locks */
	if (!r->lock) return 1;
	mutex_spin(&r->lock->mutex);
	switch (operation) {
	case PERM_READ:
		if (!r->lock->exlock && !r->lock->prlock) {
			/* no exclusive lock acquired, success */
			mutex_free(&r->lock->mutex);
			return 1;
		}
		else {
			/* exclusive lock acquired */
			mutex_free(&r->lock->mutex);
			if (vfs_lock_current(r->lock, pid) != LOCK_EX 
				&& vfs_lock_current(r->lock, pid) != LOCK_PR) {
				/* exclusive lock is for someone else */
				return 0;
			}
			else {
				/* exclusive lock is ours */
				return 1;
			}
		}
		break;
	case PERM_WRITE:
		if (r->lock->shlock) {
			/* shared lock, no writing */
			mutex_free(&r->lock->mutex);
			return 0;
		}

		if (r->lock->exlock || r->lock->prlock) {
			mutex_free(&r->lock->mutex);
			if (vfs_lock_current(r->lock, pid) == LOCK_EX 
				|| vfs_lock_current(r->lock, pid) == LOCK_PR) {
				/* exclusive lock is ours */
				return 1;
			}
			else {
				return 0;
			}
		}

		/* no lock */
		mutex_free(&r->lock->mutex);
		return 1;
	case PERM_ALTER:
	case PERM_LOCK:
		mutex_free(&r->lock->mutex);
		return 1;
	}

	return 0;
}
