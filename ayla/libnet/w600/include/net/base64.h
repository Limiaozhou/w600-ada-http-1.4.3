/*
 * Copyright 2017 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_NET_BASE64_H__
#define __AYLA_NET_BASE64_H__

#include "polarssl/base64.h"

static inline int net_base64_decode(const void *in, size_t in_len,
					void *out, size_t *out_len)
{
	int ret;
    ret = base64_decode( (unsigned char *)out, out_len, (const unsigned char *)in, in_len );
    //unsigned char inputString[] = "1234567890";
    //ret = base64_decode( (unsigned char *)out, out_len, inputString, strlen(inputString)+1);
    /*
    for(u16 i=0; i<*out_len; i++)
        printcli("%02x", *((unsigned char *)out+i));   
    printcli("\n");
    */
	return ret;
}

static inline int net_base64_encode(const void *in, size_t in_len,
					void *out, size_t *out_len)
{
	int ret;
    ret = base64_encode( (unsigned char *)out, out_len, (const unsigned char *)in, in_len );
	if (ret == 0)
		((unsigned char *)out)[*out_len] = 0;
	return ret;
}

#endif /* __AYLA_NET_BASE64_H__ */
