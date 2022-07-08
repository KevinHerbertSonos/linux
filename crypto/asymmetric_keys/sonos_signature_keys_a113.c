/*
 * Copyright (c) 2015-2020, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 *
 * Sonos Signature well known keys: a113
 */

#if defined(SONOS_ARCH_ATTR_STUB_SECBOOT_ARCH_KEYS)
#  error "stub keys should not be building this file"
#endif

#if !defined(SONOS_ARCH_ATTR_SOC_IS_A113)
#  error "wrong platform"
#endif

#include <linux/kernel.h>
#include <crypto/sonos_signature_keys.h>
#include <linux/sonos_kernel.h>

extern SonosRsaKeyEntry SRKE_unlock_apollo;
extern SonosRsaKeyEntry SRKE_unitCA_apollo;

extern SonosRsaKeyEntry SRKE_unlock_dhuez;
extern SonosRsaKeyEntry SRKE_unitCA_dhuez;

extern SonosRsaKeyEntry SRKE_unlock_fury;
extern SonosRsaKeyEntry SRKE_unitCA_fury;

extern SonosRsaKeyEntry SRKE_unlock_monaco;
extern SonosRsaKeyEntry SRKE_unitCA_monaco;

extern SonosRsaKeyEntry SRKE_unlock_tupelo;
extern SonosRsaKeyEntry SRKE_unitCA_tupelo;

/* point the various pointers at the corresponding array fields */
void
sonosInitKeyTable(void)
{
	// For products that share a kernel we poke in model-specific keys at
	// run-time but preserve the constness of this array to external callers.
	if (sonos_product_id == PRODUCT_ID_APOLLO || sonos_product_id == PRODUCT_ID_APOLLOX) {
		g_SonosSigningKeys[0] = &SRKE_unlock_apollo;
		g_SonosSigningKeys[1] = &SRKE_unitCA_apollo;
	}
	else if (sonos_product_id == PRODUCT_ID_DHUEZ) {
		g_SonosSigningKeys[0] = &SRKE_unlock_dhuez;
		g_SonosSigningKeys[1] = &SRKE_unitCA_dhuez;
	}
	else if (sonos_product_id == PRODUCT_ID_FURY) {
		g_SonosSigningKeys[0] = &SRKE_unlock_fury;
		g_SonosSigningKeys[1] = &SRKE_unitCA_fury;
	}
	else if (sonos_product_id == PRODUCT_ID_MONACO || sonos_product_id == PRODUCT_ID_MONACOSL) {
		g_SonosSigningKeys[0] = &SRKE_unlock_monaco;
		g_SonosSigningKeys[1] = &SRKE_unitCA_monaco;
	}
	else if (sonos_product_id == PRODUCT_ID_TUPELO || sonos_product_id == PRODUCT_ID_GOLDENEYE) {
		g_SonosSigningKeys[0] = &SRKE_unlock_tupelo;
		g_SonosSigningKeys[1] = &SRKE_unitCA_tupelo;
	}

	if (g_SonosSigningKeys[0] == NULL) {
		printk(KERN_CRIT "sonosInitKeyTable: failed - bad product_id (%d)\n",
				 sonos_product_id);
		return;
	}
}
