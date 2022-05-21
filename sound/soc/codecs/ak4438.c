/*
 * ak4438.c  --  audio driver for AK4438
 *
 * Copyright (C) 2019 Asahi Kasei Microdevices Corporation
 *  Author				Date		Revision
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Tsuyoshi Mutsuro    16/04/11		1.0
 * Norishige Nakashima	19/10/15		2.0		kernel 4.9
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/regmap.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include "ak4438.h"

#ifdef AK4438_DEBUG
#define akdbgprt printk
#else
#define akdbgprt(format, arg...) do {} while (0)
#endif

enum ak4438_pcm_mode {
	PCM_NORMAL_MODE = 0,
	PCM_TDM128_MODE,
	PCM_TDM256_MODE,
	PCM_TDM512_MODE,
};

enum ak4438_tranition_mode {
	TRANS_0_4080_FS = 0,
	TRANS_1_2040_FS,
	TRANS_2_510_FS,
	TRANS_3_255_FS,
};

enum ak4438_digital_filter_mode {
	DIGFTL_SHARP_ROLLOFF = 0,
	DIGFTL_SLOW_ROLLOFF,
	DIGFTL_SHORT_DELAY_SHARP_ROLLOFF,
	DIGFTL_SHORT_DELAY_SLOW_ROLLOFF,
	DIGFTL_SUPER_SLOW_ROLLOFF,
};

/* AK4438 Codec Private Data */
struct ak4438_priv {
	struct regmap *regmap;
	struct i2c_client *i2c;
	struct gpio_desc *pdn_gpiod;
	struct gpio_desc *mute_gpiod;
	int sds;	//SDS2-0 bits
	int digfil;	//SSLOW,SD,SLOW bits
	int fs;		//sampling rate
	int LR[4];	//(MONO,INVL,INVR,SELLR)x4ch
	u32 dai_fmt;
	u32 transition_mode;
	bool enable_mute_delay;
};

/* ak4438 register cache & default register settings */
static const struct reg_default ak4438_reg[] = {
	{ 0x00, 0x0D },	/* AK4438_00_CONTROL1	*/
	{ 0x01, 0x22 },	/* AK4438_01_CONTROL2	*/
	{ 0x02, 0x00 },	/* AK4438_02_CONTROL3	*/
	{ 0x03, 0xFF },	/* AK4438_03_LCHATT	*/
	{ 0x04, 0xFF },	/* AK4438_04_RCHATT	*/
	{ 0x05, 0x00 },	/* AK4438_05_CONTROL4	*/
	{ 0x06, 0x00 },	/* AK4438_06_RESERVED	*/
	{ 0x07, 0x01 },	/* AK4438_07_CONTROL6	*/
	{ 0x08, 0x00 },	/* AK4438_08_CONTROL7	*/
	{ 0x09, 0x00 },	/* AK4438_09_RESERVED	*/
	{ 0x0A, 0x0D },	/* AK4438_0A_CONTROL8	*/
	{ 0x0B, 0x0C },	/* AK4438_0B_CONTROL9	*/
	{ 0x0C, 0x00 },	/* AK4438_0C_CONTROL10	*/
	{ 0x0D, 0x00 },	/* AK4438_0D_CONTROL11	*/
	{ 0x0E, 0x50 },	/* AK4438_0E_CONTROL12	*/
	{ 0x0F, 0xFF },	/* AK4438_0F_L2CHATT	*/
	{ 0x10, 0xFF },	/* AK4438_10_R2CHATT	*/
	{ 0x11, 0xFF },	/* AK4438_11_L3CHATT	*/
	{ 0x12, 0xFF },	/* AK4438_12_R3CHATT	*/
	{ 0x13, 0xFF },	/* AK4438_13_L4CHATT	*/
	{ 0x14, 0xFF },	/* AK4438_14_R4CHATT	*/
};

/* Volume control:
 * from -127 to 0 dB in 0.5 dB steps (mute instead of -127.5 dB)
 */
static DECLARE_TLV_DB_SCALE(latt_tlv, -12750, 50, 1);
static DECLARE_TLV_DB_SCALE(ratt_tlv, -12750, 50, 1);

// DEM1 bit DEM0 bit Mode
// 0 0 44.1kHz
// 0 1 OFF (default)
// 1 0 48kHz
// 1 1 32kHz
static const char * const ak4438_dem_select_texts[] = {
	"44.1kHz", "OFF", "48kHz", "32kHz"
};
//SSLOW,SD, SLOW bits Digital Filter Setting
// 0,	0,	0 : Sharp Roll-Off Filter
// 0,	0,	1 : Slow Roll-Off Filter
// 0,	1,	0 : Short delay Sharp Roll-Off Filter
// 0,	1,	1 : Short delay Slow Roll-Off Filter
// 1,	*,	* : Super Slow Roll-Off Filter
static const char * const ak4438_digfil_select_texts[] = {
	"Sharp Roll-Off Filter",
	"Slow Roll-Off Filter",
	"Short delay Sharp Roll-Off Filter",
	"Short delay Slow Roll-Off Filter",
	"Super Slow Roll-Off Filter"
};
// DZFB: Inverting Enable of DZF
// 0: DZF goes ¡°H¡± at Zero Detection
// 1: DZF goes ¡°L¡± at Zero Detection
static const char * const ak4438_dzfb_select_texts[] = {"H", "L"};
//SDS2-0 bits: Output Data Select
//Refer to Data Sheet
static const char * const ak4438_sds_select_texts[] = {
	"Setting 0", "Setting 1", "Setting 2",
	"Setting 3", "Setting 4", "Setting 5",
	"Setting 6", "Setting 7",
};
//TDM1-0 bits: TDM Mode Setting
// 0 0 : Normal Mode
// 0 1 : TDM128 Mode
// 1 0 : TDM256 Mode
// 1 1 : TDM512 Mode
static const char * const ak4438_tdm_select_texts[] = {
	"Normal Mode", "TDM128 Mode", "TDM256 Mode", "TDM512 Mode"
};
//Mono and SELLR bit Setting (1~4)
static const char * const ak4438_dac_LR_select_texts[] = {
	"Lch In, Rch In",
	"Lch In, Rch In Invert",
	"Lch In Invert, Rch In",
	"Lch In Invert, Rch In Invert",
	"Rch In, Lch In",
	"Rch In, Lch In Invert",
	"Rch In Invert, Lch In",
	"Rch In Invert, Lch In Invert",
	"Lch In, Lch In",
	"Lch In, Lch In Invert",
	"Lch In Invert, Lch In",
	"Lch In Invert, Lch In Invert",
	"Rch In, Rch In",
	"Rch In, Rch In Invert",
	"Rch In Invert, Rch In",
	"Rch In Invert, Rch In Invert",
};
//ATS1-0 bits Attenuation Speed
static const char * const ak4438_ats_select_texts[] = {
	"4080/fs", "2040/fs", "510/fs", "255/fs",
};
//DIF2 bit Audio Interface Format Setting(BICK fs)
static const char * const ak4438_dif_select_texts[] = {"32fs,48fs", "64fs",};


#ifdef AK4438_DEBUG
static int nReg;
static const char * const ak4438_reg_select_texts[] = {
	"Read AK4438 All Reg",
};
static const struct soc_enum ak4438_debug_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak4438_reg_select_texts),
		ak4438_reg_select_texts),
};

static int get_reg_debug(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = nReg;

	return 0;
}

static int set_reg_debug(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	u32 reg = ucontrol->value.enumerated.item[0];
	int i, value;

	nReg = reg;

	for (i = AK4438_00_CONTROL1; i < AK4438_MAX_REGISTERS; i++) {
		value = snd_soc_read(codec, i);
		dev_info(codec->dev,
			 "***AK4438 Addr,Reg=(%02X, %02X)\n",
			 i, value);
	}

	return 0;
}
#endif

static int ak4438_set_transition_mode(struct snd_soc_codec *codec,
	unsigned int mode)
{
	snd_soc_update_bits(codec, AK4438_0B_CONTROL9,
		0xc0, mode << 6);

	return 0;
}

static int ak4438_set_digital_filter(struct snd_soc_codec *codec,
	unsigned int mode)
{
	unsigned int val;

	//write SD bit
	val = snd_soc_read(codec, AK4438_01_CONTROL2);
	val &= ~AK4438_SD_MASK;

	val |= ((mode & 0x02) << 4);
	snd_soc_write(codec, AK4438_01_CONTROL2, val);

	//write SLOW bit
	val = snd_soc_read(codec, AK4438_02_CONTROL3);
	val &= ~AK4438_SLOW_MASK;

	val |= (mode & 0x01);
	snd_soc_write(codec, AK4438_02_CONTROL3, val);

	//write SSLOW bit
	val = snd_soc_read(codec, AK4438_05_CONTROL4);
	val &= ~AK4438_SSLOW_MASK;

	val |= ((mode & 0x04) >> 2);
	snd_soc_write(codec, AK4438_05_CONTROL4, val);

	return 0;
}

static int get_DAC1_LR(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4438_priv *ak4438 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak4438->LR[0];

	return 0;
};

static int get_DAC2_LR(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4438_priv *ak4438 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak4438->LR[1];

	return 0;
};

static int get_DAC3_LR(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4438_priv *ak4438 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak4438->LR[2];

	return 0;
};

static int get_DAC4_LR(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4438_priv *ak4438 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak4438->LR[3];

	return 0;
};

static int get_digfil(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4438_priv *ak4438 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak4438->digfil;

	return 0;
};

static int get_sds(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4438_priv *ak4438 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak4438->sds;

	return 0;
};

static int get_attenuation_transition(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4438_priv *ak4438 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak4438->transition_mode;

	return 0;
};

static int set_digfil(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4438_priv *ak4438 = snd_soc_codec_get_drvdata(codec);
	int num;

	num = ucontrol->value.enumerated.item[0];
	if (num < DIGFTL_SHARP_ROLLOFF || num > DIGFTL_SUPER_SLOW_ROLLOFF)
		return -EINVAL;

	ak4438->digfil = num;

	ak4438_set_digital_filter(codec, ak4438->digfil);

	return 0;
};

static int set_sds(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4438_priv *ak4438 = snd_soc_codec_get_drvdata(codec);

	int reg_0b, reg_0a;

	if (ucontrol->value.enumerated.item[0] > 7)
		return -EINVAL;

	ak4438->sds = ucontrol->value.enumerated.item[0];

	//write SDS0 bit
	reg_0b = snd_soc_read(codec, AK4438_0B_CONTROL9);
	reg_0b &= ~AK4438_SDS0__MASK;

	reg_0b |= ((ak4438->sds & 0x01) << 4);
	snd_soc_write(codec, AK4438_0B_CONTROL9, reg_0b);

	//write SDS1,2 bits
	reg_0a = snd_soc_read(codec, AK4438_0A_CONTROL8);
	reg_0a &= ~AK4438_SDS12_MASK;

	reg_0a |= ((ak4438->sds & 0x02) << 4);
	reg_0a |= ((ak4438->sds & 0x04) << 2);
	snd_soc_write(codec, AK4438_0A_CONTROL8, reg_0a);

	return 0;
};

static int set_DAC1_LR(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4438_priv *ak4438 = snd_soc_codec_get_drvdata(codec);

	int reg_02, reg_05;

	if (ucontrol->value.enumerated.item[0] > 15)
		return -EINVAL;

	ak4438->LR[0] = ucontrol->value.enumerated.item[0];

	//write MONO1 and SELLR1 bits
	reg_02 = snd_soc_read(codec, AK4438_02_CONTROL3);
	reg_02 &= ~AK4438_DAC1_LR_MASK;


	reg_02 |= ((ak4438->LR[0] & 0x08) << 0);
	reg_02 |= ((ak4438->LR[0] & 0x04) >> 1);
	snd_soc_write(codec, AK4438_02_CONTROL3, reg_02);

	//write INVL1 and INVR1 bits
	reg_05 = snd_soc_read(codec, AK4438_05_CONTROL4);
	reg_05 &= ~AK4438_DAC1_INV_MASK;

	reg_05 |= ((ak4438->LR[0] & 0x02) << 6);
	reg_05 |= ((ak4438->LR[0] & 0x01) << 6);
	snd_soc_write(codec, AK4438_05_CONTROL4, reg_05);

	return 0;
};

static int set_DAC2_LR(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4438_priv *ak4438 = snd_soc_codec_get_drvdata(codec);

	int reg_0D, reg_05;

	if (ucontrol->value.enumerated.item[0] > 15)
		return -EINVAL;

	ak4438->LR[1] = ucontrol->value.enumerated.item[0];

	//write MONO2 bit
	reg_0D = snd_soc_read(codec, AK4438_0D_CONTROL11);
	reg_0D &= ~AK4438_DAC2_MASK1;

	reg_0D |= ((ak4438->LR[1] & 0x08) << 2);
	snd_soc_write(codec, AK4438_0D_CONTROL11, reg_0D);

	//write SELLR2 and INVL1 and INVR1 bits
	reg_05 = snd_soc_read(codec, AK4438_05_CONTROL4);
	reg_05 &= ~AK4438_DAC2_MASK2;

	reg_05 |= ((ak4438->LR[1] & 0x04) << 1);
	reg_05 |= ((ak4438->LR[1] & 0x02) << 4);
	reg_05 |= ((ak4438->LR[1] & 0x01) << 4);
	snd_soc_write(codec, AK4438_05_CONTROL4, reg_05);

	return 0;
};

static int set_DAC3_LR(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4438_priv *ak4438 = snd_soc_codec_get_drvdata(codec);

	int reg_0C, reg_0D;

	if (ucontrol->value.enumerated.item[0] > 15)
		return -EINVAL;

	ak4438->LR[2] = ucontrol->value.enumerated.item[0];

	//write MONO3 and SELLR3 bits
	reg_0D = snd_soc_read(codec, AK4438_0D_CONTROL11);
	reg_0D &= ~AK4438_DAC3_LR_MASK;

	reg_0D |= ((ak4438->LR[2] & 0x08) << 3);
	reg_0D |= ((ak4438->LR[2] & 0x04) << 0);
	snd_soc_write(codec, AK4438_0D_CONTROL11, reg_0D);

	//write INVL3 and INVR3 bits
	reg_0C = snd_soc_read(codec, AK4438_0C_CONTROL10);
	reg_0C &= ~AK4438_DAC3_INV_MASK;

	reg_0C |= ((ak4438->LR[2] & 0x02) << 3);
	reg_0C |= ((ak4438->LR[2] & 0x01) << 5);
	snd_soc_write(codec, AK4438_0C_CONTROL10, reg_0C);

	return 0;
};

static int set_DAC4_LR(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4438_priv *ak4438 = snd_soc_codec_get_drvdata(codec);

	int reg_0C, reg_0D;

	if (ucontrol->value.enumerated.item[0] > 15)
		return -EINVAL;

	ak4438->LR[3] = ucontrol->value.enumerated.item[0];

	//write MONO4 and SELLR4 bits
	reg_0D = snd_soc_read(codec, AK4438_0D_CONTROL11);
	reg_0D &= ~AK4438_DAC4_LR_MASK;

	reg_0D |= ((ak4438->LR[3] & 0x08) << 4);
	reg_0D |= ((ak4438->LR[3] & 0x04) << 1);
	snd_soc_write(codec, AK4438_0D_CONTROL11, reg_0D);

	//write INVL4 and INVR4 bits
	reg_0C = snd_soc_read(codec, AK4438_0C_CONTROL10);
	reg_0C &= ~AK4438_DAC4_INV_MASK;

	reg_0C |= ((ak4438->LR[3] & 0x02) << 5);
	reg_0C |= ((ak4438->LR[3] & 0x01) << 7);
	snd_soc_write(codec, AK4438_0C_CONTROL10, reg_0C);

	return 0;
};

static int set_attenuation_transition(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4438_priv *ak4438 = snd_soc_codec_get_drvdata(codec);
	int mode;

	mode = ucontrol->value.enumerated.item[0];
	if (mode < TRANS_0_4080_FS || mode > TRANS_3_255_FS)
		return -EINVAL;

	ak4438->transition_mode = mode;

	ak4438_set_transition_mode(codec, ak4438->transition_mode);

	return 0;
}

static int ak4438_digital_mute_delay_enable_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4438_priv *ak4438 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = ak4438->enable_mute_delay;

	return 0;
}

static int ak4438_digital_mute_delay_enable_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4438_priv *ak4438 = snd_soc_codec_get_drvdata(codec);

	ak4438->enable_mute_delay = ucontrol->value.integer.value[0];

	return 0;
}

static const struct soc_enum ak4438_dac_enum[] = {
/*0*/	SOC_ENUM_SINGLE(AK4438_01_CONTROL2, 1,
		ARRAY_SIZE(ak4438_dem_select_texts),
		ak4438_dem_select_texts),
/*1*/	SOC_ENUM_SINGLE(AK4438_0A_CONTROL8, 0,
		ARRAY_SIZE(ak4438_dem_select_texts),
		ak4438_dem_select_texts),
/*2*/	SOC_ENUM_SINGLE(AK4438_0E_CONTROL12, 4,
		ARRAY_SIZE(ak4438_dem_select_texts),
		ak4438_dem_select_texts),
/*3*/	SOC_ENUM_SINGLE(AK4438_0E_CONTROL12, 6,
		ARRAY_SIZE(ak4438_dem_select_texts),
		ak4438_dem_select_texts),
/*4*/	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak4438_digfil_select_texts),
		ak4438_digfil_select_texts),
/*5*/	SOC_ENUM_SINGLE(AK4438_02_CONTROL3, 2,
		ARRAY_SIZE(ak4438_dzfb_select_texts),
		ak4438_dzfb_select_texts),
/*6*/	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak4438_sds_select_texts),
		ak4438_sds_select_texts),
/*7*/	SOC_ENUM_SINGLE(AK4438_0A_CONTROL8, 6,
		ARRAY_SIZE(ak4438_tdm_select_texts),
		ak4438_tdm_select_texts),
/*8*/	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak4438_dac_LR_select_texts),
		ak4438_dac_LR_select_texts),
/*9*/	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak4438_ats_select_texts),
		ak4438_ats_select_texts),
/*10*/	SOC_ENUM_SINGLE(AK4438_00_CONTROL1, 3,
		ARRAY_SIZE(ak4438_dif_select_texts),
		ak4438_dif_select_texts),
};

static const struct snd_kcontrol_new ak4438_snd_controls[] = {
	SOC_SINGLE_TLV("AK4438 L1ch Digital Volume",
		AK4438_03_LCHATT, 0/*shift*/, 0xFF/*max value*/,
		0/*invert*/, latt_tlv),
	SOC_SINGLE_TLV("AK4438 R1ch Digital Volume",
		AK4438_04_RCHATT, 0, 0xFF, 0, ratt_tlv),
	SOC_SINGLE_TLV("AK4438 L2ch Digital Volume",
		AK4438_0F_L2CHATT, 0/*shift*/, 0xFF/*max value*/,
		0/*invert*/, latt_tlv),
	SOC_SINGLE_TLV("AK4438 R2ch Digital Volume",
		AK4438_10_R2CHATT, 0, 0xFF, 0, ratt_tlv),
	SOC_SINGLE_TLV("AK4438 L3ch Digital Volume",
		AK4438_11_L3CHATT, 0/*shift*/, 0xFF/*max value*/,
		0/*invert*/, latt_tlv),
	SOC_SINGLE_TLV("AK4438 R3ch Digital Volume",
		AK4438_12_R3CHATT, 0, 0xFF, 0, ratt_tlv),
	SOC_SINGLE_TLV("AK4438 L4ch Digital Volume",
		AK4438_13_L4CHATT, 0/*shift*/, 0xFF/*max value*/,
		0/*invert*/, latt_tlv),
	SOC_SINGLE_TLV("AK4438 R4ch Digital Volume",
		AK4438_14_R4CHATT, 0, 0xFF, 0, ratt_tlv),
	SOC_ENUM("AK4438 De-emphasis Response DAC1", ak4438_dac_enum[0]),
	SOC_ENUM("AK4438 De-emphasis Response DAC2", ak4438_dac_enum[1]),
	SOC_ENUM("AK4438 De-emphasis Response DAC3", ak4438_dac_enum[2]),
	SOC_ENUM("AK4438 De-emphasis Response DAC4", ak4438_dac_enum[3]),
	SOC_ENUM_EXT("AK4438 Digital Filter Setting", ak4438_dac_enum[4],
		get_digfil, set_digfil),
	SOC_ENUM("AK4438 Inverting Enable of DZFB", ak4438_dac_enum[5]),
	SOC_ENUM_EXT("AK4438 SDS Setting", ak4438_dac_enum[6],
		get_sds, set_sds),
	SOC_ENUM("AK4438 TDM Mode Setting", ak4438_dac_enum[7]),
	SOC_ENUM_EXT("AK4438 DAC1 LRch Setting", ak4438_dac_enum[8],
		get_DAC1_LR, set_DAC1_LR),
	SOC_ENUM_EXT("AK4438 DAC2 LRch Setting", ak4438_dac_enum[8],
		get_DAC2_LR, set_DAC2_LR),
	SOC_ENUM_EXT("AK4438 DAC3 LRch Setting", ak4438_dac_enum[8],
		get_DAC3_LR, set_DAC3_LR),
	SOC_ENUM_EXT("AK4438 DAC4 LRch Setting", ak4438_dac_enum[8],
		get_DAC4_LR, set_DAC4_LR),
	SOC_ENUM_EXT("AK4438 Attenuation Transition", ak4438_dac_enum[9],
		get_attenuation_transition, set_attenuation_transition),
	SOC_ENUM("AK4438 BICK fs Setting", ak4438_dac_enum[10]),
#ifdef AK4438_DEBUG
	SOC_ENUM_EXT("All Reg Read", ak4438_debug_enum[0],
		get_reg_debug, set_reg_debug),
#endif
	SOC_SINGLE_BOOL_EXT("Digital_Mute_Delay_Enable",
		0,
		ak4438_digital_mute_delay_enable_get,
		ak4438_digital_mute_delay_enable_put),
};

static const char * const ak4438_dac_select_texts[] = {
	"ON", "OFF",
};

static SOC_ENUM_SINGLE_VIRT_DECL(ak4438_dac_mux_enum,
	ak4438_dac_select_texts);

static const struct snd_kcontrol_new ak4438_dac1_mux_control =
	SOC_DAPM_ENUM("DAC1 Switch", ak4438_dac_mux_enum);
static const struct snd_kcontrol_new ak4438_dac2_mux_control =
	SOC_DAPM_ENUM("DAC2 Switch", ak4438_dac_mux_enum);
static const struct snd_kcontrol_new ak4438_dac3_mux_control =
	SOC_DAPM_ENUM("DAC3 Switch", ak4438_dac_mux_enum);
static const struct snd_kcontrol_new ak4438_dac4_mux_control =
	SOC_DAPM_ENUM("DAC4 Switch", ak4438_dac_mux_enum);

/* ak4438 dapm widgets */
static const struct snd_soc_dapm_widget ak4438_dapm_widgets[] = {
	SND_SOC_DAPM_DAC("AK4438 DAC1", NULL,
		AK4438_0A_CONTROL8, 2, 0),/*pw*/
	SND_SOC_DAPM_AIF_IN("AK4438 SDTI", "Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_OUTPUT("AK4438 AOUTA"),

	SND_SOC_DAPM_DAC("AK4438 DAC2", NULL,
		AK4438_0A_CONTROL8, 3, 0),/*pw*/
	SND_SOC_DAPM_OUTPUT("AK4438 AOUTB"),

	SND_SOC_DAPM_DAC("AK4438 DAC3", NULL,
		AK4438_0B_CONTROL9, 2, 0),/*pw*/
	SND_SOC_DAPM_OUTPUT("AK4438 AOUTC"),

	SND_SOC_DAPM_DAC("AK4438 DAC4", NULL,
		AK4438_0B_CONTROL9, 3, 0),/*pw*/
	SND_SOC_DAPM_OUTPUT("AK4438 AOUTD"),

	SND_SOC_DAPM_MUX("DAC1 to AOUTA", SND_SOC_NOPM, 0, 0,
		&ak4438_dac1_mux_control),/*nopm*/
	SND_SOC_DAPM_MUX("DAC2 to AOUTB", SND_SOC_NOPM, 0, 0,
		&ak4438_dac2_mux_control),/*nopm*/
	SND_SOC_DAPM_MUX("DAC3 to AOUTC", SND_SOC_NOPM, 0, 0,
		&ak4438_dac3_mux_control),/*nopm*/
	SND_SOC_DAPM_MUX("DAC4 to AOUTD", SND_SOC_NOPM, 0, 0,
		&ak4438_dac4_mux_control),/*nopm*/
};

static const struct snd_soc_dapm_route ak4438_intercon[] = {
	{"DAC1 to AOUTA", "ON", "AK4438 SDTI"},
	{"AK4438 DAC1", NULL, "DAC1 to AOUTA"},
	{"AK4438 AOUTA", NULL, "AK4438 DAC1"},

	{"DAC2 to AOUTB", "ON", "AK4438 SDTI"},
	{"AK4438 DAC2", NULL, "DAC2 to AOUTB"},
	{"AK4438 AOUTB", NULL, "AK4438 DAC2"},

	{"DAC3 to AOUTC", "ON", "AK4438 SDTI"},
	{"AK4438 DAC3", NULL, "DAC3 to AOUTC"},
	{"AK4438 AOUTC", NULL, "AK4438 DAC3"},

	{"DAC4 to AOUTD", "ON", "AK4438 SDTI"},
	{"AK4438 DAC4", NULL, "DAC4 to AOUTD"},
	{"AK4438 AOUTD", NULL, "AK4438 DAC4"},
};

static int ak4438_rstn_control(struct snd_soc_codec *codec, int bit)
{
	u8 rstn;

	rstn = snd_soc_read(codec, AK4438_00_CONTROL1);
	rstn &= ~AK4438_RSTN_MASK;

	if (bit)
		rstn |= AK4438_RSTN;

	snd_soc_write(codec, AK4438_00_CONTROL1, rstn);

	return 0;
}

static int ak4438_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct ak4438_priv *ak4438 = snd_soc_codec_get_drvdata(codec);
#ifdef AK4438_ACKS_USE_MANUAL_MODE
	u8 dfs1, dfs2;
#endif
	u8 con1, con8;
	int nfs1, data_bits, pcm_mode, fmt;

	nfs1 = params_rate(params);
	ak4438->fs = nfs1;

	data_bits = params_width(params);

	dev_dbg(codec->dev, "%s rate %d bits %d\n",
		__func__, ak4438->fs, data_bits);

	con1 = snd_soc_read(codec, AK4438_00_CONTROL1);
	con8 = snd_soc_read(codec, AK4438_0A_CONTROL8);

#ifdef AK4438_ACKS_USE_MANUAL_MODE
	dfs1 = snd_soc_read(codec, AK4438_01_CONTROL2);
	dfs1 &= ~AK4438_DFS01_MASK;

	dfs2 = snd_soc_read(codec, AK4438_05_CONTROL4);
	dfs2 &= ~AK4438_DFS2__MASK;

	switch (nfs1) {
	case 32000:
	case 44100:
	case 48000:
		dfs1 |= AK4438_DFS01_48KHZ;
		dfs2 |= AK4438_DFS2__48KHZ;
		break;
	case 88200:
	case 96000:
		dfs1 |= AK4438_DFS01_96KHZ;
		dfs2 |= AK4438_DFS2__96KHZ;
		break;
	case 176400:
	case 192000:
		dfs1 |= AK4438_DFS01_192KHZ;
		dfs2 |= AK4438_DFS2__192KHZ;
		break;
	case 384000:
		dfs1 |= AK4438_DFS01_384KHZ;
		dfs2 |= AK4438_DFS2__384KHZ;
		break;
	case 768000:
		dfs1 |= AK4438_DFS01_768KHZ;
		dfs2 |= AK4438_DFS2__768KHZ;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_write(codec, AK4438_01_CONTROL2, dfs1);
	snd_soc_write(codec, AK4438_05_CONTROL4, dfs2);
#else
	con1 |= 0x80;
#endif

	con1 &= ~AK4438_DIF2_1_0_MASK;

	fmt = ak4438->dai_fmt & SND_SOC_DAIFMT_FORMAT_MASK;

	pcm_mode = (con8 & 0xc0) >> 6;

	switch (pcm_mode) {
	case PCM_NORMAL_MODE:
		if ((fmt == SND_SOC_DAIFMT_I2S) && (data_bits == 16))
			con1 |= (0x3) << 1;
		else if ((fmt == SND_SOC_DAIFMT_I2S) && (data_bits == 24))
			con1 |= (0x3) << 1;
		else if ((fmt == SND_SOC_DAIFMT_I2S) && (data_bits == 32))
			con1 |= (0x7) << 1;
		else if ((fmt == SND_SOC_DAIFMT_LEFT_J) && (data_bits == 24))
			con1 |= (0x2) << 1;
		else if ((fmt == SND_SOC_DAIFMT_LEFT_J) && (data_bits == 32))
			con1 |= (0x6) << 1;
		else if ((fmt == SND_SOC_DAIFMT_RIGHT_J) && (data_bits == 16))
			con1 |= (0x0) << 1;
		else if ((fmt == SND_SOC_DAIFMT_RIGHT_J) && (data_bits == 20))
			con1 |= (0x1) << 1;
		else if ((fmt == SND_SOC_DAIFMT_RIGHT_J) && (data_bits == 24))
			con1 |= (0x4) << 1;
		else if ((fmt == SND_SOC_DAIFMT_RIGHT_J) && (data_bits == 32))
			con1 |= (0x5) << 1;
		break;
	case PCM_TDM128_MODE:
	case PCM_TDM256_MODE:
	case PCM_TDM512_MODE:
		if ((fmt == SND_SOC_DAIFMT_I2S) && (data_bits == 24))
			con1 |= (0x3) << 1;
		else if ((fmt == SND_SOC_DAIFMT_I2S) && (data_bits == 32))
			con1 |= (0x7) << 1;
		else if ((fmt == SND_SOC_DAIFMT_LEFT_J) && (data_bits == 24))
			con1 |= (0x2) << 1;
		else if ((fmt == SND_SOC_DAIFMT_LEFT_J) && (data_bits == 32))
			con1 |= (0x6) << 1;
		else if ((fmt == SND_SOC_DAIFMT_RIGHT_J) && (data_bits == 24))
			con1 |= (0x4) << 1;
		else if ((fmt == SND_SOC_DAIFMT_RIGHT_J) && (data_bits == 32))
			con1 |= (0x5) << 1;
		break;
	default:
		break;
	}

	con1 &= ~AK4438_RSTN_MASK;
	snd_soc_write(codec, AK4438_00_CONTROL1, con1);

	con1 |= 0x1;
	snd_soc_write(codec, AK4438_00_CONTROL1, con1);

	return 0;
}

static int ak4438_set_dai_sysclk(struct snd_soc_dai *dai, int clk_id,
	unsigned int freq, int dir)
{
	return 0;
}

static int ak4438_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	struct ak4438_priv *ak4438 = snd_soc_codec_get_drvdata(codec);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS: //Slave Mode
		ak4438->dai_fmt |= SND_SOC_DAIFMT_CBS_CFS;
		break;
	//Master Mode is not supported by AK4438
	case SND_SOC_DAIFMT_CBM_CFM:
	case SND_SOC_DAIFMT_CBS_CFM:
	case SND_SOC_DAIFMT_CBM_CFS:
	default:
		dev_info(codec->dev,
			 "Clock mode unsupported 0x%x\n",
			 (fmt & SND_SOC_DAIFMT_MASTER_MASK));
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_LEFT_J:
	case SND_SOC_DAIFMT_RIGHT_J:
	case SND_SOC_DAIFMT_I2S:
		ak4438->dai_fmt |= (fmt & SND_SOC_DAIFMT_FORMAT_MASK);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static bool ak4438_volatile(struct device *dev, unsigned int reg)
{
	bool ret;

#ifdef AK4438_DEBUG
	if (reg >= AK4438_MAX_REGISTERS)
		ret = 0;
	else
		ret = 1;
#else
	ret = 0;
#endif

	return ret;
}

static bool ak4438_readable(struct device *dev, unsigned int reg)
{
	bool ret;

	if (reg < AK4438_MAX_REGISTERS)
		ret = 1;
	else
		ret = 0;

	return ret;
}

static bool ak4438_writeable(struct device *dev, unsigned int reg)
{
	bool ret;

	if (reg < AK4438_MAX_REGISTERS)
		ret = 1;
	else
		ret = 0;

	return ret;
}

static int ak4438_trigger(struct snd_pcm_substream *substream,
	int cmd, struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "%s cmd %d\n", __func__, cmd);

	return 0;
}

static int ak4438_set_bias_level(struct snd_soc_codec *codec,
	enum snd_soc_bias_level level)
{
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);

	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
	case SND_SOC_BIAS_STANDBY:
		break;
	case SND_SOC_BIAS_OFF:
		break;
	}

	dapm->bias_level = level;

	return 0;
}

static int ak4438_set_dai_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	struct ak4438_priv *ak4438 = snd_soc_codec_get_drvdata(codec);
	int nfs, ret, att_step;
	unsigned long delay_us;

	nfs = ak4438->fs;

	dev_dbg(codec->dev, "%s mute[%s] nfs[%d] trans[%u] >>\n",
		__func__, mute ? "ON" : "OFF", nfs,
		ak4438->transition_mode);

	switch (ak4438->transition_mode) {
	case TRANS_0_4080_FS:
		att_step = 4080;
		break;
	case TRANS_1_2040_FS:
		att_step = 2040;
		break;
	case TRANS_2_510_FS:
		att_step = 510;
		break;
	case TRANS_3_255_FS:
		att_step = 255;
		break;
	default:
		att_step = 0;
		break;
	}

	delay_us = (att_step * 10 / (nfs / 100)) * 1000;

	if (mute) {
		ret = snd_soc_update_bits(codec, AK4438_01_CONTROL2, 0x01, 1);

		if (ak4438->enable_mute_delay && (delay_us > 0))
			usleep_range(delay_us, delay_us + 1);

		//External Mute ON
		if (ak4438->mute_gpiod)
			gpiod_set_value_cansleep(ak4438->mute_gpiod, 1);
	} else {
		//External Mute OFF
		if (ak4438->mute_gpiod)
			gpiod_set_value_cansleep(ak4438->mute_gpiod, 0);

		ret = snd_soc_update_bits(codec, AK4438_01_CONTROL2, 0x01, 0);

		if (ak4438->enable_mute_delay && (delay_us > 0))
			usleep_range(delay_us, delay_us + 1);
	}

	dev_dbg(codec->dev, "%s mute[%s] %d <<\n",
		__func__, mute ? "ON" : "OFF", ret);

	return 0;
}

#define AK4438_RATES	(SNDRV_PCM_RATE_8000_48000 | \
	SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000 | \
	SNDRV_PCM_RATE_176400 |	SNDRV_PCM_RATE_192000)

#define AK4438_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | \
	SNDRV_PCM_FMTBIT_S24_LE | \
	SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_ops ak4438_dai_ops = {
	.hw_params	= ak4438_hw_params,
	.set_sysclk	= ak4438_set_dai_sysclk,
	.set_fmt	= ak4438_set_dai_fmt,
	.trigger	= ak4438_trigger,
	.digital_mute	= ak4438_set_dai_mute,
};

struct snd_soc_dai_driver ak4438_dai[] = {
	{
		.name = "ak4438-AIF",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = AK4438_RATES,
			.formats = AK4438_FORMATS,
		},
		.ops = &ak4438_dai_ops,
	},
};

static int ak4438_init_reg(struct snd_soc_codec *codec)
{
	struct ak4438_priv *ak4438 = snd_soc_codec_get_drvdata(codec);

	if (ak4438->mute_gpiod)
		gpiod_set_value_cansleep(ak4438->mute_gpiod, 1);

	if (ak4438->pdn_gpiod) {
		gpiod_set_value_cansleep(ak4438->pdn_gpiod, 0);
		usleep_range(1000, 1100);
		gpiod_set_value_cansleep(ak4438->pdn_gpiod, 1);
		usleep_range(1000, 1100);
	}

	ak4438_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

#ifndef AK4438_ACKS_USE_MANUAL_MODE
	snd_soc_update_bits(codec, AK4438_00_CONTROL1,
		0x80,
		0x80);   // ACKS bit = 1; //10000000
	dev_dbg(codec->dev, "%s ACKS bit = 1\n", __func__);
#endif

	ak4438_set_transition_mode(codec, ak4438->transition_mode);
	ak4438_set_digital_filter(codec, ak4438->digfil);

	ak4438_rstn_control(codec, 0);
	ak4438_rstn_control(codec, 1);

	dev_dbg(codec->dev, "%s\n", __func__);
	return 0;
}

static int ak4438_parse_dt(struct ak4438_priv *ak4438)
{
	struct device *dev;
	struct device_node *np;
	int ret;
	u32 val;

	dev = NULL;

	dev = &(ak4438->i2c->dev);

	np = dev->of_node;
	if (!np)
		return -1;

	ak4438->pdn_gpiod = devm_gpiod_get_optional(dev,
		"ak4438,pdn-gpio", GPIOD_OUT_LOW);
	if (IS_ERR(ak4438->pdn_gpiod))
		return PTR_ERR(ak4438->pdn_gpiod);

	ak4438->mute_gpiod = devm_gpiod_get_optional(dev,
		"ak4438,mute-gpio", GPIOD_OUT_LOW);
	if (IS_ERR(ak4438->mute_gpiod))
		return PTR_ERR(ak4438->mute_gpiod);

	ret = of_property_read_u32(np, "ak4438,transition-mode", &val);
	if (ret == 0)
		ak4438->transition_mode = val & 0x3;
	else
		ak4438->transition_mode = TRANS_3_255_FS;

	ret = of_property_read_u32(np, "ak4438,digital-filter-mode", &val);
	if (ret == 0)
		ak4438->digfil = val & 0x7;
	else
		ak4438->digfil = DIGFTL_SUPER_SLOW_ROLLOFF;

	return 0;
}

static int ak4438_probe(struct snd_soc_codec *codec)
{
	struct ak4438_priv *ak4438 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dev_dbg(codec->dev, "%s\n", __func__);

	ak4438->fs = 48000;
	ak4438->enable_mute_delay = true;

	ret = ak4438_parse_dt(ak4438);

	if (ak4438->pdn_gpiod)
		gpiod_direction_output(ak4438->pdn_gpiod, 0);

	if (ak4438->mute_gpiod)
		gpiod_direction_output(ak4438->mute_gpiod, 0);

	ak4438_init_reg(codec);

	dev_dbg(codec->dev, "%s return %d\n", __func__, ret);

	return ret;
}

static int ak4438_remove(struct snd_soc_codec *codec)
{
	struct ak4438_priv *ak4438 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s\n", __func__);

	ak4438_set_bias_level(codec, SND_SOC_BIAS_OFF);

	if (ak4438->pdn_gpiod)
		gpiod_set_value_cansleep(ak4438->pdn_gpiod, 0);

	return 0;
}

static int ak4438_suspend(struct snd_soc_codec *codec)
{
	struct ak4438_priv *ak4438 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s\n", __func__);

	ak4438_set_bias_level(codec, SND_SOC_BIAS_OFF);

	regcache_cache_only(ak4438->regmap, true);
	regcache_mark_dirty(ak4438->regmap);

	if (ak4438->pdn_gpiod) {
		gpiod_set_value_cansleep(ak4438->pdn_gpiod, 0);
		mdelay(1);
	}

	return 0;
}

static int ak4438_resume(struct snd_soc_codec *codec)
{
	struct ak4438_priv *ak4438 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s\n", __func__);

	if (ak4438->pdn_gpiod) {
		gpiod_set_value_cansleep(ak4438->pdn_gpiod, 0);
		mdelay(1);
		gpiod_set_value_cansleep(ak4438->pdn_gpiod, 1);
		mdelay(1);
	}

	regcache_cache_only(ak4438->regmap, false);
	regcache_sync(ak4438->regmap);

	return 0;
}

struct snd_soc_codec_driver soc_codec_dev_ak4438 = {
	.probe = ak4438_probe,
	.remove = ak4438_remove,
	.suspend = ak4438_suspend,
	.resume = ak4438_resume,
	.set_bias_level = ak4438_set_bias_level,
	.component_driver = {
		.controls = ak4438_snd_controls,
		.num_controls = ARRAY_SIZE(ak4438_snd_controls),
		.dapm_widgets = ak4438_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(ak4438_dapm_widgets),
		.dapm_routes = ak4438_intercon,
		.num_dapm_routes = ARRAY_SIZE(ak4438_intercon),
	},
};

static const struct regmap_config ak4438_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = AK4438_MAX_REGISTERS - 1,
	.volatile_reg = ak4438_volatile,
	.writeable_reg = ak4438_writeable,
	.readable_reg = ak4438_readable,
	.reg_defaults = ak4438_reg,
	.num_reg_defaults = ARRAY_SIZE(ak4438_reg),
	.cache_type = REGCACHE_RBTREE,
};

static const struct of_device_id ak4438_if_dt_ids[] = {
	{ .compatible = "akm,ak4438", },
	{ }
};
MODULE_DEVICE_TABLE(of, ak4438_if_dt_ids);

static int ak4438_i2c_probe(struct i2c_client *i2c,
	const struct i2c_device_id *id)
{
	struct ak4438_priv *ak4438;
	int ret = 0;

	dev_dbg(&i2c->dev, "%s\n", __func__);

	ak4438 = devm_kzalloc(&i2c->dev, sizeof(struct ak4438_priv),
		GFP_KERNEL);
	if (ak4438 == NULL)
		return -ENOMEM;

	ak4438->regmap = devm_regmap_init_i2c(i2c, &ak4438_regmap);
	if (IS_ERR(ak4438->regmap))
		return PTR_ERR(ak4438->regmap);

	i2c_set_clientdata(i2c, ak4438);
	ak4438->i2c = i2c;

	ret = snd_soc_register_codec(&i2c->dev,
		&soc_codec_dev_ak4438, &ak4438_dai[0], ARRAY_SIZE(ak4438_dai));
	if (ret < 0) {
		devm_kfree(&i2c->dev, ak4438);
		pr_info("%s register codec fail %d\n", __func__, ret);
	}

	return ret;
}

static int ak4438_i2c_remove(struct i2c_client *client)//16/04/11
{
	snd_soc_unregister_codec(&client->dev);
	return 0;
}

static const struct i2c_device_id ak4438_i2c_id[] = {
	{ "ak4438", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ak4438_i2c_id);

static struct i2c_driver ak4438_i2c_driver = {
	.driver = {
		.name = "ak4438",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ak4438_if_dt_ids),
	},
	.probe = ak4438_i2c_probe,
	.remove = ak4438_i2c_remove,	//16/04/11
	.id_table = ak4438_i2c_id,
};

static int __init ak4438_modinit(void)
{
	int ret = 0;

	ret = i2c_add_driver(&ak4438_i2c_driver);
	if (ret != 0)
		pr_info("Failed to register AK4438 I2C driver: %d\n", ret);

	return ret;
}

module_init(ak4438_modinit);

static void __exit ak4438_exit(void)
{
	i2c_del_driver(&ak4438_i2c_driver);
}
module_exit(ak4438_exit);

MODULE_DESCRIPTION("ASoC ak4438 DAC driver");
MODULE_LICENSE("GPL");
