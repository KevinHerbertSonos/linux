/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MTK_RAM_CONSOLE_H__
#define __MTK_RAM_CONSOLE_H__

#include <linux/console.h>
#include <linux/pstore.h>

enum AEE_FIQ_STEP_NUM {
	AEE_FIQ_STEP_FIQ_ISR_BASE = 1,
	AEE_FIQ_STEP_WDT_FIQ_INFO = 4,
	AEE_FIQ_STEP_WDT_FIQ_STACK,
	AEE_FIQ_STEP_WDT_FIQ_LOOP,
	AEE_FIQ_STEP_WDT_FIQ_DONE,
	AEE_FIQ_STEP_WDT_IRQ_INFO = 8,
	AEE_FIQ_STEP_WDT_IRQ_KICK,
	AEE_FIQ_STEP_WDT_IRQ_SMP_STOP,
	AEE_FIQ_STEP_WDT_IRQ_TIME,
	AEE_FIQ_STEP_WDT_IRQ_STACK,
	AEE_FIQ_STEP_WDT_IRQ_GIC,
	AEE_FIQ_STEP_WDT_IRQ_LOCALTIMER,
	AEE_FIQ_STEP_WDT_IRQ_IDLE,
	AEE_FIQ_STEP_WDT_IRQ_SCHED,
	AEE_FIQ_STEP_WDT_IRQ_DONE,
	AEE_FIQ_STEP_KE_WDT_INFO = 20,
	AEE_FIQ_STEP_KE_WDT_PERCPU,
	AEE_FIQ_STEP_KE_WDT_LOG,
	AEE_FIQ_STEP_KE_SCHED_DEBUG,
	AEE_FIQ_STEP_KE_EINT_DEBUG,
	AEE_FIQ_STEP_KE_WDT_DONE,
	AEE_FIQ_STEP_KE_IPANIC_DIE = 32,
	AEE_FIQ_STEP_KE_IPANIC_START,
	AEE_FIQ_STEP_KE_IPANIC_OOP_HEADER,
	AEE_FIQ_STEP_KE_IPANIC_DETAIL,
	AEE_FIQ_STEP_KE_IPANIC_CONSOLE,
	AEE_FIQ_STEP_KE_IPANIC_USERSPACE,
	AEE_FIQ_STEP_KE_IPANIC_ANDROID,
	AEE_FIQ_STEP_KE_IPANIC_MMPROFILE,
	AEE_FIQ_STEP_KE_IPANIC_HEADER,
	AEE_FIQ_STEP_KE_IPANIC_DONE,
	AEE_FIQ_STEP_KE_NESTED_PANIC = 64,
};

#ifdef CONFIG_MTK_AEE_IPANIC
extern int ipanic_kmsg_write(unsigned int part, const char *buf, size_t size);
extern int ipanic_kmsg_get_next(int *count, u64 *id, enum pstore_type_id *type,
		struct timespec *time, char **buf, struct pstore_info *psi);
#else
static inline int ipanic_kmsg_write(unsigned int part, const char *buf,
				size_t size)
{
	return 0;
}

static inline int ipanic_kmsg_get_next(int *count, u64 *id,
			enum pstore_type_id *type, struct timespec *time,
			char **buf, struct pstore_info *psi)
{
	return 0;
}
#endif /* CONFIG_MTK_AEE_IPANIC */

#endif
