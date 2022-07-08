/*
 * Copyright (c) 2015-2020, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 *
 * Sonos Signature format support
 */

/* fixidn TODO: restructure per-SoC key files like U-Boot */
#if !defined(SONOS_ARCH_ATTR_SOC_IS_MT8521P)
#  error "wrong platform"
#endif

#include <linux/err.h>
#include <linux/key.h>
#include <keys/asymmetric-type.h>
#include <crypto/sonos_signature_keys.h>
#include <linux/sonos_kernel.h>
#include "mdp.h"

extern SonosRsaKeyEntry SRKE_unlock_domino;
extern SonosRsaKeyEntry SRKE_unitCA_domino;

extern SonosRsaKeyEntry SRKE_unlock_elrey;
extern SonosRsaKeyEntry SRKE_unitCA_elrey;

extern SonosRsaKeyEntry SRKE_unlock_hideout;
extern SonosRsaKeyEntry SRKE_unitCA_hideout;

/* a null-terminated list of signing keys known to the kernel */
const SonosRsaKeyEntry * g_SonosSigningKeys[] =
{
	NULL, // &SRKE_unlock_xxx,
	NULL, // &SRKE_unitCA_xxx,
	NULL
};

/* point the various pointers at the corresponding array fields */
void
sonosInitKeyTable()
{
#if !defined(SONOS_ARCH_ATTR_STUB_SECBOOT_ARCH_KEYS)
	SonosRsaKeyEntry *keyEntry;
	struct key *key;
	int ret;
	const struct cred *cred = NULL;
	key_perm_t perm = 0;
	int i;

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

	for (i = 0; g_SonosSigningKeys[i] != NULL; i++) {
		keyEntry = (SonosRsaKeyEntry *)g_SonosSigningKeys[i];
		keyEntry->key = NULL;
		key = key_alloc(&key_type_asymmetric, keyEntry->name,
				GLOBAL_ROOT_UID, GLOBAL_ROOT_GID,
				cred, perm, KEY_ALLOC_NOT_IN_QUOTA);
		if (IS_ERR(key)) {
			printk(KERN_CRIT "sonosInitKeyTable: key_alloc failed: %ld\n",
			       PTR_ERR(key));
		}
		else {
			ret = key_instantiate_and_link(key,
						       keyEntry->der,
						       keyEntry->derLen,
						       NULL, NULL);
			if (ret < 0) {
				printk(KERN_CRIT "sonosInitKeyTable: key_instantiate_and_link failed: %d\n", ret);
				key_put(key);
			}
			else {
				keyEntry->key = key;
			}
		}
	}
#endif
}
