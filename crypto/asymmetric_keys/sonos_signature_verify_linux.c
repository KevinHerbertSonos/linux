/*
 * Copyright (c) 2015-2018, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 *
 * Sonos Signature format support
 */

/* various macros needed by the included code */
#include <crypto/sonos_signature_macros_linux.h>

#include <crypto/sonos_signature_common_linux.h>
#include <crypto/sonos_signature_verify_linux.h>

/* most of the verify implementation */
#include "sonos_signature_verify.c.inc"
#include "sonos_attr.c.inc"
#include "sonos_attr_parse.c.inc"
#include "sonos_attr_serialize.c.inc"

#include <crypto/public_key.h>
#include <crypto/sonos_signature_keys.h>
#include <crypto/hash_info.h>
#include <linux/string.h>
#include <linux/slab.h>

int
sonosRawVerify(SonosSigningKey_t vkey,
	       SonosSignatureAlg_t signAlg,
	       SonosDigestAlg_t digestAlg,
	       const uint8_t *digest, size_t digestLen,
	       const uint8_t *signature, size_t sigLen)
{
	const SonosRsaKeyEntry *keyEntry = (const SonosRsaKeyEntry *)vkey;
	struct public_key pk;
	struct public_key_signature pks;
	int result = 0;

	if (signAlg != SONOS_SIGNATURE_ALG_RSAPKCS1 ||
	    digestAlg != SONOS_DIGEST_ALG_SHA256 ||
	    digestLen != SHA256_DIGEST_SIZE) {
		printk(KERN_ERR "sonosRawVerify: bad signAlg (%d) digestAlg (%d) or digestLen (%d)\n",
		       (int)signAlg, (int)digestAlg, (int)digestLen);
		return 0;
	}

	memset(&pk, 0, sizeof(pk));
	pk.key = (void *)keyEntry->der;
	pk.keylen = keyEntry->derLen;
	pk.pkey_algo = "rsa";
	pk.key_is_private = false;
	/* We don't need to initialize the rest of the public_key fields
	   (algo, params, paramlen) since they don't apply to RSA
	   and are already zeroed out
	*/

	memset(&pks, 0, sizeof(pks));
	/* kmemdup sig and digest so the virt_to_phys check passes */
	pks.s = (u8 *)kmemdup(signature, sigLen, GFP_KERNEL);
	pks.s_size = sigLen;
	pks.digest = (u8 *)kmemdup(digest, digestLen, GFP_KERNEL);
	pks.digest_size = digestLen;
	pks.pkey_algo = "rsa";
	pks.hash_algo = "sha256";
	pks.encoding = "pkcs1";

	if (!pks.s || !pks.digest) {
		printk(KERN_ERR "sonosRawVerify: kmemdup failed\n");
	}
	else {
		if (public_key_verify_signature(&pk, &pks) == 0) {
			result = 1;
		}
		else {
			printk(KERN_ERR "sonosRawVerify: verify_signature failed\n");
		}
	}

	kfree(pks.s);
	kfree(pks.digest);
	return result;
}

/*
 * For now this always override the lookup using cbArg/name (it ignores
 * the key identifier on the signature itself). This is because the verifier
 * always knows which specific key it expects.
 */
SonosSigningKey_t
sonosKeyLookup(const void *cbArg,
	       SonosKeyIdentifierScheme_t keyIdScheme,
	       const uint8_t *keyId,
	       size_t keyIdLen)
{
	int i;
	const SonosRsaKeyEntry *keyEntry;
	const char *name = (const char *)cbArg;

	(void)keyIdScheme;
	(void)keyId;
	(void)keyIdLen;

	for (i = 0; g_SonosSigningKeys[i] != NULL; i++) {
		keyEntry = g_SonosSigningKeys[i];
		if (strcmp(keyEntry->name, name) == 0) {
			printk(KERN_DEBUG "sonosKeyLookup(%s) chose key %d\n",
			       name, i+1);
			return (SonosSigningKey_t)keyEntry;
		}
	}

	printk(KERN_CRIT "sonosKeyLookup(%s) failed\n", name);
	return NULL;
}
