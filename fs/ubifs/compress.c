/*
 * This file is part of UBIFS.
 *
 * Copyright (C) 2006-2008 Nokia Corporation.
 * Copyright (C) 2006, 2007 University of Szeged, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Authors: Adrian Hunter
 *          Artem Bityutskiy (Битюцкий Артём)
 *          Zoltan Sogor
 */

/*
 * This file provides a single place to access to compression and
 * decompression.
 */

#include <linux/crypto.h>
#ifdef CONFIG_SONOS_SECBOOT
#include <linux/scatterlist.h>
#include <linux/sonos_sec_fs_keys.h>
#endif
#include "ubifs.h"

/* Fake description object for the "none" compressor */
static struct ubifs_compressor none_compr = {
	.compr_type = UBIFS_COMPR_NONE,
	.name = "none",
	.capi_name = "",
};

#ifdef CONFIG_UBIFS_FS_LZO
static DEFINE_MUTEX(lzo_mutex);

static struct ubifs_compressor lzo_compr = {
	.compr_type = UBIFS_COMPR_LZO,
	.comp_mutex = &lzo_mutex,
	.name = "lzo",
	.capi_name = "lzo",
};
#else
static struct ubifs_compressor lzo_compr = {
	.compr_type = UBIFS_COMPR_LZO,
	.name = "lzo",
};
#endif

#ifdef CONFIG_UBIFS_FS_ZLIB
static DEFINE_MUTEX(deflate_mutex);
static DEFINE_MUTEX(inflate_mutex);

static struct ubifs_compressor zlib_compr = {
	.compr_type = UBIFS_COMPR_ZLIB,
	.comp_mutex = &deflate_mutex,
	.decomp_mutex = &inflate_mutex,
	.name = "zlib",
	.capi_name = "deflate",
};
#else
static struct ubifs_compressor zlib_compr = {
	.compr_type = UBIFS_COMPR_ZLIB,
	.name = "zlib",
};
#endif

/* All UBIFS compressors */
struct ubifs_compressor *ubifs_compressors[UBIFS_COMPR_TYPES_CNT];

#ifdef CONFIG_SONOS_SECBOOT
__u8	sonos_ubifs_crypto_key[UBIFS_CRYPTO_KEYSIZE];
bool	use_ubifs_crypto_key;

/**
 * aes_crypt - encrypt / decrypt data.

 * @c: ubifs_info ptr, for ubifs_err
 * @str: the data to crypt
 * @len: length of the data
 * @crypto_key: the cryptographic key to use to crypt the data
 *
 * This function applies AES encryption to the data. It is done in counter
 * mode, which means that encryption and decryption are the same operation,
 * i.e., it XORs the same generated bitstream, so it can be used both for
 * encryption / decryption. Returns zero in case of success and a negative
 * error code in case of failure.
 *
 * WARNING: The operation is done in-place, so @str mutates!
 */
static int aes_crypt(const struct ubifs_info *c, const void *str, int len, const void *crypto_key)
{
	struct crypto_blkcipher *tfm;
	struct blkcipher_desc desc;
	struct scatterlist sg;
	uint8_t iv[UBIFS_CRYPTO_KEYSIZE];
	int err;

	tfm = crypto_alloc_blkcipher(UBIFS_CRYPTO_ALGORITHM, 0, 0);
	if (IS_ERR(tfm)) {
		err = PTR_ERR(tfm);
		ubifs_err(c, "failed to load transform for aes, error %d", err);
		return err;
	}

	err = crypto_blkcipher_setkey(tfm, crypto_key, UBIFS_CRYPTO_KEYSIZE);
	if (err) {
		ubifs_err(c, "cannot set the AES key, flags %#x, error %d",
			  crypto_blkcipher_get_flags(tfm), err);
		return err;
	}

	sg_init_one(&sg, str, len);
	memset(iv, 0, UBIFS_CRYPTO_KEYSIZE);
	desc.info = iv;
	desc.tfm = tfm;
	desc.flags = 0;
	err = crypto_blkcipher_encrypt(&desc, &sg, &sg, len);
	crypto_free_blkcipher(tfm);
	return err;
}
#endif

/**
 * ubifs_compress - compress data.
 * @in_buf: data to compress
 * @in_len: length of the data to compress
 * @out_buf: output buffer where compressed data should be stored
 * @out_len: output buffer length is returned here
 * @compr_type: type of compression to use on enter, actually used compression
 *              type on exit
 * @crypto_key: the encryption key or NULL if no encryption needed ##Note: only with CONFIG_SONOS_SECBOOT
 *
 * This function compresses input buffer @in_buf of length @in_len and stores
 * the result in the output buffer @out_buf and the resulting length in
 * @out_len. If the input buffer does not compress, it is just copied to the
 * @out_buf. The same happens if @compr_type is %UBIFS_COMPR_NONE or if
 * compression error occurred.
 *
 * Note, if the input buffer was not compressed, it is copied to the output
 * buffer and %UBIFS_COMPR_NONE is returned in @compr_type.
 */
#if defined CONFIG_SONOS_SECBOOT
void ubifs_compress(const struct ubifs_info *c, const void *in_buf,
		    int in_len, void *out_buf, int *out_len, int *compr_type, const void *crypto_key)
#else
void ubifs_compress(const struct ubifs_info *c, const void *in_buf,
		    int in_len, void *out_buf, int *out_len, int *compr_type)
#endif
{
	int err;
	struct ubifs_compressor *compr = ubifs_compressors[*compr_type];

	if (*compr_type == UBIFS_COMPR_NONE)
		goto no_compr;

	/* If the input data is small, do not even try to compress it */
	if (in_len < UBIFS_MIN_COMPR_LEN)
		goto no_compr;

	if (compr->comp_mutex)
		mutex_lock(compr->comp_mutex);
	err = crypto_comp_compress(compr->cc, in_buf, in_len, out_buf,
				   (unsigned int *)out_len);
	if (compr->comp_mutex)
		mutex_unlock(compr->comp_mutex);
	if (unlikely(err)) {
		ubifs_warn(c, "cannot compress %d bytes, compressor %s, error %d, leave data uncompressed",
			   in_len, compr->name, err);
		goto no_compr;
	}

	/*
	 * If the data compressed only slightly, it is better to leave it
	 * uncompressed to improve read speed.
	 */
	if (in_len - *out_len < UBIFS_MIN_COMPRESS_DIFF)
		goto no_compr;

#if defined CONFIG_SONOS_SECBOOT
	if (crypto_key) {
		aes_crypt(c, out_buf, *out_len, crypto_key);
	}
#endif
	return;

no_compr:
	memcpy(out_buf, in_buf, in_len);
	*out_len = in_len;
	*compr_type = UBIFS_COMPR_NONE;
#if defined CONFIG_SONOS_SECBOOT
	if (crypto_key) {
		aes_crypt(c, out_buf, *out_len, crypto_key);
	}
#endif
}

/**
 * ubifs_decompress - decompress data.
 * @in_buf: data to decompress
 * @in_len: length of the data to decompress
 * @out_buf: output buffer where decompressed data should
 * @out_len: output length is returned here
 * @compr_type: type of compression
 * @crypto_key: the encryption key or %NULL if no encryption needed  #Note: Only with CONFIG_SONOS_SECBOOT
 *
 * This function decompresses data from buffer @in_buf into buffer @out_buf.
 * The length of the uncompressed data is returned in @out_len. This functions
 * returns %0 on success or a negative error code on failure.
 */
#if defined CONFIG_SONOS_SECBOOT
int ubifs_decompress(const struct ubifs_info *c, const void *in_buf,
		     int in_len, void *out_buf, int *out_len, int compr_type, const void *crypto_key)
#else
int ubifs_decompress(const struct ubifs_info *c, const void *in_buf,
		     int in_len, void *out_buf, int *out_len, int compr_type)
#endif
{
	int err;
	struct ubifs_compressor *compr;

	if (unlikely(compr_type < 0 || compr_type >= UBIFS_COMPR_TYPES_CNT)) {
		ubifs_err(c, "invalid compression type %d", compr_type);
		return -EINVAL;
	}

	compr = ubifs_compressors[compr_type];

	if (unlikely(!compr->capi_name)) {
		ubifs_err(c, "%s compression is not compiled in", compr->name);
		return -EINVAL;
	}
#if defined CONFIG_SONOS_SECBOOT
	if (crypto_key) {
		aes_crypt(c, in_buf, in_len, crypto_key);
	}
#endif
	if (compr_type == UBIFS_COMPR_NONE) {
		memcpy(out_buf, in_buf, in_len);
		*out_len = in_len;
		return 0;
	}

	if (compr->decomp_mutex)
		mutex_lock(compr->decomp_mutex);
	err = crypto_comp_decompress(compr->cc, in_buf, in_len, out_buf,
				     (unsigned int *)out_len);
	if (compr->decomp_mutex)
		mutex_unlock(compr->decomp_mutex);
	if (err)
		ubifs_err(c, "cannot decompress %d bytes, compressor %s, error %d",
			  in_len, compr->name, err);

	return err;
}

/**
 * compr_init - initialize a compressor.
 * @compr: compressor description object
 *
 * This function initializes the requested compressor and returns zero in case
 * of success or a negative error code in case of failure.
 */
static int __init compr_init(struct ubifs_compressor *compr)
{
	if (compr->capi_name) {
		compr->cc = crypto_alloc_comp(compr->capi_name, 0, 0);
		if (IS_ERR(compr->cc)) {
			pr_err("UBIFS error (pid %d): cannot initialize compressor %s, error %ld",
			       current->pid, compr->name, PTR_ERR(compr->cc));
			return PTR_ERR(compr->cc);
		}
	}

	ubifs_compressors[compr->compr_type] = compr;
	return 0;
}

/**
 * compr_exit - de-initialize a compressor.
 * @compr: compressor description object
 */
static void compr_exit(struct ubifs_compressor *compr)
{
	if (compr->capi_name)
		crypto_free_comp(compr->cc);
	return;
}

/**
 * ubifs_compressors_init - initialize UBIFS compressors.
 *
 * This function initializes the compressor which were compiled in. Returns
 * zero in case of success and a negative error code in case of failure.
 */
int __init ubifs_compressors_init(void)
{
	int err;

	err = compr_init(&lzo_compr);
	if (err)
		return err;

	err = compr_init(&zlib_compr);
	if (err)
		goto out_lzo;

	ubifs_compressors[UBIFS_COMPR_NONE] = &none_compr;
	return 0;

out_lzo:
	compr_exit(&lzo_compr);
	return err;
}

/**
 * ubifs_compressors_exit - de-initialize UBIFS compressors.
 */
void ubifs_compressors_exit(void)
{
	compr_exit(&lzo_compr);
	compr_exit(&zlib_compr);
}

#if defined CONFIG_SONOS_SECBOOT && defined CONFIG_UBIFS_FS
/**
 * ubifs_set_ubifs_key - initialize the jffs cryto key
 */

void	ubifs_set_ubifs_key(const __u8* key)
{
#if 0
	int i;
#endif
	memcpy( sonos_ubifs_crypto_key, key, sizeof(sonos_ubifs_crypto_key) );
	use_ubifs_crypto_key = 1;

#if 0
	printk(KERN_INFO "crypto_key for jffs - \n");
	for ( i = 0;i < UBIFS_CRYPTO_KEYSIZE; i++) {
		printk(KERN_INFO "key[%d] = x%2x\n",i,key[i]);
	}
#endif

}
#endif
