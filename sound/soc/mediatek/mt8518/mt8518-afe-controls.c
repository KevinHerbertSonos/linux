/*
 * mt8518-afe-controls.c  --  Mediatek 8518 audio controls
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

#include "mt8518-afe-controls.h"
#include "mt8518-afe-common.h"
#include "mt8518-reg.h"
#include "mt8518-afe-utils.h"
#include "../common/mtk-base-afe.h"
#include <sound/soc.h>
#include <linux/kernel.h>

#define ENUM_TO_STR(enum) #enum

#define MT8518_APLL_ADJUST_RANGE 163840

enum mixer_control_func {
	CTRL_I18_I19_SRC = 0,
	CTRL_I10_I11_SRC,
	CTRL_SINEGEN_LOOPBACK_MODE,
	CTRL_SINEGEN_TIMING,
	CTRL_ETDM_IN_DATA_SRC,
	CTRL_ETDM_IN_SLAVE_MODE_SRC,
	CTRL_TDMOUT_CONN_SRC,
	CTRL_MULTI_IN_BCK_SEL,
	CTRL_MULTI_IN_LRCK_SEL,
	CTRL_MULTI_IN_SDATA0_SEL,
	CTRL_MULTI_IN_SDATA1_SEL,
	CTRL_MULTI_IN_SDATA2_SEL,
	CTRL_MULTI_IN_SDATA3_SEL,
	CTRL_SPDIF_IN_PORT_SEL,
};

enum i18_i19_src {
	I18_I19_FROM_ETDM_IN2 = 0,
	I18_I19_FROM_AMIC,
	I18_I19_FROM_AMIC_FIFO,
};

enum i10_i11_src {
	I10_I11_FROM_ETDM_IN1 = 0,
	I10_I11_FROM_AMIC,
	I10_I11_FROM_AMIC_FIFO,
	I10_I11_FROM_DMIC,
};

enum sinegen_loopback_mode {
	SINEGEN_I00_I01 = 0,
	SINEGEN_I02_I03,
	SINEGEN_I04_I11,
	SINEGEN_I12_I19,
	SINEGEN_I20_I21,
	SINEGEN_I22_I29,
	SINEGEN_I32_I33,
	SINEGEN_I34_I35,
	SINEGEN_I36_I37,
	SINEGEN_I38_I39,
	SINEGEN_I40_I41,
	SINEGEN_I42_I47,
	SINEGEN_O00_O01,
	SINEGEN_O02_O03,
	SINEGEN_O04_O11,
	SINEGEN_O12_O13,
	SINEGEN_O14_O15,
	SINEGEN_O16_O17,
	SINEGEN_O20_O21,
	SINEGEN_O42_O43,
	SINEGEN_O44_O45,
	SINEGEN_O46_O47,
	SINEGEN_O48_O49,
	SINEGEN_O26_O41,
	SINEGEN_UL9,
	SINEGEN_UL2,
	SINEGEN_NONE,
};

enum sinegen_timing {
	SINEGEN_8K = 0,
	SINEGEN_12K,
	SINEGEN_16K,
	SINEGEN_24K,
	SINEGEN_32K,
	SINEGEN_48K,
	SINEGEN_96K,
	SINEGEN_192K,
	SINEGEN_384K,
	SINEGEN_7D35K,
	SINEGEN_11D025K,
	SINEGEN_14D7K,
	SINEGEN_22D05K,
	SINEGEN_29D4K,
	SINEGEN_44D1K,
	SINEGEN_88D2K,
	SINEGEN_176D4K,
	SINEGEN_352D8K,
	SINEGEN_DL_1X_EN,
	SINEGEN_SGEN_EN,
};

enum etdm_in_data_src {
	ETDM_IN_FROM_PAD = 0,
	ETDM_IN_FROM_ETDM_OUT1,
	ETDM_IN_FROM_ETDM_OUT2,
};

enum etdm_in_slave_mode_src {
	ETDM_IN_SLAVE_FROM_SELF = 0,
	ETDM_IN_SLAVE_FROM_ETDM_OUT1,
	ETDM_IN_SLAVE_FROM_ETDM_OUT2,
};

enum tdmout_conn_src {
	I00 = 0,
	I01,
	I02,
	I03,
	I04,
	I05,
	I06,
	I07,
	I08,
	I09,
	I10,
	I11,
	I12,
	I13,
	I14,
	I15,
};

static const char *const i18_i19_src_func[] = {
	ENUM_TO_STR(I18_I19_FROM_ETDM_IN2),
	ENUM_TO_STR(I18_I19_FROM_AMIC),
	ENUM_TO_STR(I18_I19_FROM_AMIC_FIFO),
};

static const char *const i10_i11_src_func[] = {
	ENUM_TO_STR(I10_I11_FROM_ETDM_IN1),
	ENUM_TO_STR(I10_I11_FROM_AMIC),
	ENUM_TO_STR(I10_I11_FROM_AMIC_FIFO),
	ENUM_TO_STR(I10_I11_FROM_DMIC),
};

static const char *const sinegen_loopback_mode_func[] = {
	ENUM_TO_STR(SINEGEN_I00_I01),
	ENUM_TO_STR(SINEGEN_I02_I03),
	ENUM_TO_STR(SINEGEN_I04_I11),
	ENUM_TO_STR(SINEGEN_I12_I19),
	ENUM_TO_STR(SINEGEN_I20_I21),
	ENUM_TO_STR(SINEGEN_I22_I29),
	ENUM_TO_STR(SINEGEN_I32_I33),
	ENUM_TO_STR(SINEGEN_I34_I35),
	ENUM_TO_STR(SINEGEN_I36_I37),
	ENUM_TO_STR(SINEGEN_I38_I39),
	ENUM_TO_STR(SINEGEN_I40_I41),
	ENUM_TO_STR(SINEGEN_I42_I47),
	ENUM_TO_STR(SINEGEN_O00_O01),
	ENUM_TO_STR(SINEGEN_O02_O03),
	ENUM_TO_STR(SINEGEN_O04_O11),
	ENUM_TO_STR(SINEGEN_O12_O13),
	ENUM_TO_STR(SINEGEN_O14_O15),
	ENUM_TO_STR(SINEGEN_O16_O17),
	ENUM_TO_STR(SINEGEN_O20_O21),
	ENUM_TO_STR(SINEGEN_O42_O43),
	ENUM_TO_STR(SINEGEN_O44_O45),
	ENUM_TO_STR(SINEGEN_O46_O47),
	ENUM_TO_STR(SINEGEN_O48_O49),
	ENUM_TO_STR(SINEGEN_O26_O41),
	ENUM_TO_STR(SINEGEN_UL9),
	ENUM_TO_STR(SINEGEN_UL2),
	ENUM_TO_STR(SINEGEN_NONE),
};

static const char *const sinegen_timing_func[] = {
	ENUM_TO_STR(SINEGEN_8K),
	ENUM_TO_STR(SINEGEN_12K),
	ENUM_TO_STR(SINEGEN_16K),
	ENUM_TO_STR(SINEGEN_24K),
	ENUM_TO_STR(SINEGEN_32K),
	ENUM_TO_STR(SINEGEN_48K),
	ENUM_TO_STR(SINEGEN_96K),
	ENUM_TO_STR(SINEGEN_192K),
	ENUM_TO_STR(SINEGEN_384K),
	ENUM_TO_STR(SINEGEN_7D35K),
	ENUM_TO_STR(SINEGEN_11D025K),
	ENUM_TO_STR(SINEGEN_14D7K),
	ENUM_TO_STR(SINEGEN_22D05K),
	ENUM_TO_STR(SINEGEN_29D4K),
	ENUM_TO_STR(SINEGEN_44D1K),
	ENUM_TO_STR(SINEGEN_88D2K),
	ENUM_TO_STR(SINEGEN_176D4K),
	ENUM_TO_STR(SINEGEN_352D8K),
	ENUM_TO_STR(SINEGEN_DL_1X_EN),
	ENUM_TO_STR(SINEGEN_SGEN_EN),
};

static const char *const etdm_in_data_src_func[] = {
	ENUM_TO_STR(ETDM_IN_FROM_PAD),
	ENUM_TO_STR(ETDM_IN_FROM_ETDM_OUT1),
	ENUM_TO_STR(ETDM_IN_FROM_ETDM_OUT2),
};

static const char *const etdm_in_slave_mode_src_func[] = {
	ENUM_TO_STR(ETDM_IN_SLAVE_FROM_SELF),
	ENUM_TO_STR(ETDM_IN_SLAVE_FROM_ETDM_OUT1),
	ENUM_TO_STR(ETDM_IN_SLAVE_FROM_ETDM_OUT2),
};

static const char *const tdmout_conn_src_func[] = {
	ENUM_TO_STR(I00),
	ENUM_TO_STR(I01),
	ENUM_TO_STR(I02),
	ENUM_TO_STR(I03),
	ENUM_TO_STR(I04),
	ENUM_TO_STR(I05),
	ENUM_TO_STR(I06),
	ENUM_TO_STR(I07),
	ENUM_TO_STR(I08),
	ENUM_TO_STR(I09),
	ENUM_TO_STR(I10),
	ENUM_TO_STR(I11),
	ENUM_TO_STR(I12),
	ENUM_TO_STR(I13),
	ENUM_TO_STR(I14),
	ENUM_TO_STR(I15),
};

static const char *const multi_in_bck_sel_func[] = {
	"SPLIN_BCK",
	"I2S_IN_SLAVE_BCK",
	"I2S_OUT_SLAVE_BCK",
	"TDM_IN_SLAVE_BCK",
	"TDM_OUT_SLAVE_BCK",
	"I2S_IN_MASTER_BCK",
	"I2S_OUT_MASTER_BCK",
	"TDM_IN_MASTER_BCK",
	"TDM_OUT_MASTER_BCK",
	"NO_BCK",
	"NO_BCK",
	"NO_BCK",
	"NO_BCK",
	"NO_BCK",
	"NO_BCK",
	"NO_BCK",
};

static const char *const multi_in_lrck_sel_func[] = {
	"SPLIN_LRCK",
	"I2S_IN_SLAVE_LRCK",
	"I2S_OUT_SLAVE_LRCK",
	"TDM_IN_SLAVE_LRCK",
	"TDM_OUT_SLAVE_LRCK",
	"I2S_IN_MASTER_LRCK",
	"I2S_OUT_MASTER_LRCK",
	"TDM_IN_MASTER_LRCK",
	"TDM_OUT_MASTER_LRCK",
	"NO_LRCK",
	"NO_LRCK",
	"NO_LRCK",
	"NO_LRCK",
	"NO_LRCK",
	"NO_LRCK",
	"NO_LRCK",
};

static const char *const multi_in_sdata_sel_func[] = {
	"SPLIN_SDATA",
	"I2S_OUT_DATA",
	"NO_DATA",
	"I2S_IN_DATA",
};

static const char *const spdif_in_port_sel_func[] = {
	ENUM_TO_STR(SPDIF_IN_PORT_NONE),
	ENUM_TO_STR(SPDIF_IN_PORT_OPT),
	ENUM_TO_STR(SPDIF_IN_PORT_COAXIAL),
	ENUM_TO_STR(SPDIF_IN_PORT_ARC),
};

static const struct soc_enum mt8518_afe_soc_enums[] = {
	[CTRL_I18_I19_SRC] =
		SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(i18_i19_src_func),
				    i18_i19_src_func),
	[CTRL_I10_I11_SRC] =
		SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(i10_i11_src_func),
				    i10_i11_src_func),
	[CTRL_SINEGEN_LOOPBACK_MODE] =
		SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sinegen_loopback_mode_func),
				    sinegen_loopback_mode_func),
	[CTRL_SINEGEN_TIMING] =
		SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sinegen_timing_func),
				    sinegen_timing_func),
	[CTRL_ETDM_IN_DATA_SRC] =
		SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(etdm_in_data_src_func),
				    etdm_in_data_src_func),
	[CTRL_ETDM_IN_SLAVE_MODE_SRC] =
		SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(etdm_in_slave_mode_src_func),
				    etdm_in_slave_mode_src_func),
	[CTRL_TDMOUT_CONN_SRC] =
		SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tdmout_conn_src_func),
				    tdmout_conn_src_func),
	[CTRL_MULTI_IN_BCK_SEL] =
		SOC_ENUM_SINGLE(AFE_LOOPBACK_CFG0, 0,
			ARRAY_SIZE(multi_in_bck_sel_func),
			multi_in_bck_sel_func),
	[CTRL_MULTI_IN_LRCK_SEL] =
		SOC_ENUM_SINGLE(AFE_LOOPBACK_CFG0, 4,
			ARRAY_SIZE(multi_in_lrck_sel_func),
			multi_in_lrck_sel_func),
	[CTRL_MULTI_IN_SDATA0_SEL] =
		SOC_ENUM_SINGLE(AFE_LOOPBACK_CFG0, 8,
			ARRAY_SIZE(multi_in_sdata_sel_func),
			multi_in_sdata_sel_func),
	[CTRL_MULTI_IN_SDATA1_SEL] =
		SOC_ENUM_SINGLE(AFE_LOOPBACK_CFG0, 10,
			ARRAY_SIZE(multi_in_sdata_sel_func),
			multi_in_sdata_sel_func),
	[CTRL_MULTI_IN_SDATA2_SEL] =
		SOC_ENUM_SINGLE(AFE_LOOPBACK_CFG0, 12,
			ARRAY_SIZE(multi_in_sdata_sel_func),
			multi_in_sdata_sel_func),
	[CTRL_MULTI_IN_SDATA3_SEL] =
		SOC_ENUM_SINGLE(AFE_LOOPBACK_CFG0, 14,
			ARRAY_SIZE(multi_in_sdata_sel_func),
			multi_in_sdata_sel_func),
	[CTRL_SPDIF_IN_PORT_SEL] =
		SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(spdif_in_port_sel_func),
				    spdif_in_port_sel_func),
};

static int mt8518_afe_i18_i19_src_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int src = I18_I19_FROM_ETDM_IN2;
	unsigned int val;

	mt8518_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, AFE_CONN76, &val);

	mt8518_afe_disable_main_clk(afe);

	val = val & AFE_CONN76_I18_I19_SEL_MASK;

	if (val == AFE_CONN76_I18_I19_SEL_ETDM_IN2)
		src = I18_I19_FROM_ETDM_IN2;
	else if (val == AFE_CONN76_I18_I19_SEL_AMIC)
		src = I18_I19_FROM_AMIC;
	else if (val == AFE_CONN76_I18_I19_SEL_AMIC_FIFO)
		src = I18_I19_FROM_AMIC_FIFO;

	ucontrol->value.integer.value[0] = src;

	return 0;
}

static int mt8518_afe_i18_i19_src_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int src;

	src = ucontrol->value.integer.value[0];

	mt8518_afe_enable_main_clk(afe);

	if (src == I18_I19_FROM_ETDM_IN2)
		regmap_update_bits(afe->regmap, AFE_CONN76,
				   AFE_CONN76_I18_I19_SEL_MASK,
				   AFE_CONN76_I18_I19_SEL_ETDM_IN2);
	else if (src == I18_I19_FROM_AMIC)
		regmap_update_bits(afe->regmap, AFE_CONN76,
				   AFE_CONN76_I18_I19_SEL_MASK,
				   AFE_CONN76_I18_I19_SEL_AMIC);
	else  if (src == I18_I19_FROM_AMIC_FIFO)
		regmap_update_bits(afe->regmap, AFE_CONN76,
				   AFE_CONN76_I18_I19_SEL_MASK,
				   AFE_CONN76_I18_I19_SEL_AMIC_FIFO);

	mt8518_afe_disable_main_clk(afe);

	return 0;
}

static int mt8518_afe_i10_i11_src_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int src = I10_I11_FROM_DMIC;
	unsigned int val;

	mt8518_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, AFE_CONN76, &val);

	mt8518_afe_disable_main_clk(afe);

	val = val & AFE_CONN76_I10_I11_SEL_MASK;

	if (val == AFE_CONN76_I10_I11_SEL_DMIC)
		src = I10_I11_FROM_DMIC;
	else if (val == AFE_CONN76_I10_I11_SEL_AMIC)
		src = I10_I11_FROM_AMIC;
	else if (val == AFE_CONN76_I10_I11_SEL_ETDM_IN1)
		src = I10_I11_FROM_ETDM_IN1;
	else if (val == AFE_CONN76_I10_I11_SEL_AMIC_FIFO)
		src = I10_I11_FROM_AMIC_FIFO;

	ucontrol->value.integer.value[0] = src;

	return 0;
}

static int mt8518_afe_i10_i11_src_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int src;

	src = ucontrol->value.integer.value[0];

	mt8518_afe_enable_main_clk(afe);

	if (src == I10_I11_FROM_DMIC)
		regmap_update_bits(afe->regmap, AFE_CONN76,
				   AFE_CONN76_I10_I11_SEL_MASK,
				   AFE_CONN76_I10_I11_SEL_DMIC);
	else if (src == I10_I11_FROM_AMIC)
		regmap_update_bits(afe->regmap, AFE_CONN76,
				   AFE_CONN76_I10_I11_SEL_MASK,
				   AFE_CONN76_I10_I11_SEL_AMIC);
	else  if (src == I10_I11_FROM_ETDM_IN1)
		regmap_update_bits(afe->regmap, AFE_CONN76,
				   AFE_CONN76_I10_I11_SEL_MASK,
				   AFE_CONN76_I10_I11_SEL_ETDM_IN1);
	else  if (src == I10_I11_FROM_AMIC_FIFO)
		regmap_update_bits(afe->regmap, AFE_CONN76,
				   AFE_CONN76_I10_I11_SEL_MASK,
				   AFE_CONN76_I10_I11_SEL_AMIC_FIFO);

	mt8518_afe_disable_main_clk(afe);

	return 0;
}

static int mt8518_afe_cm_bypass_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	struct mt8518_afe_private *afe_priv = afe->platform_priv;
	struct mt8518_control_data *data = &afe_priv->ctrl_data;

	if (!strcmp(kcontrol->id.name, "CM0_Bypass_Switch"))
		ucontrol->value.integer.value[0] = data->bypass_cm0;
	else if (!strcmp(kcontrol->id.name, "CM1_Bypass_Switch"))
		ucontrol->value.integer.value[0] = data->bypass_cm1;

	return 0;
}

static int mt8518_afe_cm_bypass_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	struct mt8518_afe_private *afe_priv = afe->platform_priv;
	struct mt8518_control_data *data = &afe_priv->ctrl_data;

	if (!strcmp(kcontrol->id.name, "CM0_Bypass_Switch"))
		data->bypass_cm0 = (ucontrol->value.integer.value[0]) ?
				   true : false;
	else if (!strcmp(kcontrol->id.name, "CM1_Bypass_Switch"))
		data->bypass_cm1 = (ucontrol->value.integer.value[0]) ?
				   true : false;

	return 0;
}

static int mt8518_afe_singen_enable_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int val = 0;

	mt8518_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, AFE_SINEGEN_CON0, &val);

	mt8518_afe_disable_main_clk(afe);

	ucontrol->value.integer.value[0] = (val & AFE_SINEGEN_CON0_EN);

	return 0;
}

static int mt8518_afe_singen_enable_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);

	mt8518_afe_enable_main_clk(afe);

	if (ucontrol->value.integer.value[0]) {
		mt8518_afe_enable_top_cg(afe, MT8518_TOP_CG_TML);

		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON0,
				   AFE_SINEGEN_CON0_EN,
				   AFE_SINEGEN_CON0_EN);
	} else {
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON0,
				   AFE_SINEGEN_CON0_EN,
				   0x0);

		mt8518_afe_disable_top_cg(afe, MT8518_TOP_CG_TML);
	}

	mt8518_afe_disable_main_clk(afe);

	return 0;
}

static int mt8518_afe_sinegen_loopback_mode_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int mode;
	unsigned int val;

	mt8518_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, AFE_SINEGEN_CON0, &val);

	mt8518_afe_disable_main_clk(afe);

	mode = (val & AFE_SINEGEN_CON0_MODE_MASK) >> 27;

	if (mode >= SINEGEN_NONE)
		mode = SINEGEN_NONE;

	ucontrol->value.integer.value[0] = mode;

	return 0;
}

static int mt8518_afe_sinegen_loopback_mode_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int mode;
	unsigned int val;

	mode = ucontrol->value.integer.value[0];

	val = (mode << 27) & AFE_SINEGEN_CON0_MODE_MASK;

	mt8518_afe_enable_main_clk(afe);

	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON0,
			   AFE_SINEGEN_CON0_MODE_MASK, val);

	mt8518_afe_disable_main_clk(afe);

	return 0;
}

static int mt8518_afe_sinegen_timing_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int timing;
	unsigned int val;

	mt8518_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, AFE_SINEGEN_CON1, &val);

	mt8518_afe_disable_main_clk(afe);

	val = (val & AFE_SINEGEN_CON1_TIMING_CH1_MASK) >> 16;

	switch (val) {
	case AFE_SINEGEN_CON1_TIMING_8K:
		timing = SINEGEN_8K;
		break;
	case AFE_SINEGEN_CON1_TIMING_12K:
		timing = SINEGEN_12K;
		break;
	case AFE_SINEGEN_CON1_TIMING_16K:
		timing = SINEGEN_16K;
		break;
	case AFE_SINEGEN_CON1_TIMING_24K:
		timing = SINEGEN_24K;
		break;
	case AFE_SINEGEN_CON1_TIMING_32K:
		timing = SINEGEN_32K;
		break;
	case AFE_SINEGEN_CON1_TIMING_48K:
		timing = SINEGEN_48K;
		break;
	case AFE_SINEGEN_CON1_TIMING_96K:
		timing = SINEGEN_96K;
		break;
	case AFE_SINEGEN_CON1_TIMING_192K:
		timing = SINEGEN_192K;
		break;
	case AFE_SINEGEN_CON1_TIMING_384K:
		timing = SINEGEN_384K;
		break;
	case AFE_SINEGEN_CON1_TIMING_7D35K:
		timing = SINEGEN_7D35K;
		break;
	case AFE_SINEGEN_CON1_TIMING_11D025K:
		timing = SINEGEN_11D025K;
		break;
	case AFE_SINEGEN_CON1_TIMING_14D7K:
		timing = SINEGEN_14D7K;
		break;
	case AFE_SINEGEN_CON1_TIMING_22D05K:
		timing = SINEGEN_22D05K;
		break;
	case AFE_SINEGEN_CON1_TIMING_29D4K:
		timing = SINEGEN_29D4K;
		break;
	case AFE_SINEGEN_CON1_TIMING_44D1K:
		timing = SINEGEN_44D1K;
		break;
	case AFE_SINEGEN_CON1_TIMING_88D2K:
		timing = SINEGEN_88D2K;
		break;
	case AFE_SINEGEN_CON1_TIMING_176D4K:
		timing = SINEGEN_176D4K;
		break;
	case AFE_SINEGEN_CON1_TIMING_352D8K:
		timing = SINEGEN_352D8K;
		break;
	case AFE_SINEGEN_CON1_TIMING_DL_1X_EN:
		timing = SINEGEN_DL_1X_EN;
		break;
	case AFE_SINEGEN_CON1_TIMING_SGEN_EN:
		timing = SINEGEN_SGEN_EN;
		break;
	default:
		timing = SINEGEN_8K;
		break;
	}

	ucontrol->value.integer.value[0] = timing;

	return 0;
}

static int mt8518_afe_sinegen_timing_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int timing;
	unsigned int val;

	timing = ucontrol->value.integer.value[0];

	switch (timing) {
	case SINEGEN_8K:
		val = AFE_SINEGEN_CON1_TIMING_8K;
		break;
	case SINEGEN_12K:
		val = AFE_SINEGEN_CON1_TIMING_12K;
		break;
	case SINEGEN_16K:
		val = AFE_SINEGEN_CON1_TIMING_16K;
		break;
	case SINEGEN_24K:
		val = AFE_SINEGEN_CON1_TIMING_24K;
		break;
	case SINEGEN_32K:
		val = AFE_SINEGEN_CON1_TIMING_32K;
		break;
	case SINEGEN_48K:
		val = AFE_SINEGEN_CON1_TIMING_48K;
		break;
	case SINEGEN_96K:
		val = AFE_SINEGEN_CON1_TIMING_96K;
		break;
	case SINEGEN_192K:
		val = AFE_SINEGEN_CON1_TIMING_192K;
		break;
	case SINEGEN_384K:
		val = AFE_SINEGEN_CON1_TIMING_384K;
		break;
	case SINEGEN_7D35K:
		val = AFE_SINEGEN_CON1_TIMING_7D35K;
		break;
	case SINEGEN_11D025K:
		val = AFE_SINEGEN_CON1_TIMING_11D025K;
		break;
	case SINEGEN_14D7K:
		val = AFE_SINEGEN_CON1_TIMING_14D7K;
		break;
	case SINEGEN_22D05K:
		val = AFE_SINEGEN_CON1_TIMING_22D05K;
		break;
	case SINEGEN_29D4K:
		val = AFE_SINEGEN_CON1_TIMING_29D4K;
		break;
	case SINEGEN_44D1K:
		val = AFE_SINEGEN_CON1_TIMING_44D1K;
		break;
	case SINEGEN_88D2K:
		val = AFE_SINEGEN_CON1_TIMING_88D2K;
		break;
	case SINEGEN_176D4K:
		val = AFE_SINEGEN_CON1_TIMING_176D4K;
		break;
	case SINEGEN_352D8K:
		val = AFE_SINEGEN_CON1_TIMING_352D8K;
		break;
	case SINEGEN_DL_1X_EN:
		val = AFE_SINEGEN_CON1_TIMING_DL_1X_EN;
		break;
	case SINEGEN_SGEN_EN:
		val = AFE_SINEGEN_CON1_TIMING_SGEN_EN;
		break;
	default:
		val = AFE_SINEGEN_CON1_TIMING_8K;
		break;
	}

	mt8518_afe_enable_main_clk(afe);

	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
			   AFE_SINEGEN_CON1_TIMING_CH1_MASK |
			   AFE_SINEGEN_CON1_TIMING_CH2_MASK,
			   AFE_SINEGEN_CON1_TIMING_CH1(val) |
			   AFE_SINEGEN_CON1_TIMING_CH2(val));

	mt8518_afe_disable_main_clk(afe);

	return 0;
}

static int mt8518_afe_ul8_sinegen_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int val = 0;

	mt8518_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, ASYS_TOP_CON, &val);

	mt8518_afe_disable_main_clk(afe);

	ucontrol->value.integer.value[0] = (val & ASYS_TCON_UL8_USE_SINEGEN);

	return 0;
}

static int mt8518_afe_ul8_sinegen_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);

	mt8518_afe_enable_main_clk(afe);

	if (ucontrol->value.integer.value[0])
		regmap_update_bits(afe->regmap, ASYS_TOP_CON,
				   ASYS_TCON_UL8_USE_SINEGEN,
				   ASYS_TCON_UL8_USE_SINEGEN);
	else
		regmap_update_bits(afe->regmap, ASYS_TOP_CON,
				   ASYS_TCON_UL8_USE_SINEGEN,
				   0x0);

	mt8518_afe_disable_main_clk(afe);

	return 0;
}

static int mt8518_afe_tdm_out1_sinegen_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int val = 0;

	mt8518_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, ETDM_COWORK_CON3, &val);

	mt8518_afe_disable_main_clk(afe);

	ucontrol->value.integer.value[0] =
		(val & ETDM_COWORK_CON3_OUT1_USE_SINEGEN);

	return 0;
}

static int mt8518_afe_tdm_out1_sinegen_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);

	mt8518_afe_enable_main_clk(afe);

	if (ucontrol->value.integer.value[0])
		regmap_update_bits(afe->regmap, ETDM_COWORK_CON3,
				   ETDM_COWORK_CON3_OUT1_USE_SINEGEN,
				   ETDM_COWORK_CON3_OUT1_USE_SINEGEN);
	else
		regmap_update_bits(afe->regmap, ETDM_COWORK_CON3,
				   ETDM_COWORK_CON3_OUT1_USE_SINEGEN,
				   0x0);

	mt8518_afe_disable_main_clk(afe);

	return 0;
}

static int mt8518_afe_etdm_in_data_source_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int src = ETDM_IN_FROM_PAD;
	unsigned int val;

	mt8518_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, ETDM_COWORK_CON1, &val);

	mt8518_afe_disable_main_clk(afe);

	if (!strcmp(kcontrol->id.name, "ETDM_IN1_Data_Source_Select")) {
		val = val & ETDM_COWORK_CON1_TDM_IN1_DAT0_SEL_MASK;

		if (val == ETDM_COWORK_CON1_TDM_IN1_DAT0_SEL_PAD)
			src = ETDM_IN_FROM_PAD;
		else if (val == ETDM_COWORK_CON1_TDM_IN1_DAT0_SEL_OUT1)
			src = ETDM_IN_FROM_ETDM_OUT1;
		else if (val == ETDM_COWORK_CON1_TDM_IN1_DAT0_SEL_OUT2)
			src = ETDM_IN_FROM_ETDM_OUT2;
	} else if (!strcmp(kcontrol->id.name, "ETDM_IN2_Data_Source_Select")) {
		val = val & ETDM_COWORK_CON1_TDM_IN2_DAT0_SEL_MASK;

		if (val == ETDM_COWORK_CON1_TDM_IN2_DAT0_SEL_PAD)
			src = ETDM_IN_FROM_PAD;
		else if (val == ETDM_COWORK_CON1_TDM_IN2_DAT0_SEL_OUT1)
			src = ETDM_IN_FROM_ETDM_OUT1;
		else if (val == ETDM_COWORK_CON1_TDM_IN2_DAT0_SEL_OUT2)
			src = ETDM_IN_FROM_ETDM_OUT2;
	}

	ucontrol->value.integer.value[0] = src;

	return 0;
}

static int mt8518_afe_etdm_in_data_source_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int src;

	src = ucontrol->value.integer.value[0];

	mt8518_afe_enable_main_clk(afe);

	if (!strcmp(kcontrol->id.name, "ETDM_IN1_Data_Source_Select")) {
		if (src == ETDM_IN_FROM_PAD)
			regmap_update_bits(afe->regmap, ETDM_COWORK_CON1,
				   ETDM_COWORK_CON1_TDM_IN1_DAT0_SEL_MASK |
				   ETDM_COWORK_CON1_TDM_IN1_DAT1_3_SEL_MASK,
				   ETDM_COWORK_CON1_TDM_IN1_DAT0_SEL_PAD |
				   ETDM_COWORK_CON1_TDM_IN1_DAT1_3_SEL_PAD);
		else if (src == ETDM_IN_FROM_ETDM_OUT1)
			regmap_update_bits(afe->regmap, ETDM_COWORK_CON1,
				   ETDM_COWORK_CON1_TDM_IN1_DAT0_SEL_MASK |
				   ETDM_COWORK_CON1_TDM_IN1_DAT1_3_SEL_MASK,
				   ETDM_COWORK_CON1_TDM_IN1_DAT0_SEL_OUT1 |
				   ETDM_COWORK_CON1_TDM_IN1_DAT1_3_SEL_OUT1);
		else if (src == ETDM_IN_FROM_ETDM_OUT2)
			regmap_update_bits(afe->regmap, ETDM_COWORK_CON1,
				   ETDM_COWORK_CON1_TDM_IN1_DAT0_SEL_MASK |
				   ETDM_COWORK_CON1_TDM_IN1_DAT1_3_SEL_MASK,
				   ETDM_COWORK_CON1_TDM_IN1_DAT0_SEL_OUT2 |
				   ETDM_COWORK_CON1_TDM_IN1_DAT1_3_SEL_OUT2);
	} else if (!strcmp(kcontrol->id.name, "ETDM_IN2_Data_Source_Select")) {
		if (src == ETDM_IN_FROM_PAD)
			regmap_update_bits(afe->regmap, ETDM_COWORK_CON1,
				   ETDM_COWORK_CON1_TDM_IN2_DAT0_SEL_MASK |
				   ETDM_COWORK_CON1_TDM_IN2_DAT1_3_SEL_MASK,
				   ETDM_COWORK_CON1_TDM_IN2_DAT0_SEL_PAD |
				   ETDM_COWORK_CON1_TDM_IN2_DAT1_3_SEL_PAD);
		else if (src == ETDM_IN_FROM_ETDM_OUT1)
			regmap_update_bits(afe->regmap, ETDM_COWORK_CON1,
				   ETDM_COWORK_CON1_TDM_IN2_DAT0_SEL_MASK |
				   ETDM_COWORK_CON1_TDM_IN2_DAT1_3_SEL_MASK,
				   ETDM_COWORK_CON1_TDM_IN2_DAT0_SEL_OUT1 |
				   ETDM_COWORK_CON1_TDM_IN2_DAT1_3_SEL_OUT1);
		else if (src == ETDM_IN_FROM_ETDM_OUT2)
			regmap_update_bits(afe->regmap, ETDM_COWORK_CON1,
				   ETDM_COWORK_CON1_TDM_IN2_DAT0_SEL_MASK |
				   ETDM_COWORK_CON1_TDM_IN2_DAT1_3_SEL_MASK,
				   ETDM_COWORK_CON1_TDM_IN2_DAT0_SEL_OUT2 |
				   ETDM_COWORK_CON1_TDM_IN2_DAT1_3_SEL_OUT2);
	}

	mt8518_afe_disable_main_clk(afe);

	return 0;
}

static int mt8518_afe_etdm_in_slave_mode_src_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int src = ETDM_IN_SLAVE_FROM_SELF;
	unsigned int val;

	mt8518_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, ETDM_COWORK_CON1, &val);

	mt8518_afe_disable_main_clk(afe);

	if (!strcmp(kcontrol->id.name, "ETDM_IN1_Slave_Mode_Source_Select")) {
		val = val & ETDM_COWORK_CON0_TDM_IN1_SLV_SEL_MASK;

		if (val == ETDM_COWORK_CON0_TDM_IN1_SLV_SEL_IN1_SLV)
			src = ETDM_IN_SLAVE_FROM_SELF;
		else if (val == ETDM_COWORK_CON0_TDM_IN1_SLV_SEL_OUT1_MAS)
			src = ETDM_IN_SLAVE_FROM_ETDM_OUT1;
		else if (val == ETDM_COWORK_CON0_TDM_IN1_SLV_SEL_OUT2_MAS)
			src = ETDM_IN_SLAVE_FROM_ETDM_OUT2;
	} else if (!strcmp(kcontrol->id.name,
			   "ETDM_IN2_Slave_Mode_Source_Select")) {
		val = val & ETDM_COWORK_CON1_TDM_IN2_SLV_SEL_MASK;

		if (val == ETDM_COWORK_CON1_TDM_IN2_SLV_SEL_IN2_SLV)
			src = ETDM_IN_SLAVE_FROM_SELF;
		else if (val == ETDM_COWORK_CON1_TDM_IN2_SLV_SEL_OUT1_MAS)
			src = ETDM_IN_SLAVE_FROM_ETDM_OUT1;
		else if (val == ETDM_COWORK_CON1_TDM_IN2_SLV_SEL_OUT2_MAS)
			src = ETDM_IN_SLAVE_FROM_ETDM_OUT2;
	}

	ucontrol->value.integer.value[0] = src;

	return 0;
}

static int mt8518_afe_etdm_in_slave_mode_src_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int src;

	src = ucontrol->value.integer.value[0];

	mt8518_afe_enable_main_clk(afe);

	if (!strcmp(kcontrol->id.name, "ETDM_IN1_Data_Source_Select")) {
		if (src == ETDM_IN_SLAVE_FROM_SELF)
			regmap_update_bits(afe->regmap, ETDM_COWORK_CON0,
				   ETDM_COWORK_CON0_TDM_IN1_SLV_SEL_MASK,
				   ETDM_COWORK_CON0_TDM_IN1_SLV_SEL_IN1_SLV);
		else if (src == ETDM_COWORK_CON0_TDM_IN1_SLV_SEL_OUT1_MAS)
			regmap_update_bits(afe->regmap, ETDM_COWORK_CON1,
				   ETDM_COWORK_CON0_TDM_IN1_SLV_SEL_MASK,
				   ETDM_COWORK_CON0_TDM_IN1_SLV_SEL_OUT1_MAS);
		else if (src == ETDM_COWORK_CON0_TDM_IN1_SLV_SEL_OUT2_MAS)
			regmap_update_bits(afe->regmap, ETDM_COWORK_CON1,
				   ETDM_COWORK_CON0_TDM_IN1_SLV_SEL_MASK,
				   ETDM_COWORK_CON0_TDM_IN1_SLV_SEL_OUT2_MAS);
	} else if (!strcmp(kcontrol->id.name, "ETDM_IN2_Data_Source_Select")) {
		if (src == ETDM_IN_SLAVE_FROM_SELF)
			regmap_update_bits(afe->regmap, ETDM_COWORK_CON1,
				   ETDM_COWORK_CON1_TDM_IN2_SLV_SEL_MASK,
				   ETDM_COWORK_CON1_TDM_IN2_SLV_SEL_IN2_SLV);
		else if (src == ETDM_COWORK_CON0_TDM_IN1_SLV_SEL_OUT1_MAS)
			regmap_update_bits(afe->regmap, ETDM_COWORK_CON1,
				   ETDM_COWORK_CON1_TDM_IN2_SLV_SEL_MASK,
				   ETDM_COWORK_CON1_TDM_IN2_SLV_SEL_OUT1_MAS);
		else if (src == ETDM_COWORK_CON0_TDM_IN1_SLV_SEL_OUT2_MAS)
			regmap_update_bits(afe->regmap, ETDM_COWORK_CON1,
				   ETDM_COWORK_CON1_TDM_IN2_SLV_SEL_MASK,
				   ETDM_COWORK_CON1_TDM_IN2_SLV_SEL_OUT2_MAS);
	}

	mt8518_afe_disable_main_clk(afe);

	return 0;
}

static int mt8518_afe_pcm_loopback_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int val = 0;

	mt8518_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, PCM_INTF_CON2, &val);

	mt8518_afe_disable_main_clk(afe);

	ucontrol->value.integer.value[0] = (val & PCM_INTF_CON2_LPBK_EN);

	return 0;
}

static int mt8518_afe_pcm_loopback_put(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);

	mt8518_afe_enable_main_clk(afe);

	if (ucontrol->value.integer.value[0])
		regmap_update_bits(afe->regmap, PCM_INTF_CON2,
				   PCM_INTF_CON2_LPBK_EN,
				   PCM_INTF_CON2_LPBK_EN);
	else
		regmap_update_bits(afe->regmap, PCM_INTF_CON2,
				   PCM_INTF_CON2_LPBK_EN,
				   0x0);

	mt8518_afe_disable_main_clk(afe);

	return 0;
}

static int mt8518_afe_dmic_sinegen_enable_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int val = 0;

	mt8518_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, DMIC_TOP_CON, &val);

	mt8518_afe_disable_main_clk(afe);

	ucontrol->value.integer.value[0] =
		(val & DMIC_TOP_CON_DMIC_SGEN_RAMPGEN_EN) ? 1 : 0;

	return 0;
}

static int mt8518_afe_dmic_sinegen_enable_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int val_cfg = 0;
	unsigned int val_en = 0;
	unsigned int regs[] = {
		DMIC_TOP_CON,
		DMIC2_TOP_CON,
		DMIC3_TOP_CON,
		DMIC4_TOP_CON,
	};
	int i;

	mt8518_afe_enable_main_clk(afe);

	val_cfg |= DMIC_TOP_CON_DMIC_SGEN_AMP_DIV_6DB |
		   DMIC_TOP_CON_DMIC_SGEN_RAMPGEN_FREQ_DIV(1);

	val_en |= DMIC_TOP_CON_DMIC_SGEN_LON |
		  DMIC_TOP_CON_DMIC_SGEN_RON |
		  DMIC_TOP_CON_DMIC_SGEN_RAMPGEN_EN;

	if (ucontrol->value.integer.value[0]) {
		for (i = 0; i < ARRAY_SIZE(regs); i++)
			regmap_update_bits(afe->regmap, regs[i],
					   val_cfg | val_en,
					   val_cfg | val_en);
	} else {
		for (i = 0; i < ARRAY_SIZE(regs); i++)
			regmap_update_bits(afe->regmap, regs[i],
					   val_en,
					   0x0);
	}

	mt8518_afe_disable_main_clk(afe);

	return 0;
}

static int mt8518_afe_dmic_rampgen_enable_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int val = 0;

	mt8518_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, DMIC_TOP_CON, &val);

	mt8518_afe_disable_main_clk(afe);

	ucontrol->value.integer.value[0] =
		(val & DMIC_TOP_CON_DMIC_SGEN_RAMPGEN_EN) ? 1 : 0;

	return 0;
}

static int mt8518_afe_dmic_rampgen_enable_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int val_cfg = 0;
	unsigned int val_en = 0;
	unsigned int regs[] = {
		DMIC_TOP_CON,
		DMIC2_TOP_CON,
		DMIC3_TOP_CON,
		DMIC4_TOP_CON,
	};
	int i;

	mt8518_afe_enable_main_clk(afe);

	val_cfg |= DMIC_TOP_CON_DMIC_SGEN_RAMPGEN_FREQ_DIV(0x1F);

	val_en |= DMIC_TOP_CON_DMIC_RAMPGEN_LON |
		  DMIC_TOP_CON_DMIC_RAMPGEN_RON |
		  DMIC_TOP_CON_DMIC_SGEN_RAMPGEN_EN;

	if (ucontrol->value.integer.value[0]) {
		for (i = 0; i < ARRAY_SIZE(regs); i++)
			regmap_update_bits(afe->regmap, regs[i],
					   val_cfg | val_en,
					   val_cfg | val_en);
	} else {
		for (i = 0; i < ARRAY_SIZE(regs); i++)
			regmap_update_bits(afe->regmap, regs[i],
					   val_en,
					   0x0);
	}

	mt8518_afe_disable_main_clk(afe);

	return 0;
}

static int mt8518_afe_gasrc_in_sinegen_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int val = 0;

	mt8518_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, AFE_SINEGEN_CON1, &val);

	mt8518_afe_disable_main_clk(afe);

	ucontrol->value.integer.value[0] =
		(val & AFE_SINEGEN_CON1_GASRC_IN_SGEN);

	return 0;
}

static int mt8518_afe_gasrc_in_sinegen_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);

	mt8518_afe_enable_main_clk(afe);

	if (ucontrol->value.integer.value[0])
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
				   AFE_SINEGEN_CON1_GASRC_IN_SGEN,
				   AFE_SINEGEN_CON1_GASRC_IN_SGEN);
	else
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
				   AFE_SINEGEN_CON1_GASRC_IN_SGEN,
				   0x0);

	mt8518_afe_disable_main_clk(afe);

	return 0;
}

static int mt8518_afe_gasrc_out_sinegen_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int val = 0;

	mt8518_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, AFE_SINEGEN_CON1, &val);

	mt8518_afe_disable_main_clk(afe);

	ucontrol->value.integer.value[0] =
		(val & AFE_SINEGEN_CON1_GASRC_OUT_SGEN);

	return 0;
}

static int mt8518_afe_gasrc_out_sinegen_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);

	mt8518_afe_enable_main_clk(afe);

	if (ucontrol->value.integer.value[0])
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
				   AFE_SINEGEN_CON1_GASRC_OUT_SGEN,
				   AFE_SINEGEN_CON1_GASRC_OUT_SGEN);
	else
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
				   AFE_SINEGEN_CON1_GASRC_OUT_SGEN,
				   0x0);

	mt8518_afe_disable_main_clk(afe);

	return 0;
}

static int get_tdmout_conn_reg_shift(char *name,
	unsigned int *reg, unsigned int *shift)
{
	int ret = 0;

	if (!strcmp(name, "TDMOUT_O00_Source_Select")) {
		*reg = AFE_TDMOUT_CONN0;
		*shift = 0;
	} else if (!strcmp(name, "TDMOUT_O01_Source_Select")) {
		*reg = AFE_TDMOUT_CONN0;
		*shift = 4;
	} else if (!strcmp(name, "TDMOUT_O02_Source_Select")) {
		*reg = AFE_TDMOUT_CONN0;
		*shift = 8;
	} else if (!strcmp(name, "TDMOUT_O03_Source_Select")) {
		*reg = AFE_TDMOUT_CONN0;
		*shift = 12;
	} else if (!strcmp(name, "TDMOUT_O04_Source_Select")) {
		*reg = AFE_TDMOUT_CONN0;
		*shift = 16;
	} else if (!strcmp(name, "TDMOUT_O05_Source_Select")) {
		*reg = AFE_TDMOUT_CONN0;
		*shift = 20;
	} else if (!strcmp(name, "TDMOUT_O06_Source_Select")) {
		*reg = AFE_TDMOUT_CONN0;
		*shift = 24;
	} else if (!strcmp(name, "TDMOUT_O07_Source_Select")) {
		*reg = AFE_TDMOUT_CONN0;
		*shift = 28;
	} else if (!strcmp(name, "TDMOUT_O08_Source_Select")) {
		*reg = AFE_TDMOUT_CONN1;
		*shift = 0;
	} else if (!strcmp(name, "TDMOUT_O09_Source_Select")) {
		*reg = AFE_TDMOUT_CONN1;
		*shift = 4;
	} else if (!strcmp(name, "TDMOUT_O10_Source_Select")) {
		*reg = AFE_TDMOUT_CONN1;
		*shift = 8;
	} else if (!strcmp(name, "TDMOUT_O11_Source_Select")) {
		*reg = AFE_TDMOUT_CONN1;
		*shift = 12;
	} else if (!strcmp(name, "TDMOUT_O12_Source_Select")) {
		*reg = AFE_TDMOUT_CONN1;
		*shift = 16;
	} else if (!strcmp(name, "TDMOUT_O13_Source_Select")) {
		*reg = AFE_TDMOUT_CONN1;
		*shift = 20;
	} else if (!strcmp(name, "TDMOUT_O14_Source_Select")) {
		*reg = AFE_TDMOUT_CONN1;
		*shift = 24;
	} else if (!strcmp(name, "TDMOUT_O15_Source_Select")) {
		*reg = AFE_TDMOUT_CONN1;
		*shift = 28;
	} else {
		ret = -1;
	}

	return ret;
}

static int mt8518_afe_tdm_conn_source_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int src;
	unsigned int reg;
	unsigned int shift;
	unsigned int val;
	int ret;

	ret = get_tdmout_conn_reg_shift(kcontrol->id.name, &reg, &shift);
	if (ret)
		return ret;

	mt8518_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, reg, &val);

	mt8518_afe_disable_main_clk(afe);

	src = (val >> shift) & 0xf;

	ucontrol->value.integer.value[0] = src;

	return 0;
}

static int mt8518_afe_tdm_conn_source_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int src;
	unsigned int reg;
	unsigned int shift;
	int ret;

	src = ucontrol->value.integer.value[0];

	ret = get_tdmout_conn_reg_shift(kcontrol->id.name, &reg, &shift);
	if (ret)
		return ret;

	mt8518_afe_enable_main_clk(afe);

	regmap_update_bits(afe->regmap, reg, 0xf << shift, src << shift);

	mt8518_afe_disable_main_clk(afe);

	return 0;
}

static int mt8518_afe_spdif_output_iec61937_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	struct mt8518_afe_private *afe_priv = afe->platform_priv;
	struct mt8518_control_data *data = &afe_priv->ctrl_data;

	ucontrol->value.integer.value[0] = data->spdif_output_iec61937;

	return 0;
}

static int mt8518_afe_spdif_output_iec61937_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	struct mt8518_afe_private *afe_priv = afe->platform_priv;
	struct mt8518_control_data *data = &afe_priv->ctrl_data;

	data->spdif_output_iec61937 = (ucontrol->value.integer.value[0]) ?
				 true : false;

	dev_dbg(afe->dev, "%s iec61937 %d\n", __func__,
		data->spdif_output_iec61937);

	return 0;
}

static int mt8518_afe_spdif_in_port_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	struct mt8518_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->spdif_in_data.port;

	return 0;
}

static int mt8518_afe_spdif_in_port_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int port = ucontrol->value.integer.value[0];

	dev_dbg(afe->dev, "%s port %u\n", __func__, port);

	mt8518_afe_handle_spdif_in_port_change(afe, port);

	return 0;
}

static int mt8518_afe_spdif_in_chs_data_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = 24;
	return 0;
}

static int mt8518_afe_spdif_in_chs_data_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	struct mt8518_afe_private *afe_priv = afe->platform_priv;
	struct mt8518_spdif_in_data *spdif_in = &afe_priv->spdif_in_data;
	int i;

	if (spdif_in->port != SPDIF_IN_PORT_NONE) {
		mt8518_afe_enable_main_clk(afe);

		for (i = 0; i < SPDIF_CHSTS_NUM; i++)
			regmap_read(afe->regmap,
				    AFE_SPDIFIN_CHSTS1 + i * 4,
				    &spdif_in->subdata.ch_status[i]);

		mt8518_afe_disable_main_clk(afe);
	}

	memcpy((void *)ucontrol->value.bytes.data,
	       spdif_in->subdata.ch_status,
	       sizeof(spdif_in->subdata.ch_status));

	return 0;
}

static int mt8518_afe_spdif_in_rate_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	return 0;
}

static int mt8518_afe_spdif_in_rate_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	struct mt8518_afe_private *afe_priv = afe->platform_priv;
	struct mt8518_spdif_in_data *spdif_in = &afe_priv->spdif_in_data;

	ucontrol->value.integer.value[0] = spdif_in->subdata.rate;

	return 0;
}

static int mt8518_afe_spdif_in_iec958_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;
	return 0;
}

static int mt8518_afe_spdif_in_iec958_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	struct mt8518_afe_private *afe_priv = afe->platform_priv;
	struct mt8518_spdif_in_data *spdif_in = &afe_priv->spdif_in_data;
	int i;

	if (spdif_in->port != SPDIF_IN_PORT_NONE) {
		mt8518_afe_enable_main_clk(afe);

		for (i = 0; i < 6; i++)
			regmap_read(afe->regmap,
				    AFE_SPDIFIN_CHSTS1 + i * 4,
				    &spdif_in->subdata.ch_status[i]);

		mt8518_afe_disable_main_clk(afe);
	} else {
		memset((void *)spdif_in->subdata.ch_status,
		       0xff, sizeof(spdif_in->subdata.ch_status));
	}

	memcpy((void *)ucontrol->value.iec958.status,
	       spdif_in->subdata.ch_status,
	       sizeof(spdif_in->subdata.ch_status));

	return 0;
}

static int mt8518_afe_spdif_in_stream_type_info(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	return 0;
}

static int mt8518_afe_spdif_in_stream_type_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int val;

	mt8518_afe_enable_main_clk(afe);
	regmap_read(afe->regmap, AFE_MPHONE_MULTI_MON, &val);
	mt8518_afe_disable_main_clk(afe);

	if (val & AFE_MPHONE_MULTI_MON_DEC)
		val = val & AFE_MPHONE_MULTI_MON_ROUGH;
	else
		val = 0xff;

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int mt8518_afe_spdif_in_user_code_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = SPDIF_USERCODE_NUM;
	return 0;
}

static int mt8518_afe_spdif_in_user_code_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	struct mt8518_afe_private *afe_priv = afe->platform_priv;
	struct mt8518_spdif_in_data *spdif_in = &afe_priv->spdif_in_data;
	int i;

	if (spdif_in->port != SPDIF_IN_PORT_NONE) {
		mt8518_afe_enable_main_clk(afe);
		for (i = 0; i < SPDIF_USERCODE_NUM; i++)
			regmap_read(afe->regmap,
				    SPDIFIN_USERCODE1 + i * 4,
				    &spdif_in->subdata.user_code[i]);

		mt8518_afe_disable_main_clk(afe);
	}

	memcpy((void *)ucontrol->value.bytes.data,
	       spdif_in->subdata.user_code,
	       sizeof(spdif_in->subdata.user_code));

	return 0;
}

static int mt8518_afe_spdif_in_information_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = sizeof(struct mt8518_spdif_in_subdata) /
			sizeof(unsigned int);

	return 0;
}

static int mt8518_afe_spdif_in_information_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	struct mt8518_afe_private *afe_priv = afe->platform_priv;
	struct mt8518_spdif_in_data *spdif_in = &afe_priv->spdif_in_data;
	int i, index;

	index = 0;
	ucontrol->value.integer.value[index++] = spdif_in->subdata.rate;

	for (i = 0; i < SPDIF_USERCODE_NUM; i++)
		ucontrol->value.integer.value[i + (index++)] =
			spdif_in->subdata.user_code[i];

	for (i = 0; i < SPDIF_CHSTS_NUM; i++)
		ucontrol->value.integer.value[i + (index++)] =
			spdif_in->subdata.ch_status[i];

	ucontrol->value.integer.value[index++] = spdif_in->subdata.stream_type;

	return 0;
}

static int mt8518_apll1_clk_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	struct mt8518_afe_private *priv = afe->platform_priv;
	struct clk *clk;

	clk = priv->clocks[MT8518_CLK_APLL1];

	ucontrol->value.integer.value[0] = clk_get_rate(clk);
	return 0;
}

static int mt8518_apll1_clk_set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	struct mt8518_afe_private *priv = afe->platform_priv;
	struct clk *clk;
	unsigned long rate, min;

	rate = ucontrol->value.integer.value[0];

	min = MT8518_APLL1_RATE - MT8518_APLL_ADJUST_RANGE;

	rate = max(rate, min);

	dev_dbg(afe->dev, "%s rate %lu\n", __func__, rate);

	clk = priv->clocks[MT8518_CLK_APLL1];

	clk_set_rate(clk, rate);

	return 0;
}

static int mt8518_apll2_clk_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	struct mt8518_afe_private *priv = afe->platform_priv;
	struct clk *clk;

	clk = priv->clocks[MT8518_CLK_APLL2];

	ucontrol->value.integer.value[0] = clk_get_rate(clk);
	return 0;
}

static int mt8518_apll2_clk_set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	struct mt8518_afe_private *priv = afe->platform_priv;
	struct clk *clk;
	unsigned long rate, min;

	rate = ucontrol->value.integer.value[0];

	min = MT8518_APLL2_RATE - MT8518_APLL_ADJUST_RANGE;

	rate = max(rate, min);

	dev_dbg(afe->dev, "%s rate %lu\n", __func__, rate);

	clk = priv->clocks[MT8518_CLK_APLL2];

	clk_set_rate(clk, rate);

	return 0;
}

#define SND_SOC_CTL_RO(xname, xhandler_info, xhandler_get) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.access = SNDRV_CTL_ELEM_ACCESS_READ | \
		  SNDRV_CTL_ELEM_ACCESS_VOLATILE, \
	.info = xhandler_info, .get = xhandler_get }

static const struct snd_kcontrol_new mt8518_afe_controls[] = {
	SOC_ENUM_EXT("I18_I19_Source_Select",
		     mt8518_afe_soc_enums[CTRL_I18_I19_SRC],
		     mt8518_afe_i18_i19_src_get,
		     mt8518_afe_i18_i19_src_put),
	SOC_ENUM_EXT("I10_I11_Source_Select",
		     mt8518_afe_soc_enums[CTRL_I10_I11_SRC],
		     mt8518_afe_i10_i11_src_get,
		     mt8518_afe_i10_i11_src_put),
	SOC_SINGLE_BOOL_EXT("CM0_Bypass_Switch",
			    0,
			    mt8518_afe_cm_bypass_get,
			    mt8518_afe_cm_bypass_put),
	SOC_SINGLE_BOOL_EXT("CM1_Bypass_Switch",
			    0,
			    mt8518_afe_cm_bypass_get,
			    mt8518_afe_cm_bypass_put),
	SOC_SINGLE_BOOL_EXT("SineGen_Enable_Switch",
			    0,
			    mt8518_afe_singen_enable_get,
			    mt8518_afe_singen_enable_put),
	SOC_ENUM_EXT("SineGen_Loopback_Mode_Select",
		     mt8518_afe_soc_enums[CTRL_SINEGEN_LOOPBACK_MODE],
		     mt8518_afe_sinegen_loopback_mode_get,
		     mt8518_afe_sinegen_loopback_mode_put),
	SOC_ENUM_EXT("SineGen_Timing_Select",
		     mt8518_afe_soc_enums[CTRL_SINEGEN_TIMING],
		     mt8518_afe_sinegen_timing_get,
		     mt8518_afe_sinegen_timing_put),
	SOC_SINGLE_RANGE("SineGen_Amp_Div_Ch1",
			 AFE_SINEGEN_CON0, 5, 0, 7, 0),
	SOC_SINGLE_RANGE("SineGen_Amp_Div_Ch2",
			 AFE_SINEGEN_CON0, 17, 0, 7, 0),
	SOC_SINGLE_RANGE("SineGen_Freq_Div_Ch1",
			 AFE_SINEGEN_CON0, 0, 0, 31, 0),
	SOC_SINGLE_RANGE("SineGen_Freq_Div_Ch2",
			 AFE_SINEGEN_CON0, 12, 0, 31, 0),
	SOC_SINGLE_BOOL_EXT("UL8_SineGen_Select",
			    0,
			    mt8518_afe_ul8_sinegen_get,
			    mt8518_afe_ul8_sinegen_put),
	SOC_SINGLE_BOOL_EXT("ETDM_OUT1_SineGen_Select",
			    0,
			    mt8518_afe_tdm_out1_sinegen_get,
			    mt8518_afe_tdm_out1_sinegen_put),
	SOC_ENUM_EXT("ETDM_IN1_Data_Source_Select",
		     mt8518_afe_soc_enums[CTRL_ETDM_IN_DATA_SRC],
		     mt8518_afe_etdm_in_data_source_get,
		     mt8518_afe_etdm_in_data_source_put),
	SOC_ENUM_EXT("ETDM_IN2_Data_Source_Select",
		     mt8518_afe_soc_enums[CTRL_ETDM_IN_DATA_SRC],
		     mt8518_afe_etdm_in_data_source_get,
		     mt8518_afe_etdm_in_data_source_put),
	SOC_ENUM_EXT("ETDM_IN1_Slave_Mode_Source_Select",
		     mt8518_afe_soc_enums[CTRL_ETDM_IN_SLAVE_MODE_SRC],
		     mt8518_afe_etdm_in_slave_mode_src_get,
		     mt8518_afe_etdm_in_slave_mode_src_put),
	SOC_ENUM_EXT("ETDM_IN2_Slave_Mode_Source_Select",
		     mt8518_afe_soc_enums[CTRL_ETDM_IN_SLAVE_MODE_SRC],
		     mt8518_afe_etdm_in_slave_mode_src_get,
		     mt8518_afe_etdm_in_slave_mode_src_put),
	SOC_SINGLE_BOOL_EXT("PCM_Tx2Rx_Loopback_Switch",
			    0,
			    mt8518_afe_pcm_loopback_get,
			    mt8518_afe_pcm_loopback_put),
	SOC_SINGLE_BOOL_EXT("DMIC_SineGen_Enable_Switch",
		     0,
		     mt8518_afe_dmic_sinegen_enable_get,
		     mt8518_afe_dmic_sinegen_enable_put),
	SOC_SINGLE_BOOL_EXT("DMIC_RampGen_Enable_Switch",
		     0,
		     mt8518_afe_dmic_rampgen_enable_get,
		     mt8518_afe_dmic_rampgen_enable_put),
	SOC_SINGLE_BOOL_EXT("GASRC_In_SineGen_Select",
			    0,
			    mt8518_afe_gasrc_in_sinegen_get,
			    mt8518_afe_gasrc_in_sinegen_put),
	SOC_SINGLE_BOOL_EXT("GASRC_Out_SineGen_Select",
			    0,
			    mt8518_afe_gasrc_out_sinegen_get,
			    mt8518_afe_gasrc_out_sinegen_put),
	SOC_ENUM_EXT("TDMOUT_O00_Source_Select",
		     mt8518_afe_soc_enums[CTRL_TDMOUT_CONN_SRC],
		     mt8518_afe_tdm_conn_source_get,
		     mt8518_afe_tdm_conn_source_put),
	SOC_ENUM_EXT("TDMOUT_O01_Source_Select",
		     mt8518_afe_soc_enums[CTRL_TDMOUT_CONN_SRC],
		     mt8518_afe_tdm_conn_source_get,
		     mt8518_afe_tdm_conn_source_put),
	SOC_ENUM_EXT("TDMOUT_O02_Source_Select",
		     mt8518_afe_soc_enums[CTRL_TDMOUT_CONN_SRC],
		     mt8518_afe_tdm_conn_source_get,
		     mt8518_afe_tdm_conn_source_put),
	SOC_ENUM_EXT("TDMOUT_O03_Source_Select",
		     mt8518_afe_soc_enums[CTRL_TDMOUT_CONN_SRC],
		     mt8518_afe_tdm_conn_source_get,
		     mt8518_afe_tdm_conn_source_put),
	SOC_ENUM_EXT("TDMOUT_O04_Source_Select",
		     mt8518_afe_soc_enums[CTRL_TDMOUT_CONN_SRC],
		     mt8518_afe_tdm_conn_source_get,
		     mt8518_afe_tdm_conn_source_put),
	SOC_ENUM_EXT("TDMOUT_O05_Source_Select",
		     mt8518_afe_soc_enums[CTRL_TDMOUT_CONN_SRC],
		     mt8518_afe_tdm_conn_source_get,
		     mt8518_afe_tdm_conn_source_put),
	SOC_ENUM_EXT("TDMOUT_O06_Source_Select",
		     mt8518_afe_soc_enums[CTRL_TDMOUT_CONN_SRC],
		     mt8518_afe_tdm_conn_source_get,
		     mt8518_afe_tdm_conn_source_put),
	SOC_ENUM_EXT("TDMOUT_O07_Source_Select",
		     mt8518_afe_soc_enums[CTRL_TDMOUT_CONN_SRC],
		     mt8518_afe_tdm_conn_source_get,
		     mt8518_afe_tdm_conn_source_put),
	SOC_ENUM_EXT("TDMOUT_O08_Source_Select",
		     mt8518_afe_soc_enums[CTRL_TDMOUT_CONN_SRC],
		     mt8518_afe_tdm_conn_source_get,
		     mt8518_afe_tdm_conn_source_put),
	SOC_ENUM_EXT("TDMOUT_O09_Source_Select",
		     mt8518_afe_soc_enums[CTRL_TDMOUT_CONN_SRC],
		     mt8518_afe_tdm_conn_source_get,
		     mt8518_afe_tdm_conn_source_put),
	SOC_ENUM_EXT("TDMOUT_O10_Source_Select",
		     mt8518_afe_soc_enums[CTRL_TDMOUT_CONN_SRC],
		     mt8518_afe_tdm_conn_source_get,
		     mt8518_afe_tdm_conn_source_put),
	SOC_ENUM_EXT("TDMOUT_O11_Source_Select",
		     mt8518_afe_soc_enums[CTRL_TDMOUT_CONN_SRC],
		     mt8518_afe_tdm_conn_source_get,
		     mt8518_afe_tdm_conn_source_put),
	SOC_ENUM_EXT("TDMOUT_O12_Source_Select",
		     mt8518_afe_soc_enums[CTRL_TDMOUT_CONN_SRC],
		     mt8518_afe_tdm_conn_source_get,
		     mt8518_afe_tdm_conn_source_put),
	SOC_ENUM_EXT("TDMOUT_O13_Source_Select",
		     mt8518_afe_soc_enums[CTRL_TDMOUT_CONN_SRC],
		     mt8518_afe_tdm_conn_source_get,
		     mt8518_afe_tdm_conn_source_put),
	SOC_ENUM_EXT("TDMOUT_O14_Source_Select",
		     mt8518_afe_soc_enums[CTRL_TDMOUT_CONN_SRC],
		     mt8518_afe_tdm_conn_source_get,
		     mt8518_afe_tdm_conn_source_put),
	SOC_ENUM_EXT("TDMOUT_O15_Source_Select",
		     mt8518_afe_soc_enums[CTRL_TDMOUT_CONN_SRC],
		     mt8518_afe_tdm_conn_source_get,
		     mt8518_afe_tdm_conn_source_put),
	SOC_SINGLE_BOOL_EXT("Spdif_Output_IEC61937_Switch",
			    0,
			    mt8518_afe_spdif_output_iec61937_get,
			    mt8518_afe_spdif_output_iec61937_put),
	SOC_ENUM("Multi_In_Bck_Select",
		 mt8518_afe_soc_enums[CTRL_MULTI_IN_BCK_SEL]),
	SOC_ENUM("Multi_In_Lrck_Select",
		 mt8518_afe_soc_enums[CTRL_MULTI_IN_LRCK_SEL]),
	SOC_ENUM("Multi_In_Sdata0_Select",
		 mt8518_afe_soc_enums[CTRL_MULTI_IN_SDATA0_SEL]),
	SOC_ENUM("Multi_In_Sdata1_Select",
		 mt8518_afe_soc_enums[CTRL_MULTI_IN_SDATA1_SEL]),
	SOC_ENUM("Multi_In_Sdata2_Select",
		 mt8518_afe_soc_enums[CTRL_MULTI_IN_SDATA2_SEL]),
	SOC_ENUM("Multi_In_Sdata3_Select",
		 mt8518_afe_soc_enums[CTRL_MULTI_IN_SDATA3_SEL]),
	SOC_ENUM_EXT("Spdif_In_Port_Select",
		     mt8518_afe_soc_enums[CTRL_SPDIF_IN_PORT_SEL],
		     mt8518_afe_spdif_in_port_get,
		     mt8518_afe_spdif_in_port_put),
	SND_SOC_CTL_RO("Spdif_In_Channel_Status_Data",
		       mt8518_afe_spdif_in_chs_data_info,
		       mt8518_afe_spdif_in_chs_data_get),
	SND_SOC_CTL_RO(SNDRV_CTL_NAME_IEC958("", CAPTURE, DEFAULT),
		       mt8518_afe_spdif_in_iec958_info,
		       mt8518_afe_spdif_in_iec958_get),
	SND_SOC_CTL_RO("Spdif_In_Rate",
		       mt8518_afe_spdif_in_rate_info,
		       mt8518_afe_spdif_in_rate_get),
	SND_SOC_CTL_RO("Spdif_In_Stream_Type",
			mt8518_afe_spdif_in_stream_type_info,
			mt8518_afe_spdif_in_stream_type_get),
	SND_SOC_CTL_RO("Spdif_In_User_Code",
			mt8518_afe_spdif_in_user_code_info,
			mt8518_afe_spdif_in_user_code_get),
	SND_SOC_CTL_RO("Spdif_In_Info",
			mt8518_afe_spdif_in_information_info,
			mt8518_afe_spdif_in_information_get),
	SOC_SINGLE("Spdif_Out_to_Spdif_In_Loopback_Switch",
		   AFE_SPDIFIN_CFG1, 14, 1, 0),
	SOC_SINGLE_EXT("Apll1_Clk", 0, 0,
		MT8518_APLL1_RATE + MT8518_APLL_ADJUST_RANGE, 0,
		mt8518_apll1_clk_get, mt8518_apll1_clk_set),
	SOC_SINGLE_EXT("Apll2_Clk", 0, 0,
		MT8518_APLL2_RATE + MT8518_APLL_ADJUST_RANGE, 0,
		mt8518_apll2_clk_get, mt8518_apll2_clk_set),
};

int mt8518_afe_add_controls(struct snd_soc_platform *platform)
{
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(platform);
	struct mt8518_afe_private *afe_priv = afe->platform_priv;

	afe_priv->card = platform->component.card->snd_card;

	return snd_soc_add_platform_controls(platform, mt8518_afe_controls,
					     ARRAY_SIZE(mt8518_afe_controls));
}

