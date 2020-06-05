/*
 * Copyright 2017 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADC_CRYPTO_H__
#define __AYLA_ADC_CRYPTO_H__

#include <stdlib.h>
/*
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>
#include <mbedtls/md.h>
#include <mbedtls/aes.h>
*/

//#include <device_lock.h>
#include "wm_osal.h"
#include "wm_crypto_hard.h"
#include "cryptoApi.h"
#include "aes.h"


#define AES_BLOCK_SIZE	16
#define SHA1_SIG_LEN	20
#define AES_GET_IV_SUPPORT	1
#define SHA256_SIG_LEN	32
/*
 * SHA-256 functions.
 */
#define ADC_SHA256_HASH_SIZE 32

struct adc_sha256 {
    psDigestContext_t ctx;
};

struct adc_aes {
    psAesKey_t		ctxt;
	u8 iv[16];
};

/*
 * random number generator.
 */
struct adc_rng {
	u8 init;
	void *rng;
};

struct adc_rsa_key {
	psRsaKey_t key;
};

struct adc_hmac_ctx {
	psHmacContext_t ctx;
};

#endif /* __AYLA_ADC_CRYPTO_H__ */
