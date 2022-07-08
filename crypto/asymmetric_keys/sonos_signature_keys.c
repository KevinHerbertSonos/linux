/*
 * Copyright (c) 2015-2019, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 *
 * Sonos Signature format support
 */

#include <linux/err.h>
#include <linux/key.h>
#include <keys/asymmetric-type.h>
#include <crypto/sonos_signature_keys.h>

extern SonosRsaKeyEntry SRKE_unlock;
extern SonosRsaKeyEntry SRKE_unitCA;

/* a null-terminated list of signing keys known to the kernel */
const SonosRsaKeyEntry * const g_SonosSigningKeys[] =
{
#ifdef SONOS_ARCH_ATTR_STUB_SECBOOT_ARCH_KEYS
	NULL,
	NULL,
#else
	&SRKE_unlock,
	&SRKE_unitCA,
#endif
	NULL
};

/* point the various pointers at the corresponding array fields */
void
sonosInitKeyTable(void)
{
	SonosRsaKeyEntry *keyEntry;
	struct key *key;
	int ret;
	const struct cred *cred = NULL;
	key_perm_t perm = 0;
	int i;

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
}
