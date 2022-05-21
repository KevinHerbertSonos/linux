/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/sched_clock.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
/* #include <board-custom.h> */

#include "mt_fhreg.h"
#include "mt_freqhopping.h"
#include "sync_write.h"

#include <linux/seq_file.h>
#include <mt_freqhopping_drv.h>

#ifdef CONFIG_OF
#include <linux/of_address.h>
static void __iomem *g_fhctl_base;
static void __iomem *g_apmixed_base;
#endif

#define USER_DEFINE_SETTING_ID (1)

#define MASK21b (0x1FFFFF)
#define BIT32 (1U << 31)

#define GPU_DVFS_LOG 0

static DEFINE_SPINLOCK(g_fh_lock);

#define PERCENT_TO_DDSLMT(dDS, pERCENT_M10) (((dDS * pERCENT_M10) >> 5) / 100)

static unsigned int g_initialize;

#ifndef PER_PROJECT_FH_SETTING

/* mt8518 fhctl MB */
/* default VCO freq. */
#define ARMCA7PLL_DEF_FREQ 1599000
#define MAINPLL_DEF_FREQ 1092000
#define MMPLL_DEF_FREQ 1092000
#define TVDPLL_DEF_FREQ 1782000

/* keep track the status of each PLL */
static struct fh_pll_t g_fh_pll[FH_PLL_NUM] = {
	{FH_FH_DISABLE, FH_PLL_ENABLE, 0, ARMCA7PLL_DEF_FREQ, 0},
	{FH_FH_DISABLE, FH_PLL_ENABLE, 0, MAINPLL_DEF_FREQ, 0},
	{FH_FH_DISABLE, FH_PLL_ENABLE, 0, MMPLL_DEF_FREQ, 0},
	{FH_FH_DISABLE, FH_PLL_ENABLE, 0, TVDPLL_DEF_FREQ, 0},
};

static const struct freqhopping_ssc ssc_armca7pll_setting[] = {
	{0, 0, 0, 0, 0, 0},			   /* Means disable */
	{0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},	 /* Means User-Define */
	{ARMCA7PLL_DEF_FREQ, 0, 9, 0, 8, 0xF6000}, /* 0 ~ -8% */
	{0, 0, 0, 0, 0, 0}			   /* EOF */
};

static const struct freqhopping_ssc ssc_mainpll_setting[] = {
	{0, 0, 0, 0, 0, 0},
	{0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	{MAINPLL_DEF_FREQ, 0, 9, 0, 8, 0xA8000}, /* 0 ~ -8% */
	{0, 0, 0, 0, 0, 0} };

static const struct freqhopping_ssc ssc_mmpll_setting[] = {
	{0, 0, 0, 0, 0, 0},
	{0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	{MMPLL_DEF_FREQ, 0, 9, 0, 8, 0xD8000},
	{0, 0, 0, 0, 0, 0} };

static const struct freqhopping_ssc ssc_tvdpll_setting[] = {
	{0, 0, 0, 0, 0, 0},
	{0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	{TVDPLL_DEF_FREQ, 0, 9, 0, 8, 0x112276}, /* 0~-8% */
	{0, 0, 0, 0, 0, 0} };

static const unsigned int g_default_freq[] = {
	ARMCA7PLL_DEF_FREQ, MAINPLL_DEF_FREQ, MMPLL_DEF_FREQ, TVDPLL_DEF_FREQ};

static struct freqhopping_ssc mt_ssc_fhpll_userdefined[FH_PLL_NUM] = {
	{0, 1, 1, 2, 2, 0}, /* ARMCA7PLL */
	{0, 1, 1, 2, 2, 0}, /* MAINPLL */
	{0, 1, 1, 2, 2, 0}, /* MMPLL */
	{0, 1, 1, 2, 2, 0}, /* TVDPLL */
};

static const unsigned int fh_clk_en[] = {FH_FHCTL0_CLK_EN, FH_FHCTL1_CLK_EN,
	FH_FHCTL3_CLK_EN, FH_FHCTL4_CLK_EN};

#else /* PER_PROJECT_FH_SETTING */

PER_PROJECT_FH_SETTING

#endif /* PER_PROJECT_FH_SETTING */

static const struct freqhopping_ssc *g_ssc_setting[] = {
	ssc_armca7pll_setting, ssc_mainpll_setting, ssc_mmpll_setting,
	ssc_tvdpll_setting};

static const unsigned int g_ssc_setting_size[] = {
	ARRAY_SIZE(ssc_armca7pll_setting), ARRAY_SIZE(ssc_mainpll_setting),
	ARRAY_SIZE(ssc_mmpll_setting), ARRAY_SIZE(ssc_tvdpll_setting),
};

static unsigned long g_reg_dds[FH_PLL_NUM];
static unsigned long g_reg_cfg[FH_PLL_NUM];
static unsigned long g_reg_updnlmt[FH_PLL_NUM];
static unsigned long g_reg_mon[FH_PLL_NUM];
static unsigned long g_reg_dvfs[FH_PLL_NUM];
static unsigned long g_reg_pll_con1[FH_PLL_NUM];
/* mt8518 fhctl ME */

#define VALIDATE_PLLID(id) WARN_ON(id >= FH_PLL_NUM)

/* #define ENABLE_DVT_LTE_SIDEBAND_SIGNAL_TESTCASE */

/* caller: clk mgr */
static void mt_fh_hal_default_conf(void)
{
	FH_MSG_DEBUG("%s", __func__);

#ifdef ENABLE_DVT_LTE_SIDEBAND_SIGNAL_TESTCASE
	fh_set_field(REG_FHCTL1_DDS, (0x1FFFFFU << 0),
		     0X100000); /* /< Set default MPLL DDS */
	fh_set_field(REG_FHCTL1_DDS, (0x1U << 32), 1);

	fh_set_field(REG_FHCTL_HP_EN, (0x1U << 31),
		     1); /* /< Enable LTE Sideband signal */
	fh_set_field(REG_FHCTL_HP_EN, (0x1U << 1), 0x1); /* /< MPLL */

	fh_set_field(REG_FHCTL1_CFG, (0x1U << 0), 1); /* /< Enable */
	fh_set_field(REG_FHCTL1_CFG, (0x1U << 3), 1); /* /< DYSSC Enable */

	/* /< Set FHCTL_DSSC_CFG(0x1000CF14), Bit3 is RF3(LTE) SSC control. */
	/* Clear to 0 is enable. */
	fh_set_field(REG_FHCTL_DSSC_CFG, (0x1U << 3), 0);
	fh_set_field(REG_FHCTL_DSSC_CFG, (0x1U << 19),
		     0); /* /< RF(3) LTE BAN control */

/* fh_set_field(REG_FHCTL_DSSC3_CON, (0x1U<<1), 1); */
#endif
}

/* mt8518 fhctl MB */
int pll_id_to_fhctl_id(int pll_id)
{
	switch (pll_id) {
	case 0:
		return 0;

	case 1:
		return 1;

	case 2:
		return 11;

	case 3:
		return 10;

	default:
		return 0;
	}
}
/* mt8518 fhctl ME */

static void fh_switch2fhctl(enum FH_PLL_ID pll_id, int i_control)
{
	unsigned int mask = 0;

	VALIDATE_PLLID(pll_id);

	mask = 0x1U << pll_id_to_fhctl_id(pll_id);

	/* FIXME: clock should be turned on/off at entry functions */
	/* Turn on clock */
	/* if (i_control == 1) */
	/* fh_set_field(REG_FHCTL_CLK_CON, mask, i_control); */

	/* Release software reset */
	/* fh_set_field(REG_FHCTL_RST_CON, mask, 0); */

	/* Switch to FHCTL_CORE controller */
	fh_set_field(REG_FHCTL_HP_EN, mask, i_control);

	/* Turn off clock */
	/* if (i_control == 0) */
	/* fh_set_field(REG_FHCTL_CLK_CON, mask, i_control); */
}

static void fh_sync_ncpo_to_fhctl_dds(enum FH_PLL_ID pll_id)
{
	unsigned long reg_src = 0;
	unsigned long reg_dst = 0;

	VALIDATE_PLLID(pll_id);

	reg_src = g_reg_pll_con1[(int)pll_id];
	reg_dst = g_reg_dds[(int)pll_id];

	fh_write32(reg_dst, (fh_read32(reg_src) & MASK21b) | BIT32);
}

static void __enable_ssc(unsigned int pll_id,
			 const struct freqhopping_ssc *setting)
{
	unsigned long flags = 0;
	const unsigned long reg_cfg = g_reg_cfg[pll_id];
	const unsigned long reg_updnlmt = g_reg_updnlmt[pll_id];
	const unsigned long reg_dds = g_reg_dds[pll_id];

	FH_MSG_DEBUG("%s: %x~%x df:%d dt:%d dds:%x", __func__, setting->lowbnd,
		     setting->upbnd, setting->df, setting->dt, setting->dds);

	mb(); /* mb */

	g_fh_pll[pll_id].fh_status = FH_FH_ENABLE_SSC;

	local_irq_save(flags);

	/* Set the relative parameter registers (dt/df/upbnd/downbnd) */
	fh_set_field(REG_FHCTL_CLK_CON, fh_clk_en[pll_id], 1);
	fh_set_field(reg_cfg, MASK_FRDDSX_DYS, setting->df);
	fh_set_field(reg_cfg, MASK_FRDDSX_DTS, setting->dt);

	fh_sync_ncpo_to_fhctl_dds(pll_id);

	/* TODO: Not setting upper due to they are all 0? */
	fh_write32(reg_updnlmt,
		   (PERCENT_TO_DDSLMT((fh_read32(reg_dds) & MASK21b),
				      setting->lowbnd)
		    << 16));

	/* Switch to FHCTL */
	fh_switch2fhctl(pll_id, 1);
	mb(); /* mb */

	/* Enable SSC */
	fh_set_field(reg_cfg, FH_FRDDSX_EN, 1);
	/* Enable Hopping control */
	fh_set_field(reg_cfg, FH_FHCTLX_EN, 1);

	local_irq_restore(flags);
}

static void __disable_ssc(unsigned int pll_id,
			  const struct freqhopping_ssc *ssc_setting)
{
	unsigned long flags = 0;
	unsigned long reg_cfg = g_reg_cfg[pll_id];

	FH_MSG_DEBUG("Calling %s", __func__);

	local_irq_save(flags);

	/* Set the relative registers */
	fh_set_field(reg_cfg, FH_FRDDSX_EN, 0);
	fh_set_field(reg_cfg, FH_FHCTLX_EN, 0);
	mb(); /* mb */
	fh_switch2fhctl(pll_id, 0);
	fh_set_field(REG_FHCTL_CLK_CON, fh_clk_en[pll_id], 0);
	g_fh_pll[pll_id].fh_status = FH_FH_DISABLE;
	local_irq_restore(flags);
	mb(); /* mb */
}

/* freq is in KHz, return at which number of entry in mt_ssc_xxx_setting[] */
static noinline int __freq_to_index(enum FH_PLL_ID pll_id, int freq)
{
	unsigned int retVal = 0;
	unsigned int i =
		2; /* 0 is disable, 1 is user defines, so start from 2 */
	const unsigned int size = g_ssc_setting_size[pll_id];

	while (i < size) {
		if (freq == g_ssc_setting[pll_id][i].freq) {
			retVal = i;
			break;
		}
		++i;
	}

	return retVal;
}

static int __freqhopping_ctrl(struct freqhopping_ioctl *fh_ctl, bool enable)
{
	const struct freqhopping_ssc *pSSC_setting = NULL;
	unsigned int ssc_setting_id = 0;
	int retVal = 1;
	struct fh_pll_t *pfh_pll = NULL;

	FH_MSG("%s for pll %d", __func__, fh_ctl->pll_id);

	/* Check the out of range of frequency hopping PLL ID */
	VALIDATE_PLLID(fh_ctl->pll_id);

	pfh_pll = &g_fh_pll[fh_ctl->pll_id];

	pfh_pll->curr_freq = g_default_freq[fh_ctl->pll_id];

	if ((enable == true) && (pfh_pll->fh_status == FH_FH_ENABLE_SSC)) {
		__disable_ssc(fh_ctl->pll_id, pSSC_setting);
	} else if ((enable == false) && (pfh_pll->fh_status == FH_FH_DISABLE)) {
		retVal = 0;
		goto Exit;
	}

	/* enable freq. hopping @ fh_ctl->pll_id */
	if (enable == true) {
		if (pfh_pll->pll_status == FH_PLL_DISABLE) {
			pfh_pll->fh_status = FH_FH_ENABLE_SSC;
			retVal = 0;
			goto Exit;
		} else {
			if (pfh_pll->user_defined == true) {
				FH_MSG("Apply user defined setting");

				pSSC_setting = &mt_ssc_fhpll_userdefined
						       [fh_ctl->pll_id];
				pfh_pll->setting_id = USER_DEFINE_SETTING_ID;
			} else {
				if (pfh_pll->curr_freq != 0) {
					ssc_setting_id = pfh_pll->setting_id =
						__freq_to_index(
							fh_ctl->pll_id,
							pfh_pll->curr_freq);
				} else {
					ssc_setting_id = 0;
				}

				if (ssc_setting_id == 0) {
					/* just disable FH & exit */
					__disable_ssc(fh_ctl->pll_id,
						      pSSC_setting);
					goto Exit;
				}

				pSSC_setting = &g_ssc_setting[fh_ctl->pll_id]
							     [ssc_setting_id];
			} /* user defined */

			if (pSSC_setting == NULL) {
				FH_MSG("SSC_setting is NULL!");

				/* disable FH & exit */
				__disable_ssc(fh_ctl->pll_id, pSSC_setting);
				goto Exit;
			}

			__enable_ssc(fh_ctl->pll_id, pSSC_setting);
			retVal = 0;
		}
	} else { /* disable req. hopping @ fh_ctl->pll_id */
		__disable_ssc(fh_ctl->pll_id, pSSC_setting);
		retVal = 0;
	}

Exit:
	return retVal;
}

static void wait_dds_stable(unsigned int target_dds, unsigned long reg_mon,
			    unsigned int wait_count)
{
	unsigned int fh_dds = 0;
	unsigned int i = 0;

	fh_dds = fh_read32(reg_mon) & MASK21b;
	while ((target_dds != fh_dds) && (i < wait_count)) {
		udelay(10);
#if 0
		if (unlikely(i > 100)) {
			WARN_ON(1);
			break;
		}
#endif
		fh_dds = (fh_read32(reg_mon)) & MASK21b;
		++i;
	}
	FH_MSG("target_dds = %d, fh_dds = %d, i = %d", target_dds, fh_dds, i);
}

static int mt_fh_hal_dvfs(enum FH_PLL_ID pll_id, unsigned int dds_value)
{
	unsigned long flags = 0;

	FH_MSG_DEBUG("%s for pll %d:", __func__, pll_id);

	VALIDATE_PLLID(pll_id);

	local_irq_save(flags);

	/* 1. sync ncpo to DDS of FHCTL */
	fh_sync_ncpo_to_fhctl_dds(pll_id);

	/* FH_MSG("1. sync ncpo to DDS of FHCTL"); */
	FH_MSG_DEBUG("FHCTL%d_DDS: 0x%08x", pll_id,
		     (fh_read32(g_reg_dds[pll_id]) & MASK21b));
	fh_set_field(REG_FHCTL_CLK_CON, fh_clk_en[pll_id], 1);

	/* 2. enable DVFS and Hopping control */
	{
		unsigned long reg_cfg = g_reg_cfg[pll_id];

		fh_set_field(reg_cfg, FH_SFSTRX_EN, 1); /* enable dvfs mode */
		fh_set_field(reg_cfg, FH_FHCTLX_EN,
			     1); /* enable hopping control */
	}

	/* for slope setting. */
	/* TODO: Does this need to be changed? */
	fh_write32(REG_FHCTL_SLOPE0, 0x6003c97);

	/* FH_MSG("2. enable DVFS and Hopping control"); */

	/* 3. switch to hopping control */
	fh_switch2fhctl(pll_id, 1);
	mb(); /* mb */

	/* FH_MSG("3. switch to hopping control"); */

	/* 4. set DFS DDS */
	{
		unsigned long dvfs_req = g_reg_dvfs[pll_id];

		fh_write32(dvfs_req, (dds_value) | (BIT32)); /* set dds */

		/* FH_MSG("4. set DFS DDS"); */
		FH_MSG_DEBUG("FHCTL%d_DDS: 0x%08x", pll_id,
			     (fh_read32(dvfs_req) & MASK21b));
		FH_MSG_DEBUG("FHCTL%d_DVFS: 0x%08x", pll_id,
			     (fh_read32(dvfs_req) & MASK21b));
	}

	/* 4.1 ensure jump to target DDS */
	wait_dds_stable(dds_value, g_reg_mon[pll_id], 100);
	/* FH_MSG("4.1 ensure jump to target DDS"); */

	/* 5. write back to ncpo */
	/* FH_MSG("5. write back to ncpo"); */
	{
		unsigned long reg_dvfs = 0;
		unsigned long reg_pll_con1 = 0;

		reg_pll_con1 = g_reg_pll_con1[pll_id];
		reg_dvfs = g_reg_dvfs[pll_id];
		FH_MSG_DEBUG("PLL_CON1: 0x%08x",
			     (fh_read32(reg_pll_con1) & MASK21b));

		fh_write32(reg_pll_con1,
			   (fh_read32(g_reg_mon[pll_id]) & MASK21b) |
				   (fh_read32(reg_pll_con1) & 0xFFE00000) |
				   (BIT32));
		FH_MSG_DEBUG("PLL_CON1: 0x%08x",
			     (fh_read32(reg_pll_con1) & MASK21b));
	}

	/* 6. switch to register control */
	fh_switch2fhctl(pll_id, 0);
	mb(); /* mb */

	/* FH_MSG("6. switch to register control"); */

	local_irq_restore(flags);
	return 0;
}

/* armpll dfs mdoe */
static int mt_fh_hal_dfs_armpll(unsigned int pll, unsigned int dds)
{
	unsigned long flags = 0;
	unsigned long reg_cfg = 0;

	if (g_initialize == 0) {
		FH_MSG("(Warning) %s FHCTL isn't ready.", __func__);
		return -1;
	}

	FH_MSG("%s for pll %d dds %d", __func__, pll, dds);

	switch (pll) {
	case FH_ARMCA7_PLLID:
		reg_cfg = g_reg_cfg[pll];
		FH_MSG("(PLL_CON1): 0x%x",
		       (fh_read32(g_reg_pll_con1[pll]) & MASK21b));
		break;
	default:
		WARN_ON(1);
		return 1;
	};

	/* TODO: provelock issue spin_lock(&g_fh_lock); */
	spin_lock_irqsave(&g_fh_lock, flags);

	fh_set_field(reg_cfg, FH_FRDDSX_EN, 0); /* disable SSC mode */
	fh_set_field(reg_cfg, FH_SFSTRX_EN, 0); /* disable dvfs mode */
	fh_set_field(reg_cfg, FH_FHCTLX_EN, 0); /* disable hopping control */

	mt_fh_hal_dvfs(pll, dds);

	fh_set_field(reg_cfg, FH_FRDDSX_EN, 0); /* disable SSC mode */
	fh_set_field(reg_cfg, FH_SFSTRX_EN, 0); /* disable dvfs mode */
	fh_set_field(reg_cfg, FH_FHCTLX_EN, 0); /* disable hopping control */

	spin_unlock_irqrestore(&g_fh_lock, flags);

	return 0;
}

static int mt_fh_hal_general_pll_dfs(enum FH_PLL_ID pll_id,
				     unsigned int target_dds)
{
	const unsigned long reg_cfg = g_reg_cfg[pll_id];
	unsigned long flags = 0;

	VALIDATE_PLLID(pll_id);
	if (g_initialize == 0) {
		FH_MSG("(Warning) %s FHCTL isn't ready. ", __func__);
		return -1;
	}

	if (target_dds > FH_DDS_MASK) {
		FH_MSG("[ERROR] Overflow! [%s] [pll_id]:%d [dds]:0x%x",
		       __func__, pll_id, target_dds);
		/* Check dds overflow (21 bit) */
		WARN_ON(1);
	}

	FH_MSG("%s, [Pll_ID]:%d [current dds(CON1)]:0x%x, [target dds]:%d",
	       __func__, pll_id,
	       (fh_read32(g_reg_pll_con1[pll_id]) & FH_DDS_MASK), target_dds);

	spin_lock_irqsave(&g_fh_lock, flags);

	if (g_fh_pll[pll_id].fh_status == FH_FH_ENABLE_SSC) {
		unsigned int pll_dds = 0;
		unsigned int fh_dds = 0;

		/* only when SSC is enable, turn off MEMPLL hopping */
		fh_set_field(reg_cfg, FH_FRDDSX_EN, 0); /* disable SSC mode */
		fh_set_field(reg_cfg, FH_SFSTRX_EN, 0); /* disable dvfs mode */
		fh_set_field(reg_cfg, FH_FHCTLX_EN, 0); /* disable hp ctl */

		pll_dds = (fh_read32(g_reg_dds[pll_id])) & FH_DDS_MASK;
		fh_dds = (fh_read32(g_reg_mon[pll_id])) & FH_DDS_MASK;

		FH_MSG_DEBUG(">p:f< %x:%x", pll_dds, fh_dds);

		wait_dds_stable(pll_dds, g_reg_mon[pll_id], 100);
	}

	mt_fh_hal_dvfs(pll_id, target_dds);

	if (g_fh_pll[pll_id].fh_status == FH_FH_ENABLE_SSC) {
		const struct freqhopping_ssc *p_setting = NULL;

		if (g_fh_pll[pll_id].user_defined == true)
			p_setting = &mt_ssc_fhpll_userdefined[pll_id];
		else
			p_setting = &g_ssc_setting[pll_id][2];

		fh_set_field(reg_cfg, FH_FRDDSX_EN, 0); /* disable SSC mode */
		fh_set_field(reg_cfg, FH_SFSTRX_EN, 0); /* disable dvfs mode */
		fh_set_field(reg_cfg, FH_FHCTLX_EN, 0); /* disable hp ctl */

		fh_sync_ncpo_to_fhctl_dds(pll_id);

		fh_set_field(reg_cfg, MASK_FRDDSX_DYS, p_setting->df);
		fh_set_field(reg_cfg, MASK_FRDDSX_DTS, p_setting->dt);

		fh_write32(g_reg_updnlmt[pll_id],
			   (PERCENT_TO_DDSLMT((fh_read32(g_reg_dds[pll_id]) &
					       FH_DDS_MASK),
					      p_setting->lowbnd)
			    << 16));

		fh_switch2fhctl(pll_id, 1);

		fh_set_field(reg_cfg, FH_FRDDSX_EN, 1); /* enable SSC mode */
		fh_set_field(reg_cfg, FH_FHCTLX_EN, 1); /* enable hp ctl */

		FH_MSG("CFG: 0x%08x", fh_read32(reg_cfg));
	}
	fh_set_field(REG_FHCTL_CLK_CON, fh_clk_en[pll_id], 0);
	spin_unlock_irqrestore(&g_fh_lock, flags);

	return 0;
}

#if 0
static int fh_dvfs_proc_read(struct seq_file *m, void *v)
{
	int i = 0;

	FH_MSG("EN: %s", __func__);

	seq_puts(m, "DVFS:\r\n");
	seq_puts(m, "CFG: 0x3 is SSC mode;  0x5 is DVFS mode \r\n");
	for (i = 0; i < FH_PLL_NUM; ++i) {
		seq_printf(m, "FHCTL%d:   CFG:0x%08x    DVFS:0x%08x\r\n", i,
			   fh_read32(g_reg_cfg[i]), fh_read32(g_reg_dvfs[i]));
	}
	return 0;
}
#endif
/* #define UINT_MAX (unsigned int)(-1) */
static int fh_dumpregs_proc_read(struct seq_file *m, void *v)
{
	int i = 0;
	static unsigned int dds_max[FH_PLL_NUM] = {0};
	static unsigned int dds_min[FH_PLL_NUM] = {
		UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX};

	FH_MSG("EN: %s", __func__);

	for (i = 0; i < FH_PLL_NUM; ++i) {
		const unsigned int mon = fh_read32(g_reg_mon[i]);
		const unsigned int dds = mon & MASK21b;

		seq_printf(m, "FHCTL%d CFG, UPDNLMT, DDS, MON\r\n", i);
		seq_printf(m, "0x%08x 0x%08x 0x%08x 0x%08x\r\n",
			   fh_read32(g_reg_cfg[i]), fh_read32(g_reg_updnlmt[i]),
			   fh_read32(g_reg_dds[i]), mon);
		seq_printf(m, "FHCTL%d CFG addr: 0x%08lx\n", i, g_reg_cfg[i]);
		if (dds > dds_max[i])
			dds_max[i] = dds;
		if (dds < dds_min[i])
			dds_min[i] = dds;
	}

	seq_printf(m, "\r\nFHCTL_HP_EN:\r\n0x%08x\r\n",
		   fh_read32(REG_FHCTL_HP_EN));

	seq_puts(m, "\r\nPLL_CON0 :\r\n");
	seq_printf(m, "ARMCA7:0x%08x MAIN:0x%08x ",
		   fh_read32(REG_ARMCA7PLL_CON0), fh_read32(REG_MAINPLL_CON0));
	seq_printf(m, "MM:0x%08x TVD:0x%08x \r\n", fh_read32(REG_MMPLL_CON0),
		   fh_read32(REG_TVDPLL_CON0));

	seq_puts(m, "\r\nPLL_CON1 :\r\n");
	seq_printf(m, "ARMCA7:0x%08x MAIN:0x%08x ",
		   fh_read32(REG_ARMCA7PLL_CON1), fh_read32(REG_MAINPLL_CON1));
	seq_printf(m, "MM:0x%08x TVD:0x%08x \r\n",
			fh_read32(REG_MMPLL_CON1), fh_read32(REG_TVDPLL_CON1));

	seq_puts(m, "\r\nRecorded dds range\r\n");
	for (i = 0; i < FH_PLL_NUM; ++i) {
		seq_printf(m, "Pll%d dds max 0x%06x, min 0x%06x\r\n", i,
			   dds_max[i], dds_min[i]);
	}

	return 0;
}

static void __reg_tbl_init(void)
{
#ifdef CONFIG_OF
	int i = 0;

	/* mt8518 fhctl MB */
	const unsigned long reg_dds[] = {REG_FHCTL0_DDS, REG_FHCTL1_DDS,
					 REG_FHCTL3_DDS, REG_FHCTL4_DDS};

	const unsigned long reg_cfg[] = {REG_FHCTL0_CFG, REG_FHCTL1_CFG,
					 REG_FHCTL3_CFG, REG_FHCTL4_CFG};

	const unsigned long reg_updnlmt[] = {
		REG_FHCTL0_UPDNLMT, REG_FHCTL1_UPDNLMT, REG_FHCTL3_UPDNLMT,
		REG_FHCTL4_UPDNLMT};

	const unsigned long reg_mon[] = {REG_FHCTL0_MON, REG_FHCTL1_MON,
					 REG_FHCTL3_MON, REG_FHCTL4_MON};

	const unsigned long reg_dvfs[] = {REG_FHCTL0_DVFS, REG_FHCTL1_DVFS,
					  REG_FHCTL3_DVFS, REG_FHCTL4_DVFS};

	const unsigned long reg_pll_con1[] = {REG_ARMCA7PLL_CON1,
					      REG_MAINPLL_CON1, REG_MMPLL_CON1,
					      REG_TVDPLL_CON1};
	/* mt8518 fhctl ME */

	FH_MSG_DEBUG("EN: %s", __func__);

	for (i = 0; i < FH_PLL_NUM; ++i) {
		g_reg_dds[i] = reg_dds[i];
		g_reg_cfg[i] = reg_cfg[i];
		g_reg_updnlmt[i] = reg_updnlmt[i];
		g_reg_mon[i] = reg_mon[i];
		g_reg_dvfs[i] = reg_dvfs[i];
		g_reg_pll_con1[i] = reg_pll_con1[i];
#ifdef CONFIG_ARM64
/* FH_MSG_DEBUG("index:%d %lx %lx %lx %lx %lx %lx", i, g_reg_dds[i],
 * g_reg_cfg[i]
 * , g_reg_updnlmt[i], g_reg_mon[i], g_reg_dvfs[i], g_reg_pll_con1[i]);
 */
#else
/* FH_MSG_DEBUG("index:%d %x %x %x %x %x %x", i, g_reg_dds[i], g_reg_cfg[i]
 * , g_reg_updnlmt[i], g_reg_mon[i], g_reg_dvfs[i], g_reg_pll_con1[i]);
 */
#endif
	}
#endif
}

#ifdef CONFIG_OF
/* Device Tree Initialize */
static int __reg_base_addr_init(void)
{
	struct device_node *fhctl_node;
	struct device_node *apmixed_node;

	/* Init FHCTL base address */
	fhctl_node =
		of_find_compatible_node(NULL, NULL, "mediatek,mt8518-fhctl");
	if (!fhctl_node) {
		FH_MSG_DEBUG(" Error, Cannot find FHCTL device tree node");
		return -1;
	}

	g_fhctl_base = of_iomap(fhctl_node, 0);
	if (!g_fhctl_base) {
		FH_MSG_DEBUG("Error, FHCTL iomap failed");
		return -1;
	}
	/* FH_MSG_DEBUG("FHCTL dase address:0x%lx", (unsigned */
	/* long)g_fhctl_base); */
	/* if-else */

	/* Init APMIXED base address */
	apmixed_node = of_find_compatible_node(NULL, NULL,
					       "mediatek,mt8518-apmixedsys");
	if (!fhctl_node) {
		FH_MSG_DEBUG(" Error, Cannot find APMIXED device tree node");
		return -1;
	}

	g_apmixed_base = of_iomap(apmixed_node, 0);
	if (!g_apmixed_base) {
		FH_MSG_DEBUG("Error, APMIXED iomap failed");
		return -1;
	}
	/* FH_MSG_DEBUG("APMIXED dase address:0x%lx", (unsigned */
	/* long)g_apmixed_base); */
	/* if-else */

	if (!fhctl_node) {
		FH_MSG_DEBUG(" Error, Cannot find FHCTL device tree node");
		return -1;
	}

	__reg_tbl_init();

	return 0;
}
#endif

/* TODO: __init void mt_freqhopping_init(void) */
static int mt_fh_hal_init(void)
{
	int i = 0;
	int ret = 0;
	unsigned long flags = 0;
	unsigned int fhctl_clock_mask = 0x01000000;

	FH_MSG_DEBUG("EN: %s", __func__);

	if (g_initialize == 1)
		return -1;

#ifdef CONFIG_OF

	/* Init relevant register base address by device tree */
	ret = __reg_base_addr_init();
	if (ret)
		return -1;
#endif

	fh_set_field(REG_AP_PLL_CON0, fhctl_clock_mask, 1);

	for (i = 0; i < FH_PLL_NUM; ++i) {
		unsigned int mask = 1 << i;

		spin_lock_irqsave(&g_fh_lock, flags);

		/* TODO: clock should be turned on only when FH is needed */
		/* Turn on all clock */
		fh_set_field(REG_FHCTL_CLK_CON, mask, 1);

		/* Release software-reset to reset */
		fh_set_field(REG_FHCTL_RST_CON, mask, 0);
		fh_set_field(REG_FHCTL_RST_CON, mask, 1);

		g_fh_pll[i].setting_id = 0;
		fh_write32(g_reg_cfg[i],
			   0x00000000); /* No SSC and FH enabled */
		fh_write32(g_reg_updnlmt[i],
			   0x00000000); /* clear all the settings */
		fh_write32(g_reg_dds[i],
			   0x00000000); /* clear all the settings */

		spin_unlock_irqrestore(&g_fh_lock, flags);
	}

	g_initialize = 1;
	return 0;
}

static void mt_fh_hal_lock(unsigned long *flags)
{
	spin_lock_irqsave(&g_fh_lock, *flags);
}

static void mt_fh_hal_unlock(unsigned long *flags)
{
	spin_unlock_irqrestore(&g_fh_lock, *flags);
}

static int mt_fh_hal_get_init(void)
{
	return g_initialize;
}

static int __fh_debug_proc_read(struct seq_file *m, void *v,
				struct fh_pll_t *pll)
{
	FH_MSG("EN: %s", __func__);

	seq_puts(m, "\r\n[freqhopping debug flag]\r\n");
	seq_puts(m, "===============================================\r\n");
	seq_puts(m, "id=ARMCA7PLL=MAINPLL=MMPLL=TVDPLL\r\n");
	seq_printf(m, "  =%04d==%04d==%04d==%04d=\r\n",
		   pll[FH_ARMCA7_PLLID].fh_status, pll[FH_MAIN_PLLID].fh_status,
		   pll[FH_MM_PLLID].fh_status, pll[FH_TVD_PLLID].fh_status);
	seq_printf(m, "  =%04d==%04d==%04d==%04d=\r\n",
		   pll[FH_ARMCA7_PLLID].setting_id,
		   pll[FH_MAIN_PLLID].setting_id, pll[FH_MM_PLLID].setting_id,
		   pll[FH_TVD_PLLID].setting_id);

	return 0;
}
/* mt8518 fhctl ME */

/* *********************************************************************** */
/* This function would support special request. */
/* [History] */
/* (2014.8.13)  HQA desence SA required MEMPLL to enable SSC -2~-4%. */
/* We implement API mt_freqhopping_devctl() to */
/* complete -2~-4% SSC. (DVFS to -2% freq and enable 0~-2% SSC) */
/*  */
/* *********************************************************************** */
static int fh_ioctl_dvfs_ssc(unsigned int ctlid, void *arg)
{
	struct freqhopping_ioctl *fh_ctl = arg;

	switch (ctlid) {
	case FH_DCTL_CMD_DVFS: /* < PLL DVFS */
	{
		mt_fh_hal_dvfs(fh_ctl->pll_id, fh_ctl->ssc_setting.dds);
	} break;
	case FH_DCTL_CMD_DVFS_SSC_ENABLE: /* < PLL DVFS and enable SSC */
	{
		__disable_ssc(fh_ctl->pll_id, &(fh_ctl->ssc_setting));
		mt_fh_hal_dvfs(fh_ctl->pll_id, fh_ctl->ssc_setting.dds);
		__enable_ssc(fh_ctl->pll_id, &(fh_ctl->ssc_setting));
	} break;
	case FH_DCTL_CMD_DVFS_SSC_DISABLE: /* < PLL DVFS and disable SSC */
	{
		__disable_ssc(fh_ctl->pll_id, &(fh_ctl->ssc_setting));
		mt_fh_hal_dvfs(fh_ctl->pll_id, fh_ctl->ssc_setting.dds);
	} break;
	case FH_DCTL_CMD_SSC_ENABLE: /* < SSC enable */
	{
		__enable_ssc(fh_ctl->pll_id, &(fh_ctl->ssc_setting));
	} break;
	case FH_DCTL_CMD_SSC_DISABLE: /* < SSC disable */
	{
		__disable_ssc(fh_ctl->pll_id, &(fh_ctl->ssc_setting));
	} break;
	default:
		break;
	};

	return 0;
}

static void __ioctl(unsigned int ctlid, void *arg)
{
	switch (ctlid) {
	case FH_IO_PROC_READ: {
		struct FH_IO_PROC_READ_T *tmp =
			(struct FH_IO_PROC_READ_T *)(arg);

		__fh_debug_proc_read(tmp->m, tmp->v, tmp->pll);
	} break;

	case FH_DCTL_CMD_DVFS:		   /* < PLL DVFS */
	case FH_DCTL_CMD_DVFS_SSC_ENABLE:  /* < PLL DVFS and enable SSC */
	case FH_DCTL_CMD_DVFS_SSC_DISABLE: /* < PLL DVFS and disable SSC */
	case FH_DCTL_CMD_SSC_ENABLE:       /* < SSC enable */
	case FH_DCTL_CMD_SSC_DISABLE:      /* < SSC disable */
	{
		fh_ioctl_dvfs_ssc(ctlid, arg);
	} break;

	default:
		FH_MSG("Unrecognized ctlid %d", ctlid);
		break;
	};
}

static struct mt_fh_hal_driver g_fh_hal_drv = {
	.fh_pll = g_fh_pll,
	.pll_cnt = FH_PLL_NUM,
	.mt_fh_hal_dumpregs_read = fh_dumpregs_proc_read,
	.mt_fh_hal_init = mt_fh_hal_init,
	.mt_fh_hal_ctrl = __freqhopping_ctrl,
	.mt_fh_lock = mt_fh_hal_lock,
	.mt_fh_unlock = mt_fh_hal_unlock,
	.mt_fh_get_init = mt_fh_hal_get_init,
	.mt_dfs_armpll = mt_fh_hal_dfs_armpll,
	.mt_fh_hal_default_conf = mt_fh_hal_default_conf,
	.mt_dfs_general_pll = mt_fh_hal_general_pll_dfs,
	.ioctl = __ioctl};

struct mt_fh_hal_driver *mt_get_fh_hal_drv(void)
{
	return &g_fh_hal_drv;
}
