/*
 * mt8518-afe-debug.c  --  Mediatek 8518 audio debugfs
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Hidalgo Huang <hidalgo.huang@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "mt8518-afe-debug.h"
#include "mt8518-reg.h"
#include "mt8518-afe-utils.h"
#include "mt8518-afe-common.h"
#include "../common/mtk-base-afe.h"
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/device.h>


#ifdef CONFIG_DEBUG_FS

struct mt8518_afe_debug_fs {
	char *fs_name;
	const struct file_operations *fops;
};

struct afe_dump_reg_attr {
	uint32_t offset;
	char *name;
};

#define DUMP_REG_ENTRY(reg) {reg, #reg}

static const struct afe_dump_reg_attr etdm_dump_regs[] = {
	DUMP_REG_ENTRY(AUDIO_TOP_CON0),
	DUMP_REG_ENTRY(AUDIO_TOP_CON4),
	DUMP_REG_ENTRY(AUDIO_TOP_CON5),
	DUMP_REG_ENTRY(ASYS_TOP_CON),
	DUMP_REG_ENTRY(AFE_DAC_CON0),
	DUMP_REG_ENTRY(ETDM_IN1_CON0),
	DUMP_REG_ENTRY(ETDM_IN1_CON1),
	DUMP_REG_ENTRY(ETDM_IN1_CON2),
	DUMP_REG_ENTRY(ETDM_IN1_CON3),
	DUMP_REG_ENTRY(ETDM_IN1_CON4),
	DUMP_REG_ENTRY(ETDM_IN2_CON0),
	DUMP_REG_ENTRY(ETDM_IN2_CON1),
	DUMP_REG_ENTRY(ETDM_IN2_CON2),
	DUMP_REG_ENTRY(ETDM_IN2_CON3),
	DUMP_REG_ENTRY(ETDM_IN2_CON4),
	DUMP_REG_ENTRY(ETDM_OUT1_CON0),
	DUMP_REG_ENTRY(ETDM_OUT1_CON1),
	DUMP_REG_ENTRY(ETDM_OUT1_CON2),
	DUMP_REG_ENTRY(ETDM_OUT1_CON3),
	DUMP_REG_ENTRY(ETDM_OUT1_CON4),
	DUMP_REG_ENTRY(ETDM_OUT2_CON0),
	DUMP_REG_ENTRY(ETDM_OUT2_CON1),
	DUMP_REG_ENTRY(ETDM_OUT2_CON2),
	DUMP_REG_ENTRY(ETDM_OUT2_CON3),
	DUMP_REG_ENTRY(ETDM_OUT2_CON4),
	DUMP_REG_ENTRY(ETDM_COWORK_CON0),
	DUMP_REG_ENTRY(ETDM_COWORK_CON1),
	DUMP_REG_ENTRY(ETDM_COWORK_CON3),
};

static const struct afe_dump_reg_attr memif_dump_regs[] = {
	DUMP_REG_ENTRY(ASYS_TOP_CON),
	DUMP_REG_ENTRY(AUDIO_TOP_CON5),
	DUMP_REG_ENTRY(AFE_DAC_CON0),
	DUMP_REG_ENTRY(AFE_DAC_CON1),
	DUMP_REG_ENTRY(AFE_DL2_BASE),
	DUMP_REG_ENTRY(AFE_DL2_CUR),
	DUMP_REG_ENTRY(AFE_DL2_END),
	DUMP_REG_ENTRY(AFE_DL2_CON0),
	DUMP_REG_ENTRY(AFE_DL3_BASE),
	DUMP_REG_ENTRY(AFE_DL3_CUR),
	DUMP_REG_ENTRY(AFE_DL3_END),
	DUMP_REG_ENTRY(AFE_DL3_CON0),
	DUMP_REG_ENTRY(AFE_DL6_BASE),
	DUMP_REG_ENTRY(AFE_DL6_CUR),
	DUMP_REG_ENTRY(AFE_DL6_END),
	DUMP_REG_ENTRY(AFE_DL6_CON0),
	DUMP_REG_ENTRY(AFE_DL7_BASE),
	DUMP_REG_ENTRY(AFE_DL7_CUR),
	DUMP_REG_ENTRY(AFE_DL7_END),
	DUMP_REG_ENTRY(AFE_DL7_CON0),
	DUMP_REG_ENTRY(AFE_DL8_BASE),
	DUMP_REG_ENTRY(AFE_DL8_CUR),
	DUMP_REG_ENTRY(AFE_DL8_END),
	DUMP_REG_ENTRY(AFE_DL8_CON0),
	DUMP_REG_ENTRY(AFE_DL10_BASE),
	DUMP_REG_ENTRY(AFE_DL10_CUR),
	DUMP_REG_ENTRY(AFE_DL10_END),
	DUMP_REG_ENTRY(AFE_DL10_CON0),
	DUMP_REG_ENTRY(AFE_UL1_BASE),
	DUMP_REG_ENTRY(AFE_UL1_CUR),
	DUMP_REG_ENTRY(AFE_UL1_END),
	DUMP_REG_ENTRY(AFE_UL1_CON0),
	DUMP_REG_ENTRY(AFE_UL2_BASE),
	DUMP_REG_ENTRY(AFE_UL2_CUR),
	DUMP_REG_ENTRY(AFE_UL2_END),
	DUMP_REG_ENTRY(AFE_UL2_CON0),
	DUMP_REG_ENTRY(AFE_UL3_BASE),
	DUMP_REG_ENTRY(AFE_UL3_CUR),
	DUMP_REG_ENTRY(AFE_UL3_END),
	DUMP_REG_ENTRY(AFE_UL3_CON0),
	DUMP_REG_ENTRY(AFE_UL4_BASE),
	DUMP_REG_ENTRY(AFE_UL4_CUR),
	DUMP_REG_ENTRY(AFE_UL4_END),
	DUMP_REG_ENTRY(AFE_UL4_CON0),
	DUMP_REG_ENTRY(AFE_UL5_BASE),
	DUMP_REG_ENTRY(AFE_UL5_CUR),
	DUMP_REG_ENTRY(AFE_UL5_END),
	DUMP_REG_ENTRY(AFE_UL5_CON0),
	DUMP_REG_ENTRY(AFE_UL8_BASE),
	DUMP_REG_ENTRY(AFE_UL8_CUR),
	DUMP_REG_ENTRY(AFE_UL8_END),
	DUMP_REG_ENTRY(AFE_UL8_CON0),
	DUMP_REG_ENTRY(AFE_UL9_BASE),
	DUMP_REG_ENTRY(AFE_UL9_CUR),
	DUMP_REG_ENTRY(AFE_UL9_END),
	DUMP_REG_ENTRY(AFE_UL9_CON0),
	DUMP_REG_ENTRY(AFE_UL10_BASE),
	DUMP_REG_ENTRY(AFE_UL10_CUR),
	DUMP_REG_ENTRY(AFE_UL10_END),
	DUMP_REG_ENTRY(AFE_UL10_CON0),
	DUMP_REG_ENTRY(AFE_MEMIF_AGENT_FS_CON0),
	DUMP_REG_ENTRY(AFE_MEMIF_AGENT_FS_CON1),
	DUMP_REG_ENTRY(AFE_MEMIF_AGENT_FS_CON2),
	DUMP_REG_ENTRY(AFE_MEMIF_AGENT_FS_CON3),
	DUMP_REG_ENTRY(AFE_I2S_UL9_REORDER),
	DUMP_REG_ENTRY(AFE_I2S_UL2_REORDER),
};

static const struct afe_dump_reg_attr irq_dump_regs[] = {
	DUMP_REG_ENTRY(ASYS_TOP_CON),
	DUMP_REG_ENTRY(AFE_IRQ1_CON),
	DUMP_REG_ENTRY(AFE_IRQ2_CON),
	DUMP_REG_ENTRY(AFE_IRQ3_CON),
	DUMP_REG_ENTRY(ASYS_IRQ1_CON),
	DUMP_REG_ENTRY(ASYS_IRQ2_CON),
	DUMP_REG_ENTRY(ASYS_IRQ3_CON),
	DUMP_REG_ENTRY(ASYS_IRQ4_CON),
	DUMP_REG_ENTRY(ASYS_IRQ5_CON),
	DUMP_REG_ENTRY(ASYS_IRQ6_CON),
	DUMP_REG_ENTRY(ASYS_IRQ7_CON),
	DUMP_REG_ENTRY(ASYS_IRQ8_CON),
	DUMP_REG_ENTRY(ASYS_IRQ9_CON),
	DUMP_REG_ENTRY(ASYS_IRQ10_CON),
	DUMP_REG_ENTRY(ASYS_IRQ11_CON),
	DUMP_REG_ENTRY(ASYS_IRQ_CLR),
	DUMP_REG_ENTRY(ASYS_IRQ_STATUS),
	DUMP_REG_ENTRY(AFE_IRQ_MCU_CLR),
	DUMP_REG_ENTRY(AFE_IRQ_STATUS),
	DUMP_REG_ENTRY(AFE_IRQ_MASK),
	DUMP_REG_ENTRY(ASYS_IRQ_MASK),
};

static const struct afe_dump_reg_attr conn_dump_regs[] = {
	DUMP_REG_ENTRY(AFE_CONN0),
	DUMP_REG_ENTRY(AFE_CONN1),
	DUMP_REG_ENTRY(AFE_CONN2),
	DUMP_REG_ENTRY(AFE_CONN3),
	DUMP_REG_ENTRY(AFE_CONN4),
	DUMP_REG_ENTRY(AFE_CONN5),
	DUMP_REG_ENTRY(AFE_CONN6),
	DUMP_REG_ENTRY(AFE_CONN7),
	DUMP_REG_ENTRY(AFE_CONN8),
	DUMP_REG_ENTRY(AFE_CONN9),
	DUMP_REG_ENTRY(AFE_CONN10),
	DUMP_REG_ENTRY(AFE_CONN11),
	DUMP_REG_ENTRY(AFE_CONN12),
	DUMP_REG_ENTRY(AFE_CONN13),
	DUMP_REG_ENTRY(AFE_CONN14),
	DUMP_REG_ENTRY(AFE_CONN15),
	DUMP_REG_ENTRY(AFE_CONN16),
	DUMP_REG_ENTRY(AFE_CONN17),
	DUMP_REG_ENTRY(AFE_CONN18),
	DUMP_REG_ENTRY(AFE_CONN19),
	DUMP_REG_ENTRY(AFE_CONN20),
	DUMP_REG_ENTRY(AFE_CONN21),
	DUMP_REG_ENTRY(AFE_CONN26),
	DUMP_REG_ENTRY(AFE_CONN27),
	DUMP_REG_ENTRY(AFE_CONN28),
	DUMP_REG_ENTRY(AFE_CONN29),
	DUMP_REG_ENTRY(AFE_CONN30),
	DUMP_REG_ENTRY(AFE_CONN31),
	DUMP_REG_ENTRY(AFE_CONN32),
	DUMP_REG_ENTRY(AFE_CONN33),
	DUMP_REG_ENTRY(AFE_CONN34),
	DUMP_REG_ENTRY(AFE_CONN35),
	DUMP_REG_ENTRY(AFE_CONN36),
	DUMP_REG_ENTRY(AFE_CONN37),
	DUMP_REG_ENTRY(AFE_CONN38),
	DUMP_REG_ENTRY(AFE_CONN39),
	DUMP_REG_ENTRY(AFE_CONN40),
	DUMP_REG_ENTRY(AFE_CONN41),
	DUMP_REG_ENTRY(AFE_CONN42),
	DUMP_REG_ENTRY(AFE_CONN43),
	DUMP_REG_ENTRY(AFE_CONN44),
	DUMP_REG_ENTRY(AFE_CONN45),
	DUMP_REG_ENTRY(AFE_CONN46),
	DUMP_REG_ENTRY(AFE_CONN47),
	DUMP_REG_ENTRY(AFE_CONN48),
	DUMP_REG_ENTRY(AFE_CONN49),
	DUMP_REG_ENTRY(AFE_CONN53),
	DUMP_REG_ENTRY(AFE_CONN54),
	DUMP_REG_ENTRY(AFE_CONN55),
	DUMP_REG_ENTRY(AFE_CONN56),
	DUMP_REG_ENTRY(AFE_CONN57),
	DUMP_REG_ENTRY(AFE_CONN58),
	DUMP_REG_ENTRY(AFE_CONN59),
	DUMP_REG_ENTRY(AFE_CONN60),
	DUMP_REG_ENTRY(AFE_CONN61),
	DUMP_REG_ENTRY(AFE_CONN64),
	DUMP_REG_ENTRY(AFE_CONN65),
	DUMP_REG_ENTRY(AFE_CONN66),
	DUMP_REG_ENTRY(AFE_CONN67),
	DUMP_REG_ENTRY(AFE_CONN68),
	DUMP_REG_ENTRY(AFE_CONN69),
	DUMP_REG_ENTRY(AFE_CONN70),
	DUMP_REG_ENTRY(AFE_CONN71),
	DUMP_REG_ENTRY(AFE_CONN72),
	DUMP_REG_ENTRY(AFE_CONN73),
	DUMP_REG_ENTRY(AFE_CONN74),
	DUMP_REG_ENTRY(AFE_CONN75),
	DUMP_REG_ENTRY(AFE_CONN76),
	DUMP_REG_ENTRY(AFE_CONN_16BIT),
	DUMP_REG_ENTRY(AFE_CONN_24BIT),
	DUMP_REG_ENTRY(AFE_TDMOUT_CONN0),
	DUMP_REG_ENTRY(AFE_TDMOUT_CONN1),
	DUMP_REG_ENTRY(AFE_TDMOUT_CONN2),
};

static const struct afe_dump_reg_attr adda_dump_regs[] = {
	DUMP_REG_ENTRY(AUDIO_TOP_CON0),
	DUMP_REG_ENTRY(AUDIO_TOP_CON2),
	DUMP_REG_ENTRY(AUDIO_TOP_CON3),
	DUMP_REG_ENTRY(AUDIO_TOP_CON4),
	DUMP_REG_ENTRY(ASMO_TIMING_CON0),
	DUMP_REG_ENTRY(AFE_ASRCO5_NEW_CON0),
	DUMP_REG_ENTRY(AFE_ASRCO5_NEW_CON1),
	DUMP_REG_ENTRY(AFE_ASRCO5_NEW_CON4),
	DUMP_REG_ENTRY(AFE_ASRCO5_NEW_CON6),
	DUMP_REG_ENTRY(AFE_ASRCO5_NEW_CON7),
	DUMP_REG_ENTRY(AFE_ASRCO5_NEW_CON8),
	DUMP_REG_ENTRY(AFE_ASRCO5_NEW_CON9),
	DUMP_REG_ENTRY(DMIC_TOP_CON),
	DUMP_REG_ENTRY(DMIC2_TOP_CON),
	DUMP_REG_ENTRY(DMIC3_TOP_CON),
	DUMP_REG_ENTRY(DMIC4_TOP_CON),
	DUMP_REG_ENTRY(PWR2_TOP_CON),
	DUMP_REG_ENTRY(DMIC_IIR_ULCF_COEF_CON1),
	DUMP_REG_ENTRY(DMIC_IIR_ULCF_COEF_CON2),
	DUMP_REG_ENTRY(DMIC_IIR_ULCF_COEF_CON3),
	DUMP_REG_ENTRY(DMIC_IIR_ULCF_COEF_CON4),
	DUMP_REG_ENTRY(DMIC_IIR_ULCF_COEF_CON5),
	DUMP_REG_ENTRY(DMIC2_IIR_ULCF_COEF_CON1),
	DUMP_REG_ENTRY(DMIC2_IIR_ULCF_COEF_CON2),
	DUMP_REG_ENTRY(DMIC2_IIR_ULCF_COEF_CON3),
	DUMP_REG_ENTRY(DMIC2_IIR_ULCF_COEF_CON4),
	DUMP_REG_ENTRY(DMIC2_IIR_ULCF_COEF_CON5),
	DUMP_REG_ENTRY(DMIC3_IIR_ULCF_COEF_CON1),
	DUMP_REG_ENTRY(DMIC3_IIR_ULCF_COEF_CON2),
	DUMP_REG_ENTRY(DMIC3_IIR_ULCF_COEF_CON3),
	DUMP_REG_ENTRY(DMIC3_IIR_ULCF_COEF_CON4),
	DUMP_REG_ENTRY(DMIC3_IIR_ULCF_COEF_CON5),
	DUMP_REG_ENTRY(DMIC4_IIR_ULCF_COEF_CON1),
	DUMP_REG_ENTRY(DMIC4_IIR_ULCF_COEF_CON2),
	DUMP_REG_ENTRY(DMIC4_IIR_ULCF_COEF_CON3),
	DUMP_REG_ENTRY(DMIC4_IIR_ULCF_COEF_CON4),
	DUMP_REG_ENTRY(DMIC4_IIR_ULCF_COEF_CON5),
};

static const struct afe_dump_reg_attr gasrc_dump_regs[] = {
	DUMP_REG_ENTRY(AUDIO_TOP_CON0),
	DUMP_REG_ENTRY(AUDIO_TOP_CON4),
	DUMP_REG_ENTRY(ASMO_TIMING_CON0),
	DUMP_REG_ENTRY(PWR1_ASM_CON1),
	DUMP_REG_ENTRY(GASRC_CFG0),
	DUMP_REG_ENTRY(GASRC_TIMING_CON0),
	DUMP_REG_ENTRY(GASRC_TIMING_CON1),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON0),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON1),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON2),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON3),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON4),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON6),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON7),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON8),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON9),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON10),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON11),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON13),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON14),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON0),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON1),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON2),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON3),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON4),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON6),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON7),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON8),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON9),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON10),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON11),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON13),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON14),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON0),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON1),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON2),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON3),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON4),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON6),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON7),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON8),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON9),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON10),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON11),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON13),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON14),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON0),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON1),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON2),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON3),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON4),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON6),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON7),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON8),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON9),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON10),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON11),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON13),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON14),
};

static const struct afe_dump_reg_attr spdif_dump_regs[] = {
	DUMP_REG_ENTRY(AUDIO_TOP_CON0),
	DUMP_REG_ENTRY(AUDIO_TOP_CON2),
	DUMP_REG_ENTRY(AUDIO_TOP_CON4),
	DUMP_REG_ENTRY(AFE_SPDIF_OUT_CON0),
	DUMP_REG_ENTRY(AFE_IEC_CFG),
	DUMP_REG_ENTRY(AFE_IEC_NSNUM),
	DUMP_REG_ENTRY(AFE_IEC_BURST_INFO),
	DUMP_REG_ENTRY(AFE_IEC_BURST_LEN),
	DUMP_REG_ENTRY(AFE_IEC_NSADR),
	DUMP_REG_ENTRY(AFE_IEC_CHL_STAT0),
	DUMP_REG_ENTRY(AFE_IEC_CHL_STAT1),
	DUMP_REG_ENTRY(AFE_IEC_CHR_STAT0),
	DUMP_REG_ENTRY(AFE_IEC_CHR_STAT1),
	DUMP_REG_ENTRY(AFE_SPDIFIN_CFG0),
	DUMP_REG_ENTRY(AFE_SPDIFIN_CFG1),
	DUMP_REG_ENTRY(AFE_SPDIFIN_CHSTS1),
	DUMP_REG_ENTRY(AFE_SPDIFIN_CHSTS2),
	DUMP_REG_ENTRY(AFE_SPDIFIN_CHSTS3),
	DUMP_REG_ENTRY(AFE_SPDIFIN_CHSTS4),
	DUMP_REG_ENTRY(AFE_SPDIFIN_CHSTS5),
	DUMP_REG_ENTRY(AFE_SPDIFIN_CHSTS6),
	DUMP_REG_ENTRY(AFE_SPDIFIN_DEBUG1),
	DUMP_REG_ENTRY(AFE_SPDIFIN_DEBUG2),
	DUMP_REG_ENTRY(AFE_SPDIFIN_DEBUG3),
	DUMP_REG_ENTRY(AFE_SPDIFIN_DEBUG4),
	DUMP_REG_ENTRY(AFE_SPDIFIN_EC),
	DUMP_REG_ENTRY(AFE_SPDIFIN_CKLOCK_CFG),
	DUMP_REG_ENTRY(AFE_SPDIFIN_BR),
	DUMP_REG_ENTRY(AFE_SPDIFIN_BR_DBG1),
	DUMP_REG_ENTRY(AFE_SPDIFIN_CKFBDIV),
	DUMP_REG_ENTRY(AFE_SPDIFIN_INT_EXT),
	DUMP_REG_ENTRY(AFE_SPDIFIN_INT_EXT2),
	DUMP_REG_ENTRY(SPDIFIN_FREQ_INFO),
	DUMP_REG_ENTRY(SPDIFIN_FREQ_INFO_2),
	DUMP_REG_ENTRY(SPDIFIN_FREQ_INFO_3),
	DUMP_REG_ENTRY(SPDIFIN_FREQ_STATUS),
	DUMP_REG_ENTRY(AFE_MPHONE_MULTI_CON0),
	DUMP_REG_ENTRY(AFE_MPHONE_MULTI_CON1),
	DUMP_REG_ENTRY(AFE_MPHONE_MULTI_MON),
	DUMP_REG_ENTRY(SPDIFIN_USERCODE1),
	DUMP_REG_ENTRY(SPDIFIN_USERCODE2),
	DUMP_REG_ENTRY(SPDIFIN_USERCODE3),
	DUMP_REG_ENTRY(SPDIFIN_USERCODE4),
	DUMP_REG_ENTRY(SPDIFIN_USERCODE5),
	DUMP_REG_ENTRY(SPDIFIN_USERCODE6),
	DUMP_REG_ENTRY(SPDIFIN_USERCODE7),
	DUMP_REG_ENTRY(SPDIFIN_USERCODE8),
	DUMP_REG_ENTRY(SPDIFIN_USERCODE9),
	DUMP_REG_ENTRY(SPDIFIN_USERCODE10),
	DUMP_REG_ENTRY(SPDIFIN_USERCODE11),
	DUMP_REG_ENTRY(SPDIFIN_USERCODE12),
};

static const struct afe_dump_reg_attr dbg_dump_regs[] = {
	DUMP_REG_ENTRY(AFE_SINEGEN_CON0),
	DUMP_REG_ENTRY(AFE_SINEGEN_CON1),
	DUMP_REG_ENTRY(AFE_SINEGEN_CON2),
	DUMP_REG_ENTRY(AFE_SINEGEN_CON3),
	DUMP_REG_ENTRY(AFE_BUS_MON1),
	DUMP_REG_ENTRY(ETDM_IN1_MONITOR),
	DUMP_REG_ENTRY(ETDM_IN2_MONITOR),
	DUMP_REG_ENTRY(ETDM_OUT1_MONITOR),
	DUMP_REG_ENTRY(ETDM_OUT2_MONITOR),
	DUMP_REG_ENTRY(PCM_INTF_CON1),
	DUMP_REG_ENTRY(PCM_INTF_CON2),
	DUMP_REG_ENTRY(AFE_LOOPBACK_CFG0),
	DUMP_REG_ENTRY(AFE_UL1_CHK_SUM1),
	DUMP_REG_ENTRY(AFE_UL1_CHK_SUM2),
	DUMP_REG_ENTRY(AFE_UL2_CHK_SUM1),
	DUMP_REG_ENTRY(AFE_UL2_CHK_SUM2),
	DUMP_REG_ENTRY(AFE_UL3_CHK_SUM1),
	DUMP_REG_ENTRY(AFE_UL3_CHK_SUM2),
	DUMP_REG_ENTRY(AFE_UL4_CHK_SUM1),
	DUMP_REG_ENTRY(AFE_UL4_CHK_SUM2),
	DUMP_REG_ENTRY(AFE_UL5_CHK_SUM1),
	DUMP_REG_ENTRY(AFE_UL5_CHK_SUM2),
	DUMP_REG_ENTRY(AFE_UL8_CHK_SUM1),
	DUMP_REG_ENTRY(AFE_UL8_CHK_SUM2),
	DUMP_REG_ENTRY(AFE_UL9_CHK_SUM1),
	DUMP_REG_ENTRY(AFE_UL9_CHK_SUM2),
	DUMP_REG_ENTRY(AFE_DL2_CHK_SUM1),
	DUMP_REG_ENTRY(AFE_DL2_CHK_SUM2),
	DUMP_REG_ENTRY(AFE_DL3_CHK_SUM1),
	DUMP_REG_ENTRY(AFE_DL3_CHK_SUM2),
	DUMP_REG_ENTRY(AFE_DL6_CHK_SUM1),
	DUMP_REG_ENTRY(AFE_DL6_CHK_SUM2),
	DUMP_REG_ENTRY(AFE_DL7_CHK_SUM1),
	DUMP_REG_ENTRY(AFE_DL7_CHK_SUM2),
	DUMP_REG_ENTRY(AFE_DL8_CHK_SUM1),
	DUMP_REG_ENTRY(AFE_DL8_CHK_SUM2),
	DUMP_REG_ENTRY(AFE_DL8_CHK_SUM3),
	DUMP_REG_ENTRY(AFE_DL8_CHK_SUM4),
	DUMP_REG_ENTRY(AFE_DL10_CHK_SUM1),
	DUMP_REG_ENTRY(AFE_DL10_CHK_SUM2),
	DUMP_REG_ENTRY(AFE_DL10_CHK_SUM3),
	DUMP_REG_ENTRY(AFE_DL10_CHK_SUM4),
	DUMP_REG_ENTRY(AFE_DL10_CHK_SUM5),
	DUMP_REG_ENTRY(AFE_DL10_CHK_SUM6),
};

static ssize_t mt8518_afe_dump_registers(char __user *user_buf,
					 size_t count,
					 loff_t *pos,
					 struct mtk_base_afe *afe,
					 const struct afe_dump_reg_attr *regs,
					 size_t regs_len)
{
	ssize_t ret, i;
	char *buf;
	unsigned int reg_value;
	int n = 0;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	mt8518_afe_enable_main_clk(afe);

	for (i = 0; i < regs_len; i++) {
		if (regmap_read(afe->regmap, regs[i].offset, &reg_value))
			n += scnprintf(buf + n, count - n, "%s = N/A\n",
				       regs[i].name);
		else
			n += scnprintf(buf + n, count - n, "%s = 0x%x\n",
				       regs[i].name, reg_value);
	}

	mt8518_afe_disable_main_clk(afe);

	ret = simple_read_from_buffer(user_buf, count, pos, buf, n);
	kfree(buf);

	return ret;
}

static ssize_t mt8518_afe_etdm_read_file(struct file *file,
				    char __user *user_buf,
				    size_t count,
				    loff_t *pos)
{
	struct mtk_base_afe *afe = file->private_data;
	ssize_t ret;

	if (*pos < 0 || !count)
		return -EINVAL;

	ret = mt8518_afe_dump_registers(user_buf, count, pos, afe,
					etdm_dump_regs,
					ARRAY_SIZE(etdm_dump_regs));

	return ret;
}

static ssize_t mt8518_afe_write_file(struct file *file,
				     const char __user *user_buf,
				     size_t count,
				     loff_t *pos)
{
	char buf[64];
	size_t buf_size;
	char *start = buf;
	char *reg_str;
	char *value_str;
	const char delim[] = " ,";
	unsigned long reg, value;
	struct mtk_base_afe *afe = file->private_data;

	buf_size = min(count, (sizeof(buf) - 1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	buf[buf_size] = 0;

	reg_str = strsep(&start, delim);
	if (!reg_str || !strlen(reg_str))
		return -EINVAL;

	value_str = strsep(&start, delim);
	if (!value_str || !strlen(value_str))
		return -EINVAL;

	if (kstrtoul(reg_str, 16, &reg))
		return -EINVAL;

	if (kstrtoul(value_str, 16, &value))
		return -EINVAL;

	mt8518_afe_enable_main_clk(afe);

	regmap_write(afe->regmap, reg, value);

	mt8518_afe_disable_main_clk(afe);

	return buf_size;
}

static ssize_t mt8518_afe_memif_read_file(struct file *file,
					  char __user *user_buf,
					  size_t count,
					  loff_t *pos)
{
	struct mtk_base_afe *afe = file->private_data;
	ssize_t ret;

	if (*pos < 0 || !count)
		return -EINVAL;

	ret = mt8518_afe_dump_registers(user_buf, count, pos, afe,
					memif_dump_regs,
					ARRAY_SIZE(memif_dump_regs));

	return ret;
}

static ssize_t mt8518_afe_irq_read_file(struct file *file,
					char __user *user_buf,
					size_t count,
					loff_t *pos)
{
	struct mtk_base_afe *afe = file->private_data;
	ssize_t ret;

	if (*pos < 0 || !count)
		return -EINVAL;

	ret = mt8518_afe_dump_registers(user_buf, count, pos, afe,
					irq_dump_regs,
					ARRAY_SIZE(irq_dump_regs));

	return ret;
}

static ssize_t mt8518_afe_conn_read_file(struct file *file,
					 char __user *user_buf,
					 size_t count,
					 loff_t *pos)
{
	struct mtk_base_afe *afe = file->private_data;
	ssize_t ret;

	if (*pos < 0 || !count)
		return -EINVAL;

	ret = mt8518_afe_dump_registers(user_buf, count, pos, afe,
					conn_dump_regs,
					ARRAY_SIZE(conn_dump_regs));

	return ret;
}

static ssize_t mt8518_afe_adda_read_file(struct file *file,
					 char __user *user_buf,
					 size_t count,
					 loff_t *pos)
{
	struct mtk_base_afe *afe = file->private_data;
	ssize_t ret;

	if (*pos < 0 || !count)
		return -EINVAL;

	ret = mt8518_afe_dump_registers(user_buf, count, pos, afe,
					adda_dump_regs,
					ARRAY_SIZE(adda_dump_regs));

	return ret;
}

static ssize_t mt8518_afe_gasrc_read_file(struct file *file,
					  char __user *user_buf,
					  size_t count,
					  loff_t *pos)
{
	struct mtk_base_afe *afe = file->private_data;
	ssize_t ret;

	if (*pos < 0 || !count)
		return -EINVAL;

	ret = mt8518_afe_dump_registers(user_buf, count, pos, afe,
					gasrc_dump_regs,
					ARRAY_SIZE(gasrc_dump_regs));

	return ret;
}

static ssize_t mt8518_afe_spdif_read_file(struct file *file,
					  char __user *user_buf,
					  size_t count,
					  loff_t *pos)
{
	struct mtk_base_afe *afe = file->private_data;
	ssize_t ret;

	if (*pos < 0 || !count)
		return -EINVAL;

	ret = mt8518_afe_dump_registers(user_buf, count, pos, afe,
					spdif_dump_regs,
					ARRAY_SIZE(spdif_dump_regs));

	return ret;
}

static ssize_t mt8518_afe_dbg_read_file(struct file *file,
					char __user *user_buf,
					size_t count,
					loff_t *pos)
{
	struct mtk_base_afe *afe = file->private_data;
	ssize_t ret;

	if (*pos < 0 || !count)
		return -EINVAL;

	ret = mt8518_afe_dump_registers(user_buf, count, pos, afe,
					dbg_dump_regs,
					ARRAY_SIZE(dbg_dump_regs));

	return ret;
}

static const struct file_operations mt8518_afe_etdm_fops = {
	.open = simple_open,
	.read = mt8518_afe_etdm_read_file,
	.write = mt8518_afe_write_file,
	.llseek = default_llseek,
};

static const struct file_operations mt8518_afe_memif_fops = {
	.open = simple_open,
	.read = mt8518_afe_memif_read_file,
	.write = mt8518_afe_write_file,
	.llseek = default_llseek,
};

static const struct file_operations mt8518_afe_irq_fops = {
	.open = simple_open,
	.read = mt8518_afe_irq_read_file,
	.write = mt8518_afe_write_file,
	.llseek = default_llseek,
};

static const struct file_operations mt8518_afe_conn_fops = {
	.open = simple_open,
	.read = mt8518_afe_conn_read_file,
	.write = mt8518_afe_write_file,
	.llseek = default_llseek,
};

static const struct file_operations mt8518_afe_adda_fops = {
	.open = simple_open,
	.read = mt8518_afe_adda_read_file,
	.write = mt8518_afe_write_file,
	.llseek = default_llseek,
};

static const struct file_operations mt8518_afe_gasrc_fops = {
	.open = simple_open,
	.read = mt8518_afe_gasrc_read_file,
	.write = mt8518_afe_write_file,
	.llseek = default_llseek,
};

static const struct file_operations mt8518_afe_spdif_fops = {
	.open = simple_open,
	.read = mt8518_afe_spdif_read_file,
	.write = mt8518_afe_write_file,
	.llseek = default_llseek,
};

static const struct file_operations mt8518_afe_dbg_fops = {
	.open = simple_open,
	.read = mt8518_afe_dbg_read_file,
	.write = mt8518_afe_write_file,
	.llseek = default_llseek,
};

static const
struct mt8518_afe_debug_fs afe_debug_fs[MT8518_AFE_DEBUGFS_NUM] = {
	{"mtksocaudioetdm", &mt8518_afe_etdm_fops},
	{"mtksocaudiomemif", &mt8518_afe_memif_fops},
	{"mtksocaudioirq", &mt8518_afe_irq_fops},
	{"mtksocaudioconn", &mt8518_afe_conn_fops},
	{"mtksocaudioadda", &mt8518_afe_adda_fops},
	{"mtksocaudiogasrc", &mt8518_afe_gasrc_fops},
	{"mtksocaudiospdif", &mt8518_afe_spdif_fops},
	{"mtksocaudiodbg", &mt8518_afe_dbg_fops},
};

#endif

void mt8518_afe_init_debugfs(struct mtk_base_afe *afe)
{
#ifdef CONFIG_DEBUG_FS
	struct mt8518_afe_private *afe_priv = afe->platform_priv;
	int i;

	for (i = 0; i < ARRAY_SIZE(afe_debug_fs); i++) {
		afe_priv->debugfs_dentry[i] =
			debugfs_create_file(afe_debug_fs[i].fs_name,
				0644, NULL, afe, afe_debug_fs[i].fops);
		if (!afe_priv->debugfs_dentry[i])
			dev_warn(afe->dev, "%s create %s debugfs failed\n",
				 __func__, afe_debug_fs[i].fs_name);
	}
#endif
}

void mt8518_afe_cleanup_debugfs(struct mtk_base_afe *afe)
{
#ifdef CONFIG_DEBUG_FS
	struct mt8518_afe_private *afe_priv = afe->platform_priv;
	int i;

	if (!afe_priv)
		return;

	for (i = 0; i < MT8518_AFE_DEBUGFS_NUM; i++)
		debugfs_remove(afe_priv->debugfs_dentry[i]);
#endif
}
