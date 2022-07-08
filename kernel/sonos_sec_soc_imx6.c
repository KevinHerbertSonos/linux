/*
 * Copyright (c) 2014-2019, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <linux/sonos_sec_general.h>
#include <linux/sonos_sec_blob_api.h>
#include "sonos_unlock.h"

/* in drivers/char/fsl_otp.c */
extern int fsl_otp_read_by_index(unsigned int index, u32 *pValue);

extern struct platform_device *sonos_sm_get_pdev(void);
extern int sonos_sm_init(struct platform_device *pdev);
extern void sonos_sm_exit(void);
extern int sonos_sm_encdec(struct platform_device *pdev, struct crypt_operation *op);

bool sonos_get_cpuid(uint8_t *buf, size_t buf_len)
{
	uint64_t cpu_id;
	uint32_t half;

	if (buf_len != sizeof(cpu_id)) {
		return false;
	}

	if (fsl_otp_read_by_index(1, &half)) {
		return false;
	}
	cpu_id = ((uint64_t)half) << 32;
	if (fsl_otp_read_by_index(2, &half)) {
		return false;
	}
	cpu_id |= half;

	cpu_id = cpu_to_be64(cpu_id);
	memcpy(buf, &cpu_id, sizeof cpu_id);
	return true;
}

bool sonos_get_unlock_counter(uint32_t *pValue)
{
	return fsl_otp_read_by_index(8*SONOS_FUSE_UNLOCK_CTR_BANK +
			             SONOS_FUSE_UNLOCK_CTR_WORD, pValue) == 0;
}

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
		      void *out, size_t *pOutLen,
		      const void *keymod, size_t keymodLen)
{
	int status;
	struct crypt_operation crypt_op;
	size_t outLen = *pOutLen;

	*pOutLen = 0;

	if (regions_overlap(in, inLen, out, outLen)) {
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
	crypt_op.output_length = &outLen;
	crypt_op.original_length = isEncrypt ? inLen : inLen - SONOS_BLOB_IMX6_RED_OVERHEAD;
	memcpy(crypt_op.keymod, keymod,
	       min(keymodLen, sizeof(crypt_op.keymod)));

	status = sonos_sm_encdec(sonos_sm_get_pdev(), &crypt_op);
	if (status) {
		printk(KERN_CRIT "sonos_blob_encdec: sm_encdec failed: %d\n",
		       status);
	}
	else {
		*pOutLen = outLen;
	}

	// Release the unit and keyslot...
	sonos_sm_exit();
	return status;
}
EXPORT_SYMBOL(sonos_blob_encdec);
