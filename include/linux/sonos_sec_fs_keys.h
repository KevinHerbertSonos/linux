/*
 * Copyright (c) 2018-2019, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#ifndef SONOS_SEC_FS_KEYS_H
#define SONOS_SEC_FS_KEYS_H

#include <linux/types.h>

#if defined(CONFIG_DM_CRYPT)
/*
 * If hexKey is a sentinel key then use the TLS 1.2 PRF (w/ SHA256) to combine
 * the appropriate MDP key (the master secret) and the sentinel key (the nonce)
 * to make a new key of the same length.
 *
 * Returns true on success (not a sentinel or is a sentinel with successful
 * transformation) and false on failure (is a sentinel but transformation
 * failed).
 */
extern bool sonos_replace_luks_key_if_sentinel(char *hexKey);
#endif

#if defined(CONFIG_UBIFS_FS)
#define UBIFS_CRYPT_TYPE_NONE		0
#define UBIFS_CRYPT_TYPE_FIXED		4
#define UBIFS_CRYPT_TYPE_RED_KEY	6

/* use 128-bit AES in counter mode for UBIFS */
#define UBIFS_CRYPTO_KEYSIZE		16
#define UBIFS_CRYPTO_ALGORITHM		"ctr(aes)"

extern void sonos_set_proc_crypt(int);
extern void ubifs_set_ubifs_key(const u8 *);
extern bool sonos_set_ubifs_key(u32 type);
#endif

#endif
