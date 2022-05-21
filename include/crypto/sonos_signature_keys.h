/*
 * Copyright (c) 2015-2018, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 *
 * Sonos Signature format support
 */

#ifndef SONOS_SIGNATURE_KEYS_H
#define SONOS_SIGNATURE_KEYS_H

#include <linux/types.h>

/*
 * We currently ignore the incoming key identifier entirely as in any given
 * context we always know which key we expect and the verify caller identifies
 * the expected key using a string (the callback argument).
 */
typedef struct {
	const uint8_t *der;
	size_t derLen;
	const char *name;
} SonosRsaKeyEntry;

/* a null-terminated list of signing keys */
extern const SonosRsaKeyEntry * g_SonosSigningKeys[];

/* initialize the table with keys for the given product id */
extern void sonosInitKeyTable(void);

#endif
