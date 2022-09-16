/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2014-2019, Sonos, Inc.
 *
 * Includes the PRF from TLS 1.2 (hard-coded to use SHA256):
 * Code ported from mbed TLS 2.7.0 (tls_prf_generic in library/ssl_tls.c).
 *
 *  Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
 *  SPDX-License-Identifier: GPL-2.0
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <crypto/hash.h>

#include <linux/sonos_mdp_global.h>
#include <linux/sonos_sec_blob_api.h>
#include <linux/sonos_sec_fs_keys.h>
#include "mdp.h"

/*
 * The max symmetric algorithm key length corresponds to aes256 w/ XTS.
 *
 * No MDP3 secret is actually this long (those are 32 bytes max) but just
 * use this for thoroughness. We could bump those lengths up later without
 * changing this code.
 */
#define MAX_KEY_LEN 64

static bool sonos_decrypt_mdp_fskey(const uint8_t *buf, size_t bufLen,
				    uint8_t *out, size_t *pOutLen,
				    const char *modifier)
{
	struct mdp_key_hdr hdr;

	/* consume the MDP key header */
	if (bufLen < sizeof(hdr)) {
		printk(KERN_CRIT "bad fskey input len %zu\n", bufLen);
		return false;
	}
	memcpy(&hdr, buf, sizeof(hdr));
	buf += sizeof(hdr);
	bufLen -= sizeof(hdr);
	if (hdr.m_len > bufLen) {
		printk(KERN_CRIT "bad fskey header len %zu\n", bufLen);
		return false;
	}

#if defined(SONOS_ARCH_ATTR_SOC_IS_IMX6)
	/*
	 * Decrypting the fs key only makes sense for red keys. On some
	 * platforms there is a black key preceding the red one that
	 * we need to skip over.
	 */
	if (hdr.m_magic == MDP_KEY_HDR_MAGIC_CAAM_AES128_BLACK_KEY) {
		/* skip the black key */
		buf += hdr.m_len;
		bufLen -= hdr.m_len;

		/* consume the next MDP key header (should be red) */
		if (bufLen < sizeof(hdr)) {
			printk(KERN_CRIT "bad fskey input len %u\n", bufLen);
			return false;
		}
		memcpy(&hdr, buf, sizeof(hdr));
		buf += sizeof(hdr);
		bufLen -= sizeof(hdr);
		if (hdr.m_len > bufLen) {
			printk(KERN_CRIT "bad fskey header len %u\n", bufLen);
			return false;
		}
	}
#endif

	if (hdr.m_magic != MDP_KEY_HDR_MAGIC_CAAM_AES128_RED_KEY) {
		printk(KERN_CRIT "no fskey red header\n");
		return false;
	}

	if (sonos_blob_encdec(false, buf, hdr.m_len, out, pOutLen,
			      modifier, strlen(modifier)) != 0) {
		printk(KERN_CRIT "fskey decrypt failed\n");
		return false;
	}

	return true;
}

#define BLOB_KEYLABEL_JFFS	"ubifs"
#define BLOB_KEYLABEL_ROOTFS	"rootfs"

static bool sonos_get_mdp_rootfs_key(uint8_t *out, size_t *pOutLen)
{
	return sys_mdp3.mdp3_version >= MDP3_VERSION_DEV_CERT &&
	       sonos_decrypt_mdp_fskey(sys_mdp3.mdp3_fskey2,
				       sizeof(sys_mdp3.mdp3_fskey2),
				       out, pOutLen,
				       BLOB_KEYLABEL_ROOTFS);
}

static bool sonos_get_mdp_jffs_key(uint8_t *out, size_t *pOutLen)
{
	return sys_mdp3.mdp3_version >= MDP3_VERSION_DEV_CERT &&
	       sonos_decrypt_mdp_fskey(sys_mdp3.mdp3_fskey1,
				       sizeof(sys_mdp3.mdp3_fskey1),
				       out, pOutLen,
				       BLOB_KEYLABEL_JFFS);
}

#if defined(CONFIG_UBIFS_FS)

bool sonos_set_ubifs_key(u32 type)
{
	bool retval = false;
	uint8_t key[MAX_KEY_LEN];
	size_t keyLen = sizeof(key);

	if (type == UBIFS_CRYPT_TYPE_NONE) {
		// Don't set a key - the fs will be accessed without encryption/decryption
		retval = true;
	}
	else if (type == UBIFS_CRYPT_TYPE_FIXED) {
		static const u8 key1[16] = { 0x55, 0x61, 0x4b, 0xde, 0x49, 0x5e, 0x0e, 0xd1, 0x50, 0x43, 0x77, 0x87, 0x94, 0x8e, 0x16, 0x3b };
		ubifs_set_ubifs_key(key1);
		retval = true;
	}
	else if (type == UBIFS_CRYPT_TYPE_RED_KEY) {
		// decrypt the key using a hw specific mechanism
		if (sonos_get_mdp_jffs_key(key, &keyLen) &&
		    keyLen >= UBIFS_CRYPTO_KEYSIZE) {
			ubifs_set_ubifs_key(key);
			retval = true;
		}
	}

	return retval;
}
EXPORT_SYMBOL(sonos_set_ubifs_key);

#endif

#ifdef CONFIG_DM_CRYPT
/*
 * The rest of this file is code for using a PRF to derive LUKS master keys
 * by mixing a master secret (MDP secret value) and a nonce (the sentinel
 * LUKS master key value contained in the LUKS header which contains some
 * randomness in it).
 *
 * We patch dm-crypt to call sonos_replace_luks_key_if_sentinel when it is
 * opening a LUKS volume in order to plug it into all of this logic.
 */
#define SENTINEL1_BYTE 0
#define SENTINEL2_BYTE 0xff

/* constant time to not leak key info */
static bool mem_is_fixed_value(const void *vbuf, int c, size_t len)
{
	const unsigned char *buf = vbuf;
	size_t i;
	int different = 0;

	for (i = 0; i < len; i++) {
		different |= c ^ buf[i];
	}
	return !different;
}

/* check for either of the two sentinel values in constant time */
static bool key_contains_sentinel(const void *buf, size_t len)
{
	bool hasSentinel;
	hasSentinel  = mem_is_fixed_value(buf, SENTINEL1_BYTE, len / 2);
	hasSentinel |= mem_is_fixed_value(buf, SENTINEL2_BYTE, len / 2);
	return hasSentinel;
}

/* cut/pasted from net/bluetooth/amp.c */
static int hmac_sha256(const u8 *key, u8 ksize, char *plaintext, u8 psize, u8 *output)
{
	struct crypto_shash *tfm;
	struct shash_desc *shash;
	int ret;

	if (!ksize) {
		return -EINVAL;
	}

	tfm = crypto_alloc_shash("hmac(sha256)", 0, 0);
	if (IS_ERR(tfm)) {
		return PTR_ERR(tfm);
	}

	ret = crypto_shash_setkey(tfm, key, ksize);
	if (ret) {
		goto failed;
	}

	shash = kzalloc(sizeof(*shash) + crypto_shash_descsize(tfm),
			GFP_KERNEL);
	if (!shash) {
		ret = -ENOMEM;
		goto failed;
	}

	shash->tfm = tfm;

	ret = crypto_shash_digest(shash, plaintext, psize, output);

	kfree(shash);

failed:
	crypto_free_shash(tfm);
	return ret;
}


#define SHA256_DIGEST_LEN (256 / 8)
static int prf(const uint8_t *secret, size_t slen,
	       const char *label,
	       const uint8_t *random, size_t rlen,
	       uint8_t *dstbuf, size_t dlen)
{
	size_t nb;
	size_t i, j, k, md_len = SHA256_DIGEST_LEN;
	unsigned char tmp[128];
	unsigned char h_i[SHA256_DIGEST_LEN];
	int result = -EINVAL;

	if (sizeof(tmp) < md_len + strlen(label) + rlen) {
		return -EINVAL;
	}

	nb = strlen(label);
	memcpy(tmp + md_len, label, nb);
	memcpy(tmp + md_len + nb, random, rlen);
	nb += rlen;

	result = hmac_sha256(secret, slen, tmp + md_len, nb, tmp);
	if (result != 0) {
		goto end;
	}

	for (i = 0; i < dlen; i += md_len)
	{
		result = hmac_sha256(secret, slen, tmp, md_len + nb, h_i);
		if (result != 0) {
			goto end;
		}

		result = hmac_sha256(secret, slen, tmp, md_len, tmp);
		if (result != 0) {
			goto end;
		}

		k = (i + md_len > dlen) ? dlen % md_len : md_len;

		for (j = 0; j < k; j++) {
			dstbuf[i + j] = h_i[j];
		}
	}

	result = 0;

end:
	memzero_explicit(tmp, sizeof(tmp));
	memzero_explicit(h_i, sizeof(h_i));

	return result;
}

bool sonos_replace_luks_key_if_sentinel(char *keyInHex)
{
	/* the incoming key will be used as a nonce if it is a sentinel */
	uint8_t keyIn[MAX_KEY_LEN];
	size_t keyInLen;
	/* the decrypted key from MDP */
	uint8_t keyMdp[MAX_KEY_LEN];
	size_t keyMdpLen = sizeof(keyMdp);
	/* the PRF output combining keyMdp and keyIn */
	uint8_t keyOut[MAX_KEY_LEN];
	size_t keyOutLen;

	size_t keyInHexLen = strlen(keyInHex);
	size_t i;
	bool result = false;

#if defined(SONOS_ARCH_ATTR_SOC_IS_IMX6) || defined(SONOS_ARCH_ATTR_SOC_IS_MT8521P)
	#define LEGACY_LUKS_ROOTFS_SENTINEL_KEY "ae3a8b0e383ec17d5f21fcc8d6873681"
	if (strcmp(keyInHex, LEGACY_LUKS_ROOTFS_SENTINEL_KEY) == 0) {
		/* get the rootfs MDP key and use it directly */
		keyOutLen = 16;
		result = sonos_get_mdp_rootfs_key(keyOut, &keyOutLen);
		if (!result) {
			goto end;
		}
	}
	else
#endif
	{
		if (keyInHexLen % 2 != 0) {
			goto end;
		}

		/* hex decode */
		keyInLen = keyOutLen = keyInHexLen / 2;
		if (keyInLen > sizeof(keyIn) ||
		    hex2bin(keyIn, keyInHex, keyInLen) != 0) {
			goto end;
		}

		/* stop now if we don't have a sentinel */
		if (!key_contains_sentinel(keyIn, keyInLen)) {
			result = true;
			goto end;
		}

		/* get the MDP key */
		result = keyIn[0] == SENTINEL1_BYTE
			? sonos_get_mdp_jffs_key(keyMdp, &keyMdpLen)
			: sonos_get_mdp_rootfs_key(keyMdp, &keyMdpLen);
		if (!result) {
			goto end;
		}

		/* combine the MDP key and nonce */
		if (prf(keyMdp, keyMdpLen, "sonos luks", keyIn, keyInLen,
			keyOut, keyOutLen) != 0) {
			goto end;
		}
	}

	/* overwrite the (hex) key with the new result */
	for (i = 0; i < keyOutLen; i++, keyInHex += 2) {
		sprintf(keyInHex, "%02x", keyOut[i]);
	}
	result = true;

end:
	memzero_explicit(keyIn, sizeof(keyIn));
	memzero_explicit(keyMdp, sizeof(keyMdp));
	memzero_explicit(keyOut, sizeof(keyOut));
	return result;
}
EXPORT_SYMBOL(sonos_replace_luks_key_if_sentinel);
#endif

