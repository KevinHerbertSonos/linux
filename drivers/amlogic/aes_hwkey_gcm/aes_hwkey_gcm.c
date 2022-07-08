/*
 * drivers/amlogic/aes_hwkey_gcm/aes_hwkey_gcm.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/amlogic/secmon.h>

//#define AHG_TEST

#ifdef AHG_TEST
#include <linux/slab.h>
#endif //AHG_TEST

#include "aes_hwkey_gcm.h"

/* Must be kept in sync with bl31: */

/* bl31 cmd id: */
#define AES_GCM_HUK     0x82000071
/* cmd sub-function id: */
#define AES_HWKEY_GCM_FID_INIT   0x120
#define AES_HWKEY_GCM_FID_ADD    0x121
#define AES_HWKEY_GCM_FID_FINAL  0x122

#define MAX_AES_GCM_CTX (8)
#define FLAG_DECRYPT (0<<0)
#define FLAG_ENCRYPT (1<<0)

#define MAX_AAD_LEN  (3072)
#define MAX_DATA_LEN (3072)

#ifndef HWKEY_IV_LEN
  #define HWKEY_IV_LEN  12
#endif
#ifndef HWKEY_TAG_LEN
  #define HWKEY_TAG_LEN 16
#endif

struct data_head {
	u32 cmd;
	u32 fid;
	u32 idx;
	u32 flag; // FLAG_ENCRYPT
};

struct init_data {
	struct data_head h;
	u8  iv[HWKEY_IV_LEN];
	u8  tag[HWKEY_TAG_LEN];
	u32 aad_len;
	u8	aad[MAX_AAD_LEN];
};

struct add_data {
	struct data_head h;
	u32 len;
	u8	data[MAX_DATA_LEN];
};

struct tag_data {
	struct data_head h;
	u32 len;
	u8	tag[HWKEY_TAG_LEN];
};
/* /Must be kept in sync with bl31 */


static void __iomem *sharemem_output;
static void __iomem *sharemem_input;
static noinline long fn_smc(u64 function_id, u64 arg0, u64 arg1,
					 u64 arg2)
{
	register long x0 asm("x0") = function_id;
	register long x1 asm("x1") = arg0;
	register long x2 asm("x2") = arg1;
	register long x3 asm("x3") = arg2;
	asm volatile(
			__asmeq("%0", "x0")
			__asmeq("%1", "x1")
			__asmeq("%2", "x2")
			__asmeq("%3", "x3")
			"smc	#0\n"
		: "+r" (x0)
		: "r" (x1), "r" (x2), "r" (x3));

	return x0;
}

#define TRACEPR() pr_err("%s L%d\n", __func__, __LINE__)
#define	PRDUMP(p, l)  print_hex_dump(KERN_ERR, "", DUMP_PREFIX_NONE, \
		16, 1, (p), (l), true)

/**
 * final argument specifies whether this is encrypt or decrypt
 * encrypt passes null for tag
 */
int aes_hwkey_gcm_init(struct aes_gcm_ctx *ctx,
		const u8 *iv,
		const u8 *aad, size_t aad_len,
		const u8 *tag,
		int do_encrypt)
{
	struct init_data *dp;
	long r;
	int idx;

	if (!ctx || !iv || (!do_encrypt && !tag) || !sharemem_input ||
			!sharemem_output) {
		return -EINVAL;
	}
	if (aad_len > MAX_AAD_LEN || (aad_len && !aad))
		return -EINVAL;

	memset(ctx, 0, sizeof(struct aes_gcm_ctx));
	dp = (struct init_data *)sharemem_input;

	sharemem_mutex_lock();
	memset(dp, 0, sizeof(struct init_data));
	dp->h.cmd = AES_GCM_HUK;
	dp->h.fid = AES_HWKEY_GCM_FID_INIT;
	dp->h.flag = do_encrypt ? FLAG_ENCRYPT : FLAG_DECRYPT;
	memcpy(&dp->iv[0], iv, HWKEY_IV_LEN);
	if (!do_encrypt && tag)
		memcpy(&dp->tag[0], tag, HWKEY_TAG_LEN);
	dp->aad_len = aad_len;
	if (aad)
		memcpy(&dp->aad[0], aad, aad_len);

	r = fn_smc(dp->h.cmd, 0, 0, 0);
	sharemem_mutex_unlock();

	idx = (int32_t)(r & 0xffffffff);
	if (idx < 0 || idx >= MAX_AES_GCM_CTX)
		return -ENOENT;
	ctx->init = 1;
	ctx->idx = idx;
	ctx->do_encrypt = do_encrypt;
	return 0;
}
EXPORT_SYMBOL(aes_hwkey_gcm_init);

/**
 * encrypts or decrypts (depending on init)
 * len must be block size multiple
 * we don't really want/need this to be multi-call interface
 * fails if this is decrypt and tag from init does not match
 */
int aes_hwkey_gcm_process(struct aes_gcm_ctx *ctx,
		const u8 *in, u8 *out, size_t len)
{
	struct add_data *dp;
	long r;

	if (!ctx || !ctx->init  || ctx->idx > MAX_AES_GCM_CTX || !in || !out ||
			!sharemem_input || !sharemem_output) {
		return -EINVAL;
	}
	if (len == 0 || (len % 16) || len > MAX_DATA_LEN)
		return -EINVAL;

	dp = (struct add_data *)sharemem_input;

	sharemem_mutex_lock();
	memset(dp, 0, sizeof(struct add_data));
	dp->h.cmd = AES_GCM_HUK;
	dp->h.fid = AES_HWKEY_GCM_FID_ADD;
	dp->h.idx = ctx->idx;
	dp->h.flag = ctx->do_encrypt ? FLAG_ENCRYPT : FLAG_DECRYPT;

	dp->len = len;
	memcpy(&dp->data[0], in, len);

	r = fn_smc(dp->h.cmd, 0, 0, 0);

	if (r == 0) {
		dp = (struct add_data *)sharemem_output;
		if (dp->h.cmd == AES_GCM_HUK &&
				dp->h.fid == AES_HWKEY_GCM_FID_ADD &&
				dp->h.idx == ctx->idx)
			memcpy(out, &dp->data[0], len);
	}
	sharemem_mutex_unlock();

	if (r)
		return -ENOENT;
	return 0;
}
EXPORT_SYMBOL(aes_hwkey_gcm_process);

/**
 * encrypt calls this one at the end to receive the generated tag
 */
int aes_hwkey_gcm_get_tag(struct aes_gcm_ctx *ctx, u8 *tag)
{
	struct tag_data *dp;
	long r;

	if (!ctx || !tag || !sharemem_input || !sharemem_output)
		return -EINVAL;

	dp = (struct tag_data *)sharemem_input;

	sharemem_mutex_lock();
	memset(dp, 0, sizeof(struct tag_data));
	dp->h.cmd = AES_GCM_HUK;
	dp->h.fid = AES_HWKEY_GCM_FID_FINAL;
	dp->h.idx = ctx->idx;
	dp->h.flag = ctx->do_encrypt ? FLAG_ENCRYPT : FLAG_DECRYPT;

	r = fn_smc(dp->h.cmd, 0, 0, 0);
	memcpy(dp, sharemem_output, sizeof(struct tag_data));
	sharemem_mutex_unlock();

	if (r || dp->h.cmd != AES_GCM_HUK ||
			dp->h.fid != AES_HWKEY_GCM_FID_FINAL ||
			dp->h.idx != ctx->idx)
		return -ENOENT;
	memcpy(tag, &dp->tag[0], HWKEY_TAG_LEN);
	return 0;
}
EXPORT_SYMBOL(aes_hwkey_gcm_get_tag);

#ifdef AHG_TEST
static void test_a(void)
{
	struct aes_gcm_ctx ctx;
	uint8_t iv[12] = { 0xca, 0xfe, 0xba, 0xbe, 0xfa, 0xce, 0xdb, 0xad,
		0xde, 0xca, 0xf8, 0x88,};
	uint8_t aad[16] = { 0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef,
		0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef };
	uint8_t tag[16] = { 0 };
	// depending on chip:
	uint8_t exptag[16] = { 0xb1, 0x36, 0xe8, 0xd5, 0xa9, 0x7e, 0x60, 0x28,
		0xbe, 0x20, 0x99, 0x58, 0x38, 0xe1, 0x5f, 0xcf };
	int r;
	uint8_t *pt = NULL, *ct = NULL;
	int i;
	size_t len = 2048;

	pt = kzalloc(len, GFP_KERNEL);
	ct = kzalloc(len, GFP_KERNEL);
	if (!pt || !ct)
		goto end;
	for (i = 0; i < len; i++)
		pt[i] = i % 256;

	/* encrypt */
	pr_err("encrypt........\n");
	pr_err("iv\n");
	PRDUMP(iv, sizeof(iv));
	pr_err("aad\n");
	PRDUMP(aad, sizeof(aad));
	pr_err("len=%zu\n", len);
	pr_err("plaintext\n");
	PRDUMP(pt, len);

	r = aes_hwkey_gcm_init(&ctx, iv, aad, sizeof(aad), NULL, 1);
	pr_err("init returned r=%d\n", r);
	if (r < 0)
		goto end;

	r = aes_hwkey_gcm_process(&ctx, pt, ct, len);
	pr_err("process returned r=%d\n", r);

	r = aes_hwkey_gcm_get_tag(&ctx, &tag[0]);
	pr_err("get tag returned r=%d\n", r);

	pr_err("ciphertext\n");
	PRDUMP(ct, len);
	pr_err("tag\n");
	PRDUMP(tag, 16);
	if (memcmp(tag, exptag, 16))
		pr_err("unexpected tag\n");


	/* decrypt */
	pr_err("decrypt........\n");
	r = aes_hwkey_gcm_init(&ctx, iv, aad, sizeof(aad), tag, 0);
	pr_err("init returned r=%d\n", r);
	if (r < 0)
		goto end;

	r = aes_hwkey_gcm_process(&ctx, ct, pt, len);
	pr_err("process returned r=%d\n", r);

	pr_err("ct\n");
	PRDUMP(ct, len);
	pr_err("pt\n");
	PRDUMP(pt, len);
end:
	kfree(pt);
	kfree(ct);
}
#endif //AHG_TEST


int __init aes_hwkey_gcm_device_init(void)
{
	sharemem_output = get_secmon_sharemem_output_base();
	sharemem_input = get_secmon_sharemem_input_base();
	if (!sharemem_output || !sharemem_input) {
		pr_err("sharemem fail\n");
		return -ENOMEM;
	}
#ifdef AHG_TEST
	test_a();
#endif //AHG_TEST
	return 0;
}
/* Called after secmon (module_init()) */
late_initcall(aes_hwkey_gcm_device_init);
