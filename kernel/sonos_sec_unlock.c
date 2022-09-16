/*
 * Copyright (c) 2014-2018, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <crypto/sonos_signature_common_linux.h>
#include <crypto/sonos_signature_verify_linux.h>
#include <linux/sonos_mdp_global.h>
#include <linux/sonos_sec_general.h>

/* macros needed by the portable implementation of unlock/authz checking */
#define SU_BE32_TO_CPU		be32_to_cpu
#define SU_CPU_TO_BE32		cpu_to_be32
#define SU_GET_CPUID		sonos_get_cpuid
#define SU_GET_UNLOCK_COUNTER	sonos_get_unlock_counter
#define SU_PRINT		printk
#define SU_PLVL_DEBUG		KERN_DEBUG
#define SU_PLVL_INFO		KERN_INFO
#define SU_PLVL_ERR		KERN_ERR

/* get the portable implementation of unlock/authz checking */
#include "sonos_unlock_token.c.inc"
#include "sonos_unlock.c.inc"

EXPORT_SYMBOL(sonosUnlockVerifyCpuSerialSig);
EXPORT_SYMBOL(sonosUnlockIsDeviceUnlocked);
EXPORT_SYMBOL(sonosUnlockIsAuthFeatureEnabled);

/* Support the persistent unlock functionality... */
/* NOTE:	This function is NOT safe to run from interrupt context */
int is_mdp_authorized(u32 mdp_authorization_flag)
{
	int result = 0;
	SonosSignature *sig = NULL;

	// This structure is pretty massive and could cause
	// us to run out of stack if we try to allocate it there,
	// so use kmalloc instead.	Optimization ideas are welcome.
	sig = kmalloc(sizeof(*sig), GFP_KERNEL);

	result = sig &&
		 sonosUnlockIsAuthFeatureEnabled(mdp_authorization_flag,
						 &sys_mdp,
						 &sys_mdp3,
						 sig,
						 sonosHash,
						 sonosRawVerify,
						 sonosKeyLookup,
						 "unit",
						 "unlock",
						 NULL);

	kfree(sig);
	return result;
}
EXPORT_SYMBOL(is_mdp_authorized);

/*
 *	Unlike the other authorization checks, the call to check
 *	sysrq happens at interrupt level, and the UnlockIsAuthFeature
 *	function is not interrupt safe.  So set a static at boot time,
 *	and only check that.
 */
static int sysrq_authorization = 0;
int is_sysrq_authorized(void)
{
	return sysrq_authorization;
}
EXPORT_SYMBOL(is_sysrq_authorized);

void check_sysrq_authorization(void)
{
	sysrq_authorization = is_mdp_authorized(MDP_AUTH_FLAG_KERNEL_DEBUG_ENABLE);
}
EXPORT_SYMBOL(check_sysrq_authorization);

#if defined(SONOS_ARCH_ATTR_STUB_SECBOOT_BLOB)
int sonos_blob_encdec(bool isEncrypt, const void *in, size_t inLen,
		      void *vOut, size_t *pOutLen,
		      const void *keymodArg, size_t keymodLen)
{
	return -1;
}
EXPORT_SYMBOL(sonos_blob_encdec);
#endif
