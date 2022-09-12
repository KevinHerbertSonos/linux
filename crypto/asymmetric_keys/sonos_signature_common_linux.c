/*
 * Copyright (c) 2015-2018, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 *
 * Sonos Signature format support
 */

/* various macros needed by the included code */
#include <crypto/sonos_signature_macros_linux.h>

#include <crypto/sha.h>
#include <crypto/hash.h>
#include <linux/crypto.h>
#include <linux/err.h>
#include <linux/scatterlist.h>
#include <linux/highmem.h>

#include <crypto/sonos_signature_common_linux.h>

/* most of the common implementation */
#include "sonos_signature_common.c.inc"

int
sonosHash(SonosDigestAlg_t alg, const void *buf, size_t bufLen,
	  uint8_t *digest, size_t *pDigestLen)
{
	struct crypto_shash *tfm;
	int result = 0;

	if (alg != SONOS_DIGEST_ALG_SHA256 ||
	    *pDigestLen < SHA256_DIGEST_SIZE) {
		printk(KERN_ERR "sonosHash: bad alg (%d) or digestLen (%d)\n",
					 (int)alg, (int)*pDigestLen);
		return 0;
	}

	tfm = crypto_alloc_shash("sha256", 0, 0);
	if (IS_ERR(tfm)) {
		printk(KERN_ERR "sonosHash: could not allocate crypto hash\n");
		return 0;
	}

	{
		SHASH_DESC_ON_STACK(desc, tfm);
		desc->tfm = tfm;

		if (crypto_shash_digest(desc, buf, bufLen, digest)) {
			printk(KERN_ERR "sonosHash: crypto_hash_digest failed\n");
		}
		else {
			*pDigestLen = SHA256_DIGEST_SIZE;
			result = 1;
		}
	}

	crypto_free_shash(tfm);

	return result;
}
