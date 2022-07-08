/*
 * Copyright (c) 2014-2019, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 *
 * The following file is used for prototypes and data structures for
 * sonos APIs embedded in the Linux kernel.
 *
 * See kernel/sonos.c.
 */

#ifndef SONOS_KERNEL_H
#define SONOS_KERNEL_H

#if defined(CONFIG_SONOS)

/* Board disambiguation - value from the dtb */
extern int sonos_product_id;

#if defined(SONOS_ARCH_ATTR_SOC_IS_MT8521P)
/* Match gpio values defined in patches/2016.11/to_add/board/sonos/mt8521p/board_parameter.h */
#define PRODUCT_ID_ELREY 3
#define PRODUCT_ID_HIDEOUT 1
#define PRODUCT_ID_DOMINO 2

/* NOTE:  usage of these defines should be wrapped in SOC_IS checks, as the product id is only
 * unique on a per-SoC basis.
 */
#define PRODUCT_ID_IS_ELREY ( (sonos_product_id == PRODUCT_ID_ELREY) )
#define PRODUCT_ID_IS_HIDEOUT ( (sonos_product_id == PRODUCT_ID_HIDEOUT) )
#define PRODUCT_ID_IS_DOMINO ( (sonos_product_id == PRODUCT_ID_DOMINO) )
#endif

#if defined(CONFIG_SONOS_SECBOOT)
#define UBIFS_CRYPT_TYPE_NONE		0
#define UBIFS_CRYPT_TYPE_FIXED		4
#define UBIFS_CRYPT_TYPE_BLACK_KEY	5
#define UBIFS_CRYPT_TYPE_RED_KEY	6

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
/* blackening a key can require an extra AES block */
#define SONOS_BLOB_MAX_ENC_OVERHEAD	(SONOS_BLOB_IMX6_RED_OVERHEAD + \
					 AES_BLOCK_SIZE)

#elif defined(SONOS_ARCH_ATTR_SOC_IS_MT8521P)
#define SONOS_BLOB_MTK_GCM_TAG_SIZE	16
/* PKCS#5 padding, IV, GCM tag */
#define SONOS_BLOB_MAX_ENC_OVERHEAD	(AES_BLOCK_SIZE + AES_BLOCK_SIZE + \
					 SONOS_BLOB_MTK_GCM_TAG_SIZE)
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

extern int sonos_set_ubifs_key(u32);
extern void sonos_set_proc_crypt(int);

// NOTE: 4.4.24-mtk kernel currently does not have this function for i.MX6
// In the 3.10 kernel, equivalent i.MX6 functions are in drivers/char/fsl_otp.c
extern int sonos_get_cpuid(uint8_t *, size_t);
extern int sonos_get_unlock_counter(uint32_t *);
#endif

#ifdef CONFIG_VF610_ADC
// see drivers/iio/adc/vf610_adc.c
extern int vf610_read_adc(int chan, int *mvolts);
#elif defined(CONFIG_MEDIATEK_MT6577_AUXADC)
// see drivers/iio/adc/mt6577_auxadc.c
extern int mt6577_read_adc(int chan, int *mvolts);
#endif

// returns completion status, passes back milli-volts
extern int read_adc_voltage(int chan, int *mvolts);

// disable/enable NAND access during shutdown
// see drivers/mtd/nand/nand_base.c
extern int nand_shutdown_access(int);

#include "mdp.h"
extern int bootgeneration;
extern int bootsection;
extern struct manufacturing_data_page sys_mdp;
#endif // CONFIG_SONOS
#endif // SONOS_KERNEL_H
