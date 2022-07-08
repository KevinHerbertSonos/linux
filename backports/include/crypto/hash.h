#ifndef __BACKPORT_LINUX_HASH_H
#define __BACKPORT_LINUX_HASH_H
#include_next <crypto/hash.h>

static inline void shash_desc_zero(struct shash_desc *desc)
{
	memzero_explicit(desc,
			 sizeof(*desc) + crypto_shash_descsize(desc->tfm));
}

#endif /* __BACKPORT_LINUX_HASH_H */
