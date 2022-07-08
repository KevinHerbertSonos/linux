/*
 * Copyright (c) 2014-2017, Sonos, Inc.
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

#endif

#if defined(SONOS_ARCH_ENCORE)

extern void *sonos_orientation_register_callback(void (*function)(int orient, void *param), void *param);
extern int sonos_orientation_unregister_callback(void *entry);
extern void sonos_orientation_change_event(int orient);

#endif // SONOS_ARCH_ENCORE

// returns completion status, passes back milli-volts
// see drivers/iio/adc/vf610_adc.c
extern int vf610_read_adc(int chan, int *mvolts);
extern int read_adc_voltage(int chan, int *mvolts);

// disable/enable NAND access during shutdown
// see drivers/mtd/nand/nand_base.c
extern int nand_shutdown_access(int);

#if defined(CONFIG_IMX_SDMA)
/*
 * The following are special Sonos functions retrofitted into the
 * imx-sdma driver.
 */

#include <linux/dmaengine.h>
#include <linux/types.h>

extern dma_addr_t sdma_sonos_swap_data_pointer(struct dma_chan *chan,
		u32 index, dma_addr_t data_phys);

#endif // CONFIG_IMX_SDMA

#endif // CONFIG_SONOS
#endif // SONOS_KERNEL_H
