/*
 * Copyright (c) 2015-2021, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 *
 * Sonos Signature well known keys: mt8518s
 */

#if defined(SONOS_ARCH_ATTR_STUB_SECBOOT_ARCH_KEYS)
#  error "stub keys should not be building this file"
#endif

#if !defined(SONOS_ARCH_ATTR_SOC_IS_MT8518S)
#  error "wrong platform"
#endif

#include <linux/kernel.h>
#include <crypto/sonos_signature_keys.h>
#include <linux/sonos_kernel.h>

extern SonosRsaKeyEntry SRKE_unlock_bravo;
extern SonosRsaKeyEntry SRKE_unitCA_bravo;

/* point the various pointers at the corresponding array fields */
void
sonosInitKeyTable(void)
{
	g_SonosSigningKeys[0] = &SRKE_unlock_bravo;
	g_SonosSigningKeys[1] = &SRKE_unitCA_bravo;
}
