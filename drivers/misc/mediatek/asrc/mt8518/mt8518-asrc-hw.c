/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <sound/pcm_params.h>
#include "mt8518-asrc-clk.h"
#include <linux/of_address.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/device.h>
#include <linux/of_platform.h>

#include "mt8518-asrc-hw.h"
#include "mt8518-asrc-reg.h"

static const u32  PeriodModeVal_Dm48[FS_NUM] = {
	0x60000, 0x40000, 0x30000, 0x20000, 0x18000, 0x10000, 0x8000, 0x4000,
	0x2000, 0, 0, 0, 0, 0, 0, 0, 0x687d6, 0x45A8F, 0x343EB, 0x22D47,
	0x1A1F6, 0x116A4, 0x8B52, 0x45A9, 0x22D4
};

static const u32  FreqModeVal[FS_NUM] = {
	0x50000, 0x78000, 0xA0000, 0xF0000, 0x140000, 0x1E0000, 0x3C0000,
	0x780000, 0xF00000, 0, 0, 0, 0, 0, 0, 0, 0x49800, 0x6E400, 0x93000,
	0xDC800, 0x126000, 0x1B9000, 0x372000, 0x6E4000, 0xDC8000
};

static inline void afe_set_bit(u32 addr, int bit)
{
	afe_msk_write(addr,
	      (u32)(0x1U) << (u32)bit, (u32)(0x1U) << (u32)bit);
}

static inline void afe_clear_bit(u32 addr, int bit)
{
	afe_msk_write(addr, 0x0, (u32)(0x1U) << (u32)bit);
}

static inline void afe_write_bits(u32 addr, u32 value, int bits, int len)
{
	u32 u4TargetBitField =
	      (((u32)(0x1U) << (u32)len) - (u32)(1U)) << (u32)bits;
	u32 u4TargetValue = (value << (u32)bits) & u4TargetBitField;
	u32 u4CurrValue;

	u4CurrValue = afe_read(addr);
	afe_write(addr, ((u4CurrValue & (~u4TargetBitField))
	      | u4TargetValue));
}

static inline u32 afe_read_bits(u32 addr, int bits, int len)
{
	u32 u4TargetBitField =
	      (((u32)(0x1U) << (u32)len) - (u32)(1U)) << (u32)bits;
	u32 u4CurrValue = afe_read(addr);

	return (u4CurrValue & u4TargetBitField) >> (u32)bits;
}

#define DENOMINATOR_48K 0x3C00
#define DENOMINATOR_44K 0x3720

static bool clk_group_44k(enum afe_sampling_rate output_fs)
{
	if (output_fs >= FS_7350HZ && output_fs < FS_NUM)
		return true;
	return false;
}

static u32 afe_mem_asrc_sel_cali_denominator(
	enum afe_mem_asrc_id id,
	enum afe_sampling_rate output_fs)
{
	//default for 48k
	u32 denominator = DENOMINATOR_48K;
	const int offset = 3;
	int cali_clk_sel_bit = MEM_ASRC_1_CALI_CLK_SEL_POS
			+ id * offset;
	if (unlikely(id >= ASRC_MAX_NUM)) {
		pr_debug("%s() error: invalid asrc[%d]\n", __func__, id);
		return 0;
	}
	if (output_fs >= FS_NUM)
		return 0;

	if (clk_group_44k(output_fs)) {
		denominator = DENOMINATOR_44K;
		afe_set_bit(MASRC_ASM_CON2, cali_clk_sel_bit);
	} else {
		afe_clear_bit(MASRC_ASM_CON2, cali_clk_sel_bit);
	}
	return denominator;
}

static u32 afe_mem_asrc_get_cali_denominator(
	enum afe_mem_asrc_id id)
{
	u32 addr;

	if (unlikely(id >= ASRC_MAX_NUM)) {
		pr_debug("%s() error: invalid asrc[%d]\n", __func__, id);
		return -EINVAL;
	}
	addr = REG_ASRC_CALI_DENOMINATOR +
		(u32)(id) * MASRC_OFFSET;

	return afe_read_bits(addr, POS_ASRC_CALI_DENOMINATOR, 24);
}

static u32 afe_mem_asrc_get_cali_clk_rate(
	enum afe_sampling_rate output_fs)
{
	u32 val = 0;

	/* a2sys/a1sys clock rate */
	if (clk_group_44k(output_fs))
		val = asrc_get_clock_rate(MT8518_CLK_APLL1) / 4;
	else
		val = asrc_get_clock_rate(MT8518_CLK_APLL2) / 4;

	return val;
}

static u32 afe_mem_asrc_get_cali_cyc(
	enum afe_mem_asrc_id id)
{
	u32 addr;

	if (unlikely(id >= ASRC_MAX_NUM)) {
		pr_debug("%s() error: invalid asrc[%d]\n", __func__, id);
		return -EINVAL;
	}
	addr = REG_ASRC_FREQ_CALI_CYC +
		(u32)(id) * MASRC_OFFSET;

	return 1 + afe_read_bits(addr, POS_ASRC_FREQ_CALI_CYC, 16);
}
static u32 afe_mem_asrc_get_cali_trk_mode(
	enum afe_mem_asrc_id id)
{
	u32 addr;

	if (unlikely(id >= ASRC_MAX_NUM)) {
		pr_debug("%s() error: invalid asrc[%d]\n", __func__, id);
		return -EINVAL;
	}
	addr = REG_ASRC_FREQ_CALI_CTRL+
		(u32)(id) * MASRC_OFFSET;

	return afe_read_bits(addr, POS_FREQ_UPDATE_FS2, 1);
}

static u32 afe_mem_asrc_get_cali_trk_src(
	enum afe_mem_asrc_id id)
{
	u32 addr;

	if (unlikely(id >= ASRC_MAX_NUM)) {
		pr_debug("%s() error: invalid asrc[%d]\n", __func__, id);
		return -EINVAL;
	}
	addr = REG_ASRC_FREQ_CALI_CTRL+
		(u32)(id) * MASRC_OFFSET;

	return afe_read_bits(addr, POS_SRC_SEL, 2);
}

#define RATIOVER 23U
#define INV_COEF 24U
#define NO_NEED 25U
#define TBL_SZ_MEMASRC_IIR_COEF (48)

static const u32 *get_iir_coef(enum afe_sampling_rate input_fs,
			enum afe_sampling_rate output_fs, size_t *count)
{
	static const u32 IIR_COEF_384_TO_352[TBL_SZ_MEMASRC_IIR_COEF] = {
	0x10bea3af, 0x2007e9be, 0x10bea3af, 0xe2821372, 0xf0848d58, 0x00000003,
	0x08f9d435, 0x113d1a1f, 0x08f9d435, 0xe31a73c5, 0xf1030af1, 0x00000003,
	0x09dd37b9, 0x13106967, 0x09dd37b9, 0xe41398e1, 0xf1c98ae5, 0x00000003,
	0x0b55c74b, 0x16182d46, 0x0b55c74b, 0xe5bce8cb, 0xf316f594, 0x00000003,
	0x0e02cb05, 0x1b950f07, 0x0e02cb05, 0xf44d829a, 0xfaa9876b, 0x00000004,
	0x13e0e18e, 0x277f6d77, 0x13e0e18e, 0xf695efae, 0xfc700da4, 0x00000004,
	0x0db3df0d, 0x1b6240b3, 0x0db3df0d, 0xf201ce8e, 0xfca24567, 0x00000003,
	0x06b31e0f, 0x0cca96d1, 0x06b31e0f, 0xc43a9021, 0xe051c370, 0x00000002
	};

	static const u32 IIR_COEF_256_TO_192[TBL_SZ_MEMASRC_IIR_COEF] = {
	0x0de3c667, 0x137bf0e3, 0x0de3c667, 0xd9575388, 0xe0d4770d, 0x00000002,
	0x0e54ed46, 0x1474090f, 0x0e54ed46, 0xdb1c8213, 0xe2a7b6b7, 0x00000002,
	0x0d58713b, 0x13bde364, 0x05d8713b, 0xde0a3770, 0xe5183cde, 0x00000002,
	0x0bdcfce3, 0x128ef355, 0x0bdcfce3, 0xe2be28af, 0xe8affd19, 0x00000002,
	0x139091b3, 0x20f20a8e, 0x139091b3, 0xe9ed58af, 0xedff795d, 0x00000002,
	0x0e68e9cd, 0x1a4cb00b, 0x0e68e9cd, 0xf3ba2b24, 0xf5275137, 0x00000002,
	0x13079595, 0x251713f9, 0x13079595, 0xf78c204d, 0xf227616a, 0x00000000,
	0x00000000, 0x2111eb8f, 0x2111eb8f, 0x0014ac5b, 0x00000000, 0x00000006
	};

	static const u32 IIR_COEF_352_TO_256[TBL_SZ_MEMASRC_IIR_COEF] = {
	0x0db45c84, 0x1113e68a, 0x0db45c84, 0xdf58fbd3, 0xe0e51ba2, 0x00000002,
	0x0e0c4d8f, 0x11eaf5ef, 0x0e0c4d8f, 0xe11e9264, 0xe2da4b80, 0x00000002,
	0x0cf2558c, 0x1154c11a, 0x0cf2558c, 0xe41c6288, 0xe570c517, 0x00000002,
	0x0b5132d7, 0x10545ecd, 0x0b5132d7, 0xe8e2e944, 0xe92f8dc6, 0x00000002,
	0x1234ffbb, 0x1cfba5c7, 0x1234ffbb, 0xf00653e0, 0xee9406e3, 0x00000002,
	0x0cfd073a, 0x170277ad, 0x0cfd073a, 0xf96e16e7, 0xf59562f9, 0x00000002,
	0x08506c2b, 0x1011cd72, 0x08506c2b, 0x164a9eae, 0xe4203311, 0xffffffff,
	0x00000000, 0x3d58af1e, 0x3d58af1e, 0x001bee13, 0x00000000, 0x00000007
	};

	static const u32 IIR_COEF_384_TO_256[TBL_SZ_MEMASRC_IIR_COEF] = {
	0x0eca2fa9, 0x0f2b0cd3, 0x0eca2fa9, 0xf50313ef, 0xf15857a7, 0x00000003,
	0x0ee239a9, 0x1045115c, 0x0ee239a9, 0xec9f2976, 0xe5090807, 0x00000002,
	0x0ec57a45, 0x11d000f7, 0x0ec57a45, 0xf0bb67bb, 0xe84c86de, 0x00000002,
	0x0e85ba7e, 0x13ee7e9a, 0x0e85ba7e, 0xf6c74ebb, 0xecdba82c, 0x00000002,
	0x1cba1ac9, 0x2da90ada, 0x1cba1ac9, 0xfecba589, 0xf2c756e1, 0x00000002,
	0x0f79dec4, 0x1c27f5e0, 0x0f79dec4, 0x03c44399, 0xfc96c6aa, 0x00000003,
	0x1104a702, 0x21a72c89, 0x1104a702, 0x1b6a6fb8, 0xfb5ee0f2, 0x00000001,
	0x0622fc30, 0x061a0c67, 0x0622fc30, 0xe88911f2, 0xe0da327a, 0x00000002
	};

	static const u32 IIR_COEF_352_TO_192[TBL_SZ_MEMASRC_IIR_COEF] = {
	0x1c012b9a, 0x09302bd9, 0x1c012b9a, 0x0056c6d0, 0xe2b7f35c, 0x00000002,
	0x1b60cee5, 0x0b59639b, 0x1b60cee5, 0x045dc965, 0xca2264a0, 0x00000001,
	0x19ec96ad, 0x0eb20aa9, 0x19ec96ad, 0x0a6789cd, 0xd08944ba, 0x00000001,
	0x17c243aa, 0x1347e7fc, 0x17c243aa, 0x131e03a8, 0xd9241dd4, 0x00000001,
	0x1563b168, 0x1904032f, 0x1563b168, 0x0f0d206b, 0xf1d7f8e1, 0x00000002,
	0x14cd0206, 0x2169e2af, 0x14cd0206, 0x14a5d991, 0xf7279caf, 0x00000002,
	0x0aac4c7f, 0x14cb084b, 0x0aac4c7f, 0x30bc41c6, 0xf5565720, 0x00000001,
	0x0cea20d5, 0x03bc5f00, 0x0cea20d5, 0xfeec800a, 0xc1b99664, 0x00000001
	};

	static const u32 IIR_COEF_384_TO_192[TBL_SZ_MEMASRC_IIR_COEF] = {
	0x1bd356f3, 0x012e014f, 0x1bd356f3, 0x081be0a6, 0xe28e2407, 0x00000002,
	0x0d7d8ee8, 0x01b9274d, 0x0d7d8ee8, 0x09857a7b, 0xe4cae309, 0x00000002,
	0x0c999cbe, 0x038e89c5, 0x0c999cbe, 0x0beae5bc, 0xe7ded2a4, 0x00000002,
	0x0b4b6e2c, 0x061cd206, 0x0b4b6e2c, 0x0f6a2551, 0xec069422, 0x00000002,
	0x13ad5974, 0x129397e7, 0x13ad5974, 0x13d3c166, 0xf11cacb8, 0x00000002,
	0x126195d4, 0x1b259a6c, 0x126195d4, 0x184cdd94, 0xf634a151, 0x00000002,
	0x092aa1ea, 0x11add077, 0x092aa1ea, 0x3682199e, 0xf31b28fc, 0x00000001,
	0x0e09b91b, 0x0010b76f, 0x0e09b91b, 0x0f0e2575, 0xc19d364a, 0x00000001
	};

	static const u32 IIR_COEF_384_TO_176[TBL_SZ_MEMASRC_IIR_COEF] = {
	0x1b4feb25, 0xfa1874df, 0x1b4feb25, 0x0fc84364, 0xe27e7427, 0x00000002,
	0x0d22ad1f, 0xfe465ea8, 0x0d22ad1f, 0x10d89ab2, 0xe4aa760e, 0x00000002,
	0x0c17b497, 0x004c9a14, 0x0c17b497, 0x12ba36ef, 0xe7a11513, 0x00000002,
	0x0a968b87, 0x031b65c2, 0x0a968b87, 0x157c39d1, 0xeb9561ce, 0x00000002,
	0x11cea26a, 0x0d025bcc, 0x11cea26a, 0x18ef4a32, 0xf05a2342, 0x00000002,
	0x0fe5d188, 0x156af55c, 0x0fe5d188, 0x1c6234df, 0xf50cd288, 0x00000002,
	0x07a1ea25, 0x0e900dd7, 0x07a1ea25, 0x3d441ae6, 0xf0314c15, 0x00000001,
	0x0dd3517a, 0xfc7f1621, 0x0dd3517a, 0x1ee4972a, 0xc193ad77, 0x00000001
	};

	static const u32 IIR_COEF_256_TO_96[TBL_SZ_MEMASRC_IIR_COEF] = {
	0x0bad1c6d, 0xf7125e39, 0x0bad1c6d, 0x200d2195, 0xe0e69a20, 0x00000002,
	0x0b7cc85d, 0xf7b2aa2b, 0x0b7cc85d, 0x1fd4a137, 0xe2d2e8fc, 0x00000002,
	0x09ad4898, 0xf9f3edb1, 0x09ad4898, 0x202ffee3, 0xe533035b, 0x00000002,
	0x073ebe31, 0xfcd552f2, 0x073ebe31, 0x2110eb62, 0xe84975f6, 0x00000002,
	0x092af7cc, 0xff2b1fc9, 0x092af7cc, 0x2262052a, 0xec1ceb75, 0x00000002,
	0x09655d3e, 0x04f0939d, 0x09655d3e, 0x47cf219d, 0xe075904a, 0x00000001,
	0x021b3ca5, 0x03057f44, 0x021b3ca5, 0x4a5c8f68, 0xe72b7f7b, 0x00000001,
	0x00000000, 0x389ecf53, 0x358ecf53, 0x04b60049, 0x00000000, 0x00000004
	};

	static const u32 IIR_COEF_352_TO_128[TBL_SZ_MEMASRC_IIR_COEF] = {
	0x0c4deacd, 0xf5b3be35, 0x0c4deacd, 0x20349d1f, 0xe0b9a80d, 0x00000002,
	0x0c5dbbaa, 0xf6157998, 0x0c5dbbaa, 0x200c143d, 0xe25209ea, 0x00000002,
	0x0a9de1bd, 0xf85ee460, 0x0a9de1bd, 0x206099de, 0xe46a166c, 0x00000002,
	0x081f9a34, 0xfb7ffe47, 0x081f9a34, 0x212dd0f7, 0xe753c9ab, 0x00000002,
	0x0a6f9ddb, 0xfd863e9e, 0x0a6f9ddb, 0x226bd8a2, 0xeb2ead0b, 0x00000002,
	0x05497d0e, 0x01ebd7f0, 0x05497d0e, 0x23eba2f6, 0xef958aff, 0x00000002,
	0x008e7c5f, 0x00be6aad, 0x008e7c5f, 0x4a74b30a, 0xe6b0319a, 0x00000001,
	0x00000000, 0x38f3c5aa, 0x38f3c5aa, 0x012e1306, 0x00000000, 0x00000006
	};

	static const u32 IIR_COEF_384_TO_128[TBL_SZ_MEMASRC_IIR_COEF] = {
	0x0cf188aa, 0xf37845cc, 0x0cf188aa, 0x126b5cbc, 0xf10e5785, 0x00000003,
	0x0c32c481, 0xf503c49b, 0x0c32c481, 0x24e5a686, 0xe3edcb35, 0x00000002,
	0x0accda0f, 0xf7ad602d, 0x0accda0f, 0x2547ad4f, 0xe65c4390, 0x00000002,
	0x08d6d7fb, 0xfb56b002, 0x08d6d7fb, 0x25f3f39f, 0xe9860165, 0x00000002,
	0x0d4b1ceb, 0xff189a5d, 0x0d4b1ceb, 0x26d3a3a5, 0xed391db5, 0x00000002,
	0x0a060fcf, 0x07a2d23a, 0x0a060fcf, 0x27b2168e, 0xf0c10173, 0x00000002,
	0x040b6e8c, 0x0742638c, 0x040b6e8c, 0x5082165c, 0xe5f8f032, 0x00000001,
	0x067a1ae1, 0xf98acf04, 0x067a1ae1, 0x2526b255, 0xe0ab23e6, 0x00000002
	};

	static const u32 IIR_COEF_352_TO_96[TBL_SZ_MEMASRC_IIR_COEF] = {
	0x0ba3aaf1, 0xf0c12941, 0x0ba3aaf1, 0x2d8fe4ae, 0xe097f1ad, 0x00000002,
	0x0be92064, 0xf0b1f1a9, 0x0be92064, 0x2d119d04, 0xe1e5fe1b, 0x00000002,
	0x0a1220de, 0xf3a9aff8, 0x0a1220de, 0x2ccb18cb, 0xe39903cf, 0x00000002,
	0x07794a30, 0xf7c2c155, 0x07794a30, 0x2ca647c8, 0xe5ef0ccd, 0x00000002,
	0x0910b1c4, 0xf84c9886, 0x0910b1c4, 0x2c963877, 0xe8fbcb7a, 0x00000002,
	0x041d6154, 0xfec82c8a, 0x041d6154, 0x2c926893, 0xec6aa839, 0x00000002,
	0x005b2676, 0x0050bb1f, 0x005b2676, 0x5927e9f4, 0xde9fd5bc, 0x00000001,
	0x00000000, 0x2b1e5dc1, 0x2b1e5dc1, 0x0164aa09, 0x00000000, 0x00000006
	};

	static const u32 IIR_COEF_384_TO_96[TBL_SZ_MEMASRC_IIR_COEF] = {
	0x0481f41d, 0xf9c1b194, 0x0481f41d, 0x31c66864, 0xe0581a1d, 0x00000002,
	0x0a3e5a4c, 0xf216665d, 0x0a3e5a4c, 0x31c3de69, 0xe115ebae, 0x00000002,
	0x0855f15c, 0xf5369aef, 0x0855f15c, 0x323c17ad, 0xe1feed04, 0x00000002,
	0x05caeeeb, 0xf940c54b, 0x05caeeeb, 0x33295d2b, 0xe3295c94, 0x00000002,
	0x0651a46a, 0xfa4d6542, 0x0651a46a, 0x3479d138, 0xe49580b2, 0x00000002,
	0x025e0ccb, 0xff36a412, 0x025e0ccb, 0x35f517d7, 0xe6182a82, 0x00000002,
	0x0085eff3, 0x0074e0ca, 0x0085eff3, 0x372ef0de, 0xe7504e71, 0x00000002,
	0x00000000, 0x29b76685, 0x29b76685, 0x0deab1c3, 0x00000000, 0x00000003
	};

	static const u32 IIR_COEF_384_TO_88[TBL_SZ_MEMASRC_IIR_COEF] = {
	0x0c95e01f, 0xed56f8fc, 0x0c95e01f, 0x191b8467, 0xf0c99b0e, 0x00000003,
	0x0bbee41a, 0xef0e8160, 0x0bbee41a, 0x31c02b41, 0xe2ef4cd9, 0x00000002,
	0x0a2d258f, 0xf2225b96, 0x0a2d258f, 0x314c8bd2, 0xe4c10e08, 0x00000002,
	0x07f9e42a, 0xf668315f, 0x07f9e42a, 0x30cf47d4, 0xe71e3418, 0x00000002,
	0x0afd6fa9, 0xf68f867d, 0x0afd6fa9, 0x3049674d, 0xe9e0cf4b, 0x00000002,
	0x06ebc830, 0xffaa9acd, 0x06ebc830, 0x2fcee1bf, 0xec81ee52, 0x00000002,
	0x010de038, 0x01a27806, 0x010de038, 0x2f82d453, 0xee2ade9b, 0x00000002,
	0x064f0462, 0xf68a0d30, 0x064f0462, 0x32c81742, 0xe07f3a37, 0x00000002
	};

	static const u32 IIR_COEF_256_TO_48[TBL_SZ_MEMASRC_IIR_COEF] = {
	0x02b72fb4, 0xfb7c5152, 0x02b72fb4, 0x374ab8ef, 0xe039095c, 0x00000002,
	0x05ca62de, 0xf673171b, 0x05ca62de, 0x1b94186a, 0xf05c2de7, 0x00000003,
	0x09a9656a, 0xf05ffe29, 0x09a9656a, 0x37394e81, 0xe1611f87, 0x00000002,
	0x06e86c29, 0xf54bf713, 0x06e86c29, 0x37797f41, 0xe24ce1f6, 0x00000002,
	0x07a6b7c2, 0xf5491ea7, 0x07a6b7c2, 0x37e40444, 0xe3856d91, 0x00000002,
	0x02bf8a3e, 0xfd2f5fa6, 0x02bf8a3e, 0x38673190, 0xe4ea5a4d, 0x00000002,
	0x007e1bd5, 0x000e76ca, 0x007e1bd5, 0x38da5414, 0xe61afd77, 0x00000002,
	0x00000000, 0x2038247b, 0x2038247b, 0x07212644, 0x00000000, 0x00000004
	};

	static const u32 IIR_COEF_352_TO_64[TBL_SZ_MEMASRC_IIR_COEF] = {
	0x05c89f29, 0xf6443184, 0x05c89f29, 0x1bbe0f00, 0xf034bf19, 0x00000003,
	0x05e47be3, 0xf6284bfe, 0x05e47be3, 0x1b73d610, 0xf0a9a268, 0x00000003,
	0x09eb6c29, 0xefbc8df5, 0x09eb6c29, 0x365264ff, 0xe286ce76, 0x00000002,
	0x0741f28e, 0xf492d155, 0x0741f28e, 0x35a08621, 0xe4320cfe, 0x00000002,
	0x087cdc22, 0xf3daa1c7, 0x087cdc22, 0x34c55ef0, 0xe6664705, 0x00000002,
	0x038022af, 0xfc43da62, 0x038022af, 0x33d2b188, 0xe8e92eb8, 0x00000002,
	0x001de8ed, 0x0001bd74, 0x001de8ed, 0x33061aa8, 0xeb0d6ae7, 0x00000002,
	0x00000000, 0x3abd8743, 0x3abd8743, 0x032b3f7f, 0x00000000, 0x00000005
	};

	static const u32 IIR_COEF_384_TO_64[TBL_SZ_MEMASRC_IIR_COEF] = {
	0x05690759, 0xf69bdff3, 0x05690759, 0x392fbdf5, 0xe032c3cc, 0x00000002,
	0x05c3ff7a, 0xf60d6b05, 0x05c3ff7a, 0x1c831a72, 0xf052119a, 0x00000003,
	0x0999efb9, 0xefae71b0, 0x0999efb9, 0x3900fd02, 0xe13a60b9, 0x00000002,
	0x06d5aa46, 0xf4c1d0ea, 0x06d5aa46, 0x39199f34, 0xe20c15e1, 0x00000002,
	0x077f7d1d, 0xf49411e4, 0x077f7d1d, 0x394b3591, 0xe321be50, 0x00000002,
	0x02a14b6b, 0xfcd3c8a5, 0x02a14b6b, 0x398b4c12, 0xe45e5473, 0x00000002,
	0x00702155, 0xffef326c, 0x00702155, 0x39c46c90, 0xe56c1e59, 0x00000002,
	0x00000000, 0x1c69d66c, 0x1c69d66c, 0x0e76f270, 0x00000000, 0x00000003
	};

	static const u32 IIR_COEF_352_TO_48[TBL_SZ_MEMASRC_IIR_COEF] = {
	0x05be8a21, 0xf589fb98, 0x05be8a21, 0x1d8de063, 0xf026c3d8, 0x00000003,
	0x05ee4f4f, 0xf53df2e5, 0x05ee4f4f, 0x1d4d87e2, 0xf07d5518, 0x00000003,
	0x0a015402, 0xee079bc7, 0x0a015402, 0x3a0a0c2b, 0xe1e16c40, 0x00000002,
	0x07512c6a, 0xf322f651, 0x07512c6a, 0x394e82c2, 0xe326def2, 0x00000002,
	0x087a5316, 0xf1d3ba1f, 0x087a5316, 0x385bbd4a, 0xe4dbe26b, 0x00000002,
	0x035bd161, 0xfb2b7588, 0x035bd161, 0x37464782, 0xe6d6a034, 0x00000002,
	0x00186dd8, 0xfff28830, 0x00186dd8, 0x365746b9, 0xe88d9a4a, 0x00000002,
	0x00000000, 0x2cd02ed1, 0x2cd02ed1, 0x035f6308, 0x00000000, 0x00000005
	};

	static const u32 IIR_COEF_384_TO_48[TBL_SZ_MEMASRC_IIR_COEF] = {
	0x0c68c88c, 0xe9266466, 0x0c68c88c, 0x1db3d4c3, 0xf0739c07, 0x00000003,
	0x05c69407, 0xf571a70a, 0x05c69407, 0x1d6f1d3b, 0xf0d89718, 0x00000003,
	0x09e8d133, 0xee2a68df, 0x09e8d133, 0x3a32d61b, 0xe2c2246a, 0x00000002,
	0x079233b7, 0xf2d17252, 0x079233b7, 0x3959a2c3, 0xe4295381, 0x00000002,
	0x09c2822e, 0xf0613d7b, 0x09c2822e, 0x385c3c48, 0xe5d3476b, 0x00000002,
	0x050e0b2c, 0xfa200d5d, 0x050e0b2c, 0x37688f21, 0xe76fc030, 0x00000002,
	0x006ddb6e, 0x00523f01, 0x006ddb6e, 0x36cd234d, 0xe8779510, 0x00000002,
	0x0635039f, 0xf488f773, 0x0635039f, 0x3be42508, 0xe0488e99, 0x00000002
	};

	static const u32 IIR_COEF_384_TO_44[TBL_SZ_MEMASRC_IIR_COEF] = {
	0x0c670696, 0xe8dc1ef2, 0x0c670696, 0x1e05c266, 0xf06a9f0d, 0x00000003,
	0x05c60160, 0xf54b9f4a, 0x05c60160, 0x1dc3811d, 0xf0c7e4db, 0x00000003,
	0x09e74455, 0xeddfc92a, 0x09e74455, 0x3adfddda, 0xe28c4ae3, 0x00000002,
	0x078ea9ae, 0xf28c3ba7, 0x078ea9ae, 0x3a0a98e8, 0xe3d93541, 0x00000002,
	0x09b32647, 0xefe954c5, 0x09b32647, 0x3910a244, 0xe564f781, 0x00000002,
	0x04f0e9e4, 0xf9b7e8d5, 0x04f0e9e4, 0x381f6928, 0xe6e5316c, 0x00000002,
	0x006303ee, 0x003ae836, 0x006303ee, 0x37852c0e, 0xe7db78c1, 0x00000002,
	0x06337ac0, 0xf46665c5, 0x06337ac0, 0x3c818406, 0xe042df81, 0x00000002
	};

	static const u32 IIR_COEF_352_TO_32[TBL_SZ_MEMASRC_IIR_COEF] = {
	0x07d25973, 0xf0fd68ae, 0x07d25973, 0x3dd9d640, 0xe02aaf11, 0x00000002,
	0x05a0521d, 0xf5390cc4, 0x05a0521d, 0x1ec7dff7, 0xf044be0d, 0x00000003,
	0x04a961e1, 0xf71c730b, 0x04a961e1, 0x1e9edeee, 0xf082b378, 0x00000003,
	0x06974728, 0xf38e3bf1, 0x06974728, 0x3cd69b60, 0xe1afd01c, 0x00000002,
	0x072d4553, 0xf2c1e0e2, 0x072d4553, 0x3c54fdc3, 0xe28e96b6, 0x00000002,
	0x02802de3, 0xfbb07dd5, 0x02802de3, 0x3bc4f40f, 0xe38a3256, 0x00000002,
	0x000ce31b, 0xfff0d7a8, 0x000ce31b, 0x3b4bbb40, 0xe45f55d6, 0x00000002,
	0x00000000, 0x1ea1b887, 0x1ea1b887, 0x03b1b27d, 0x00000000, 0x00000005
	};

	static const u32 IIR_COEF_384_TO_32[TBL_SZ_MEMASRC_IIR_COEF] = {
	0x0c5074a7, 0xe83ee090, 0x0c5074a7, 0x1edf8fe7, 0xf04ec5d0, 0x00000003,
	0x05bbb01f, 0xf4fa20a7, 0x05bbb01f, 0x1ea87e16, 0xf093b881, 0x00000003,
	0x04e8e57f, 0xf69fc31d, 0x04e8e57f, 0x1e614210, 0xf0f1139e, 0x00000003,
	0x07756686, 0xf1f67c0b, 0x07756686, 0x3c0a3b55, 0xe2d8c5a6, 0x00000002,
	0x097212e8, 0xeede0608, 0x097212e8, 0x3b305555, 0xe3ff02e3, 0x00000002,
	0x0495d6c0, 0xf8bf1399, 0x0495d6c0, 0x3a5c93a1, 0xe51e0d14, 0x00000002,
	0x00458b2d, 0xfffdc761, 0x00458b2d, 0x39d4793b, 0xe5d6d407, 0x00000002,
	0x0609587b, 0xf456ed0f, 0x0609587b, 0x3e1d20e1, 0xe0315c96, 0x00000002
	};

	static const u32 IIR_COEF_352_TO_24[TBL_SZ_MEMASRC_IIR_COEF] = {
	0x062002ee, 0xf4075ac9, 0x062002ee, 0x1f577599, 0xf0166280, 0x00000003,
	0x05cdb68c, 0xf4ab2e81, 0x05cdb68c, 0x1f2a7a17, 0xf0484eb7, 0x00000003,
	0x04e3078b, 0xf67b954a, 0x04e3078b, 0x1ef25b71, 0xf08a5bcf, 0x00000003,
	0x071fc81e, 0xf23391f6, 0x071fc81e, 0x3d4bc51b, 0xe1cdf67e, 0x00000002,
	0x08359c1c, 0xf04d3910, 0x08359c1c, 0x3c80bf1e, 0xe2c6cf99, 0x00000002,
	0x0331888d, 0xfa1ebde6, 0x0331888d, 0x3b94c153, 0xe3e96fad, 0x00000002,
	0x00143063, 0xffe1d1af, 0x00143063, 0x3ac672e3, 0xe4e7f96f, 0x00000002,
	0x00000000, 0x2d7cf831, 0x2d7cf831, 0x074e3a4f, 0x00000000, 0x00000004
	};

	static const u32 IIR_COEF_384_TO_24[TBL_SZ_MEMASRC_IIR_COEF] = {
	0x0c513993, 0xe7dbde26, 0x0c513993, 0x1f4e3b98, 0xf03b6bee, 0x00000003,
	0x05bd9980, 0xf4c4fb19, 0x05bd9980, 0x1f21aa2b, 0xf06fa0e5, 0x00000003,
	0x04eb9c21, 0xf6692328, 0x04eb9c21, 0x1ee6fb2f, 0xf0b6982c, 0x00000003,
	0x07795c9e, 0xf18d56cf, 0x07795c9e, 0x3d345c1a, 0xe229a2a1, 0x00000002,
	0x096d3d11, 0xee265518, 0x096d3d11, 0x3c7d096a, 0xe30bee74, 0x00000002,
	0x0478f0db, 0xf8270d5a, 0x0478f0db, 0x3bc96998, 0xe3ea3cf8, 0x00000002,
	0x0037d4b8, 0xffdedcf0, 0x0037d4b8, 0x3b553ec9, 0xe47a2910, 0x00000002,
	0x0607e296, 0xf42bc1d7, 0x0607e296, 0x3ee67cb9, 0xe0252e31, 0x00000002
	};

	static const struct {
		const u32 *coef;
		size_t cnt;
	} iir_coef_tbl_list[23] = {
		{ IIR_COEF_384_TO_352, ARRAY_SIZE(IIR_COEF_384_TO_352) },/* 0 */
		{ IIR_COEF_256_TO_192, ARRAY_SIZE(IIR_COEF_256_TO_192) },/* 1 */
		{ IIR_COEF_352_TO_256, ARRAY_SIZE(IIR_COEF_352_TO_256) },/* 2 */
		{ IIR_COEF_384_TO_256, ARRAY_SIZE(IIR_COEF_384_TO_256) },/* 3 */
		{ IIR_COEF_352_TO_192, ARRAY_SIZE(IIR_COEF_352_TO_192) },/* 4 */
		{ IIR_COEF_384_TO_192, ARRAY_SIZE(IIR_COEF_384_TO_192) },/* 5 */
		{ IIR_COEF_384_TO_176, ARRAY_SIZE(IIR_COEF_384_TO_176) },/* 6 */
		{ IIR_COEF_256_TO_96, ARRAY_SIZE(IIR_COEF_256_TO_96) },/* 7 */
		{ IIR_COEF_352_TO_128, ARRAY_SIZE(IIR_COEF_352_TO_128) },/* 8 */
		{ IIR_COEF_384_TO_128, ARRAY_SIZE(IIR_COEF_384_TO_128) },/* 9 */
		{ IIR_COEF_352_TO_96, ARRAY_SIZE(IIR_COEF_352_TO_96) },/* 10 */
		{ IIR_COEF_384_TO_96, ARRAY_SIZE(IIR_COEF_384_TO_96) },/* 11 */
		{ IIR_COEF_384_TO_88, ARRAY_SIZE(IIR_COEF_384_TO_88) },/* 12 */
		{ IIR_COEF_256_TO_48, ARRAY_SIZE(IIR_COEF_256_TO_48) },/* 13 */
		{ IIR_COEF_352_TO_64, ARRAY_SIZE(IIR_COEF_352_TO_64) },/* 14 */
		{ IIR_COEF_384_TO_64, ARRAY_SIZE(IIR_COEF_384_TO_64) },/* 15 */
		{ IIR_COEF_352_TO_48, ARRAY_SIZE(IIR_COEF_352_TO_48) },/* 16 */
		{ IIR_COEF_384_TO_48, ARRAY_SIZE(IIR_COEF_384_TO_48) },/* 17 */
		{ IIR_COEF_384_TO_44, ARRAY_SIZE(IIR_COEF_384_TO_44) },/* 18 */
		{ IIR_COEF_352_TO_32, ARRAY_SIZE(IIR_COEF_352_TO_32) },/* 19 */
		{ IIR_COEF_384_TO_32, ARRAY_SIZE(IIR_COEF_384_TO_32) },/* 20 */
		{ IIR_COEF_352_TO_24, ARRAY_SIZE(IIR_COEF_352_TO_24) },/* 21 */
		{ IIR_COEF_384_TO_24, ARRAY_SIZE(IIR_COEF_384_TO_24) },/* 22 */
	};

	static const u32 freq_new_index[FS_NUM] = {
		0, 1, 2, 3, 4, 5, 6, 7,
		8, 99, 99, 99, 99, 99, 99, 99,
		9, 10, 11, 12, 13, 14, 15, 16,
		17
	};

	static const u32 iir_coef_tbl_matrix[18][18] = {
		{
			NO_NEED, NO_NEED, NO_NEED, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED, NO_NEED, 0, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED
		},
		{
			3, NO_NEED, NO_NEED, NO_NEED, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED, INV_COEF, 0, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED, NO_NEED, NO_NEED,
			NO_NEED
		},
		{
			5, 1, NO_NEED, NO_NEED, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED, 6, INV_COEF, 0,
			NO_NEED, NO_NEED, NO_NEED, NO_NEED, NO_NEED,
			NO_NEED
		},
		{
			9, 5, 3, NO_NEED, NO_NEED, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED, INV_COEF, 6, INV_COEF, 0,
			NO_NEED, NO_NEED, NO_NEED, NO_NEED, NO_NEED
		},
		{
			11, 7, 5, 1, NO_NEED, NO_NEED, NO_NEED, NO_NEED,
			NO_NEED, 12, INV_COEF, 6, INV_COEF, 0, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED
		},
		{
			15, 11, 9, 5, 3, NO_NEED, NO_NEED, NO_NEED, NO_NEED,
			INV_COEF, 12, INV_COEF, 6, INV_COEF, 0, NO_NEED,
			NO_NEED, NO_NEED
		},
		{
			20, 17, 15, 11, 9, 5, NO_NEED, NO_NEED, NO_NEED,
			INV_COEF, 18, INV_COEF, 12, INV_COEF, 6, 0, NO_NEED,
			NO_NEED
		},
		{
			RATIOVER, 22, 20, 17, 15, 11, 5, NO_NEED, NO_NEED,
			RATIOVER, RATIOVER, INV_COEF, 18, INV_COEF, 12,
			6, 0, NO_NEED
		},
		{
			RATIOVER, RATIOVER, RATIOVER, 22, 20, 17, 11, 5,
			NO_NEED, RATIOVER, RATIOVER, RATIOVER,
			RATIOVER, INV_COEF, 18, 12, 6, 0
		},
		{
			NO_NEED, NO_NEED, NO_NEED, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED
		},
		{
			2, NO_NEED, NO_NEED, NO_NEED, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED, 3, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED, NO_NEED, NO_NEED,
			NO_NEED
		},
		{
			4, INV_COEF, NO_NEED, NO_NEED, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED, 5, 1, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED, NO_NEED, NO_NEED,
			NO_NEED
		},
		{
			8, 4, 2, NO_NEED, NO_NEED, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED, 9, 5, 3, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED, NO_NEED
		},
		{
			10, INV_COEF, 4, INV_COEF, NO_NEED, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED, 11, 7, 5, 1, NO_NEED,
			NO_NEED, NO_NEED, NO_NEED, NO_NEED
		},
		{
			14, 10, 8, 4, 2, NO_NEED, NO_NEED, NO_NEED, NO_NEED,
			15, 11, 9, 5, 3, NO_NEED, NO_NEED, NO_NEED, NO_NEED
		},
		{
			19, 16, 14, 10, 8, 4, NO_NEED, NO_NEED, NO_NEED, 20,
			17, 15, 11, 9, 5, NO_NEED, NO_NEED, NO_NEED
		},
		{
			RATIOVER, 21, 19, 16, 14, 10, 4, NO_NEED, NO_NEED,
			RATIOVER, 22, 20, 17, 15, 11, 5, NO_NEED, NO_NEED
		},
		{
			RATIOVER, RATIOVER, RATIOVER, 21, 19, 16, 10, 4,
			NO_NEED, RATIOVER, RATIOVER, RATIOVER, 22, 20,
			17, 11, 5, NO_NEED
		}
	};

	const u32 *coef = NULL;
	size_t cnt = 0;
	u32 i = freq_new_index[input_fs];
	u32 j = freq_new_index[output_fs];

	if (i >= 18 || j >= 18) {
		pr_debug("%s() error: input_fs=0x%x, i=%u, output_fs=0x%x, j=%u\n",
			__func__, input_fs, i, output_fs, j);
	} else {
		u32 k = iir_coef_tbl_matrix[i][j];

		pr_debug("%s() input_fs=0x%x, output_fs=0x%x, i=%u, j=%u, k=%u\n",
			__func__, input_fs, output_fs, i, j, k);

		if (k >= NO_NEED)
			pr_debug("%s() warning: NO_NEED\n", __func__);
		else if (k == RATIOVER)
			pr_debug("%s() warning: up-sampling ratio exceeds 16\n",
			__func__);
		else if (k == INV_COEF)
			pr_debug("%s() warning: up-sampling ratio need gen coef\n",
			__func__);
		else {
			coef = iir_coef_tbl_list[k].coef;
			cnt = iir_coef_tbl_list[k].cnt;
		}
	}
	*count = cnt;
	return coef;
}

/********************* memory asrc *********************/
int afe_power_on_mem_asrc_brg(int on)
{
	static DEFINE_MUTEX(lock);
	static int status;

	mutex_lock(&lock);
	if (on != 0) {
		if (status == 0)
			afe_clear_bit(MASRC_TOP_CON, PDN_ASRC_BRG_POS);
		++status;
	} else {
		if (status > 0 && (status-1) == 0)
			afe_set_bit(MASRC_TOP_CON, PDN_ASRC_BRG_POS);
		status--;
	}
	mutex_unlock(&lock);

	return 0;
}

int afe_power_on_mem_asrc(enum afe_mem_asrc_id id, int on)
{
	int pos;

	if (unlikely(id >= ASRC_MAX_NUM)) {
		pr_debug("%s() error: invalid asrc[%d]\n", __func__, id);
		return -EINVAL;
	}
	if (on != 0) {
		pos = PDN_MEM_ASRC1_POS + (int)(id);
		afe_clear_bit(MASRC_TOP_CON, pos);
		/* force reset */
		pos = MEM_ASRC_1_RESET_POS + (int)(id);
		afe_set_bit(MASRC_ASM_CON2, pos);
		afe_clear_bit(MASRC_ASM_CON2, pos);
	} else {
		pos = PDN_MEM_ASRC1_POS + (int)(id);
		afe_set_bit(MASRC_TOP_CON, pos);
	}
	//dump whole register
	pr_debug("%s - MASRC_TOP_CON: 0x%08x  ...\n", __func__,
		afe_read_bits(MASRC_TOP_CON, 0, 32));
	pr_debug("%s - MASRC_ASM_CON2: 0x%08x  ...\n", __func__,
		afe_read_bits(MASRC_ASM_CON2, 0, 32));

	return 0;
}

u32 afe_mem_asrc_irq_status(enum afe_mem_asrc_id id)
{
	u32 addr = REG_ASRC_IFR + (u32)(id) * MASRC_OFFSET;

	return afe_read(addr);
}

void afe_mem_asrc_irq_clear(enum afe_mem_asrc_id id, u32 status)
{
	u32 addr = REG_ASRC_IFR + (u32)(id) * MASRC_OFFSET;

	afe_write(addr, status);
}

int afe_mem_asrc_irq_enable(enum afe_mem_asrc_id id, u32 interrupts, int en)
{
	u32 addr = REG_ASRC_IER + (u32)(id) * MASRC_OFFSET;
	u32 val = (en != 0) ? interrupts : 0U;

	afe_msk_write(addr, val, interrupts);

	return 0;
}

int afe_mem_asrc_irq_is_enabled(enum afe_mem_asrc_id id, u32 interrupt)
{
	u32 addr = REG_ASRC_IER + (u32)(id) * MASRC_OFFSET;

	return (((u32)afe_read(addr) & interrupt) != 0U) ? 1 : 0;
}

static enum afe_sampling_rate freq_to_fs(u32 freq)
{
	if (freq < (0x049800U + 0x050000U) / 2U)
		return FS_7350HZ;	/* 0x049800 */
	else if (freq < (0x050000U + 0x06E400U) / 2U)
		return FS_8000HZ;	/* 0x050000 */
	else if (freq < (0x06E400U + 0x078000U) / 2U)
		return FS_11025HZ;	/* 0x06E400 */
	else if (freq < (0x078000U + 0x093000U) / 2U)
		return FS_12000HZ;	/* 0x078000 */
	else if (freq < (0x093000U + 0x0A0000U) / 2U)
		return FS_14700HZ;	/* 0x093000 */
	else if (freq < (0x0A0000U + 0x0DC800U) / 2U)
		return FS_16000HZ;	/* 0x0A0000 */
	else if (freq < (0x0DC800U + 0x0F0000U) / 2U)
		return FS_22050HZ;	/* 0x0DC800 */
	else if (freq < (0x0F0000U + 0x126000U) / 2U)
		return FS_24000HZ;	/* 0x0F0000 */
	else if (freq < (0x126000U + 0x140000U) / 2U)
		return FS_29400HZ;	/* 0x126000 */
	else if (freq < (0x140000U + 0x1B9000U) / 2U)
		return FS_32000HZ;	/* 0x140000 */
	else if (freq < (0x1B9000U + 0x1E0000U) / 2U)
		return FS_44100HZ;	/* 0x1B9000 */
	else if (freq < (0x1E0000U + 0x372000U) / 2U)
		return FS_48000HZ;	/* 0x1E0000 */
	else if (freq < (0x372000u + 0x3C0000U) / 2U)
		return FS_88200HZ;	/* 0x372000 */
	else if (freq < (0x3C0000U + 0x6E4000U) / 2U)
		return FS_96000HZ;	/* 0x3C0000 */
	else if (freq < (0x6E4000U + 0x780000U) / 2U)
		return FS_176400HZ;	/* 0x6E4000 */
	else if (freq < (0x780000U + 0xDC8000U) / 2U)
		return FS_192000HZ;	/* 0x780000 */
	else if (freq < (0xDC8000U + 0xF00000U) / 2U)
		return FS_352800HZ;	/* 0xDC8000 */
	else
		return FS_384000HZ;	/* 0xF00000 */
}

static u32 cali_trx_src_mapping(
	enum afe_mem_asrc_tracking_source tracking_src)
{
	switch (tracking_src) {
	case MEM_ASRC_MULTI_IN:
	case MEM_ASRC_SPDIF_IN:
		return 0;
	case MEM_ASRC_PCMIF_SLAVE:
		return 1;
	case MEM_ASRC_ETDM_IN2_SLAVE:
		return 2;
	case MEM_ASRC_ETDM_IN1_SLAVE:
		return 3;
	default:
		break;
	}

	return 0;
}

int afe_mem_asrc_cali_setup(enum afe_mem_asrc_id id,
	enum afe_mem_asrc_tracking_mode tracking_mode,
	enum afe_mem_asrc_tracking_source tracking_src,
	u32 cali_cycle,
	u32 input_freq,
	u32 output_freq)
{
	int asrc_running;
	u32 cali_status, addr_offset, freq;
	u32 denominator = DENOMINATOR_48K;
	u32 tmp;
	enum afe_sampling_rate input_fs = freq_to_fs(input_freq);
	enum afe_sampling_rate output_fs = freq_to_fs(output_freq);

	addr_offset = (u32)(id) * MASRC_OFFSET;
	if (unlikely(id >= ASRC_MAX_NUM)) {
		pr_debug("%s() error: invalid asrc[%d]\n", __func__, id);
		return -EINVAL;
	}
	if (tracking_src >= MEM_ASRC_TRX_SRC_NUM) {
		pr_debug("%s() error: invalid tracking_src:%d\n", __func__,
			tracking_src);
		return -EINVAL;
	}

	/* check freq setting if asrc is running. */
	asrc_running = afe_read_bits(REG_ASRC_GEN_CONF + addr_offset,
			  POS_ASRC_BUSY, 1);
	if (asrc_running == 1U) {
		/* check output freq */
		freq = afe_read_bits(REG_ASRC_FREQUENCY_1 + addr_offset
				, 0, 24);
		if (output_freq != freq) {
			pr_debug("%s() error: output freq is different with running setting:%u, %u\n",
				__func__, output_freq, freq);
			return -EINVAL;
		}
	}

	/* Check Freq Cali status */
	cali_status = afe_read_bits(REG_ASRC_FREQ_CALI_CTRL
			+ addr_offset, POS_FREQ_CALC_BUSY, 1);
	if (cali_status != 0)
		pr_debug(" Warning! Freq Calibration is busy!\n");
	cali_status = afe_read_bits(REG_ASRC_FREQ_CALI_CTRL
			+ addr_offset, POS_CALI_EN, 1);
	if (cali_status != 0)
		pr_debug(" Warning! Freq Calibration is busy!\n");

	pr_debug("%s() mode:%d, tracking src:%d, cali_cycle:%d\n",
		__func__, tracking_mode, tracking_src, cali_cycle);
	pr_debug("%s() input_freq:%u, output_freq:%u, input_fs:%d, output_fs:%d\n",
		__func__, input_freq, output_freq, input_fs, output_fs);

	/* check whether or not tracking mode/src is valid. */
	switch (tracking_mode) {
	case MEM_ASRC_TRACKING_RX:
		denominator = afe_mem_asrc_sel_cali_denominator(id,
				output_fs);
		if (tracking_src != MEM_ASRC_SPDIF_IN &&
			tracking_src != MEM_ASRC_MULTI_IN) {
			pr_debug("%s() RX not support this src:%d!\n",
				__func__, tracking_src);
			return -EINVAL;
		}
		/* when cali is running,
		 * others set different parameters need to block.
		 */
		if (cali_status != 0) {
			freq = afe_read_bits(REG_ASRC_FREQUENCY_0 + addr_offset
					, 0, 24);
			if (output_freq != freq) {
				pr_debug("%s() output freq is different: %u, %u\n",
					__func__, output_freq, freq);
				return -EINVAL;
			}

			tmp = 1 + afe_read_bits(REG_ASRC_FREQ_CALI_CYC +
					addr_offset,
					POS_ASRC_FREQ_CALI_CYC, 16);
			if (tmp != cali_cycle) {
				pr_debug("%s() cali_cycle is different: %u, %u\n",
					__func__, tmp, cali_cycle);
				return -EINVAL;
			}

			pr_debug("%s() same cali setting, so bypass following setting.\n",
					__func__);
			return 0;
		}
		break;
	case MEM_ASRC_TRACKING_TX:
		pr_debug("%s() TX not support!\n", __func__);
		if (tracking_src == MEM_ASRC_SPDIF_IN ||
			tracking_src == MEM_ASRC_MULTI_IN)
			pr_debug("%s() TX not support this src:%d!\n",
			__func__, tracking_src);
		return -EINVAL;
	case MEM_ASRC_NO_TRACKING:
		return -EINVAL;
	default:
		return -EINVAL;
	}

	/* HW usage: cali_cycle - 1 */
	afe_write_bits(REG_ASRC_FREQ_CALI_CYC + addr_offset, cali_cycle - 1,
			POS_ASRC_FREQ_CALI_CYC, 16);
	afe_write_bits(REG_ASRC_CALI_DENOMINATOR + addr_offset,
			denominator, POS_ASRC_CALI_DENOMINATOR, 24);
	afe_write_bits(REG_ASRC_FREQ_CALI_CTRL + addr_offset,
			cali_trx_src_mapping(tracking_src), POS_SRC_SEL, 2);

	/* set tracking mode TX/RX */
	if (tracking_mode == MEM_ASRC_TRACKING_RX) {
		/* MEM_ASRC_TRACKING_RX. Freq Mode   Bit9 = 1. */
		afe_set_bit(REG_ASRC_FREQ_CALI_CTRL + addr_offset,
			POS_FREQ_UPDATE_FS2);
		afe_write_bits(REG_ASRC_FREQUENCY_0 + addr_offset,
			FreqModeVal[output_fs], 0, 24);
		afe_write_bits(REG_ASRC_FREQUENCY_2 + addr_offset,
			FreqModeVal[input_fs], 0, 24);
	} else {
		/* MEM_ASRC_TRACKING_TX. Period Mode Bit9 = 0 */
		afe_clear_bit(REG_ASRC_FREQ_CALI_CTRL + addr_offset,
			POS_FREQ_UPDATE_FS2);
		afe_write_bits(REG_ASRC_FREQUENCY_0 + addr_offset,
			PeriodModeVal_Dm48[input_fs], 0, 24);
		afe_write_bits(REG_ASRC_FREQUENCY_2 + addr_offset,
			PeriodModeVal_Dm48[output_fs], 0, 24);
	}

	afe_write_bits(REG_ASRC_CH01_CNFG + addr_offset, 2, POS_IFS, 2);
	afe_write_bits(REG_ASRC_CH01_CNFG + addr_offset, 0, POS_OFS, 2);
	/* BYPASS_DEGLITCH */
	afe_set_bit(REG_ASRC_FREQ_CALI_CTRL + addr_offset,
		POS_BYPASS_DEGLITCH);
	/* AUTO_RESTART */
	afe_set_bit(REG_ASRC_FREQ_CALI_CTRL + addr_offset,
		POS_AUTO_RESTART);

	return 0;
}

int afe_mem_asrc_cali_enable(enum afe_mem_asrc_id id, int en)
{
	u32 addr_offset;

	if (unlikely(id >= ASRC_MAX_NUM)) {
		pr_debug("%s() error: invalid asrc[%d]\n", __func__, id);
		return -EINVAL;
	}
	addr_offset = (u32)(id) * MASRC_OFFSET;

	/* CALI_EN or DISABLE */
	if (en == 0)
		afe_clear_bit(REG_ASRC_FREQ_CALI_CTRL + addr_offset,
			POS_CALI_EN);
	else {
		afe_set_bit(REG_ASRC_FREQ_CALI_CTRL + addr_offset,
			POS_CALI_EN);
		// AUTO_FS2_UPDATE
		afe_set_bit(REG_ASRC_FREQ_CALI_CTRL + addr_offset,
			POS_AUTO_FS2_UPDATE);
	}

	return 0;
}

#define CALI_PRECISION 100000
#define AVOID_OVERFLOW_ONE 8192
#define AVOID_OVERFLOW_TWO 1024
#define CALI_GET_RES_PERIOD_MODE (1)
#define CALI_GET_RES_FREQ_MODE (!CALI_GET_RES_PERIOD_MODE)

u64 afe_mem_asrc_calibration_result(enum afe_mem_asrc_id id,
				u32 freq)
{
	u32 cali_result, offset;
	u32 cali_cycles, cali_reg_pos;
	u32 cali_addr = REG_ASRC_FREQ_CALI_RESULT;
	u32 denominator, cali_clk;
	u64 cali_result_rate;
	enum afe_mem_asrc_tracking_mode trk_mode;
	enum afe_sampling_rate fs;
	enum afe_mem_asrc_tracking_source trk_src;

	trk_mode = (afe_mem_asrc_get_cali_trk_mode(id) == 1) ?
			MEM_ASRC_TRACKING_RX :
			MEM_ASRC_TRACKING_TX;
	trk_src = afe_mem_asrc_get_cali_trk_src(id);
	fs = freq_to_fs(freq);
	if (unlikely(id >= ASRC_MAX_NUM)) {
		pr_debug("%s() error: invalid asrc[%d]\n", __func__, id);
		return 0;
	}

	offset = (u32)(id) * MASRC_OFFSET;
	pr_debug("%s() freq:%u, trk_mode:%d, trk_src:%d, fs:%d\n",
		__func__, freq, trk_mode, trk_src, fs);

	switch (trk_mode) {
	case MEM_ASRC_TRACKING_RX:
#if CALI_GET_RES_PERIOD_MODE
		cali_addr = REG_ASRC_PRD_CALI_RESULT + offset;
		cali_reg_pos = POS_ASRC_PRD_CALI_RESULT;
#else
		cali_addr = REG_ASRC_FREQ_CALI_RESULT + offset;
		cali_reg_pos = POS_ASRC_FREQ_CALI_RESULT;
#endif
		break;
	case MEM_ASRC_TRACKING_TX:
		cali_addr = REG_ASRC_PRD_CALI_RESULT + offset;
		cali_reg_pos = POS_ASRC_PRD_CALI_RESULT;
		pr_debug("tx mode not support.\n");
		return 0;
	case MEM_ASRC_NO_TRACKING:
	default:
		pr_debug("wrong trk mode.\n");
		return 0;
	}

	denominator = afe_mem_asrc_get_cali_denominator(id);
	cali_cycles = afe_mem_asrc_get_cali_cyc(id);
	cali_clk = afe_mem_asrc_get_cali_clk_rate(fs);

	cali_result = afe_read_bits(cali_addr, cali_reg_pos, 24);
	if ((cali_result == 0) || (cali_result == 0xffffff)) {
		pr_debug("[%s] invalid cali_result:0x%08x\n", __func__,
			cali_result);
		return 0;
	}

	/* calculate cali_freq (10^-5 Hz) */
	if (trk_mode == MEM_ASRC_TRACKING_RX) {
#if CALI_GET_RES_PERIOD_MODE
		/* fs = (cali_clk / cali_result) * cali_cycles */
		cali_result_rate = (u64)cali_clk * CALI_PRECISION;
		do_div(cali_result_rate, cali_result);
		cali_result_rate *= cali_cycles;
#else
		/* fs = (((cali_clk / denominator) * cali_cycles / 2^13)
		 *	* ((cali_result * 10^5) / 2^10))
		 */
		cali_result_rate = ((cali_clk / denominator) * cali_cycles /
				AVOID_OVERFLOW_ONE) *
				((u64)cali_result * CALI_PRECISION /
				AVOID_OVERFLOW_TWO);
		pr_debug("[%s] denominator:0x%08x\n", __func__, denominator);
#endif
		pr_debug("[%s] cali_result_rate: %llu\n", __func__,
			cali_result_rate);
		pr_debug("[%s] cali_result:0x%08x\n", __func__, cali_result);
		pr_debug("[%s] cali_clk:0x%08x\n", __func__, cali_clk);
	}

	return cali_result_rate;
}

int afe_mem_asrc_configurate(enum afe_mem_asrc_id id,
	const struct afe_mem_asrc_config *config)
{
	u32 addr_offset;
	const u32 *coef;
	size_t coef_count = 0;
	enum afe_sampling_rate input_fs, output_fs;
	int ret = 0;

	if (config == NULL) {
		pr_debug("%s() error: invalid config parameter\n", __func__);
		return -EINVAL;
	}
	if (unlikely(id >= ASRC_MAX_NUM)) {
		pr_debug("%s() error: invalid asrc[%d]\n", __func__, id);
		return -EINVAL;
	}
	if ((config->input_buffer.base & 0xFU) != 0U) {
		pr_debug("%s() error: input_buffer.base(0x%08x) is not 16 byte align\n",
		       __func__, config->input_buffer.base);
		return -EINVAL;
	}
	if ((config->input_buffer.size & 0xFU) != 0U) {
		pr_debug("%s() error: input_buffer.size(0x%08x) is not 16 byte align\n",
		       __func__, config->input_buffer.base);
		return -EINVAL;
	}
	if (config->input_buffer.size < 64U
	    || config->input_buffer.size > 0xffff0U) {
		pr_debug("%s() error: input_buffer.size(0x%08x) is too small or too large\n",
		       __func__, config->input_buffer.size);
		return -EINVAL;
	}
	if ((config->output_buffer.base & 0xFU) != 0U) {
		pr_debug("%s() error: output_buffer.base(0x%08x) is not 16 byte align\n",
		       __func__, config->output_buffer.base);
		return -EINVAL;
	}
	if ((config->output_buffer.size & 0xFU) != 0U) {
		pr_debug("%s() error: output_buffer.size(0x%08x) is not 16 byte align\n",
		       __func__, config->output_buffer.base);
		return -EINVAL;
	}
	if (config->output_buffer.size < 64U
	    || config->output_buffer.size > 0xffff0U) {
		pr_debug("%s() error: output_buffer.size(0x%08x) is too small or too large\n",
		       __func__, config->output_buffer.size);
		return -EINVAL;
	}
	input_fs = freq_to_fs(config->input_buffer.freq);
	output_fs = freq_to_fs(config->output_buffer.freq);
	pr_debug("%s() config->input_buffer.freq=0x%08x(%u)\n", __func__,
		  config->input_buffer.freq, input_fs);
	pr_debug("%s() config->output_buffer.freq=0x%08x(%u)\n", __func__,
		  config->output_buffer.freq, output_fs);
	addr_offset = (u32)(id) * MASRC_OFFSET;
	/* check whether mem-asrc is running */
	if (afe_read_bits(REG_ASRC_GEN_CONF + addr_offset,
			  POS_ASRC_BUSY, 1) == 1U) {
		pr_debug("%s() error: asrc[%d] is running\n", __func__, id);
		return -EBUSY;
	}
	/* when there is only 1 block data left
	 * in the input buffer, issue interrupt
	 */
	/* times of 512bit. */
	afe_write_bits(REG_ASRC_IBUF_INTR_CNT0 + addr_offset,
		       0xFF, POS_CH01_IBUF_INTR_CNT, 8);
	/* when there is only 1 block space in
	 * the output buffer, issue interrupt
	 */
	/* times of 512bit. 0xFF means if more than 16kB, send interrupt */
	afe_write_bits(REG_ASRC_OBUF_INTR_CNT0 + addr_offset,
		       0xFF, POS_CH01_OBUF_INTR_CNT, 8);
	/* clear all interrupt flag */
	afe_mem_asrc_irq_clear(id, IBUF_EMPTY_INT | OBUF_OV_INT
			       | IBUF_AMOUNT_INT | OBUF_AMOUNT_INT);
	/* iir coeffient setting for down-sample */
	coef = get_iir_coef(input_fs, output_fs, &coef_count);
	if (coef != NULL) {
		int i;

		/* turn on IIR coef setting path */
		afe_set_bit(REG_ASRC_GEN_CONF + addr_offset,
			    POS_DSP_CTRL_COEFF_SRAM);
		/* Load Coef */
		afe_write_bits(REG_ASRC_IIR_CRAM_ADDR + addr_offset,
			    0, POS_ASRC_IIR_CRAM_ADDR, 32);
		for (i = 0; i < (int)coef_count; ++i)
			afe_write(REG_ASRC_IIR_CRAM_DATA + addr_offset,
				  coef[i]);
		afe_write_bits(REG_ASRC_IIR_CRAM_ADDR + addr_offset,
			    0, POS_ASRC_IIR_CRAM_ADDR, 32);
		/* turn off IIR coe setting path */
		afe_clear_bit(REG_ASRC_GEN_CONF + addr_offset,
			    POS_DSP_CTRL_COEFF_SRAM);
		/* set IIR_stage-1 */
		afe_write_bits(REG_ASRC_CH01_CNFG + addr_offset, 7,
			       POS_IIR_STAGE, 3);
		afe_set_bit(REG_ASRC_CH01_CNFG + addr_offset, POS_IIR_EN);
		afe_set_bit(REG_ASRC_GEN_CONF + addr_offset, POS_CH_CLEAR);
		afe_set_bit(REG_ASRC_GEN_CONF + addr_offset, POS_CH_EN);
	} else
		afe_clear_bit(REG_ASRC_CH01_CNFG + addr_offset, POS_IIR_EN);
	pr_debug("%s() config->input_buffer.base=0x%08x\n",
		  __func__, config->input_buffer.base);
	pr_debug("%s() config->input_buffer.size=0x%08x\n",
		  __func__, config->input_buffer.size);
	/* set input buffer's base and size */
	afe_write(REG_ASRC_IBUF_SADR + addr_offset, config->input_buffer.base);
	afe_write_bits(REG_ASRC_IBUF_SIZE + addr_offset,
		       config->input_buffer.size, POS_CH_IBUF_SIZE, 20);
	pr_debug("%s() config->output_buffer.base=0x%08x\n",
		  __func__, config->output_buffer.base);
	pr_debug("%s() config->output_buffer.size=0x%08x\n",
		  __func__, config->output_buffer.size);

	/* set input buffer's rp and wp */
	afe_write(REG_ASRC_CH01_IBUF_RDPNT + addr_offset,
		  config->input_buffer.base);
	afe_write(REG_ASRC_CH01_IBUF_WRPNT + addr_offset,
		  config->input_buffer.base);

	/* set output buffer's base and size */
	afe_write(REG_ASRC_OBUF_SADR + addr_offset,
		  config->output_buffer.base);
	afe_write_bits(REG_ASRC_OBUF_SIZE + addr_offset,
		  config->output_buffer.size,
		       POS_CH_OBUF_SIZE, 20);
	/* set output buffer's rp and wp */
	afe_write(REG_ASRC_CH01_OBUF_WRPNT + addr_offset,
		  config->output_buffer.base);
	afe_write(REG_ASRC_CH01_OBUF_RDPNT + addr_offset,
		  config->output_buffer.base +
		  config->output_buffer.size - 16U);
	if (config->input_buffer.bitwidth == 16U)
		afe_set_bit(REG_ASRC_CH01_CNFG + addr_offset, POS_IBIT_WIDTH);
	else
		afe_clear_bit(REG_ASRC_CH01_CNFG + addr_offset, POS_IBIT_WIDTH);
	if (config->output_buffer.bitwidth == 16U)
		afe_set_bit(REG_ASRC_CH01_CNFG + addr_offset, POS_OBIT_WIDTH);
	else
		afe_clear_bit(REG_ASRC_CH01_CNFG + addr_offset, POS_OBIT_WIDTH);
	if (config->stereo != 0)
		afe_clear_bit(REG_ASRC_CH01_CNFG + addr_offset, POS_MONO);
	else
		afe_set_bit(REG_ASRC_CH01_CNFG + addr_offset, POS_MONO);
	afe_write_bits(REG_ASRC_CH01_CNFG + addr_offset, 0x8,
		       POS_CLAC_AMOUNT, 8);
	afe_write_bits(REG_ASRC_MAX_OUT_PER_IN0 + addr_offset, 0,
		       POS_CH01_MAX_OUT_PER_IN0, 4);
	if (config->tracking_mode == MEM_ASRC_NO_TRACKING) {
		afe_write_bits(REG_ASRC_FREQUENCY_0 + addr_offset,
			       config->input_buffer.freq, 0, 24);
		afe_write_bits(REG_ASRC_FREQUENCY_1 + addr_offset,
			       config->output_buffer.freq, 0, 24);
		afe_write_bits(REG_ASRC_CH01_CNFG + addr_offset, 0, POS_IFS, 2);
		afe_write_bits(REG_ASRC_CH01_CNFG + addr_offset, 1, POS_OFS, 2);
	}
	else {
		ret = afe_mem_asrc_cali_setup(id, config->tracking_mode,
			config->tracking_src, config->cali_cycle,
			config->input_buffer.freq, config->output_buffer.freq);
		if (ret != 0)
			return -EINVAL;
	}

	return 0;
}

int afe_mem_asrc_enable(enum afe_mem_asrc_id id, int en)
{
	u32 addr;

	if (unlikely(id >= ASRC_MAX_NUM)) {
		pr_debug("%s() error: invalid id %u\n", __func__, id);
		return -EINVAL;
	}
	addr = REG_ASRC_GEN_CONF + (u32)(id) * MASRC_OFFSET;
	if (en != 0) {
		afe_set_bit(addr, POS_CH_CLEAR);
		afe_set_bit(addr, POS_CH_EN);
		afe_set_bit(addr, POS_ASRC_EN);
	} else {
		afe_clear_bit(addr, POS_CH_EN);
		afe_clear_bit(addr, POS_ASRC_EN);
	}
	return 0;
}

u32 afe_mem_asrc_get_ibuf_rp(enum afe_mem_asrc_id id)
{
	u32 offset;

	if (unlikely(id >= ASRC_MAX_NUM)) {
		pr_debug("%s() error: invalid asrc[%d]\n", __func__, id);
		return 0;
	}
	offset = (u32)(id) * MASRC_OFFSET;

	return (u32)afe_read(REG_ASRC_CH01_IBUF_RDPNT + offset)
		& (~(u32) 0xF);
}

u32 afe_mem_asrc_get_ibuf_wp(enum afe_mem_asrc_id id)
{
	u32 offset;

	if (unlikely(id >= ASRC_MAX_NUM)) {
		pr_debug("%s() error: invalid asrc[%d]\n", __func__, id);
		return 0;
	}
	offset = (u32)(id) * MASRC_OFFSET;

	return (u32)afe_read(REG_ASRC_CH01_IBUF_WRPNT + offset)
		& (~(u32) 0xF);
}

int afe_mem_asrc_set_ibuf_wp(enum afe_mem_asrc_id id, u32 p)
{
	u32 addr_offset;
	u32 base;
	u32 size;

	if (unlikely(id >= ASRC_MAX_NUM)) {
		pr_debug("%s() error: invalid asrc[%d]\n", __func__, id);
		return -EINVAL;
	}
	addr_offset = (u32)(id) * MASRC_OFFSET;
	base = afe_read(REG_ASRC_IBUF_SADR + addr_offset);
	size = afe_read(REG_ASRC_IBUF_SIZE + addr_offset);
	if (unlikely(p < base || p >= (base + size))) {
		pr_debug("%s() error: can't update input buffer's wp:0x%08x (base:0x%08x, size:0x%08x)\n",
			__func__, p, base, size);
		return -EINVAL;
	}
	afe_write(REG_ASRC_CH01_IBUF_WRPNT + addr_offset, p);
	return 0;
}

u32 afe_mem_asrc_get_obuf_rp(enum afe_mem_asrc_id id)
{
	u32 offset;

	if (unlikely(id >= ASRC_MAX_NUM)) {
		pr_debug("%s() error: invalid asrc[%d]\n", __func__, id);
		return 0;
	}
	offset = (u32)(id) * MASRC_OFFSET;

	return (u32)afe_read(REG_ASRC_CH01_OBUF_RDPNT + offset)
		& (~(u32) 0xF);
}

u32 afe_mem_asrc_get_obuf_wp(enum afe_mem_asrc_id id)
{
	u32 offset;

	if (unlikely(id >= ASRC_MAX_NUM)) {
		pr_debug("%s() error: invalid asrc[%d]\n", __func__, id);
		return 0;
	}
	offset = (u32)(id) * MASRC_OFFSET;

	return (u32)afe_read(REG_ASRC_CH01_OBUF_WRPNT + offset)
		& (~(u32) 0xF);
}

int afe_mem_asrc_set_obuf_rp(enum afe_mem_asrc_id id, u32 p)
{
	u32 addr_offset;
	u32 base;
	u32 size;

	if (unlikely(id >= ASRC_MAX_NUM)) {
		pr_debug("%s() error: invalid asrc[%d]\n", __func__, id);
		return -EINVAL;
	}
	addr_offset = (u32)(id) * MASRC_OFFSET;
	base = afe_read((u32)REG_ASRC_OBUF_SADR + addr_offset);
	size = afe_read((u32)REG_ASRC_OBUF_SIZE + addr_offset);
	if (unlikely(p < base || p >= (base + size))) {
		pr_debug("%s() error: can't update output buffer's rp:0x%08x (base:0x%08x, size:0x%08x)\n",
			__func__, p, base, size);
		return -EINVAL;
	}
	afe_write(REG_ASRC_CH01_OBUF_RDPNT + addr_offset, p);
	return 0;
}

int afe_mem_asrc_set_ibuf_freq(enum afe_mem_asrc_id id, u32 freq)
{
	u32 addr = REG_ASRC_FREQUENCY_0 + (u32)(id) * MASRC_OFFSET;

	if (unlikely(id >= ASRC_MAX_NUM)) {
		pr_debug("%s() error: invalid asrc[%d]\n", __func__, id);
		return 0;
	}
	afe_write_bits(addr, freq, 0, 24);
	return 0;
}

int afe_mem_asrc_set_obuf_freq(enum afe_mem_asrc_id id, u32 freq)
{
	u32 addr = REG_ASRC_FREQUENCY_1 + (u32)(id) * MASRC_OFFSET;

	if (unlikely(id >= ASRC_MAX_NUM)) {
		pr_debug("%s() error: invalid asrc[%d]\n", __func__, id);
		return 0;
	}
	afe_write_bits(addr, freq, 0, 24);
	return 0;
}
