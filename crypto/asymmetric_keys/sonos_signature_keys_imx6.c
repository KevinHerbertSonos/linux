/*
 * Copyright (c) 2015-2020, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 *
 * Sonos Signature well known keys: imx6
 */

#if defined(SONOS_ARCH_ATTR_STUB_SECBOOT_ARCH_KEYS)
#  error "stub keys should not be building this file"
#endif

#if !defined(SONOS_ARCH_ATTR_SOC_IS_IMX6)
#  error "wrong platform"
#endif

#include <linux/kernel.h>
#include <crypto/sonos_signature_keys.h>
#include <linux/sonos_kernel.h>

extern SonosRsaKeyEntry SRKE_unlock_bootleg;
extern SonosRsaKeyEntry SRKE_unitCA_bootleg;

extern SonosRsaKeyEntry SRKE_unlock_chaplin;
extern SonosRsaKeyEntry SRKE_unitCA_chaplin;

extern SonosRsaKeyEntry SRKE_unlock_encore;
extern SonosRsaKeyEntry SRKE_unitCA_encore;

extern SonosRsaKeyEntry SRKE_unlock_gravity;
extern SonosRsaKeyEntry SRKE_unitCA_gravity;

extern SonosRsaKeyEntry SRKE_unlock_neptune;
extern SonosRsaKeyEntry SRKE_unitCA_neptune;

extern SonosRsaKeyEntry SRKE_unlock_paramount;
extern SonosRsaKeyEntry SRKE_unitCA_paramount;

extern SonosRsaKeyEntry SRKE_unlock_royale;
extern SonosRsaKeyEntry SRKE_unitCA_royale;

extern SonosRsaKeyEntry SRKE_unlock_solbase;
extern SonosRsaKeyEntry SRKE_unitCA_solbase;

extern SonosRsaKeyEntry SRKE_unlock_titan;
extern SonosRsaKeyEntry SRKE_unitCA_titan;

extern SonosRsaKeyEntry SRKE_unlock_vertigo;
extern SonosRsaKeyEntry SRKE_unitCA_vertigo;

/* point the various pointers at the corresponding array fields */
void
sonosInitKeyTable(void)
{
	// For products that share a kernel we poke in model-specific keys at
	// run-time but preserve the constness of this array to external callers.
	if (sonos_product_id == PRODUCT_ID_BOOTLEG) {
		g_SonosSigningKeys[0] = &SRKE_unlock_bootleg;
		g_SonosSigningKeys[1] = &SRKE_unitCA_bootleg;
	}
	else if (sonos_product_id == PRODUCT_ID_CHAPLIN) {
		g_SonosSigningKeys[0] = &SRKE_unlock_chaplin;
		g_SonosSigningKeys[1] = &SRKE_unitCA_chaplin;
	}
	else if (sonos_product_id == PRODUCT_ID_ENCORE) {
		g_SonosSigningKeys[0] = &SRKE_unlock_encore;
		g_SonosSigningKeys[1] = &SRKE_unitCA_encore;
	}
	else if (sonos_product_id == PRODUCT_ID_GRAVITY) {
		g_SonosSigningKeys[0] = &SRKE_unlock_gravity;
		g_SonosSigningKeys[1] = &SRKE_unitCA_gravity;
	}
	else if (sonos_product_id == PRODUCT_ID_NEPTUNE) {
		g_SonosSigningKeys[0] = &SRKE_unlock_neptune;
		g_SonosSigningKeys[1] = &SRKE_unitCA_neptune;
	}
	else if (sonos_product_id == PRODUCT_ID_PARAMOUNT) {
		g_SonosSigningKeys[0] = &SRKE_unlock_paramount;
		g_SonosSigningKeys[1] = &SRKE_unitCA_paramount;
	}
	else if (sonos_product_id == PRODUCT_ID_ROYALE) {
		g_SonosSigningKeys[0] = &SRKE_unlock_royale;
		g_SonosSigningKeys[1] = &SRKE_unitCA_royale;
	}
	else if (sonos_product_id == PRODUCT_ID_SOLBASE) {
		g_SonosSigningKeys[0] = &SRKE_unlock_solbase;
		g_SonosSigningKeys[1] = &SRKE_unitCA_solbase;
	}
	else if (sonos_product_id == PRODUCT_ID_TITAN) {
		g_SonosSigningKeys[0] = &SRKE_unlock_titan;
		g_SonosSigningKeys[1] = &SRKE_unitCA_titan;
	}
	else if (sonos_product_id == PRODUCT_ID_VERTIGO) {
		g_SonosSigningKeys[0] = &SRKE_unlock_vertigo;
		g_SonosSigningKeys[1] = &SRKE_unitCA_vertigo;
	}

	if (g_SonosSigningKeys[0] == NULL) {
		printk(KERN_CRIT "sonosInitKeyTable: failed - bad product_id (%d)\n",
				 sonos_product_id);
		return;
	}
}
