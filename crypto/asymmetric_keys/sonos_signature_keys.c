/*
 * Copyright (c) 2015-2019, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 *
 * Sonos Signature well-known public keys
 */

#include <crypto/sonos_signature_keys.h>

/* a null-terminated list of signing keys known to the kernel */
const SonosRsaKeyEntry * g_SonosSigningKeys[] =
{
	NULL, // &SRKE_unlock_xxx,
	NULL, // &SRKE_unitCA_xxx,
	NULL
};

