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

#ifndef __MTK_MEMINFO_H__
#define __MTK_MEMINFO_H__
#include <linux/cma.h>
#include <linux/of_reserved_mem.h>

/* physical offset */
extern phys_addr_t get_phys_offset(void);
/* physical DRAM size */
extern phys_addr_t get_max_DRAM_size(void);
/* DRAM size controlled by kernel */
extern phys_addr_t get_memory_size(void);
extern phys_addr_t mtk_get_max_DRAM_size(void);
extern phys_addr_t get_zone_movable_cma_base(void);
extern phys_addr_t get_zone_movable_cma_size(void);
extern void *vmap_reserved_mem(phys_addr_t start, phys_addr_t size,
		pgprot_t prot);
#ifdef CONFIG_MTK_MEMORY_LOWPOWER
extern phys_addr_t memory_lowpower_base(void);
extern phys_addr_t memory_lowpower_size(void);
extern struct single_cma_registration memory_lowpower_registration;
#endif /* end CONFIG_MTK_MEMORY_LOWPOWER */
extern int get_mntl_buf(u64 *base, u64 *size);

#ifdef CONFIG_MTK_DCS
#define DCS_SCREENOFF_ONLY_MODE
enum dcs_status {
	DCS_NORMAL,
	DCS_LOWPOWER,
	DCS_BUSY,
	DCS_NR_STATUS,
};
enum dcs_kicker {
	DCS_KICKER_MHL,
	DCS_KICKER_PERF,
	DCS_KICKER_WFD,
	DCS_KICKER_VENC,
	DCS_KICKER_CAMERA,
	DCS_KICKER_DEBUG,
	DCS_NR_KICKER,
};
extern int dcs_enter_perf(enum dcs_kicker kicker);
extern int dcs_exit_perf(enum dcs_kicker kicker);
extern int dcs_get_dcs_status_lock(int *ch, enum dcs_status *status);
extern int dcs_get_dcs_status_trylock(int *ch, enum dcs_status *status);
extern void dcs_get_dcs_status_unlock(void);
extern bool dcs_initialied(void);
extern int dcs_full_init(void);
extern char * const dcs_status_name(enum dcs_status status);
extern int dcs_set_lbw_region(u64 start, u64 end);
extern int dcs_mpu_protection(int enable);
/* DO _NOT_ USE APIS below UNLESS YOU KNOW HOW TO USE THEM */
extern int __dcs_get_dcs_status(int *ch, enum dcs_status *dcs_status);
extern int dcs_switch_to_lowpower(void);
extern int memory_lowpower_fb_event(struct notifier_block *notifier,
		unsigned long event, void *data);
#endif /* end CONFIG_MTK_DCS */
#endif /* end __MTK_MEMINFO_H__ */
