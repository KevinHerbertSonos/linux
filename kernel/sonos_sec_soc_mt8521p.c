/*
 * Copyright (c) 2014-2018, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <linux/random.h>
#include "tz_mtk_crypto_api.h"

#include <linux/sonos_sec_general.h>
#include <linux/sonos_sec_blob_api.h>

#include <trustzone/tz_cross/ta_efuse.h>
#include <trustzone/tz_cross/efuse_info.h>
#define SONOS_FUSE_CUS_DAT_LEN	16
#define SONOS_FUSE_UNLOCK_OFFSET 8

bool sonos_get_cpuid(uint8_t *buf, size_t buf_len)
{
	return buf_len == 8 &&
	       tee_fuse_read(HRID, buf, 8) == 0;
}

bool sonos_get_unlock_counter(uint32_t *pValue)
{
	uint8_t fuseVal[SONOS_FUSE_CUS_DAT_LEN];

	if (tee_fuse_read(CUS_DAT, fuseVal, sizeof(fuseVal))) {
		return false;
	}
	memcpy(pValue, fuseVal + SONOS_FUSE_UNLOCK_OFFSET, sizeof(*pValue));
	memzero_explicit(fuseVal, sizeof(fuseVal));
	return true;
}

/*
 * fixidn TODO:
 * Move the platform-specific AES GCM stuff into its own abstraction and
 * then 95% of this code can be moved into a common file (instead of having
 * it all cut/pasted per SoC).
 */

/* Returns 1 if regions overlap; 0 otherwise */
static int
regions_overlap(const void *in, size_t inLen,
		const void *out, size_t outLen)
{
	const void *high;
	const void *low;
	size_t lowLen;

	/* handle the degenerate cases */
	if (inLen == 0 || outLen == 0) {
		return 0;
	}

	if (in < out) {
		low = in;
		lowLen = inLen;
		high = out;
	}
	else {
		low = out;
		lowLen = outLen;
		high = in;
	}

	return (high - low) < lowLen;
}

int sonos_blob_encdec(bool isEncrypt, const void *in, size_t inLen,
		      void *vOut, size_t *pOutLen,
		      const void *keymodArg, size_t keymodLen)
{
	int result = -1;
	int mtkResult;
	struct mtk_crypto_ctx ctx;
	u8 iv[SONOS_BLOB_GCM_IV_SIZE];
	size_t requiredOutLen;
	u8 paddingLen;
	size_t outLen = *pOutLen;
	size_t i;
	u8 *padded = NULL;
	size_t paddedLen;
	u8 *out = (u8 *)vOut;
	u8 keymod[SECMEM_KEYMOD_LEN];

	*pOutLen = 0;

	if (regions_overlap(in, inLen, out, outLen)) {
		printk(KERN_CRIT "sonos_blob_encdec: overlapped in/out\n");
		return -1;
	}

	memset(&ctx, 0, sizeof(ctx));
	memset(keymod, 0, sizeof(keymod));

	mtkResult = mtk_crypto_ctx_init(&ctx);
	if (mtkResult != 0) {
		printk(KERN_CRIT "sonos_blob_encdec: crypto_ctx_init failed: %d\n", mtkResult);
		return mtkResult;
	}

	memcpy(keymod, keymodArg, min(keymodLen, sizeof(keymod)));

	if (isEncrypt) {
		u8 publicIv[SONOS_BLOB_GCM_IV_SIZE];
		u8 gcmTag[SONOS_BLOB_GCM_TAG_SIZE];

		if (inLen > SONOS_BLOB_MAX_ENC_INPUT_SIZE) {
			printk(KERN_CRIT "sonos_blob_encdec: enc inLen too long: %lu\n",
			       (unsigned long)inLen);
			goto end;
		}

		/*
		 * to the input length we add:
		 * PKCS#5 padding, 1 block of IV, GCM tag
		 */
		paddingLen = AES_BLOCK_SIZE - (inLen % AES_BLOCK_SIZE);
		paddedLen = inLen + paddingLen;
		requiredOutLen = paddedLen +
				 SONOS_BLOB_GCM_IV_SIZE + SONOS_BLOB_GCM_TAG_SIZE;
		if (outLen < requiredOutLen) {
			printk(KERN_CRIT "sonos_blob_encdec: enc outLen too short: %lu\n",
			       (unsigned long)outLen);
			goto end;
		}

		/*
		 * Generate the IV. The "public" one is attached to the
		 * encrypted output here, but the one we actually use has the
		 * keymod mixed in with it.
		 */
		memset(publicIv, 0x00, SONOS_BLOB_GCM_IV_SIZE);
		get_random_bytes(publicIv, SONOS_BLOB_GCM_IV_SIZE);
		for (i = 0; i < SONOS_BLOB_GCM_IV_SIZE; i++) {
			iv[i] = publicIv[i] ^ keymod[i % sizeof(keymod)];
		}

		mtkResult = mtk_crypto_cfg_aes_aad(&ctx, keymod, sizeof(keymod),
							 NULL);
		if (mtkResult != 0) {
			printk(KERN_CRIT "sonos_blob_encdec: enc aes_aad failed: %d\n", mtkResult);
			goto end;
		}

		/*
		 * add PKCS#5 padding
		 */
		padded = kmalloc(paddedLen, GFP_KERNEL);
		if (padded == NULL) {
			printk(KERN_CRIT "sonos_blob_encdec: enc kmalloc failed\n");
			goto end;
		}
		memcpy(padded, in, inLen);
		memset(padded + inLen, paddingLen, paddingLen);

		mtkResult = mtk_crypto_aes_using_hw_key(&ctx, 0,
							AES_GCM_MOD,
							AES_OP_MODE_ENC,
							iv, padded, out,
							paddedLen);
		if (mtkResult != 0) {
			printk(KERN_CRIT "sonos_blob_encdec: enc aes_using_hw_key failed: %d\n", mtkResult);
			goto end;
		}

		/*
		 * By our own convention output the ciphertext (the encryption
		 * of the PKCS#5 padded input), the public IV, and the GCM tag.
		 */
		memcpy(out + paddedLen, publicIv, SONOS_BLOB_GCM_IV_SIZE);
		mtk_crypto_get_aes_tag(&ctx, gcmTag);
		memcpy(out + paddedLen + SONOS_BLOB_GCM_IV_SIZE,
		       gcmTag, SONOS_BLOB_GCM_TAG_SIZE);
	}
	else {
		// DECRYPT
		const u8 *publicIv;
		const u8 *gcmTag;
		u8 *padding;

		if (inLen < AES_BLOCK_SIZE + SONOS_BLOB_GCM_IV_SIZE + SONOS_BLOB_GCM_TAG_SIZE) {
			printk(KERN_CRIT "sonos_blob_encdec: dec inLen too short: %lu\n",
			       (unsigned long)inLen);
			goto end;
		}

		paddedLen = inLen - (SONOS_BLOB_GCM_IV_SIZE + SONOS_BLOB_GCM_TAG_SIZE);
		if (paddedLen % AES_BLOCK_SIZE != 0) {
			printk(KERN_CRIT "sonos_blob_encdec: dec inLen not a block multiple: %lu\n",
			       (unsigned long)inLen);
			goto end;
		}

		/*
		 * By our own convention input is ciphertext (the encryption
		 * of the PKCS#5 padded input), the public IV, and the GCM tag.
		 */
		publicIv = in + paddedLen;
		for (i = 0; i < SONOS_BLOB_GCM_IV_SIZE; i++) {
			iv[i] = publicIv[i] ^ keymod[i % sizeof(keymod)];
		}

		gcmTag = publicIv + SONOS_BLOB_GCM_IV_SIZE;
		mtkResult = mtk_crypto_cfg_aes_aad(&ctx, keymod, sizeof(keymod),
							 (u8 *)gcmTag);
		if (mtkResult != 0) {
			printk(KERN_CRIT "sonos_blob_encdec: dec aes_aad failed: %d\n", mtkResult);
			goto end;
		}

		/*
		 * allocate space for padded decryption (so caller doesn't
		 * have to provide space for padding; exact size of their
		 * original input will work)
		 */
		padded = kmalloc(paddedLen, GFP_KERNEL);
		if (padded == NULL) {
			printk(KERN_CRIT "sonos_blob_encdec: dec kmalloc failed\n");
			goto end;
		}

		mtkResult = mtk_crypto_aes_using_hw_key(&ctx, 0,
							AES_GCM_MOD,
							AES_OP_MODE_DEC,
							iv, in, padded,
							paddedLen);
		if (mtkResult != 0) {
			printk(KERN_CRIT "sonos_blob_encdec: dec aes_using_hw_key failed: %d\n", mtkResult);
			goto end;
		}

		/* check the PKCS#5 padding */
		paddingLen = padded[paddedLen - 1];
		if (paddingLen < 1 ||
				paddingLen > AES_BLOCK_SIZE ||
				paddingLen > paddedLen) {
			printk(KERN_CRIT "sonos_blob_encdec: dec invalid padding\n");
			goto end;
		}
		padding = padded + paddedLen - paddingLen;
		for (i = 0; i < paddingLen; i++) {
			if (padding[i] != paddingLen) {
				printk(KERN_CRIT "sonos_blob_encdec: dec invalid padding\n");
				goto end;
			}
		}

		requiredOutLen = paddedLen - paddingLen;
		if (outLen < requiredOutLen) {
			printk(KERN_CRIT "sonos_blob_encdec: dec outLen too short: (in:%lu, out:%lu, required:%lu)\n",
			       (unsigned long)inLen,
			       (unsigned long)outLen,
			       (unsigned long)requiredOutLen);
			goto end;
		}

		/* copy the unpadded output */
		memcpy(out, padded, requiredOutLen);
	}
	*pOutLen = requiredOutLen;
	result = 0;

end:
	mtkResult = mtk_crypto_ctx_uninit(&ctx);
	if (mtkResult != 0) {
		printk(KERN_CRIT "sonos_blob_encdec: crypto_ctx_uninit failed: %d\n", mtkResult);
		result = mtkResult;
	}
	if (padded) {
		memzero_explicit(padded, paddedLen);
		kfree(padded);
	}
	memzero_explicit(iv, sizeof(iv));
	memzero_explicit(keymod, sizeof(keymod));
	return result;
}
EXPORT_SYMBOL(sonos_blob_encdec);
