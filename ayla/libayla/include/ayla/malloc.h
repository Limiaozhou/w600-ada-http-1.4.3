/*
 * Copyright 2015 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_MALLOC_H__
#define __AYLA_MALLOC_H__

#if 1
#include <wm_mem.h>

#define malloc tls_mem_alloc
#define realloc tls_mem_realloc
#define free tls_mem_free
#define calloc tls_mem_calloc
#define ayla_free tls_mem_free

 void *ayla_calloc(size_t count, size_t size);
#endif /* WMSDK */

#endif /* __AYLA_MALLOC_H__ */
