/*
 * Copyright 2017 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */

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
#include <net/net_crypto.h>


/**
 * aes_cbc_encrypt - AES CBC encryption
 * @key: Encryption key
 * @iv: Encryption IV for CBC mode (16 bytes)
 * @data: Data to encrypt in-place
 * @data_len: Length of data in bytes (must be divisible by 16)
 * Returns: 0 on success, -1 on failure
 */
int aes_cbc_encrypt(psAesKey_t *ctx, u8 *iv, u8 *data, size_t data_len)
{
	u8 *pos = data;
	int i, j, blocks;

	blocks = data_len / AES_BLOCK_SIZE;
	for (i = 0; i < blocks; i++) {
		for (j = 0; j < AES_BLOCK_SIZE; j++)
			iv[j] ^= pos[j];
		psAesEncryptBlock(iv, iv, ctx);
		os_memcpy(pos, iv, AES_BLOCK_SIZE);
		pos += AES_BLOCK_SIZE;
	}
	return 0;
}

/**
 * aes_cbc_decrypt - AES CBC decryption
 * @key: Decryption key
 * @iv: Decryption IV for CBC mode (16 bytes)
 * @data: Data to decrypt in-place
 * @data_len: Length of data in bytes (must be divisible by 16)
 * Returns: 0 on success, -1 on failure
 */
 
int aes_cbc_decrypt(psAesKey_t *ctx, u8 *iv, u8 *data, size_t data_len)
{
	u8 tmp[AES_BLOCK_SIZE];
	u8 *pos = data;
	int i, j, blocks;

	blocks = data_len / AES_BLOCK_SIZE;
	for (i = 0; i < blocks; i++) {
		os_memcpy(tmp, pos, AES_BLOCK_SIZE);
		psAesDecryptBlock(pos, pos, ctx);
		for (j = 0; j < AES_BLOCK_SIZE; j++)
			pos[j] ^= iv[j];
		os_memcpy(iv, tmp, AES_BLOCK_SIZE);
		pos += AES_BLOCK_SIZE;
	}
	return 0;
}

int adc_sha1(const void *buf, size_t len,
    const void *buf2, size_t len2, void *sig)
{
    psDigestContext_t   sha;

	psSha1Init(&sha);
	psSha1Update(&sha, buf, len);
    if(buf2){
	    psSha1Update(&sha, buf2, len2);
    }
	psSha1Final(&sha, (u8_t *)sig);
	return 0;

}

void adc_sha256_init(struct adc_sha256 *sha_ctx)
{
    psSha256Init(&sha_ctx->ctx);
}

void adc_sha256_update(struct adc_sha256 *sha_ctx,
    const void *buf, size_t len, const void *buf2, size_t len2)
{
    psSha256Update(&sha_ctx->ctx, buf, len);
    if(buf2){
	    psSha256Update(&sha_ctx->ctx, buf2, len2);
    }
}

void adc_sha256_final(struct adc_sha256 *sha_ctx,
		void *sign)
{
    psSha256Final(&sha_ctx->ctx, (u8_t *)sign);
}



struct adc_dev;

struct adc_dev *adc_aes_open(void)
{
	return NULL;
}

int adc_aes_cbc_key_set(struct adc_dev *dev, struct adc_aes *aes,
		void *key, size_t key_len, void *iv, int decrypt)
{
    psAesInitKey(key, key_len, &aes->ctxt);
    memcpy(aes->iv, iv, sizeof(aes->iv));
	return 0;
}

/*
 * If possible, return the Initialization Vector (IV) from the AES key context.
 * If not possible, return -1, and leave buffer untouched.
 */
int adc_aes_iv_get(struct adc_aes *aes, void *buf, size_t len)
{
    if (len > sizeof(aes->iv)) {
		len = sizeof(aes->iv);
	}
	memcpy(buf, aes->iv, len);
	return 0;
}

int adc_aes_cbc_encrypt(struct adc_dev *dev, struct adc_aes *aes,
				void *buf, size_t len)
{
    aes_cbc_encrypt(&aes->ctxt, aes->iv, buf, len);
	return 0;
}

int adc_aes_cbc_decrypt(struct adc_dev *dev, struct adc_aes *aes,
				void *buf, size_t len)
{
    aes_cbc_decrypt(&aes->ctxt, aes->iv, buf, len);
	return 0;
}

int adc_aes_entropy_add(void *random_data, size_t len)
{
	return 0;
}



void adc_rng_init(struct adc_rng *rng)
{
}

int adc_rng_random_fill(struct adc_rng *rng,
					void *buf, size_t len)
{
    int ret;
    tls_crypto_random_init(tls_os_get_time(), CRYPTO_RNG_SWITCH_16);
	ret = tls_crypto_random_bytes(buf, len);
	tls_crypto_random_stop();
	return ret;
}


/*
 * Set key from binary ASN-1 sequence buffer.
 * Returns key size, or 0 on failure.
 * Caller must call adc_rsa_key_clear(), even on failure.
 */
 //TODO
size_t adc_rsa_key_set(struct adc_rsa_key *key,
				void *buf, size_t keylen)
{
    int rc;
    //unsigned char buf_p[512] = {0};
    uint32 len;
	int oi;
    unsigned char *p = buf;
	unsigned char *buf_p = NULL;
	unsigned char *end = buf + keylen;
	if (getAsnSequence(&p, (int32)(end - p), &len) < 0) {
		printf("Couldn't parse signed element sequence\n");
		return 0;
	}
	if (getAsnAlgorithmIdentifier(&p, (int32)(end - p), &oi, &len) < 0) {
		//printf("go to no_version\n");
		goto no_version;
	}
	goto oid_ver;

no_version:
	buf_p = tls_mem_alloc(512);
	if(NULL == buf_p)
	{
		return PS_FAILURE;
	}
	memset( buf_p, 0, 512 );
    buf_p[0] = 0x03;
    buf_p[1] = 0x82;
    buf_p[2] = ((keylen + 1) >> 8) & 0xFF;
    buf_p[3] = (keylen + 1) & 0xFF;
    buf_p[4] = 0x01;
    memcpy(buf_p+5, buf, keylen);
    p = buf_p;
	end = buf_p + keylen + 5;

oid_ver:
    rc = getAsnRsaPubKey(NULL, &p, (int32)(end - p), &key->key);
    if(rc < 0){
		printcli("Couldn't get RSA pub key from cert\n");
    }
	if(buf_p)
	{
    	tls_mem_free(buf_p);
	}
	return key->key.size;
}

void adc_rsa_key_clear(struct adc_rsa_key *key)
{
    if(key) {
    	pstm_clear(&(key->key.N));
    	pstm_clear(&(key->key.e));
    	pstm_clear(&(key->key.d));
    	pstm_clear(&(key->key.p));
    	pstm_clear(&(key->key.q));
    	pstm_clear(&(key->key.dP));
    	pstm_clear(&(key->key.dQ));
    	pstm_clear(&(key->key.qP));
    	memset(&key->key, 0, sizeof(key->key));
    }
}

int adc_rsa_encrypt_pub(struct adc_rsa_key *key,
				void *in, size_t in_len,
				void *out, size_t out_len,
				struct adc_rng *rng)
{
    int ret;
    
    ret = psRsaEncryptPub(NULL, key, in, in_len, out, out_len, NULL);
	return ret;
}

//TODO
int adc_rsa_verify(struct adc_rsa_key *key,
				void *in, size_t in_len,
				void *out, size_t out_len)
{
    int ret;
    
    ret = psRsaDecryptPub(NULL, &key->key, in, in_len, out, out_len, NULL);    
	if (ret < 0)
		return ret;
	else
		return out_len;
}



void adc_hmac_sha256_init(struct adc_hmac_ctx *ctx,
					 const void *seed, size_t len)
{
    psHmacSha2Init(&ctx->ctx, seed, len, SHA256_SIG_LEN);
}

void adc_hmac_sha256_update(struct adc_hmac_ctx *ctx,
	const void *buf, size_t len)
{
    psHmacSha2Update(&ctx->ctx, buf, len, SHA256_SIG_LEN);
}

int adc_hmac_sha256_final(struct adc_hmac_ctx *ctx, void *sign)
{
    psHmacSha2Final(&ctx->ctx, sign, SHA256_SIG_LEN);
	return 0;
}

