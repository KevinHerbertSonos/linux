/*
 * Copyright (c) 2021, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <linux/random.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/efuse.h>
#include "../drivers/amlogic/efuse/efuse.h"
#if !defined(SONOS_ARCH_ATTR_STUB_SECBOOT_BLOB)
#include "../drivers/amlogic/aes_hwkey_gcm/aes_hwkey_gcm.h"
#endif

#include <linux/sonos_sec_general.h>
#include <linux/sonos_sec_blob_api.h>

#define SONOS_FUSE_GP_REE_LEN	16
bool sonos_get_cpuid(uint8_t *buf, size_t buf_len)
{
	if (buf_len != CHIPID_LEN) {
		return false;
	}
	cpuinfo_get_chipid(buf, CHIPID_LEN);
	return true;
}

bool sonos_get_unlock_counter(uint32_t *pValue)
{
	uint8_t fuseVal[SONOS_FUSE_GP_REE_LEN];
	uint32_t fuseValSize = sizeof(fuseVal);

	if (meson_efuse_obj_read(EFUSE_OBJ_GP_REE, fuseVal, &fuseValSize)) {
		return false;
	}
	memcpy(pValue, fuseVal, sizeof(*pValue));
	memzero_explicit(fuseVal, sizeof(fuseVal));
	return true;
}


#if !defined(SONOS_ARCH_ATTR_STUB_SECBOOT_BLOB)
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
	int amlResult;
	struct aes_gcm_ctx ctx;
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

		amlResult = aes_hwkey_gcm_init(&ctx, iv, keymod, sizeof(keymod), NULL, 1);
		if (amlResult != 0) {
			printk(KERN_CRIT "sonos_blob_encdec: enc gcm_init failed: %d\n", amlResult);
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

		amlResult = aes_hwkey_gcm_process(&ctx, padded, out, paddedLen);

		if (amlResult != 0) {
			printk(KERN_CRIT "sonos_blob_encdec: enc gcm_process failed: %d\n", amlResult);
			goto end;
		}

		/*
		 * By our own convention output the ciphertext (the encryption
		 * of the PKCS#5 padded input), the public IV, and the GCM tag.
		 */
		amlResult = aes_hwkey_gcm_get_tag(&ctx, gcmTag);
		if (amlResult != 0) {
			printk(KERN_CRIT "sonos_blob_encdec: enc gcm_get_tag failed: %d\n", amlResult);
			goto end;
		}

		memcpy(out + paddedLen, publicIv, SONOS_BLOB_GCM_IV_SIZE);
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

		amlResult = aes_hwkey_gcm_init(&ctx, iv, keymod, sizeof(keymod), gcmTag, 0);
		if (amlResult != 0) {
			printk(KERN_CRIT "sonos_blob_encdec: dec gcm_init failed: %d\n", amlResult);
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

		amlResult = aes_hwkey_gcm_process(&ctx, in, padded, paddedLen);

		if (amlResult != 0) {
			printk(KERN_CRIT "sonos_blob_encdec: dec gcm_process failed: %d\n", amlResult);
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
	if (padded) {
		memzero_explicit(padded, paddedLen);
		kfree(padded);
	}
	memzero_explicit(iv, sizeof(iv));
	memzero_explicit(keymod, sizeof(keymod));
	return result;
}
EXPORT_SYMBOL(sonos_blob_encdec);
#endif
