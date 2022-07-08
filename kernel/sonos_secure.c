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
#include "sect_upgrade_header.h"

extern struct manufacturing_data_page sys_mdp;
extern struct manufacturing_data_page3 sys_mdp3;

extern int get_imx6_cpuid(uint8_t *, size_t);
extern int get_imx6_unlock_counter(uint32_t *);

/* macros needed by the portable implementation of unlock/authz checking */
#define SU_BE32_TO_CPU		be32_to_cpu
#define SU_CPU_TO_BE32		cpu_to_be32
#define SU_GET_CPUID		get_imx6_cpuid
#define SU_GET_UNLOCK_COUNTER	get_imx6_unlock_counter
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
