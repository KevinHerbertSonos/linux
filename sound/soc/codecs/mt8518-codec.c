/*
 * mt8518-codec.c  --  Mediatek 8518 ALSA SoC Codec driver
 *
 * Copyright (c) 2018 MediaTek Inc.
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

#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/mfd/syscon.h>
#include <linux/of_platform.h>
#include <linux/pm.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>

#include "mt8518-codec.h"

#define MT8518_CODEC_NAME "mt8518-codec"

enum regmap_module_id {
	REGMAP_AFE = 0,
	REGMAP_APMIXEDSYS,
	REGMAP_NUMS,
};

enum micbias0_out_vol_sel {
	MICBIAS0_VREF_1900mV = 0,
	MICBIAS0_VREF_2000mV,
	MICBIAS0_VREF_2100mV,
	MICBIAS0_VREF_2200mV,
};

struct mt8518_codec_priv {
	struct snd_soc_codec *codec;
	struct regmap *regmap;
	struct regmap *regmap_modules[REGMAP_NUMS];
	int adda_afe_on_ref_cnt;
	spinlock_t adda_afe_on_lock;
	struct clk *clk;
	unsigned int micbias0_setup_time_us;
	unsigned int micbias0_always_on;
	unsigned int micbias0_out_vol_sel;
	unsigned int dl_nle_support;
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs;
#endif
};

static int module_reg_read(void *context,
	unsigned int reg, unsigned int *val,
	enum regmap_module_id id, unsigned int offset)
{
	struct mt8518_codec_priv *mt8518_codec =
			(struct mt8518_codec_priv *) context;
	int ret = 0;

	if (!(mt8518_codec && mt8518_codec->regmap_modules[id]))
		return -EIO;

	ret = regmap_read(mt8518_codec->regmap_modules[id],
			(reg & (~offset)), val);

	return ret;
}

static int module_reg_write(void *context,
	unsigned int reg, unsigned int val,
	enum regmap_module_id id, unsigned int offset)
{
	struct mt8518_codec_priv *mt8518_codec =
			(struct mt8518_codec_priv *) context;
	int ret = 0;

	if (!(mt8518_codec && mt8518_codec->regmap_modules[id]))
		return -EIO;

	ret = regmap_write(mt8518_codec->regmap_modules[id],
			(reg & (~offset)), val);

	return ret;
}

struct mt8518_codec_module_info {
	int id;
	unsigned int offset;
};

static const struct mt8518_codec_module_info mt8518_codec_modules[] = {
	{ .id = REGMAP_AFE, .offset = AFE_OFFSET },
	{ .id = REGMAP_APMIXEDSYS, .offset = APMIXED_OFFSET },
};

static int mt8518_codec_found_module_id(unsigned int reg)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mt8518_codec_modules); i++)
		if (reg & mt8518_codec_modules[i].offset)
			return mt8518_codec_modules[i].id;

	return -EINVAL;
}

static unsigned int mt8518_codec_found_module_offset(unsigned int reg)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mt8518_codec_modules); i++)
		if (reg & mt8518_codec_modules[i].offset)
			return mt8518_codec_modules[i].offset;

	return 0;
}

static int mt8518_codec_reg_read(void *context,
	unsigned int reg, unsigned int *val)
{
	int id;
	unsigned int offset;

	id = mt8518_codec_found_module_id(reg);
	offset = mt8518_codec_found_module_offset(reg);

	if ((id < 0) || (offset == 0))
		return -EIO;

	return module_reg_read(context, reg, val,
			id, offset);
}

static int mt8518_codec_reg_write(void *context,
		unsigned int reg, unsigned int val)
{
	int id;
	unsigned int offset;

	id = mt8518_codec_found_module_id(reg);
	offset = mt8518_codec_found_module_offset(reg);

	if ((id < 0) || (offset == 0))
		return -EIO;

	return module_reg_write(context, reg, val,
			id, offset);
}

static void mt8518_codec_regmap_lock(void *lock_arg)
{
}

static void mt8518_codec_regmap_unlock(void *lock_arg)
{
}

static const struct regmap_config mt8518_codec_regmap = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_read = mt8518_codec_reg_read,
	.reg_write = mt8518_codec_reg_write,
	.lock = mt8518_codec_regmap_lock,
	.unlock = mt8518_codec_regmap_unlock,
	.cache_type = REGCACHE_NONE,
};

/* Audio Amp Playback Volume
 * {-2, 0, +2, +4, +6, +8, +10, +12} dB
 */
static const DECLARE_TLV_DB_SCALE(dl_audio_amp_gain_tlv, -200, 200, 0);

/* Voice Amp Playback Volume
 * {-18, -16, -14, -12, -10, ..., +12} dB
 */
static const DECLARE_TLV_DB_SCALE(dl_voice_amp_gain_tlv, -1800, 200, 0);

/* PGA Capture Volume
 * {-6, 0, +6, +12, +18, +24} dB
 */
static const DECLARE_TLV_DB_SCALE(ul_pga_gain_tlv, -600, 600, 0);

static const struct snd_kcontrol_new mt8518_codec_snd_controls[] = {
	/* DL Audio amplifier gain adjustment */
	SOC_DOUBLE_TLV("Audio Amp Playback Volume",
		AUDIO_CODEC_CON00, 0, 3, 7, 0,
		dl_audio_amp_gain_tlv),
	/* DL Voice amplifier gain adjustment */
	SOC_SINGLE_TLV("Voice Amp Playback Volume",
		AUDIO_CODEC_CON01, 11, 15, 0,
		dl_voice_amp_gain_tlv),
	/* UL PGA gain adjustment */
	SOC_DOUBLE_R_TLV("PGA Capture Volume",
		AADC_CODEC_CON00, AADC_CODEC_CON01, 19, 5, 0,
		ul_pga_gain_tlv),
};

static int mt8518_codec_enable_adda_afe_on(
	struct snd_soc_component *component)
{
	struct mt8518_codec_priv *mt8518_codec =
		snd_soc_component_get_drvdata(component);
	unsigned long flags;

	spin_lock_irqsave(&mt8518_codec->adda_afe_on_lock, flags);

	mt8518_codec->adda_afe_on_ref_cnt++;
	if (mt8518_codec->adda_afe_on_ref_cnt == 1)
		snd_soc_component_update_bits(component,
			AFE_ADDA_UL_DL_CON0, 0x1, 0x1);

	spin_unlock_irqrestore(&mt8518_codec->adda_afe_on_lock, flags);

	return 0;
}

static int mt8518_codec_disable_adda_afe_on(
	struct snd_soc_component *component)
{
	struct mt8518_codec_priv *mt8518_codec =
		snd_soc_component_get_drvdata(component);
	unsigned long flags;

	spin_lock_irqsave(&mt8518_codec->adda_afe_on_lock, flags);

	mt8518_codec->adda_afe_on_ref_cnt--;
	if (mt8518_codec->adda_afe_on_ref_cnt == 0)
		snd_soc_component_update_bits(component,
			AFE_ADDA_UL_DL_CON0, 0x1, 0x0);
	else if (mt8518_codec->adda_afe_on_ref_cnt < 0)
		mt8518_codec->adda_afe_on_ref_cnt = 0;

	spin_unlock_irqrestore(&mt8518_codec->adda_afe_on_lock, flags);

	return 0;
}

static int mt8518_codec_ana_clk_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_component_update_bits(component,
			AUDIO_CODEC_CON00,
			AUDIO_CODEC_CON00_AUD_LDO_EN_MASK,
			AUDIO_CODEC_CON00_AUD_LDO_EN_MASK);
		snd_soc_component_update_bits(component,
			AUDIO_CODEC_CON01,
			AUDIO_CODEC_CON01_CLKSQ_EN_MASK,
			AUDIO_CODEC_CON01_CLKSQ_EN_MASK);
		snd_soc_component_update_bits(component,
			AUDIO_CODEC_CON02,
			AUDIO_CODEC_CON02_AUD_CLKDIV4_EN_MASK,
			AUDIO_CODEC_CON02_AUD_CLKDIV4_EN_MASK);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component,
			AUDIO_CODEC_CON02,
			AUDIO_CODEC_CON02_AUD_CLKDIV4_EN_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDIO_CODEC_CON01,
			AUDIO_CODEC_CON01_CLKSQ_EN_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDIO_CODEC_CON00,
			AUDIO_CODEC_CON00_AUD_LDO_EN_MASK,
			0x0);
		break;
	default:
		return 0;
	}

	return 0;
}

static int mt8518_codec_ana_dac_clk_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		break;
	case SND_SOC_DAPM_POST_PMD:
		break;
	default:
		return 0;
	}

	return 0;
}

static int mt8518_codec_ana_adc_clk_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_component_update_bits(component,
			AUDIO_CODEC_CON04,
			AUDIO_CODEC_CON04_UL_CLK_GATE_EN_MASK,
			AUDIO_CODEC_CON04_UL_CLK_GATE_EN_MASK);
		snd_soc_component_update_bits(component,
			AADC_CODEC_CON02,
			AADC_CODEC_CON02_AUDUL_CLK_EN_MASK,
			AADC_CODEC_CON02_AUDUL_CLK_EN_MASK);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component,
			AADC_CODEC_CON02,
			AADC_CODEC_CON02_AUDUL_CLK_EN_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDIO_CODEC_CON04,
			AUDIO_CODEC_CON04_UL_CLK_GATE_EN_MASK,
			0x0);
		break;
	default:
		return 0;
	}

	return 0;
}

static int mt8518_codec_dig_adda_clk_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mt8518_codec_enable_adda_afe_on(component);
		break;
	case SND_SOC_DAPM_POST_PMD:
		mt8518_codec_disable_adda_afe_on(component);
		break;
	default:
		return 0;
	}

	return 0;
}

static int mt8518_codec_micbias0_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct mt8518_codec_priv *mt8518_codec =
		snd_soc_component_get_drvdata(component);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		usleep_range(mt8518_codec->micbias0_setup_time_us,
			mt8518_codec->micbias0_setup_time_us + 1);
		break;
	default:
		return 0;
	}

	return 0;
}

static int mt8518_codec_turn_on_micbias0(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);

	snd_soc_dapm_force_enable_pin(dapm, "AU_MICBIAS0");
	snd_soc_dapm_sync(dapm);

	return 0;
}

static int mt8518_codec_turn_off_micbias0(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);

	snd_soc_dapm_disable_pin(dapm, "AU_MICBIAS0");
	snd_soc_dapm_sync(dapm);

	return 0;
}

/* HPOUT Mux */
static const char * const hp_out_mux_text[] = {
	"OPEN", "AUDIO_AMP",
};

static SOC_ENUM_SINGLE_DECL(mt8518_codec_hp_out_mux_enum,
	SND_SOC_NOPM, 0, hp_out_mux_text);

static const struct snd_kcontrol_new mt8518_codec_hp_out_mux =
	SOC_DAPM_ENUM("HPOUT Mux", mt8518_codec_hp_out_mux_enum);

/* LINEOUT Mux  */
static const char * const line_out_mux_text[] = {
	"OPEN", "VOICE_AMP",
};

static SOC_ENUM_SINGLE_DECL(mt8518_codec_line_out_mux_enum,
	SND_SOC_NOPM, 0, line_out_mux_text);

static const struct snd_kcontrol_new mt8518_codec_line_out_mux =
	SOC_DAPM_ENUM("LINEOUT Mux", mt8518_codec_line_out_mux_enum);

/* Left PGA Mux */
static const char * const left_pga_mux_text[] = {
	"CH0", "CH1", "OPEN",
};

static SOC_ENUM_SINGLE_DECL(mt8518_codec_left_pga_mux_enum,
	AADC_CODEC_CON00, 22, left_pga_mux_text);

static const struct snd_kcontrol_new mt8518_codec_left_pga_mux =
	SOC_DAPM_ENUM("Left PGA Mux", mt8518_codec_left_pga_mux_enum);

/* Right PGA Mux */
static const char * const right_pga_mux_text[] = {
	"CH1", "CH0", "OPEN",
};

static SOC_ENUM_SINGLE_DECL(mt8518_codec_right_pga_mux_enum,
	AADC_CODEC_CON01, 22, right_pga_mux_text);

static const struct snd_kcontrol_new mt8518_codec_right_pga_mux =
	SOC_DAPM_ENUM("Right PGA Mux", mt8518_codec_right_pga_mux_enum);

static const struct snd_soc_dapm_widget mt8518_codec_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("AIF RX", "AIF Playback", 0,
		SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("Left DAC", NULL, AUDIO_CODEC_CON00, 15, 0),
	SND_SOC_DAPM_DAC("Right DAC", NULL, AUDIO_CODEC_CON00, 14, 0),
	SND_SOC_DAPM_PGA_S("Left Audio Amp", 1,
		AUDIO_CODEC_CON00, 12, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("Right Audio Amp", 1,
		AUDIO_CODEC_CON00, 11, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("HP Depop VCM", 2,
		AUDIO_CODEC_CON01, 28, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Voice Amp", AUDIO_CODEC_CON01, 10, 0, NULL, 0),
	SND_SOC_DAPM_MUX("HPOUT Mux", SND_SOC_NOPM, 0, 0,
		&mt8518_codec_hp_out_mux),
	SND_SOC_DAPM_MUX("LINEOUT Mux", SND_SOC_NOPM, 0, 0,
		&mt8518_codec_line_out_mux),

	SND_SOC_DAPM_AIF_OUT("AIF TX", "AIF Capture", 0,
		SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_ADC("Left ADC", NULL, AADC_CODEC_CON00, 17, 0),
	SND_SOC_DAPM_ADC("Right ADC", NULL, AADC_CODEC_CON01, 17, 0),
	SND_SOC_DAPM_PGA("Left PGA", AADC_CODEC_CON00, 18, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Right PGA", AADC_CODEC_CON01, 18, 0, NULL, 0),
	SND_SOC_DAPM_MUX("Left PGA Mux", SND_SOC_NOPM, 0, 0,
			&mt8518_codec_left_pga_mux),
	SND_SOC_DAPM_MUX("Right PGA Mux", SND_SOC_NOPM, 0, 0,
			&mt8518_codec_right_pga_mux),

	SND_SOC_DAPM_SUPPLY_S("ANA_CLK", 1, SND_SOC_NOPM, 0, 0,
		mt8518_codec_ana_clk_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("ANA_DAC_CLK", 2, SND_SOC_NOPM, 0, 0,
		mt8518_codec_ana_dac_clk_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("ANA_ADC_CLK", 2, SND_SOC_NOPM, 0, 0,
		mt8518_codec_ana_adc_clk_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("DIG_ADC_CLK", 3, SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DIG_DAC_CLK", 3, SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DIG_ADDA_CLK", 4, SND_SOC_NOPM, 0, 0,
		mt8518_codec_dig_adda_clk_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("ANA_DAC_Vcm14", 5,
		AUDIO_CODEC_CON00, 31, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ANA_ADC_L_Vcm14", 5,
		AADC_CODEC_CON00, 13, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ANA_ADC_R_Vcm14", 5,
		AADC_CODEC_CON01, 13, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ANA_DAC_Vref24", 5,
		AUDIO_CODEC_CON01, 18, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ANA_ADC_L_Vref24", 5,
		AADC_CODEC_CON00, 14, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ANA_ADC_R_Vref24", 5,
		AADC_CODEC_CON01, 14, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ANA_DAC_VCM1", 5,
		AUDIO_CODEC_CON00, 13, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ANA_DAC_VCM2", 5,
		AUDIO_CODEC_CON01, 19, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("AU_MICBIAS0", 6, AADC_CODEC_CON02, 21, 0,
		mt8518_codec_micbias0_event, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_INPUT("AU_VIN0"),
	SND_SOC_DAPM_INPUT("AU_VIN1"),
	SND_SOC_DAPM_OUTPUT("AU_HPL"),
	SND_SOC_DAPM_OUTPUT("AU_HPR"),
	SND_SOC_DAPM_OUTPUT("AU_LOL"),
};

static const struct snd_soc_dapm_route mt8518_codec_intercon[] = {
	{ "AIF TX", NULL, "ANA_CLK" },
	{ "AIF RX", NULL, "ANA_CLK" },
	{ "AIF TX", NULL, "ANA_ADC_CLK" },
	{ "AIF RX", NULL, "ANA_DAC_CLK" },
	{ "AIF TX", NULL, "DIG_ADC_CLK" },
	{ "AIF RX", NULL, "DIG_DAC_CLK" },
	{ "AIF TX", NULL, "DIG_ADDA_CLK" },
	{ "AIF RX", NULL, "DIG_ADDA_CLK" },
	{ "AIF TX", NULL, "ANA_ADC_L_Vcm14" },
	{ "AIF TX", NULL, "ANA_ADC_R_Vcm14" },
	{ "AIF RX", NULL, "ANA_DAC_Vcm14" },
	{ "AIF TX", NULL, "ANA_ADC_L_Vref24" },
	{ "AIF TX", NULL, "ANA_ADC_R_Vref24" },
	{ "AIF RX", NULL, "ANA_DAC_Vref24" },
	{ "AIF RX", NULL, "ANA_DAC_VCM1" },
	{ "AIF RX", NULL, "ANA_DAC_VCM2" },

	{"Left DAC", NULL, "AIF RX"},
	{"Right DAC", NULL, "AIF RX"},
	{"Left Audio Amp", NULL, "Left DAC"},
	{"Right Audio Amp", NULL, "Right DAC"},

	{"HP Depop VCM", NULL, "Left Audio Amp"},
	{"HP Depop VCM", NULL, "Right Audio Amp"},
	{"HPOUT Mux", "AUDIO_AMP", "HP Depop VCM"},
	{"AU_HPL", NULL, "HPOUT Mux"},
	{"AU_HPR", NULL, "HPOUT Mux"},

	{"Voice Amp", NULL, "Left DAC"},
	{"LINEOUT Mux", "VOICE_AMP", "Voice Amp"},
	{"AU_LOL", NULL, "LINEOUT Mux"},

	{"AIF TX", NULL, "Left ADC"},
	{"AIF TX", NULL, "Right ADC"},

	{"Left ADC", NULL, "Left PGA"},
	{"Right ADC", NULL, "Right PGA"},

	{"Left PGA", NULL, "Left PGA Mux"},
	{"Left PGA", NULL, "Left PGA Mux"},

	{"Right PGA", NULL, "Right PGA Mux"},
	{"Right PGA", NULL, "Right PGA Mux"},

	{"Left PGA Mux", "CH0", "AU_VIN0"},
	{"Left PGA Mux", "CH1", "AU_VIN1"},

	{"Right PGA Mux", "CH1", "AU_VIN0"},
	{"Right PGA Mux", "CH0", "AU_VIN1"},

	{"AU_VIN0", NULL, "AU_MICBIAS0"},
	{"AU_VIN1", NULL, "AU_MICBIAS0"},
};

struct mt8518_codec_rate {
	unsigned int rate;
	unsigned int regvalue;
};

static const struct mt8518_codec_rate mt8518_codec_ul_voice_modes[] = {
	{ .rate =  8000, .regvalue = 0, },
	{ .rate = 16000, .regvalue = 1, },
	{ .rate = 32000, .regvalue = 2, },
	{ .rate = 48000, .regvalue = 3, },
};

static const struct mt8518_codec_rate mt8518_codec_ul_rates[] = {
	{ .rate =  8000, .regvalue = 0, },
	{ .rate = 16000, .regvalue = 0, },
	{ .rate = 32000, .regvalue = 0, },
	{ .rate = 48000, .regvalue = 1, },
};

static const struct mt8518_codec_rate mt8518_codec_dl_voice_modes[] = {
	{ .rate =   8000, .regvalue = 1, },
	{ .rate =  11025, .regvalue = 0, },
	{ .rate =  12000, .regvalue = 0, },
	{ .rate =  16000, .regvalue = 1, },
	{ .rate =  22050, .regvalue = 0, },
	{ .rate =  24000, .regvalue = 0, },
	{ .rate =  32000, .regvalue = 0, },
	{ .rate =  44100, .regvalue = 0, },
	{ .rate =  48000, .regvalue = 0, },
};

static const struct mt8518_codec_rate mt8518_codec_dl_input_modes[] = {
	{ .rate =   8000, .regvalue = 0, },
	{ .rate =  11025, .regvalue = 1, },
	{ .rate =  12000, .regvalue = 2, },
	{ .rate =  16000, .regvalue = 3, },
	{ .rate =  22050, .regvalue = 4, },
	{ .rate =  24000, .regvalue = 5, },
	{ .rate =  32000, .regvalue = 6, },
	{ .rate =  44100, .regvalue = 7, },
	{ .rate =  48000, .regvalue = 8, },
};

static const struct mt8518_codec_rate mt8518_codec_dl_rates[] = {
	{ .rate =   8000, .regvalue = 0, },
	{ .rate =  11025, .regvalue = 1, },
	{ .rate =  12000, .regvalue = 2, },
	{ .rate =  16000, .regvalue = 3, },
	{ .rate =  22050, .regvalue = 4, },
	{ .rate =  24000, .regvalue = 5, },
	{ .rate =  32000, .regvalue = 6, },
	{ .rate =  44100, .regvalue = 7, },
	{ .rate =  48000, .regvalue = 8, },
};

static int mt8518_codec_rate_to_val(const struct mt8518_codec_rate *table,
	int table_nums, int rate)
{
	int i;

	for (i = 0; i < table_nums; i++)
		if (table[i].rate == rate)
			return table[i].regvalue;

	return -EINVAL;
}

struct mt8518_codec_rate_ctrl {
	unsigned int top_ctrl_reg;
	unsigned int top_ctrl_w_val;
	unsigned int top_ctrl_r_val;
};

static const
	struct mt8518_codec_rate_ctrl top_ctrl_regs[] = {
	[SNDRV_PCM_STREAM_PLAYBACK] = {
		.top_ctrl_reg	= ABB_AFE_CON11,
		.top_ctrl_w_val	= ABB_AFE_CON11_TOP_CTRL,
		.top_ctrl_r_val	= ABB_AFE_CON11_TOP_CTRL_STATUS,
	},
	[SNDRV_PCM_STREAM_CAPTURE] = {
		.top_ctrl_reg	= ABB_ULAFE_CON11,
		.top_ctrl_w_val	= ABB_ULAFE_CON11_TOP_CTRL,
		.top_ctrl_r_val	= ABB_ULAFE_CON11_TOP_CTRL_STATUS,
	},
};

static int mt8518_codec_valid_new_rate(struct snd_soc_component *component,
	int stream)
{
	unsigned int reg = top_ctrl_regs[stream].top_ctrl_reg;
	unsigned int w_val = top_ctrl_regs[stream].top_ctrl_w_val;
	unsigned int r_val = top_ctrl_regs[stream].top_ctrl_r_val;
	unsigned int reg_val;

	/* toggle top_ctrl status */
	if (snd_soc_component_read(component, reg, &reg_val) < 0)
		return 0;

	if (reg_val & r_val)
		snd_soc_component_update_bits(component, reg, w_val, 0x0);
	else
		snd_soc_component_update_bits(component, reg, w_val, w_val);

	return 0;
}

static int mt8518_codec_setup_ul_rate(struct snd_soc_component *component,
	int rate)
{
	uint32_t val = 0;

	val = mt8518_codec_rate_to_val(mt8518_codec_ul_voice_modes,
		ARRAY_SIZE(mt8518_codec_ul_voice_modes), rate);
	if (val < 0)
		goto err;

	snd_soc_component_update_bits(component, AFE_AD_UL_SRC_CON0,
		AFE_AD_UL_SRC_CON0_UL_VOICE_MODE_MASK,
		AFE_AD_UL_SRC_CON0_UL_VOICE_MODE(val));

	val = mt8518_codec_rate_to_val(mt8518_codec_ul_rates,
		ARRAY_SIZE(mt8518_codec_ul_rates), rate);
	if (val < 0)
		goto err;

	snd_soc_component_update_bits(component, ABB_ULAFE_CON1,
		ABB_ULAFE_CON1_UL_RATE_MASK,
		ABB_ULAFE_CON1_UL_RATE(val));
	mt8518_codec_valid_new_rate(component, SNDRV_PCM_STREAM_CAPTURE);

	return 0;

err:
	dev_err(component->dev, "%s error to setup ul rate\n", __func__);
	return -EINVAL;
}

static int mt8518_codec_setup_dl_rate(struct snd_soc_component *component,
	int rate)
{
	uint32_t val = 0;

	val = mt8518_codec_rate_to_val(mt8518_codec_dl_voice_modes,
		ARRAY_SIZE(mt8518_codec_dl_voice_modes), rate);
	if (val < 0)
		goto err;

	snd_soc_component_update_bits(component, AFE_ADDA_DL_SRC2_CON0,
		AFE_ADDA_DL_SRC2_CON0_DL_VOICE_MODE_MASK,
		AFE_ADDA_DL_SRC2_CON0_DL_VOICE_MODE(val));

	val = mt8518_codec_rate_to_val(mt8518_codec_dl_input_modes,
		ARRAY_SIZE(mt8518_codec_dl_input_modes), rate);
	if (val < 0)
		goto err;

	snd_soc_component_update_bits(component, AFE_ADDA_DL_SRC2_CON0,
		AFE_ADDA_DL_SRC2_CON0_DL_INPUT_MODE_MASK,
		AFE_ADDA_DL_SRC2_CON0_DL_INPUT_MODE(val));

	val = mt8518_codec_rate_to_val(mt8518_codec_dl_rates,
		ARRAY_SIZE(mt8518_codec_dl_rates), rate);
	if (val < 0)
		goto err;

	snd_soc_component_update_bits(component, ABB_AFE_CON1,
		ABB_AFE_CON1_DL_RATE_MASK,
		ABB_AFE_CON1_DL_RATE(val));
	mt8518_codec_valid_new_rate(component, SNDRV_PCM_STREAM_PLAYBACK);

	return 0;

err:
	dev_err(component->dev, "%s error to setup dl rate\n", __func__);
	return -EINVAL;
}

static int mt8518_codec_configure_ul(struct snd_soc_component *component)
{
	snd_soc_component_update_bits(component, ABB_ULAFE_CON0,
		ABB_ULAFE_CON0_AMIC_SCK_SEL_MASK,
		ABB_ULAFE_CON0_AMIC_SCK_SEL_VAL);

	snd_soc_component_update_bits(component, ABB_ULAFE_CON11,
		ABB_ULAFE_CON11_ASYNC_FIFO_SRPT_MASK,
		ABB_ULAFE_CON11_ASYNC_FIFO_SRPT_VAL(0x3));

	return 0;
}

static int mt8518_codec_configure_dl_nle(struct snd_soc_component *component,
	int rate)
{
	static struct {
		unsigned int reg;
		unsigned int mask;
		unsigned int val;
	} config_regs[] = {
		{
			AFE_NLE_ZCD_LCH_CFG,
			AFE_NLE_ZCD_CH_CFG_ZCD_CHECK_MODE_MASK,
			AFE_NLE_ZCD_CH_CFG_ZCD_CHECK_MODE_VAL(0x0),
		},
		{
			AFE_NLE_ZCD_LCH_CFG,
			AFE_NLE_ZCD_CH_CFG_ZCD_MODE_SEL_MASK,
			AFE_NLE_ZCD_CH_CFG_ZCD_MODE_SEL_VAL(0x0),
		},
		{
			AFE_NLE_PWR_DET_LCH_CFG,
			AFE_NLE_PWR_DET_CH_CFG_H2L_HOLD_TIME_MASK,
			AFE_NLE_PWR_DET_CH_CFG_H2L_HOLD_TIME_DEF_VAL,
		},
		{
			AFE_NLE_PWR_DET_LCH_CFG,
			AFE_NLE_PWR_DET_CH_CFG_NLE_VTH_MASK,
			/* -40dB: 10^(-40/20) * 2^20 */
			AFE_NLE_PWR_DET_CH_CFG_NLE_VTH_VAL(0x28F6),
		},
		{
			AFE_NLE_GAIN_ADJ_LCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_ADJ_BYPASS_ZCD_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_ADJ_BYPASS_ZCD_VAL(0x0),
		},
		{
			AFE_NLE_GAIN_ADJ_LCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_TIME_OUT_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_TIME_OUT_VAL(0x1),
		},
		{
			AFE_NLE_GAIN_ADJ_LCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_HOLD_TIME_PER_JUMP_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_HOLD_TIME_PER_JUMP_VAL(0x0),
		},
		{
			AFE_NLE_GAIN_ADJ_LCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_STEP_PER_JUMP_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_STEP_PER_JUMP_VAL(0x0),
		},
		{
			AFE_NLE_GAIN_ADJ_LCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_STEP_PER_ZCD_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_STEP_PER_ZCD_VAL(0x3),
		},
		{
			AFE_NLE_GAIN_ADJ_LCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_AG_MIN_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_AG_MIN_VAL(0xA),
		},
		{
			AFE_NLE_GAIN_ADJ_LCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_AG_MAX_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_AG_MAX_VAL(0x2),
		},
		{
			AFE_NLE_GAIN_IMP_LCH_CFG0,
			AFE_NLE_GAIN_IMP_CH_CFG0_AG_DELAY_MASK,
			AFE_NLE_GAIN_IMP_CH_CFG0_AG_DELAY_VAL(0xC),
		},
		{
			AFE_NLE_ZCD_RCH_CFG,
			AFE_NLE_ZCD_CH_CFG_ZCD_CHECK_MODE_MASK,
			AFE_NLE_ZCD_CH_CFG_ZCD_CHECK_MODE_VAL(0x0),
		},
		{
			AFE_NLE_ZCD_RCH_CFG,
			AFE_NLE_ZCD_CH_CFG_ZCD_MODE_SEL_MASK,
			AFE_NLE_ZCD_CH_CFG_ZCD_MODE_SEL_VAL(0x0),
		},
		{
			AFE_NLE_PWR_DET_RCH_CFG,
			AFE_NLE_PWR_DET_CH_CFG_H2L_HOLD_TIME_MASK,
			AFE_NLE_PWR_DET_CH_CFG_H2L_HOLD_TIME_DEF_VAL,
		},
		{
			AFE_NLE_PWR_DET_RCH_CFG,
			AFE_NLE_PWR_DET_CH_CFG_NLE_VTH_MASK,
			/* -40dB: 10^(-40/20) * 2^20 */
			AFE_NLE_PWR_DET_CH_CFG_NLE_VTH_VAL(0x28F6),
		},
		{
			AFE_NLE_GAIN_ADJ_RCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_ADJ_BYPASS_ZCD_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_ADJ_BYPASS_ZCD_VAL(0x0),
		},
		{
			AFE_NLE_GAIN_ADJ_RCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_TIME_OUT_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_TIME_OUT_VAL(0x1),
		},
		{
			AFE_NLE_GAIN_ADJ_RCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_HOLD_TIME_PER_JUMP_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_HOLD_TIME_PER_JUMP_VAL(0x0),
		},
		{
			AFE_NLE_GAIN_ADJ_RCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_STEP_PER_JUMP_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_STEP_PER_JUMP_VAL(0x0),
		},
		{
			AFE_NLE_GAIN_ADJ_RCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_STEP_PER_ZCD_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_STEP_PER_ZCD_VAL(0x3),
		},
		{
			AFE_NLE_GAIN_ADJ_RCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_AG_MIN_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_AG_MIN_VAL(0xA),
		},
		{
			AFE_NLE_GAIN_ADJ_RCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_AG_MAX_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_AG_MAX_VAL(0x2),
		},
		{
			AFE_NLE_GAIN_IMP_RCH_CFG0,
			AFE_NLE_GAIN_IMP_CH_CFG0_AG_DELAY_MASK,
			AFE_NLE_GAIN_IMP_CH_CFG0_AG_DELAY_VAL(0xC),
		},
	};
	int i;

	snd_soc_component_update_bits(component, AFE_NLE_PRE_BUF_CFG,
		AFE_NLE_PRE_BUF_CFG_POINT_END_MASK,
		(rate * (H2L_HOLD_TIME_MS - 1) / 1000) - 1);

	for (i = 0; i < ARRAY_SIZE(config_regs); i++)
		snd_soc_component_update_bits(component, config_regs[i].reg,
			config_regs[i].mask, config_regs[i].val);
	return 0;
}

static int mt8518_codec_configure_dl(struct snd_soc_component *component,
	int rate)
{
	struct mt8518_codec_priv *mt8518_codec =
		snd_soc_component_get_drvdata(component);

	snd_soc_component_update_bits(component, AFE_ADDA_DL_SRC2_CON1,
		AFE_ADDA_DL_SRC2_CON1_GAIN_CTRL_MASK,
		AFE_ADDA_DL_SRC2_CON1_GAIN_CTRL_VAL(0xf74f));

	snd_soc_component_update_bits(component, AFE_ADDA_DL_SRC2_CON0,
		AFE_ADDA_DL_SRC2_CON0_MUTE_OFF_CTRL_MASK,
		AFE_ADDA_DL_SRC2_CON0_MUTE_OFF_CTRL_VAL);

	snd_soc_component_update_bits(component, ABB_AFE_CON5,
		ABB_AFE_CON5_SDM_GAIN_VAL_MASK,
		ABB_AFE_CON5_SDM_GAIN_VAL(0x1c));

	if (mt8518_codec->dl_nle_support)
		mt8518_codec_configure_dl_nle(component, rate);

	return 0;
}

static int mt8518_codec_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	int ret = 0;
	struct snd_soc_component *component = dai->component;
	int rate = params_rate(params);

	dev_dbg(component->dev, "%s rate = %dHz\n", __func__, rate);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		ret = mt8518_codec_setup_ul_rate(component, rate);
	else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = mt8518_codec_setup_dl_rate(component, rate);

	if (ret < 0) {
		dev_err(component->dev, "%s error to setup rate\n", __func__);
		return ret;
	}

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		mt8518_codec_configure_ul(component);
	else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		mt8518_codec_configure_dl(component, rate);

	return ret;
}

static int mt8518_codec_enable_ul(struct snd_soc_component *component)
{
	snd_soc_component_update_bits(component, ABB_ULAFE_CON0,
		ABB_ULAFE_CON0_UL_EN_MASK,
		ABB_ULAFE_CON0_UL_EN_VAL);

	snd_soc_component_update_bits(component, ABB_ULAFE_CON0,
		ABB_ULAFE_CON0_AFE_ON_UL_MASK,
		ABB_ULAFE_CON0_AFE_ON_UL_VAL);

	snd_soc_component_update_bits(component, AFE_AD_UL_SRC_CON0,
		AFE_AD_UL_SRC_CON0_UL_SRC_ON_MASK,
		AFE_AD_UL_SRC_CON0_UL_SRC_ON_VAL);

	snd_soc_component_update_bits(component, AFE_AD_UL_DL_CON0,
		AFE_AD_UL_DL_CON0_ADDA_AFE_ON,
		AFE_AD_UL_DL_CON0_ADDA_AFE_ON);

	return 0;
}

static struct snd_soc_dapm_widget *mt8518_codec_get_dapm_widget(
	struct snd_soc_dapm_context *dapm, const char *w_name)
{
	struct snd_soc_dapm_widget *w;

	list_for_each_entry(w, &dapm->card->widgets, list) {
		if (!strcmp(w->name, w_name)) {
			if (w->dapm == dapm)
				return w;
		}
	}

	return NULL;
}

static bool mt8518_codec_is_voice_amp_enabled(
	struct snd_soc_component *component)
{
	struct mt8518_codec_priv *mt8518_codec =
		snd_soc_component_get_drvdata(component);
	struct snd_soc_dapm_context *dapm =
		snd_soc_codec_get_dapm(mt8518_codec->codec);
	struct snd_soc_dapm_widget *w;

	w = mt8518_codec_get_dapm_widget(dapm, "Voice Amp");
	if (w && w->power)
		return true;

	return false;
}

static int mt8518_codec_enable_dl_nle(struct snd_soc_component *component)
{
	/* Only Audio Amp support NLE
	 * Voice Amp don't support NLE
	 * if Voice Amp enabled, don't enable NLE
	 */
	if (mt8518_codec_is_voice_amp_enabled(component))
		return 0;

	snd_soc_component_update_bits(component, AFE_NLE_CFG,
		AFE_NLE_CFG_SW_RSTB_MASK,
		AFE_NLE_CFG_SW_RSTB_VAL(0x1));

	snd_soc_component_update_bits(component, AFE_NLE_CFG,
		AFE_NLE_CFG_AFE_NLE_ON_MASK,
		AFE_NLE_CFG_AFE_NLE_ON);

	return 0;
}

static int mt8518_codec_enable_dl(struct snd_soc_component *component)
{
	struct mt8518_codec_priv *mt8518_codec =
		snd_soc_component_get_drvdata(component);

	if (mt8518_codec->dl_nle_support)
		mt8518_codec_enable_dl_nle(component);

	snd_soc_component_update_bits(component, AFE_ADDA_UL_DL_CON0,
		AFE_ADDA_UL_DL_CON0_DL_ASRC_ON_MASK,
		AFE_ADDA_UL_DL_CON0_DL_ASRC_ON_VAL);

	snd_soc_component_update_bits(component, AFE_ADDA_DL_SRC2_CON0,
		AFE_ADDA_DL_SRC2_CON0_GAIN_ON_MASK,
		AFE_ADDA_DL_SRC2_CON0_GAIN_ON_VAL);

	snd_soc_component_update_bits(component, AFE_ADDA_DL_SRC2_CON0,
		AFE_ADDA_DL_SRC2_CON0_DL_SRC_ON_MASK,
		AFE_ADDA_DL_SRC2_CON0_DL_SRC_ON_VAL);

	snd_soc_component_update_bits(component, ABB_AFE_CON0,
		ABB_AFE_CON0_DL_EN_MASK,
		ABB_AFE_CON0_DL_EN_VAL);

	return 0;
}

static int mt8518_codec_disable_ul(struct snd_soc_component *component)
{
	snd_soc_component_update_bits(component, AFE_AD_UL_DL_CON0,
		AFE_AD_UL_DL_CON0_ADDA_AFE_ON,
		0x0);

	snd_soc_component_update_bits(component, ABB_ULAFE_CON0,
		ABB_ULAFE_CON0_UL_EN_MASK,
		0x0);

	snd_soc_component_update_bits(component, AFE_AD_UL_SRC_CON0,
		AFE_AD_UL_SRC_CON0_UL_SRC_ON_MASK,
		0x0);

	snd_soc_component_update_bits(component, ABB_ULAFE_CON0,
		ABB_ULAFE_CON0_AFE_ON_UL_MASK,
		0x0);

	return 0;
}

static int mt8518_codec_disable_dl_nle(struct snd_soc_component *component)
{
	snd_soc_component_update_bits(component, AFE_NLE_CFG,
		AFE_NLE_CFG_AFE_NLE_ON_MASK,
		0x0);

	snd_soc_component_update_bits(component, AFE_NLE_CFG,
		AFE_NLE_CFG_SW_RSTB_MASK,
		AFE_NLE_CFG_SW_RSTB_VAL(0x0));

	return 0;
}

static int mt8518_codec_disable_dl(struct snd_soc_component *component)
{
	struct mt8518_codec_priv *mt8518_codec =
		snd_soc_component_get_drvdata(component);

	snd_soc_component_update_bits(component, ABB_AFE_CON0,
		ABB_AFE_CON0_DL_EN_MASK,
		0x0);

	snd_soc_component_update_bits(component, AFE_ADDA_DL_SRC2_CON0,
		AFE_ADDA_DL_SRC2_CON0_GAIN_ON_MASK,
		0x0);

	snd_soc_component_update_bits(component, AFE_ADDA_DL_SRC2_CON0,
		AFE_ADDA_DL_SRC2_CON0_DL_SRC_ON_MASK,
		0x0);

	snd_soc_component_update_bits(component, AFE_ADDA_UL_DL_CON0,
		AFE_ADDA_UL_DL_CON0_DL_ASRC_ON_MASK,
		0x0);

	if (mt8518_codec->dl_nle_support)
		mt8518_codec_disable_dl_nle(component);

	return 0;
}

static int mt8518_codec_trigger(struct snd_pcm_substream *substream,
	int cmd, struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			mt8518_codec_disable_ul(component);
		else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			mt8518_codec_disable_dl(component);
		break;
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			mt8518_codec_enable_ul(component);
		else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			mt8518_codec_enable_dl(component);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

#define MT8518_CODEC_DL_RATES SNDRV_PCM_RATE_8000_48000
#define MT8518_CODEC_UL_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 | \
	SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000)

#define MT8518_CODEC_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
	SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_soc_dai_ops mt8518_codec_dai_ops = {
	.hw_params	= mt8518_codec_hw_params,
	.trigger	= mt8518_codec_trigger,
};

static struct snd_soc_dai_driver mt8518_codec_dai[] = {
	{
		.name = "mt8518-codec-dai",
		.playback = {
			.stream_name = "AIF Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MT8518_CODEC_DL_RATES,
			.formats = MT8518_CODEC_FORMATS,
		},
		.capture = {
			.stream_name = "AIF Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MT8518_CODEC_UL_RATES,
			.formats = MT8518_CODEC_FORMATS,
		},
		.ops = &mt8518_codec_dai_ops,
	},
};

#ifdef CONFIG_DEBUG_FS
struct mt8518_codec_reg_attr {
	uint32_t offset;
	char *name;
};

#define DUMP_REG_ENTRY(reg) {reg, #reg}

static const struct mt8518_codec_reg_attr mt8518_codec_dump_reg_list[] = {
	/* audio_top_sys */
	DUMP_REG_ENTRY(AFE_ADDA_DL_SRC2_CON0),
	DUMP_REG_ENTRY(AFE_ADDA_DL_SRC2_CON1),
	DUMP_REG_ENTRY(AFE_ADDA_TOP_CON0),
	DUMP_REG_ENTRY(AFE_ADDA_UL_DL_CON0),
	DUMP_REG_ENTRY(AFE_NLE_CFG),
	DUMP_REG_ENTRY(AFE_NLE_PRE_BUF_CFG),
	DUMP_REG_ENTRY(AFE_NLE_PWR_DET_LCH_CFG),
	DUMP_REG_ENTRY(AFE_NLE_ZCD_LCH_CFG),
	DUMP_REG_ENTRY(AFE_NLE_GAIN_ADJ_LCH_CFG0),
	DUMP_REG_ENTRY(AFE_NLE_GAIN_IMP_LCH_CFG0),
	DUMP_REG_ENTRY(AFE_NLE_PWE_DET_LCH_MON),
	DUMP_REG_ENTRY(AFE_NLE_GAIN_ADJ_LCH_MON0),
	DUMP_REG_ENTRY(AFE_NLE_LCH_MON0),
	DUMP_REG_ENTRY(AFE_NLE_PWR_DET_RCH_CFG),
	DUMP_REG_ENTRY(AFE_NLE_ZCD_RCH_CFG),
	DUMP_REG_ENTRY(AFE_NLE_GAIN_ADJ_RCH_CFG0),
	DUMP_REG_ENTRY(AFE_NLE_GAIN_IMP_RCH_CFG0),
	DUMP_REG_ENTRY(AFE_NLE_PWE_DET_RCH_MON),
	DUMP_REG_ENTRY(AFE_NLE_GAIN_ADJ_RCH_MON0),
	DUMP_REG_ENTRY(AFE_NLE_RCH_MON0),
	DUMP_REG_ENTRY(ABB_AFE_CON0),
	DUMP_REG_ENTRY(ABB_AFE_CON1),
	DUMP_REG_ENTRY(ABB_AFE_CON2),
	DUMP_REG_ENTRY(ABB_AFE_CON3),
	DUMP_REG_ENTRY(ABB_AFE_CON4),
	DUMP_REG_ENTRY(ABB_AFE_CON5),
	DUMP_REG_ENTRY(ABB_AFE_CON6),
	DUMP_REG_ENTRY(ABB_AFE_CON7),
	DUMP_REG_ENTRY(ABB_AFE_CON8),
	DUMP_REG_ENTRY(ABB_AFE_CON10),
	DUMP_REG_ENTRY(ABB_AFE_CON11),
	DUMP_REG_ENTRY(ABB_AFE_SDM_TEST),
	DUMP_REG_ENTRY(AFE_AD_UL_DL_CON0),
	DUMP_REG_ENTRY(AFE_AD_UL_SRC_CON0),
	DUMP_REG_ENTRY(AFE_AD_UL_SRC_CON1),
	DUMP_REG_ENTRY(ABB_ULAFE_CON0),
	DUMP_REG_ENTRY(ABB_ULAFE_CON1),
	DUMP_REG_ENTRY(ABB_ULAFE_CON2),
	DUMP_REG_ENTRY(ABB_ULAFE_CON7),
	DUMP_REG_ENTRY(ABB_ULAFE_CON11),

	/* apmixedsys */
	DUMP_REG_ENTRY(AUDIO_CODEC_CON00),
	DUMP_REG_ENTRY(AUDIO_CODEC_CON01),
	DUMP_REG_ENTRY(AUDIO_CODEC_CON02),
	DUMP_REG_ENTRY(AUDIO_CODEC_CON03),
	DUMP_REG_ENTRY(AUDIO_CODEC_CON04),
	DUMP_REG_ENTRY(AADC_CODEC_CON00),
	DUMP_REG_ENTRY(AADC_CODEC_CON01),
	DUMP_REG_ENTRY(AADC_CODEC_CON02),
};

static ssize_t mt8518_codec_debug_read(struct file *file,
	char __user *user_buf,
	size_t count, loff_t *pos)
{
	struct mt8518_codec_priv *mt8518_codec = file->private_data;
	struct snd_soc_codec *codec = mt8518_codec->codec;
	struct snd_soc_component *component = &codec->component;
	ssize_t ret, i;
	char *buf;
	int n = 0;
	unsigned int val;

	if (*pos < 0 || !count)
		return -EINVAL;

	buf = kzalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(mt8518_codec_dump_reg_list); i++) {
		if (snd_soc_component_read(component,
			mt8518_codec_dump_reg_list[i].offset, &val) < 0)
			val = 0;
		n += scnprintf(buf + n, count - n, "%s = 0x%x\n",
			mt8518_codec_dump_reg_list[i].name, val);
	}

	ret = simple_read_from_buffer(user_buf, count, pos, buf, n);

	kfree(buf);

	return ret;
}

static const struct file_operations mt8518_codec_debug_ops = {
	.open = simple_open,
	.read = mt8518_codec_debug_read,
	.llseek = default_llseek,
};
#endif

static int mt8518_codec_setup_micbias0(
	struct mt8518_codec_priv *mt8518_codec)
{
	struct snd_soc_codec *codec = mt8518_codec->codec;
	struct snd_soc_component *component = &codec->component;
	unsigned int sel = mt8518_codec->micbias0_out_vol_sel;

	snd_soc_component_update_bits(component, AADC_CODEC_CON02,
		AADC_CODEC_CON02_AUDMICBIASVREF_MASK,
		AADC_CODEC_CON02_AUDMICBIASVREF_VAL(sel));

	return 0;
}

static void mt8518_codec_init_regs(struct snd_soc_component *component)
{
	static struct {
		unsigned int reg;
		unsigned int mask;
		unsigned int val;
	} init_regs[] = {
		{
			AUDIO_CODEC_CON04,
			AUDIO_CODEC_CON04_LCH_TIRE_LOW_MASK,
			AUDIO_CODEC_CON04_LCH_TIRE_LOW_EN,
		},
		{
			AUDIO_CODEC_CON04,
			AUDIO_CODEC_CON04_RCH_TIRE_LOW_MASK,
			AUDIO_CODEC_CON04_RCH_TIRE_LOW_EN,
		},
		{
			AUDIO_CODEC_CON03,
			AUDIO_CODEC_CON03_LDO2P2_VOSEL_MASK,
			AUDIO_CODEC_CON03_LDO2P2_VOSEL_VAL(0x4),
		},
		{
			AUDIO_CODEC_CON03,
			AUDIO_CODEC_CON03_LDO2P2_PWDB_MASK,
			AUDIO_CODEC_CON03_LDO2P2_PWDB_MASK,
		},
		{
			AUDIO_CODEC_CON03,
			AUDIO_CODEC_CON03_LDO2P8_VOSEL_MASK,
			AUDIO_CODEC_CON03_LDO2P8_VOSEL_VAL(0x3),
		},
		{
			AUDIO_CODEC_CON03,
			AUDIO_CODEC_CON03_LDO2P8_IQ_SEL_MASK,
			AUDIO_CODEC_CON03_LDO2P8_IQ_SEL_VAL(0x1),
		},
		{
			AUDIO_CODEC_CON03,
			AUDIO_CODEC_CON03_LDO2P8_PWDB_MASK,
			AUDIO_CODEC_CON03_LDO2P8_PWDB_MASK,
		},
		{
			AADC_CODEC_CON02,
			AADC_CODEC_CON02_AUDUL_LDO2P2_VOSEL_MASK,
			AADC_CODEC_CON02_AUDUL_LDO2P2_VOSEL_VAL(0x4),
		},
		{
			AADC_CODEC_CON02,
			AADC_CODEC_CON02_AUDUL_LDO2P2_PWDB_MASK,
			AADC_CODEC_CON02_AUDUL_LDO2P2_PWDB_MASK,
		},
		{
			AUDIO_CODEC_CON04,
			AUDIO_CODEC_CON04_LCH_TIRE_LOW_MASK,
			0x0,
		},
		{
			AUDIO_CODEC_CON04,
			AUDIO_CODEC_CON04_RCH_TIRE_LOW_MASK,
			0x0,
		},
		{
			AUDIO_CODEC_CON01,
			AUDIO_CODEC_CON01_ABUF_BIAS_MASK,
			AUDIO_CODEC_CON01_ABUF_BIAS_VAL(0x0),
		},
		{
			AUDIO_CODEC_CON01,
			AUDIO_CODEC_CON01_VDPG_MASK,
			AUDIO_CODEC_CON01_VDPG_VAL(0x9),
		},
		{
			AUDIO_CODEC_CON03,
			AUDIO_CODEC_CON03_DP_VCM_EN,
			AUDIO_CODEC_CON03_DP_VCM_EN,
		},
		{
			AUDIO_CODEC_CON04,
			AUDIO_CODEC_CON04_DP_GAMP_GAIN_SEL_MASK,
			AUDIO_CODEC_CON04_DP_GAMP_GAIN_SEL_VAL(0x2),
		},
		{
			AUDIO_CODEC_CON03,
			AUDIO_CODEC_CON03_DP_RAMP_ISEL_MASK,
			AUDIO_CODEC_CON03_DP_RAMP_ISEL_VAL(0x6),
		},
		{
			AUDIO_CODEC_CON03,
			AUDIO_CODEC_CON03_CK_SEL_MASK,
			AUDIO_CODEC_CON03_CK_SEL_VAL(0x4),
		},
		{
			AUDIO_CODEC_CON03,
			AUDIO_CODEC_CON03_DP_RAMP_CAP_RSTB,
			AUDIO_CODEC_CON03_DP_RAMP_CAP_RSTB,
		},
		{
			AUDIO_CODEC_CON03,
			AUDIO_CODEC_CON03_DP_RAMPGEN_EN_MASK,
			AUDIO_CODEC_CON03_DP_RAMPGEN_EN_VAL(0x1),
		},
		{
			AUDIO_CODEC_CON03,
			AUDIO_CODEC_CON03_ADEPOPX_EN,
			AUDIO_CODEC_CON03_ADEPOPX_EN,
		},
		{
			AUDIO_CODEC_CON03,
			AUDIO_CODEC_CON03_CK_EN,
			AUDIO_CODEC_CON03_CK_EN,
		},
		{
			AUDIO_CODEC_CON03,
			AUDIO_CODEC_CON03_DP_RAMP_START,
			AUDIO_CODEC_CON03_DP_RAMP_START,
		},
		{
			AUDIO_CODEC_CON04,
			AUDIO_CODEC_CON04_DP_RAMPGEN_SEL_MASK,
			AUDIO_CODEC_CON04_DP_RAMPGEN_SEL_VAL(0x0),
		},
		{
			AUDIO_CODEC_CON03,
			AUDIO_CODEC_CON03_DP_RAMPGEN_EN_MASK,
			AUDIO_CODEC_CON03_DP_RAMPGEN_EN_VAL(0x0),
		},
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(init_regs); i++)
		snd_soc_component_update_bits(component, init_regs[i].reg,
			init_regs[i].mask, init_regs[i].val);
}

static struct regmap *mt8518_codec_get_regmap_of(const char *phandle_name,
		struct device *dev)
{
	struct regmap *regmap = NULL;

	regmap = syscon_regmap_lookup_by_phandle(dev->of_node,
		phandle_name);
	if (!IS_ERR(regmap)) {
		dev_dbg(dev, "%s found regmap of syscon node %s\n",
			__func__, phandle_name);
		return regmap;
	}
	dev_err(dev, "%s failed to get regmap of syscon node %s\n",
		__func__, phandle_name);

	return NULL;
}

static const char * const mt8518_codec_regmap_of_str[REGMAP_NUMS] = {
	"mediatek,afe-regmap",
	"mediatek,apmixedsys-regmap",
};

static int mt8518_codec_parse_of(struct snd_soc_codec *codec)
{
	struct device *dev = codec->dev;
	struct mt8518_codec_priv *mt8518_codec =
		snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	int i;

	for (i = 0; i < REGMAP_NUMS; i++) {
		mt8518_codec->regmap_modules[i] = mt8518_codec_get_regmap_of(
				mt8518_codec_regmap_of_str[i], dev);
		if (!mt8518_codec->regmap_modules[i]) {
			dev_err(dev, "%s failed to get %s\n", __func__,
				mt8518_codec_regmap_of_str[i]);
			ret = -EPROBE_DEFER;
			break;
		}
	}

	of_property_read_u32(dev->of_node,
		"mediatek,micbias0-setup-time-us",
		&mt8518_codec->micbias0_setup_time_us);

	of_property_read_u32(dev->of_node,
		"mediatek,micbias0_always_on",
		&mt8518_codec->micbias0_always_on);

	of_property_read_u32(dev->of_node,
		"mediatek,micbias0_out_vol_sel",
		&mt8518_codec->micbias0_out_vol_sel);

	of_property_read_u32(dev->of_node,
		"mediatek,downlink-nle-support",
		&mt8518_codec->dl_nle_support);

	return ret;
}

static int mt8518_codec_clk_probe(struct snd_soc_codec *codec)
{
	struct mt8518_codec_priv *mt8518_codec =
		snd_soc_codec_get_drvdata(codec);

	mt8518_codec->clk = devm_clk_get(codec->dev, "bus");
	if (IS_ERR(mt8518_codec->clk)) {
		dev_dbg(codec->dev, "%s devm_clk_get %s fail\n",
			__func__, "bus");
		return PTR_ERR(mt8518_codec->clk);
	}

	return clk_prepare_enable(mt8518_codec->clk);
}

static int mt8518_codec_probe(struct snd_soc_codec *codec)
{
	struct mt8518_codec_priv *mt8518_codec =
		snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dev_dbg(codec->dev, "%s\n", __func__);

	mt8518_codec->codec = codec;

	ret = mt8518_codec_parse_of(codec);
	if (ret < 0)
		return ret;

	spin_lock_init(&mt8518_codec->adda_afe_on_lock);

	ret = mt8518_codec_clk_probe(codec);
	if (ret < 0)
		return ret;

	mt8518_codec_init_regs(&codec->component);

	mt8518_codec_setup_micbias0(mt8518_codec);

	if (mt8518_codec->micbias0_always_on)
		mt8518_codec_turn_on_micbias0(codec);

#ifdef CONFIG_DEBUG_FS
	mt8518_codec->debugfs = debugfs_create_file("mt8518_codec_regs",
			0644, NULL, mt8518_codec, &mt8518_codec_debug_ops);
#endif

	return ret;
}

static int mt8518_codec_remove(struct snd_soc_codec *codec)
{
	struct mt8518_codec_priv *mt8518_codec =
		snd_soc_codec_get_drvdata(codec);

	if (mt8518_codec->micbias0_always_on)
		mt8518_codec_turn_off_micbias0(codec);

#ifdef CONFIG_DEBUG_FS
	debugfs_remove(mt8518_codec->debugfs);
#endif

	clk_disable_unprepare(mt8518_codec->clk);

	mt8518_codec->codec = NULL;

	return 0;
}

static int mt8518_codec_suspend(struct snd_soc_codec *codec)
{
	struct mt8518_codec_priv *mt8518_codec =
		snd_soc_codec_get_drvdata(codec);

	if (mt8518_codec->micbias0_always_on)
		mt8518_codec_turn_off_micbias0(codec);

	clk_disable_unprepare(mt8518_codec->clk);

	return 0;
}

static int mt8518_codec_resume(struct snd_soc_codec *codec)
{
	struct mt8518_codec_priv *mt8518_codec =
		snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	ret = clk_prepare_enable(mt8518_codec->clk);

	if (mt8518_codec->micbias0_always_on)
		mt8518_codec_turn_on_micbias0(codec);

	return ret;
}

static struct regmap *mt8518_codec_get_regmap(struct device *dev)
{
	struct mt8518_codec_priv *mt8518_codec = dev_get_drvdata(dev);

	return mt8518_codec->regmap;
}

static struct snd_soc_codec_driver soc_codec_dev_mt8518_codec = {
	.probe = mt8518_codec_probe,
	.remove = mt8518_codec_remove,
	.suspend = mt8518_codec_suspend,
	.resume = mt8518_codec_resume,
	.get_regmap = mt8518_codec_get_regmap,

	.component_driver = {
		.controls		= mt8518_codec_snd_controls,
		.num_controls		= ARRAY_SIZE(mt8518_codec_snd_controls),
		.dapm_widgets		= mt8518_codec_dapm_widgets,
		.num_dapm_widgets	= ARRAY_SIZE(mt8518_codec_dapm_widgets),
		.dapm_routes		= mt8518_codec_intercon,
		.num_dapm_routes	= ARRAY_SIZE(mt8518_codec_intercon),
	},
};

static int mt8518_codec_dev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mt8518_codec_priv *mt8518_codec = NULL;

	dev_dbg(dev, "%s dev name %s\n", __func__, dev_name(dev));

	if (dev->of_node) {
		dev_set_name(dev, "%s", MT8518_CODEC_NAME);
		dev_dbg(dev, "%s set dev name %s\n", __func__, dev_name(dev));
	}

	mt8518_codec = devm_kzalloc(dev,
			sizeof(struct mt8518_codec_priv), GFP_KERNEL);
	if (!mt8518_codec)
		return -ENOMEM;

	/* get regmap of codec */
	mt8518_codec->regmap = devm_regmap_init(dev, NULL, mt8518_codec,
		&mt8518_codec_regmap);
	if (IS_ERR(mt8518_codec->regmap)) {
		dev_err(dev, "%s failed to get regmap of codec\n", __func__);
		devm_kfree(dev, mt8518_codec);
		mt8518_codec->regmap = NULL;
		return -EINVAL;
	}

	dev_set_drvdata(dev, mt8518_codec);

	return snd_soc_register_codec(dev,
			&soc_codec_dev_mt8518_codec, mt8518_codec_dai,
			ARRAY_SIZE(mt8518_codec_dai));
}

static const struct of_device_id mt8518_codec_of_match[] = {
	{.compatible = "mediatek," MT8518_CODEC_NAME,},
	{}
};

MODULE_DEVICE_TABLE(of, mt8518_codec_of_match);

static struct platform_driver mt8518_codec_driver = {
	.probe = mt8518_codec_dev_probe,
	.driver = {
		.name = MT8518_CODEC_NAME,
		.owner = THIS_MODULE,
		.of_match_table = mt8518_codec_of_match,
	},
};

module_platform_driver(mt8518_codec_driver);

MODULE_DESCRIPTION("ASoC MT8518 codec driver");
MODULE_LICENSE("GPL");
