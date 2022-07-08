/*
 * Copyright (c) 2014-2017, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <linux/module.h>
#include "mdp.h"
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/sonos_kernel.h>
#include <linux/slab.h>
#include <crypto/sonos_signature_common_linux.h>
#include <crypto/sonos_signature_verify_linux.h>
#if defined(SONOS_ARCH_ATTR_SOC_IS_MT8521P)
#include <linux/random.h>
#include "tz_mtk_crypto_api.h"
#endif

#include "sect_upgrade_header.h"

extern struct manufacturing_data_page sys_mdp;
extern struct manufacturing_data_page3 sys_mdp3;

/* macros needed by the portable implementation of unlock/authz checking */
#define SU_BE32_TO_CPU		be32_to_cpu
#define SU_CPU_TO_BE32		cpu_to_be32
#define SU_GET_CPUID		sonos_get_cpuid
#define SU_GET_UNLOCK_COUNTER	sonos_get_unlock_counter
#define SU_PRINT		printk
#define SU_PLVL_DEBUG		KERN_DEBUG
#define SU_PLVL_INFO		KERN_INFO
#define SU_PLVL_ERR		KERN_ERR

/* get the portable implementation of unlock/authz checking */
#include "sonos_unlock_token.c.inc"
#include "sonos_unlock.c.inc"

EXPORT_SYMBOL(sonosUnlockVerifyCpuSerialSig);
EXPORT_SYMBOL(sonosUnlockIsDeviceUnlocked);
EXPORT_SYMBOL(sonosUnlockIsAuthFeatureEnabled);

/* Support the persistent unlock functionality... */
/* NOTE:  This function is NOT safe to run from interrupt context */
int is_mdp_authorized(u32 mdp_authorization_flag)
{
	int result = 0;
	SonosSignature *sig = NULL;

	// This structure is pretty massive and could cause
	// us to run out of stack if we try to allocate it there,
	// so use kmalloc instead.  Optimization ideas are welcome.
	sig = kmalloc(sizeof(*sig), GFP_KERNEL);

	result = sig &&
		sonosUnlockIsAuthFeatureEnabled(mdp_authorization_flag,
						&sys_mdp,
						&sys_mdp3,
						sig,
						sonosHash,
						sonosRawVerify,
						sonosKeyLookup,
						"unit",
						"unlock",
						NULL);

	kfree(sig);
	return result;
}
EXPORT_SYMBOL(is_mdp_authorized);

/*
 *	Unlike the other authorization checks, the call to check
 *	sysrq happens at interrupt level, and the UnlockIsAuthFeature
 *	function is not interrupt safe.  So set a static at boot time,
 *	and only check that.
 */
static int sysrq_authorization = 0;
int is_sysrq_authorized(void)
{
	return sysrq_authorization;
}
EXPORT_SYMBOL(is_sysrq_authorized);

void check_sysrq_authorization(void)
{
	sysrq_authorization = is_mdp_authorized(MDP_AUTH_FLAG_KERNEL_DEBUG_ENABLE);
}
EXPORT_SYMBOL(check_sysrq_authorization);

/* Provide access to the decrypt functionality before mounting rootfs... */
struct dm_ioctl;
extern int dm_dev_create(struct dm_ioctl *param, size_t param_size);
EXPORT_SYMBOL(dm_dev_create);
extern int dm_table_load(struct dm_ioctl *param, size_t param_size);
EXPORT_SYMBOL(dm_table_load);
extern int dm_dev_suspend(struct dm_ioctl *param, size_t param_size);
EXPORT_SYMBOL(dm_dev_suspend);

#if defined(SONOS_ARCH_ATTR_SOC_IS_IMX6)

/* Handle the security keys needed... */

extern struct platform_device *sonos_sm_get_pdev(void);
extern int sonos_sm_init (struct platform_device *pdev);
extern void sonos_sm_exit (void);
extern int sonos_sm_encdec(struct platform_device *pdev, struct crypt_operation *op);

#elif defined(SONOS_ARCH_ATTR_SOC_IS_MT8521P)
#else
#error "unsupported secure boot ARCH type"
#endif

#define BLOB_KEYLABEL_UBIFS     "ubifs"
#define BLOB_KEYLABEL_ROOTFS    "rootfs"

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

/* Returns 0 on success, non-zero on failure */
#if defined(SONOS_ARCH_ATTR_SOC_IS_IMX6)
int sonos_blob_encdec(bool isEncrypt, const void *in, size_t inLen,
		      void *out, size_t *pOutLen,
		      const void *keymod, size_t keymodLen)
{
	int status;
	struct crypt_operation crypt_op;

	if (regions_overlap(in, inLen, out, *pOutLen)) {
		printk(KERN_CRIT "sonos_blob_encdec: overlapped in/out\n");
		return -1;
	}

	// Can't validate without a key...set up the unit/keyslot
	// in the CAAM driver...
	status = sonos_sm_init(sonos_sm_get_pdev());
	if (status) {
		printk(KERN_CRIT "sonos_blob_encdec: sm_init failed: %d\n",
		       status);
		return status;
	}

	// Set up the encryption command for the CAAM driver...
	memset(&crypt_op, 0, sizeof(crypt_op));
	crypt_op.is_encrypt = isEncrypt;
	crypt_op.input_buffer = (void*)in;
	crypt_op.input_length = inLen;
	crypt_op.output_buffer = out;
	crypt_op.output_length = pOutLen;
	crypt_op.original_length = isEncrypt ? inLen : inLen - SONOS_BLOB_IMX6_RED_OVERHEAD;
	memcpy(crypt_op.keymod, keymod,
	       min(keymodLen, sizeof(crypt_op.keymod)));

	status = sonos_sm_encdec(sonos_sm_get_pdev(), &crypt_op);
	if (status) {
		printk(KERN_CRIT "sonos_blob_encdec: sm_encdec failed: %d\n",
		       status);
	}

	// Release the unit and keyslot...
	sonos_sm_exit();
	return status;
}
#elif defined(SONOS_ARCH_ATTR_SOC_IS_MT8521P)
int sonos_blob_encdec(bool isEncrypt, const void *in, size_t inLen,
		      void *vOut, size_t *pOutLen,
		      const void *keymodArg, size_t keymodLen)
{
	int result = -1;
	int mtkResult;
	struct mtk_crypto_ctx ctx;
	u8 iv[AES_BLOCK_SIZE];
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
		u8 publicIv[AES_BLOCK_SIZE];
		u8 gcmTag[SONOS_BLOB_MTK_GCM_TAG_SIZE];

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
				 AES_BLOCK_SIZE + SONOS_BLOB_MTK_GCM_TAG_SIZE;
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
		memset(publicIv, 0x00, AES_BLOCK_SIZE);
		get_random_bytes(publicIv, AES_BLOCK_SIZE);
		for (i = 0; i < AES_BLOCK_SIZE; i++) {
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
		memcpy(out + paddedLen,
		       publicIv, AES_BLOCK_SIZE);
		mtk_crypto_get_aes_tag(&ctx, gcmTag);
		memcpy(out + paddedLen + AES_BLOCK_SIZE,
		       gcmTag, SONOS_BLOB_MTK_GCM_TAG_SIZE);
	}
	else {
		// DECRYPT
		const u8 *publicIv;
		const u8 *gcmTag;
		u8 *padding;

		if (inLen < AES_BLOCK_SIZE + AES_BLOCK_SIZE + SONOS_BLOB_MTK_GCM_TAG_SIZE) {
			printk(KERN_CRIT "sonos_blob_encdec: dec inLen too short: %lu\n",
			       (unsigned long)inLen);
			goto end;
		}

		paddedLen =
			inLen - (AES_BLOCK_SIZE + SONOS_BLOB_MTK_GCM_TAG_SIZE);
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
		for (i = 0; i < AES_BLOCK_SIZE; i++) {
			iv[i] = publicIv[i] ^ keymod[i % sizeof(keymod)];
		}

		gcmTag = publicIv + AES_BLOCK_SIZE;
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
		memset(padded, 0, paddedLen);
		kfree(padded);
	}
	memset(iv, 0, sizeof(iv));
	memset(keymod, 0, sizeof(keymod));
	return result;
}
#endif
EXPORT_SYMBOL(sonos_blob_encdec);

/* Returns 0 on failure, 1 on success */
static int sonos_decrypt_fskey(const uint8_t* buf, size_t bufLen,
		               uint8_t* out, size_t *pOutLen,
			       const char* modifier)
{
	struct mdp_key_hdr hdr;

	/* consume the MDP key header */
	if (bufLen < sizeof(hdr)) {
		printk(KERN_CRIT "bad fskey input len %u\n", bufLen);
		return 0;
	}
	memcpy(&hdr, buf, sizeof(hdr));
	buf += sizeof(hdr);
	bufLen -= sizeof(hdr);
	if (hdr.m_len > bufLen) {
		printk(KERN_CRIT "bad fskey header len %u\n", bufLen);
		return 0;
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
			return 0;
		}
		memcpy(&hdr, buf, sizeof(hdr));
		buf += sizeof(hdr);
		bufLen -= sizeof(hdr);
		if (hdr.m_len > bufLen) {
			printk(KERN_CRIT "bad fskey header len %u\n", bufLen);
			return 0;
		}
	}
#endif

	if (hdr.m_magic != MDP_KEY_HDR_MAGIC_CAAM_AES128_RED_KEY) {
		printk(KERN_CRIT "no fskey red header\n");
		return 0;
	}

	if (sonos_blob_encdec(false, buf, hdr.m_len,
			      out, pOutLen,
			      modifier, strlen(modifier)) != 0) {
		printk(KERN_CRIT "fskey decrypt failed\n");
		return 0;
	}

	return 1;
}

// MA! We really dont want to return a string here, we want to return a binary
// key. The preferred API would be something like:
//   int sonos_get_rootfs_key(int type, uint8_t *key, size_t* keyLen)
//
// Instead, we will have to convert our key into a hexstring.
int sonos_get_rootfs_key(int type, char *keystring)
{
	int retval = 0;
	uint8_t key[16];
	size_t keyLen = sizeof(key);

	// Calling function must allocate at least 65 bytes for the key string and
	// the terminator (although we currently only ever write 33 bytes at
	// most).

	// always null-terminate the output buffer no matter what
	keystring[0] = '\0';

	if (type == SECT_UPGRADE_ROOTFS_FORMAT_PLAINTEXT) {
		// No key, because we're not encrypting
		retval = 1;
	}
	else if (type == SECT_UPGRADE_ROOTFS_FORMAT_FIXED_KEY) {
		strlcpy(keystring, "a0d92447ee704af85fa924b59137aec2", 33);
		retval = 1;
	}
	else if (type == SECT_UPGRADE_ROOTFS_FORMAT_RED_KEY) {
		// decrypt the key using a hw specific mechanism
		if (sys_mdp3.mdp3_version >= MDP3_VERSION_DEV_CERT &&
		    sonos_decrypt_fskey(sys_mdp3.mdp3_fskey2,
			    		sizeof(sys_mdp3.mdp3_fskey2),
					key, &keyLen, BLOB_KEYLABEL_ROOTFS) &&
		    keyLen == sizeof(key)) {
			int i;
			for (i = 0; i < sizeof(key); i++) {
				snprintf(&keystring[i * 2], 33 - (i * 2), "%02x", key[i]);
			}
			retval = 1;
		}
	}

	return retval;
}
EXPORT_SYMBOL(sonos_get_rootfs_key);

extern void ubifs_set_ubifs_key(const u8*);
int sonos_set_ubifs_key(u32 type)
{
	int retval = 0;
	uint8_t key[16];
	size_t keyLen = sizeof(key);

	if (type == UBIFS_CRYPT_TYPE_NONE) {
		// Don't set a key - the fs will be accessed without encryption/decryption
		retval = 1;
	}
	else if (type == UBIFS_CRYPT_TYPE_FIXED) {
		static const u8 key1[16] = { 0x55, 0x61, 0x4b, 0xde, 0x49, 0x5e, 0x0e, 0xd1, 0x50, 0x43, 0x77, 0x87, 0x94, 0x8e, 0x16, 0x3b };
		ubifs_set_ubifs_key(key1);
		retval = 1;
	}
	else if (type == UBIFS_CRYPT_TYPE_RED_KEY) {
		// decrypt the key using a hw specific mechanism
		if (sys_mdp3.mdp3_version >= MDP3_VERSION_DEV_CERT &&
		    sonos_decrypt_fskey(sys_mdp3.mdp3_fskey1,
			    		sizeof(sys_mdp3.mdp3_fskey1),
					key, &keyLen, BLOB_KEYLABEL_UBIFS) &&
		    keyLen == sizeof(key)) {
			ubifs_set_ubifs_key(key);
			retval = 1;
		}
	}

	return retval;
}
EXPORT_SYMBOL(sonos_set_ubifs_key);

/* API for root file system encryption */
struct mtd_info;
void get_rootfs_ubi_info(struct mtd_info * mtd, int *vol_id, int *ubi_num);
EXPORT_SYMBOL(get_rootfs_ubi_info);
struct ubi_volume_desc * rootfs_open(int vol_id, int ubi_num);
EXPORT_SYMBOL(rootfs_open);
int rootfs_update(struct ubi_volume_desc *desc, int64_t bytes);
EXPORT_SYMBOL(rootfs_update);
ssize_t rootfs_write(struct ubi_volume_desc *desc, const char *buf,
		size_t count, int flag);
EXPORT_SYMBOL(rootfs_write);
int rootfs_release(struct ubi_volume_desc *desc);
EXPORT_SYMBOL(rootfs_release);

// TODO If this code is used for i.MX6, it needs i.MX6 versions
// Current i.MX6 functions are in linux-3.10/drivers/char/fsl_otp.c
// and not ported to 4.4.24-mtk
#if defined(SONOS_ARCH_ATTR_SOC_IS_MT8521P)

#include <trustzone/tz_cross/ta_efuse.h>
#include <trustzone/tz_cross/efuse_info.h>
#define SONOS_FUSE_CUS_DAT_LEN  16
#define SONOS_FUSE_UNLOCK_OFFSET 8

int sonos_get_cpuid(uint8_t* buf, size_t buf_len)
{
	return buf_len == 8 &&
	       tee_fuse_read(HRID, buf, 8) == 0;
}

int sonos_get_unlock_counter(uint32_t *pValue)
{
	uint8_t fuseVal[SONOS_FUSE_CUS_DAT_LEN];

	if (tee_fuse_read(CUS_DAT, fuseVal, sizeof(fuseVal))) {
		return 0;
	}
	memcpy(pValue, fuseVal + SONOS_FUSE_UNLOCK_OFFSET, sizeof(*pValue));
	memset(fuseVal, 0, sizeof(fuseVal));
	return 1;
}
#endif
/* avoid SWPBL-181188 2022-03-08 */
