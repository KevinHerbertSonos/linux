/*
 * Copyright (c) 2014-2020, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#ifndef SONOS_SEC_BLOB_API_H
#define SONOS_SEC_BLOB_API_H

#define SECMEM_KEYMOD_LEN 8

#include <crypto/aes.h>
#include <linux/types.h>

#if defined(SONOS_ARCH_ATTR_SOC_IS_IMX6)
struct crypt_operation {
	bool	is_encrypt;
	void	*input_buffer;
	size_t	input_length;
	void	*output_buffer;
	size_t	*output_length;
	size_t	original_length;
	char	keymod[SECMEM_KEYMOD_LEN];
};

struct platform_device;
extern int sonos_sm_encdec(struct platform_device *pdev,
			   struct crypt_operation *op);

#define SONOS_BLOB_IMX6_RED_OVERHEAD	48

/* blackening a key can require an extra AES block, only i.MX6 */
#define SONOS_BLOB_MAX_ENC_OVERHEAD	(SONOS_BLOB_IMX6_RED_OVERHEAD + \
					 AES_BLOCK_SIZE)

#else
    /* use 12 unless a particular hardware platform requires another value */
    #if defined(SONOS_ARCH_ATTR_SOC_IS_MT8518S) || \
        defined(SONOS_ARCH_ATTR_SOC_IS_MT8521P)
        /* MTK TEE only supports 16 byte IV */
        #define SONOS_BLOB_GCM_IV_SIZE  16
    #elif defined(SONOS_ARCH_ATTR_SOC_IS_A113)
        #define SONOS_BLOB_GCM_IV_SIZE  12
    #else
        #error "GCM IV size unknown on unsupported platform"
    #endif
    #define SONOS_BLOB_GCM_TAG_SIZE	16

    /* PKCS#5 padding, IV, GCM tag */
    #define SONOS_BLOB_MAX_ENC_OVERHEAD	(AES_BLOCK_SIZE + SONOS_BLOB_GCM_IV_SIZE + \
					 SONOS_BLOB_GCM_TAG_SIZE)
#endif

#define SONOS_BLOB_BUFFER_SIZE		2048
#define SONOS_BLOB_MAX_ENC_INPUT_SIZE	(SONOS_BLOB_BUFFER_SIZE - \
					 SONOS_BLOB_MAX_ENC_OVERHEAD)

/*
 * returns 0 on success, non-zero on error
 * '*pOutLen' is a value/result argument (it is read here and then written)
 * 'in' and 'out' are not allowed to overlap
 */
extern int sonos_blob_encdec(bool isEncrypt, const void *in, size_t inLen,
			     void *out, size_t *pOutLen,
			     const void *keymod, size_t keymodLen);

#endif
