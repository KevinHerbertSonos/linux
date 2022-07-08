/*
 * mt8518-afe-utils.c  --  Mediatek 8518 audio utility
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

#include "mt8518-afe-utils.h"
#include "mt8518-afe-common.h"
#include "mt8518-reg.h"
#include "../common/mtk-base-afe.h"
#include <linux/device.h>

#ifdef COMMON_CLOCK_FRAMEWORK_API
static const char *aud_clks[MT8518_CLK_NUM] = {
	[MT8518_CLK_TOP_AUD_26M] = "top_aud_26m",
	[MT8518_CLK_TOP_AUD_BUS] = "top_aud_intbus",
	[MT8518_CLK_FA1SYS] = "fa1sys",
	[MT8518_CLK_FA2SYS] = "fa2sys",
	[MT8518_CLK_HAPLL1] = "hapll1",
	[MT8518_CLK_HAPLL2] = "hapll2",
	[MT8518_CLK_AUD1] = "aud1",
	[MT8518_CLK_AUD2] = "aud2",
	[MT8518_CLK_FASM_L] = "fasm_l",
	[MT8518_CLK_FASM_M] = "fasm_m",
	[MT8518_CLK_FASM_H] = "fasm_h",
	[MT8518_CLK_SPDIF_IN] = "spdif_in",
	[MT8518_CLK_APLL12_DIV0] = "apll12_div0",
	[MT8518_CLK_APLL12_DIV3] = "apll12_div3",
	[MT8518_CLK_APLL12_DIV4] = "apll12_div4",
	[MT8518_CLK_APLL12_DIV6] = "apll12_div6",
	[MT8518_CLK_I2S0_M_SEL] =  "i2s0_m_sel",
	[MT8518_CLK_I2S3_M_SEL] =  "i2s3_m_sel",
	[MT8518_CLK_I2S4_M_SEL] =  "i2s4_m_sel",
	[MT8518_CLK_I2S6_M_SEL] =  "i2s6_m_sel",
	[MT8518_CLK_APLL1] =  "apll1",
	[MT8518_CLK_APLL2] =  "apll2",
};
#endif

int mt8518_afe_init_audio_clk(struct mtk_base_afe *afe)
{
#ifdef COMMON_CLOCK_FRAMEWORK_API
	size_t i;
	struct mt8518_afe_private *afe_priv = afe->platform_priv;

	for (i = 0; i < ARRAY_SIZE(aud_clks); i++) {
		afe_priv->clocks[i] = devm_clk_get(afe->dev, aud_clks[i]);
		if (IS_ERR(afe_priv->clocks[i])) {
			dev_err(afe->dev, "%s devm_clk_get %s fail\n",
				__func__, aud_clks[i]);
			return PTR_ERR(afe_priv->clocks[i]);
		}
	}
#endif
	return 0;
}

int mt8518_afe_enable_clk(struct mtk_base_afe *afe, struct clk *clk)
{
#ifdef COMMON_CLOCK_FRAMEWORK_API
	int ret;

	if (clk) {
		ret = clk_prepare_enable(clk);
		if (ret) {
			dev_err(afe->dev, "Failed to enable clk\n");
			return ret;
		}
	} else {
		dev_err(afe->dev, "null clk\n");
	}
#endif
	return 0;
}

void mt8518_afe_disable_clk(struct mtk_base_afe *afe, struct clk *clk)
{
#ifdef COMMON_CLOCK_FRAMEWORK_API
	if (clk)
		clk_disable_unprepare(clk);
	else
		dev_err(afe->dev, "null clk\n");
#endif
}

int mt8518_afe_set_clk_rate(struct mtk_base_afe *afe, struct clk *clk,
			    unsigned int rate)
{
#ifdef COMMON_CLOCK_FRAMEWORK_API
	int ret;

	if (clk) {
		ret = clk_set_rate(clk, rate);
		if (ret) {
			dev_err(afe->dev, "Failed to set rate\n");
			return ret;
		}
	}
#endif
	return 0;
}

int mt8518_afe_set_clk_parent(struct mtk_base_afe *afe, struct clk *clk,
			      struct clk *parent)
{
#ifdef COMMON_CLOCK_FRAMEWORK_API
	int ret;

	if (clk && parent) {
		ret = clk_set_parent(clk, parent);
		if (ret) {
			dev_err(afe->dev, "Failed to set parent\n");
			return ret;
		}
	}
#endif
	return 0;
}

static unsigned int get_top_cg_reg(unsigned int cg_type)
{
	switch (cg_type) {
	case MT8518_TOP_CG_AFE:
	case MT8518_TOP_CG_APLL:
	case MT8518_TOP_CG_APLL2:
	case MT8518_TOP_CG_DAC:
	case MT8518_TOP_CG_DAC_PREDIS:
	case MT8518_TOP_CG_ADC:
	case MT8518_TOP_CG_TML:
	case MT8518_TOP_CG_UPLINK_TML:
	case MT8518_TOP_CG_SPDIF_OUT:
		return AUDIO_TOP_CON0;
	case MT8518_TOP_CG_I2S_IN:
	case MT8518_TOP_CG_TDM_IN:
	case MT8518_TOP_CG_I2S_OUT:
	case MT8518_TOP_CG_TDM_OUT:
	case MT8518_TOP_CG_ASRC11:
	case MT8518_TOP_CG_ASRC12:
	case MT8518_TOP_CG_DL_ASRC:
	case MT8518_TOP_CG_A1SYS:
	case MT8518_TOP_CG_A2SYS:
	case MT8518_TOP_CG_AFE_CONN:
	case MT8518_TOP_CG_PCMIF:
	case MT8518_TOP_CG_GASRC0:
	case MT8518_TOP_CG_GASRC1:
	case MT8518_TOP_CG_GASRC2:
	case MT8518_TOP_CG_GASRC3:
	case MT8518_TOP_CG_MULTI_IN:
	case MT8518_TOP_CG_INTDIR:
		return AUDIO_TOP_CON4;
	case MT8518_TOP_CG_DMIC0:
	case MT8518_TOP_CG_DMIC1:
	case MT8518_TOP_CG_DMIC2:
	case MT8518_TOP_CG_DMIC3:
		return PWR2_TOP_CON;
	case MT8518_TOP_CG_A1SYS_TIMING:
	case MT8518_TOP_CG_A2SYS_TIMING:
		return ASYS_TOP_CON;
	default:
		return 0;
	}
}

static unsigned int get_top_cg_mask(unsigned int cg_type)
{
	switch (cg_type) {
	case MT8518_TOP_CG_AFE:
		return AUD_TCON0_PDN_AFE;
	case MT8518_TOP_CG_APLL:
		return AUD_TCON0_PDN_APLL;
	case MT8518_TOP_CG_APLL2:
		return AUD_TCON0_PDN_APLL2;
	case MT8518_TOP_CG_DAC:
		return AUD_TCON0_PDN_DAC;
	case MT8518_TOP_CG_DAC_PREDIS:
		return AUD_TCON0_PDN_DAC_PREDIS;
	case MT8518_TOP_CG_ADC:
		return AUD_TCON0_PDN_ADC;
	case MT8518_TOP_CG_TML:
		return AUD_TCON0_PDN_TML;
	case MT8518_TOP_CG_UPLINK_TML:
		return AUD_TCON0_PDN_UPLINK_TML;
	case MT8518_TOP_CG_SPDIF_OUT:
		return AUD_TCON0_PDN_SPDIF_OUT;
	case MT8518_TOP_CG_I2S_IN:
		return AUD_TCON4_PDN_I2S_IN;
	case MT8518_TOP_CG_TDM_IN:
		return AUD_TCON4_PDN_TDM_IN;
	case MT8518_TOP_CG_I2S_OUT:
		return AUD_TCON4_PDN_I2S_OUT;
	case MT8518_TOP_CG_TDM_OUT:
		return AUD_TCON4_PDN_TDM_OUT;
	case MT8518_TOP_CG_ASRC11:
		return AUD_TCON4_PDN_ASRC11;
	case MT8518_TOP_CG_ASRC12:
		return AUD_TCON4_PDN_ASRC12;
	case MT8518_TOP_CG_DL_ASRC:
		return AUD_TCON4_PDN_DL_ASRC;
	case MT8518_TOP_CG_A1SYS:
		return AUD_TCON4_PDN_A1SYS;
	case MT8518_TOP_CG_A2SYS:
		return AUD_TCON4_PDN_A2SYS;
	case MT8518_TOP_CG_MULTI_IN:
		return AUD_TCON4_PDN_MULTI_IN;
	case MT8518_TOP_CG_INTDIR:
		return AUD_TCON4_PDN_INTDIR;
	case MT8518_TOP_CG_AFE_CONN:
		return AUD_TCON4_PDN_AFE_CONN;
	case MT8518_TOP_CG_PCMIF:
		return AUD_TCON4_PDN_PCMIF;
	case MT8518_TOP_CG_GASRC0:
		return AUD_TCON4_PDN_GASRC0;
	case MT8518_TOP_CG_GASRC1:
		return AUD_TCON4_PDN_GASRC1;
	case MT8518_TOP_CG_GASRC2:
		return AUD_TCON4_PDN_GASRC2;
	case MT8518_TOP_CG_GASRC3:
		return AUD_TCON4_PDN_GASRC3;
	case MT8518_TOP_CG_DMIC0:
		return PWR2_TOP_CON_PDN_DMIC0;
	case MT8518_TOP_CG_DMIC1:
		return PWR2_TOP_CON_PDN_DMIC1;
	case MT8518_TOP_CG_DMIC2:
		return PWR2_TOP_CON_PDN_DMIC2;
	case MT8518_TOP_CG_DMIC3:
		return PWR2_TOP_CON_PDN_DMIC3;
	case MT8518_TOP_CG_A1SYS_TIMING:
		return ASYS_TCON_A1SYS_TIMING_ON;
	case MT8518_TOP_CG_A2SYS_TIMING:
		return ASYS_TCON_A2SYS_TIMING_ON;
	default:
		return 0;
	}
}

static unsigned int get_top_cg_on_val(unsigned int cg_type)
{
	switch (cg_type) {
	case MT8518_TOP_CG_A1SYS_TIMING:
	case MT8518_TOP_CG_A2SYS_TIMING:
		return get_top_cg_mask(cg_type);
	default:
		return 0;
	}
}

static unsigned int get_top_cg_off_val(unsigned int cg_type)
{
	switch (cg_type) {
	case MT8518_TOP_CG_A1SYS_TIMING:
	case MT8518_TOP_CG_A2SYS_TIMING:
		return 0;
	default:
		return get_top_cg_mask(cg_type);
	}
}

int mt8518_afe_enable_top_cg(struct mtk_base_afe *afe, unsigned int cg_type)
{
	struct mt8518_afe_private *afe_priv = afe->platform_priv;
	unsigned int reg = get_top_cg_reg(cg_type);
	unsigned int mask = get_top_cg_mask(cg_type);
	unsigned int val = get_top_cg_on_val(cg_type);
	unsigned long flags;
	bool need_update = false;

	spin_lock_irqsave(&afe_priv->afe_ctrl_lock, flags);

	afe_priv->top_cg_ref_cnt[cg_type]++;
	if (afe_priv->top_cg_ref_cnt[cg_type] == 1)
		need_update = true;

	spin_unlock_irqrestore(&afe_priv->afe_ctrl_lock, flags);

	if (need_update)
		regmap_update_bits(afe->regmap, reg, mask, val);

	return 0;
}

int mt8518_afe_disable_top_cg(struct mtk_base_afe *afe, unsigned int cg_type)
{
	struct mt8518_afe_private *afe_priv = afe->platform_priv;
	unsigned int reg = get_top_cg_reg(cg_type);
	unsigned int mask = get_top_cg_mask(cg_type);
	unsigned int val = get_top_cg_off_val(cg_type);
	unsigned long flags;
	bool need_update = false;

	spin_lock_irqsave(&afe_priv->afe_ctrl_lock, flags);

	afe_priv->top_cg_ref_cnt[cg_type]--;
	if (afe_priv->top_cg_ref_cnt[cg_type] == 0)
		need_update = true;
	else if (afe_priv->top_cg_ref_cnt[cg_type] < 0)
		afe_priv->top_cg_ref_cnt[cg_type] = 0;

	spin_unlock_irqrestore(&afe_priv->afe_ctrl_lock, flags);

	if (need_update)
		regmap_update_bits(afe->regmap, reg, mask, val);

	return 0;
}

int mt8518_afe_enable_main_clk(struct mtk_base_afe *afe)
{
	struct mt8518_afe_private *afe_priv = afe->platform_priv;

	mt8518_afe_enable_clk(afe, afe_priv->clocks[MT8518_CLK_TOP_AUD_BUS]);
	mt8518_afe_enable_clk(afe, afe_priv->clocks[MT8518_CLK_FA1SYS]);
	mt8518_afe_enable_clk(afe, afe_priv->clocks[MT8518_CLK_FA2SYS]);

	mt8518_afe_enable_top_cg(afe, MT8518_TOP_CG_A1SYS);
	mt8518_afe_enable_top_cg(afe, MT8518_TOP_CG_AFE);
	mt8518_afe_enable_top_cg(afe, MT8518_TOP_CG_A2SYS);
	mt8518_afe_enable_top_cg(afe, MT8518_TOP_CG_A1SYS_TIMING);
	mt8518_afe_enable_top_cg(afe, MT8518_TOP_CG_A2SYS_TIMING);
	mt8518_afe_enable_top_cg(afe, MT8518_TOP_CG_AFE_CONN);

	mt8518_afe_enable_afe_on(afe);

	return 0;
}

int mt8518_afe_disable_main_clk(struct mtk_base_afe *afe)
{
	struct mt8518_afe_private *afe_priv = afe->platform_priv;

	mt8518_afe_disable_afe_on(afe);

	mt8518_afe_disable_top_cg(afe, MT8518_TOP_CG_AFE_CONN);
	mt8518_afe_disable_top_cg(afe, MT8518_TOP_CG_A2SYS_TIMING);
	mt8518_afe_disable_top_cg(afe, MT8518_TOP_CG_A1SYS_TIMING);
	mt8518_afe_disable_top_cg(afe, MT8518_TOP_CG_A2SYS);
	mt8518_afe_disable_top_cg(afe, MT8518_TOP_CG_A1SYS);
	mt8518_afe_disable_top_cg(afe, MT8518_TOP_CG_AFE);

	mt8518_afe_disable_clk(afe, afe_priv->clocks[MT8518_CLK_FA1SYS]);
	mt8518_afe_disable_clk(afe, afe_priv->clocks[MT8518_CLK_FA2SYS]);
	mt8518_afe_disable_clk(afe, afe_priv->clocks[MT8518_CLK_TOP_AUD_BUS]);

	return 0;
}

int mt8518_afe_enable_reg_rw_clk(struct mtk_base_afe *afe)
{
	struct mt8518_afe_private *afe_priv = afe->platform_priv;

	mt8518_afe_enable_clk(afe, afe_priv->clocks[MT8518_CLK_TOP_AUD_BUS]);
	mt8518_afe_enable_clk(afe, afe_priv->clocks[MT8518_CLK_FA1SYS]);
	mt8518_afe_enable_top_cg(afe, MT8518_TOP_CG_A1SYS);
	mt8518_afe_enable_top_cg(afe, MT8518_TOP_CG_AFE);
	mt8518_afe_enable_top_cg(afe, MT8518_TOP_CG_AFE_CONN);
	return 0;
}

int mt8518_afe_disable_reg_rw_clk(struct mtk_base_afe *afe)
{
	struct mt8518_afe_private *afe_priv = afe->platform_priv;

	mt8518_afe_disable_top_cg(afe, MT8518_TOP_CG_AFE_CONN);
	mt8518_afe_disable_top_cg(afe, MT8518_TOP_CG_A1SYS);
	mt8518_afe_disable_top_cg(afe, MT8518_TOP_CG_AFE);
	mt8518_afe_disable_clk(afe, afe_priv->clocks[MT8518_CLK_FA1SYS]);
	mt8518_afe_disable_clk(afe, afe_priv->clocks[MT8518_CLK_TOP_AUD_BUS]);
	return 0;
}

int mt8518_afe_enable_afe_on(struct mtk_base_afe *afe)
{
	struct mt8518_afe_private *afe_priv = afe->platform_priv;
	unsigned long flags;
	bool need_update = false;

	spin_lock_irqsave(&afe_priv->afe_ctrl_lock, flags);

	afe_priv->afe_on_ref_cnt++;
	if (afe_priv->afe_on_ref_cnt == 1)
		need_update = true;

	spin_unlock_irqrestore(&afe_priv->afe_ctrl_lock, flags);

	if (need_update)
		regmap_update_bits(afe->regmap, AFE_DAC_CON0, 0x1, 0x1);

	return 0;
}

int mt8518_afe_disable_afe_on(struct mtk_base_afe *afe)
{
	struct mt8518_afe_private *afe_priv = afe->platform_priv;
	unsigned long flags;
	bool need_update = false;

	spin_lock_irqsave(&afe_priv->afe_ctrl_lock, flags);

	afe_priv->afe_on_ref_cnt--;
	if (afe_priv->afe_on_ref_cnt == 0)
		need_update = true;
	else if (afe_priv->afe_on_ref_cnt < 0)
		afe_priv->afe_on_ref_cnt = 0;

	spin_unlock_irqrestore(&afe_priv->afe_ctrl_lock, flags);

	if (need_update)
		regmap_update_bits(afe->regmap, AFE_DAC_CON0, 0x1, 0x0);

	return 0;
}

static const struct mtk_base_irq_data *mt8518_get_irq_data(
	struct mtk_base_afe *afe, unsigned int irq_id) {
	int i;

	for (i = 0; i < afe->irqs_size; i++) {
		if (irq_id == afe->irqs->irq_data[i].id)
			return &afe->irqs->irq_data[i];
	}

	return NULL;
}

int mt8518_afe_enable_irq(struct mtk_base_afe *afe, unsigned int irq_id)
{
	const struct mtk_base_irq_data *data;

	data = mt8518_get_irq_data(afe, irq_id);
	if (!data)
		return -EINVAL;

	regmap_update_bits(afe->regmap,
			   data->irq_en_reg,
			   1 << data->irq_en_shift,
			   1 << data->irq_en_shift);

	return 0;
}

int mt8518_afe_disable_irq(struct mtk_base_afe *afe, unsigned int irq_id)
{
	const struct mtk_base_irq_data *data;

	data = mt8518_get_irq_data(afe, irq_id);
	if (!data)
		return -EINVAL;

	regmap_update_bits(afe->regmap,
			   data->irq_en_reg,
			   1 << data->irq_en_shift,
			   0 << data->irq_en_shift);

	return 0;
}

static int mt8518_afe_enable_spdif_in(struct mtk_base_afe *afe)
{
	struct mt8518_afe_private *afe_priv = afe->platform_priv;
	unsigned int port_sel;
	unsigned int mux_sel;
	unsigned int port = afe_priv->spdif_in_data.port;

	switch (port) {
	case SPDIF_IN_PORT_OPT:
	case SPDIF_IN_PORT_COAXIAL:
	case SPDIF_IN_PORT_ARC:
		mux_sel = afe_priv->spdif_in_data.ports_mux[port];
		break;
	case SPDIF_IN_PORT_NONE:
	default:
		dev_notice(afe->dev, "%s unexpected port %u\n",
			   __func__, port);
		return -EINVAL;
	}

	switch (mux_sel) {
	case SPDIF_IN_MUX_0:
		port_sel = AFE_SPDIFIN_INT_EXT_SEL_OPTICAL;
		break;
	case SPDIF_IN_MUX_1:
		port_sel = AFE_SPDIFIN_INT_EXT_SEL_COAXIAL;
		break;
	case SPDIF_IN_MUX_2:
		port_sel = AFE_SPDIFIN_INT_EXT_SEL_ARC;
		break;
	default:
		dev_notice(afe->dev, "%s unexpected mux_sel %u\n",
			   __func__, mux_sel);
		return -EINVAL;
	}

	dev_dbg(afe->dev, "%s port %u mux_sel %u\n", __func__, port, mux_sel);

	mt8518_afe_enable_main_clk(afe);

	mt8518_afe_enable_clk(afe, afe_priv->clocks[MT8518_CLK_SPDIF_IN]);

	mt8518_afe_enable_top_cg(afe, MT8518_TOP_CG_INTDIR);

	regmap_write(afe->regmap, SPDIFIN_FREQ_INFO, 0x00877986);
	regmap_write(afe->regmap, SPDIFIN_FREQ_INFO_2, 0x006596e8);
	regmap_write(afe->regmap, SPDIFIN_FREQ_INFO_3, 0x000005a5);
	regmap_write(afe->regmap, AFE_SPDIFIN_BR, 0x00039000);

	regmap_update_bits(afe->regmap,
			   AFE_SPDIFIN_INT_EXT2,
			   SPDIFIN_594MODE_MASK,
			   SPDIFIN_594MODE_EN);

	regmap_update_bits(afe->regmap,
			   AFE_SPDIFIN_CFG1,
			   AFE_SPDIFIN_CFG1_SET_MASK,
			   AFE_SPDIFIN_CFG1_INT_BITS |
			   AFE_SPDIFIN_CFG1_SEL_BCK_SPDIFIN |
			   AFE_SPDIFIN_CFG1_FIFOSTART_5POINTS |
			   AFE_SPDIFIN_CFG1_SEL_DEC0_CLK_EN |
			   AFE_SPDIFIN_CFG1_SEL_DEC0_DATA_EN);

	regmap_update_bits(afe->regmap,
			   AFE_SPDIFIN_INT_EXT,
			   AFE_SPDIFIN_INT_EXT_SET_MASK,
			   AFE_SPDIFIN_INT_EXT_DATALAT_ERR_EN |
			   AFE_SPDIFIN_INT_EXT_DERR_NEW_RETEN |
			   port_sel);

	regmap_update_bits(afe->regmap,
			   AFE_SPDIFIN_CFG0,
			   AFE_SPDIFIN_CFG0_SET_MASK,
			   AFE_SPDIFIN_CFG0_FLIP |
			   AFE_SPDIFIN_CFG0_GMAT_BC_256_CYCLES |
			   AFE_SPDIFIN_CFG0_DPERR2IDLE_EN |
			   AFE_SPDIFIN_CFG0_DERR2IDLE_EN |
			   AFE_SPDIFIN_CFG0_INT_EN |
			   AFE_SPDIFIN_CFG0_TIMEOUT2IDLE_EN |
			   AFE_SPDIFIN_CFG0_DE_CNT(4) |
			   AFE_SPDIFIN_CFG0_DE_SEL_CNT |
			   AFE_SPDIFIN_CFG0_MAX_LEN_NUM(237));

	mt8518_afe_enable_irq(afe, MT8518_AFE_IRQ2);

	regmap_update_bits(afe->regmap,
			   AFE_SPDIFIN_CFG0,
			   AFE_SPDIFIN_CFG0_EN,
			   AFE_SPDIFIN_CFG0_EN);

	return 0;
}

static int mt8518_afe_disable_spdif_in(struct mtk_base_afe *afe)
{
	struct mt8518_afe_private *afe_priv = afe->platform_priv;

	dev_dbg(afe->dev, "%s\n", __func__);

        regmap_update_bits(afe->regmap,
			   AFE_SPDIFIN_INT_EXT2,
			   AFE_SPDIFIN_INT_EXT2_LRCK_CHANGE_EN_MASK,
			   0x0);

	regmap_update_bits(afe->regmap,
			   AFE_SPDIFIN_CFG0,
			   AFE_SPDIFIN_CFG0_EN,
			   0x0);

	mt8518_afe_disable_irq(afe, MT8518_AFE_IRQ2);

	mt8518_afe_disable_top_cg(afe, MT8518_TOP_CG_INTDIR);

	mt8518_afe_disable_clk(afe, afe_priv->clocks[MT8518_CLK_SPDIF_IN]);

	mt8518_afe_disable_main_clk(afe);

	return 0;
}

int mt8518_afe_handle_spdif_in_port_change(struct mtk_base_afe *afe,
	unsigned int port)
{
	struct mt8518_afe_private *afe_priv = afe->platform_priv;
	struct mt8518_spdif_in_data *spdif_in = &afe_priv->spdif_in_data;
	int ret = 0;

	if (port != SPDIF_IN_PORT_NONE &&
	    port != SPDIF_IN_PORT_OPT &&
	    port != SPDIF_IN_PORT_COAXIAL &&
	    port != SPDIF_IN_PORT_ARC)
		return -EINVAL;

	if (spdif_in->port != port) {
		if (spdif_in->port != SPDIF_IN_PORT_NONE)
			ret = mt8518_afe_disable_spdif_in(afe);

		if (port == SPDIF_IN_PORT_NONE) {
			/* sample rate */
			spdif_in->subdata.rate = 0;
			/* usercode */
			memset((void *)spdif_in->subdata.user_code,
				0xff, sizeof(spdif_in->subdata.user_code));
			/* channel status */
			memset((void *)spdif_in->subdata.ch_status,
				0xff, sizeof(spdif_in->subdata.ch_status));
			/* stream type */
			spdif_in->subdata.stream_type = 0xff;
		}

		spdif_in->port = port;

		if (spdif_in->port != SPDIF_IN_PORT_NONE)
			ret = mt8518_afe_enable_spdif_in(afe);
	}

	return ret;
}

int mt8518_afe_block_dpidle(struct mtk_base_afe *afe)
{
	struct mt8518_afe_private *afe_priv = afe->platform_priv;

	mutex_lock(&afe_priv->block_dpidle_mutex);

	if (afe_priv->block_dpidle_ref_cnt == 0) {
		regmap_update_bits(afe_priv->scpsys,
			BLOCK_DPIDLE_REG,
			BLOCK_DPIDLE_REG_MASK,
			BLOCK_DPIDLE_REG_BIT_ON);
	}

	afe_priv->block_dpidle_ref_cnt++;

	mutex_unlock(&afe_priv->block_dpidle_mutex);

	return 0;
}

int mt8518_afe_unblock_dpidle(struct mtk_base_afe *afe)
{
	struct mt8518_afe_private *afe_priv = afe->platform_priv;

	mutex_lock(&afe_priv->block_dpidle_mutex);

	afe_priv->block_dpidle_ref_cnt--;

	if (afe_priv->block_dpidle_ref_cnt == 0) {
		regmap_update_bits(afe_priv->scpsys,
			BLOCK_DPIDLE_REG,
			BLOCK_DPIDLE_REG_MASK,
			BLOCK_DPIDLE_REG_BIT_OFF);
	} else if (afe_priv->block_dpidle_ref_cnt < 0) {
		afe_priv->block_dpidle_ref_cnt = 0;
	}

	mutex_unlock(&afe_priv->block_dpidle_mutex);

	return 0;
}

int mt8518_afe_get_be_idx(int id)
{
	int index;

	if (id < MT8518_AFE_BACKEND_BASE || id >= MT8518_AFE_BACKEND_END)
		return -EINVAL;

	index = id - MT8518_AFE_BACKEND_BASE;

	return index;
}

int mt8518_afe_set_be_active(struct mtk_base_afe *afe, int id)
{
	struct mt8518_afe_private *afe_priv = afe->platform_priv;
	int index;

	index = mt8518_afe_get_be_idx(id);
	if (index < 0)
		return -EINVAL;

	afe_priv->be_active_status |= BIT(index);

	return 0;
}

int mt8518_afe_clear_be_active(struct mtk_base_afe *afe, int id)
{
	struct mt8518_afe_private *afe_priv = afe->platform_priv;
	int index;

	index = mt8518_afe_get_be_idx(id);
	if (index < 0)
		return -EINVAL;

	afe_priv->be_active_status &= ~BIT(index);

	return 0;
}

bool mt8518_afe_is_be_active(struct mtk_base_afe *afe, int id)
{
	struct mt8518_afe_private *afe_priv = afe->platform_priv;
	int index;

	index = mt8518_afe_get_be_idx(id);
	if (index < 0)
		return false;

	if (afe_priv->be_active_status & BIT(index))
		return true;

	return false;
}

