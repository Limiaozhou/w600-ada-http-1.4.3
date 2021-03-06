/*
 * Copyright 2015 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __ADA_LOCK_H__
#define __ADA_LOCK_H__

struct ada_lock;	/* semi-opaque, will be cast to an os-specific lock */

/*
 * Multi-threaded version, requiring locks.
 */
struct ada_lock *ada_lock_create(const char *name);
void ada_lock(struct ada_lock *);
void ada_unlock(struct ada_lock *);

#endif /* __ADA_LOCK_H__ */
