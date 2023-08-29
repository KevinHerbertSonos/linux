/*
 * Copyright (c) 2015-2023, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 *
 * Sonos Signature well known keys: s767
 */

#if defined(SONOS_ARCH_ATTR_STUB_SECBOOT_ARCH_KEYS)
#  error "stub keys should not be building this file"
#endif

#if !defined(SONOS_ARCH_ATTR_SOC_IS_S767)
#  error "wrong platform"
#endif

#include <linux/kernel.h>
#include <crypto/sonos_signature_keys.h>
#include <linux/sonos_kernel.h>

extern SonosRsaKeyEntry SRKE_unlock_jaws;
extern SonosRsaKeyEntry SRKE_unitCA_jaws;

extern SonosRsaKeyEntry SRKE_unlock_lasso;
extern SonosRsaKeyEntry SRKE_unitCA_lasso;

extern SonosRsaKeyEntry SRKE_unlock_optimo;
extern SonosRsaKeyEntry SRKE_unitCA_optimo;

extern SonosRsaKeyEntry SRKE_unlock_pallas;
extern SonosRsaKeyEntry SRKE_unitCA_pallas;

extern SonosRsaKeyEntry SRKE_unlock_prima;
extern SonosRsaKeyEntry SRKE_unitCA_prima;

/* point the various pointers at the corresponding array fields */
void
sonosInitKeyTable(void)
{
	// For products that share a kernel we poke in model-specific keys at
	// run-time but preserve the constness of this array to external callers.
	if (sonos_product_id == PRODUCT_ID_OPTIMO1 ||
	    sonos_product_id == PRODUCT_ID_OPTIMO1SL || 
	    sonos_product_id == PRODUCT_ID_OPTIMO2 ||
            sonos_product_id == PRODUCT_ID_RAVEN) {
		g_SonosSigningKeys[0] = &SRKE_unlock_optimo;
		g_SonosSigningKeys[1] = &SRKE_unitCA_optimo;
	}
// TODO FIXME remove this if 0 in the arch feature branch,
// when the arch is defined without STUB_SECBOOT_ARCH_KEYS
#if 0
	else if (sonos_product_id == PRODUCT_ID_JAWS ||
                 sonos_product_id == PRODUCT_ID_LOTUS) {
		g_SonosSigningKeys[0] = &SRKE_unlock_jaws;
		g_SonosSigningKeys[1] = &SRKE_unitCA_jaws;
	}
#endif
	else if (sonos_product_id == PRODUCT_ID_LASSO) {
		g_SonosSigningKeys[0] = &SRKE_unlock_lasso;
		g_SonosSigningKeys[1] = &SRKE_unitCA_lasso;
	}
	else if (sonos_product_id == PRODUCT_ID_PALLAS_PLUS) {
		g_SonosSigningKeys[0] = &SRKE_unlock_pallas;
		g_SonosSigningKeys[1] = &SRKE_unitCA_pallas;
	}
	else if (sonos_product_id == PRODUCT_ID_PRIMA) {
		g_SonosSigningKeys[0] = &SRKE_unlock_prima;
		g_SonosSigningKeys[1] = &SRKE_unitCA_prima;
	}
	if (g_SonosSigningKeys[0] == NULL) {
		printk(KERN_CRIT "sonosInitKeyTable: failed - bad product_id (%d)\n",
				 sonos_product_id);
		return;
	}
}
