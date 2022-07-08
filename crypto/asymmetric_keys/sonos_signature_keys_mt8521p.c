/*
 * Copyright (c) 2015-2021, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 *
 * Sonos Signature well known keys: mt8521p
 */

#if defined(SONOS_ARCH_ATTR_STUB_SECBOOT_ARCH_KEYS)
#  error "stub keys should not be building this file"
#endif

#if !defined(SONOS_ARCH_ATTR_SOC_IS_MT8521P)
#  error "wrong platform"
#endif

#include <linux/kernel.h>
#include <crypto/sonos_signature_keys.h>
#include <linux/sonos_kernel.h>

extern SonosRsaKeyEntry SRKE_unlock_domino;
extern SonosRsaKeyEntry SRKE_unitCA_domino;

extern SonosRsaKeyEntry SRKE_unlock_elrey;
extern SonosRsaKeyEntry SRKE_unitCA_elrey;

extern SonosRsaKeyEntry SRKE_unlock_hideout;
extern SonosRsaKeyEntry SRKE_unitCA_hideout;

/* point the various pointers at the corresponding array fields */
void
sonosInitKeyTable(void)
{
	// For products that share a kernel we poke in model-specific keys at
	// run-time but preserve the constness of this array to external callers.
	if (sonos_product_id == PRODUCT_ID_DOMINO) {
		g_SonosSigningKeys[0] = &SRKE_unlock_domino;
		g_SonosSigningKeys[1] = &SRKE_unitCA_domino;
	}
	else if (sonos_product_id == PRODUCT_ID_ELREY) {
		g_SonosSigningKeys[0] = &SRKE_unlock_elrey;
		g_SonosSigningKeys[1] = &SRKE_unitCA_elrey;
	}
	else if (sonos_product_id == PRODUCT_ID_HIDEOUT) {
		g_SonosSigningKeys[0] = &SRKE_unlock_hideout;
		g_SonosSigningKeys[1] = &SRKE_unitCA_hideout;
	}

	if (g_SonosSigningKeys[0] == NULL) {
		printk(KERN_CRIT "sonosInitKeyTable: failed - bad product_id (%d)\n",
				 sonos_product_id);
		return;
	}
}
