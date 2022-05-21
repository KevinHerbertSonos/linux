/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Chen Zhong <chen.zhong@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk-provider.h>
#include <linux/io.h>

#include "clkdbg.h"

#define DUMP_INIT_STATE		0

/*
 * clkdbg dump_regs
 */

enum {
	topckgen,
	scpsys,
	apmixed,
	mcusys,
	mmsys,
};

#define REGBASE_V(_phys, _id_name) { .phys = _phys, .name = #_id_name }

/*
 * checkpatch.pl ERROR:COMPLEX_MACRO
 *
 * #define REGBASE(_phys, _id_name) [_id_name] = REGBASE_V(_phys, _id_name)
 */

static struct regbase rb[] = {
	[topckgen]  = REGBASE_V(0x10000000, topckgen),
	[scpsys]    = REGBASE_V(0x10006000, scpsys),
	[apmixed]   = REGBASE_V(0x10018000, apmixed),
	[mcusys]	= REGBASE_V(0x10200000, mcusys),
	[mmsys]     = REGBASE_V(0x14000000, mmsys),
};

#define REGNAME(_base, _ofs, _name)	\
	{ .base = &rb[_base], .ofs = _ofs, .name = #_name }

static struct regname rn[] = {
	REGNAME(topckgen, 0x000, CLK_MUX_SEL0),
	REGNAME(topckgen, 0x004, CLK_MUX_SEL1),
	REGNAME(topckgen, 0x020, CLK_GATING_CTRL0),
	REGNAME(topckgen, 0x024, CLK_GATING_CTRL1),
	REGNAME(topckgen, 0x03C, CLK_GATING_CTRL7),
	REGNAME(topckgen, 0x040, CLK_MUX_SEL8),
	REGNAME(topckgen, 0x044, CLK_SEL_9),
	REGNAME(topckgen, 0x070, CLK_GATING_CTRL8),
	REGNAME(topckgen, 0x07c, CLK_MUX_SEL13),
	REGNAME(topckgen, 0x0c0, CLK_MUX_SEL14),
	REGNAME(topckgen, 0x0c4, CLK_MUX_SEL15),
	REGNAME(topckgen, 0x0c8, CLK_MUX_SEL16),
	REGNAME(topckgen, 0x0cc, CLK_MUX_SEL17),
	REGNAME(topckgen, 0x0d4, CLK_MUX_SEL19),
	REGNAME(topckgen, 0x048, CLK_SEL_10),
	REGNAME(topckgen, 0x04C, CLK_SEL_11),
	REGNAME(topckgen, 0x078, CLK_SEL_12),
	REGNAME(topckgen, 0x0dc, CLK_MUX_SEL21),
	REGNAME(topckgen, 0x0e0, CLK_GATING_CTRL10),
	REGNAME(topckgen, 0x0e8, CLK_GATING_CTRL12),
	REGNAME(topckgen, 0x0ec, CLK_GATING_CTRL13),
	REGNAME(topckgen, 0x0f4, CLK_MUX_SEL22),
	REGNAME(topckgen, 0x0f8, CLK_MUX_SEL23),
	REGNAME(scpsys, 0x23c, SPM_DIS_PWR_CON),
	REGNAME(scpsys, 0xd10, SPM_AUDAFE_PWR_CON),
	REGNAME(scpsys, 0xd14, SPM_AUDSRC_PWR_CON),
	REGNAME(scpsys, 0xd30, SPM_CM4_PWR_CON),
	REGNAME(scpsys, 0x60c, SPM_PWR_STATUS),
	REGNAME(scpsys, 0x610, SPM_PWR_STATUS_2ND),
	REGNAME(apmixed, 0x100, ARMPLL_CON0),
	REGNAME(apmixed, 0x104, ARMPLL_CON1),
	REGNAME(apmixed, 0x108, ARMPLL_CON2),
	REGNAME(apmixed, 0x10C, ARMPLL_CON3),
	REGNAME(apmixed, 0x110, ARMPLL_PWR_CON0),
	REGNAME(apmixed, 0x120, MAINPLL_CON0),
	REGNAME(apmixed, 0x124, MAINPLL_CON1),
	REGNAME(apmixed, 0x128, MAINPLL_CON2),
	REGNAME(apmixed, 0x12C, MAINPLL_CON3),
	REGNAME(apmixed, 0x130, MAINPLL_PWR_CON0),
	REGNAME(apmixed, 0x140, UNIVPLL_CON0),
	REGNAME(apmixed, 0x144, UNIVPLL_CON1),
	REGNAME(apmixed, 0x148, UNIVPLL_CON2),
	REGNAME(apmixed, 0x14C, UNIVPLL_CON3),
	REGNAME(apmixed, 0x150, UNIVPLL_PWR_CON0),
	REGNAME(apmixed, 0x160, MMPLL_CON0),
	REGNAME(apmixed, 0x164, MMPLL_CON1),
	REGNAME(apmixed, 0x168, MMPLL_CON2),
	REGNAME(apmixed, 0x16C, MMPLL_CON3),
	REGNAME(apmixed, 0x170, MMPLL_PWR_CON0),
	REGNAME(apmixed, 0x180, APLL1_CON0),
	REGNAME(apmixed, 0x184, APLL1_CON1),
	REGNAME(apmixed, 0x188, APLL1_CON2),
	REGNAME(apmixed, 0x18C, APLL1_CON3),
	REGNAME(apmixed, 0x190, APLL1_PWR_CON0),
	REGNAME(apmixed, 0x194, APLL1_CON_TUNER),
	REGNAME(apmixed, 0x1A0, APLL2_CON0),
	REGNAME(apmixed, 0x1A4, APLL2_CON1),
	REGNAME(apmixed, 0x1A8, APLL2_CON2),
	REGNAME(apmixed, 0x1AC, APLL2_CON3),
	REGNAME(apmixed, 0x1B0, APLL2_PWR_CON0),
	REGNAME(apmixed, 0x1B4, APLL2_CON_TUNER),
	REGNAME(apmixed, 0x1C0, TVDPLL_CON0),
	REGNAME(apmixed, 0x1C4, TVDPLL_CON1),
	REGNAME(apmixed, 0x1C8, TVDPLL_CON2),
	REGNAME(apmixed, 0x1CC, TVDPLL_CON3),
	REGNAME(apmixed, 0x1D0, TVDPLL_PWR_CON0),
	REGNAME(mcusys, 0x7c0, MCU_BUS_MUX),
	REGNAME(mmsys, 0x100, MMSYS_CG_CON0),
	{}
};

static const struct regname *get_all_regnames(void)
{
	return rn;
}

static void __init init_regbase(void)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(rb); i++)
		rb[i].virt = ioremap(rb[i].phys, PAGE_SIZE);
}

/*
 * clkdbg fmeter
 */

#include <linux/delay.h>

#ifndef GENMASK
#define GENMASK(h, l)	(((1U << ((h) - (l) + 1)) - 1) << (l))
#endif

#define ALT_BITS(o, h, l, v) \
	(((o) & ~GENMASK(h, l)) | (((v) << (l)) & GENMASK(h, l)))

#define clk_readl(addr)		readl(addr)
#define clk_writel(addr, val)	\
	do { writel(val, addr); wmb(); } while (0) /* sync write */
#define clk_writel_mask(addr, mask, val)	\
	clk_writel(addr, (clk_readl(addr) & ~(mask)) | (val))

#define ABS_DIFF(a, b)	((a) > (b) ? (a) - (b) : (b) - (a))

enum FMETER_TYPE {
	FT_NULL,
	ABIST,
	CKGEN
};

#define FMCLK(_t, _i, _n) { .type = _t, .id = _i, .name = _n }

static const struct fmeter_clk fclks[] = {
	FMCLK(CKGEN,  1, "g_mainpll_d8_188m_ck"),
	FMCLK(CKGEN,  2, "g_mainpll_d11_137m_ck"),
	FMCLK(CKGEN,  3, "g_mainpll_d12_126m_ck"),
	FMCLK(CKGEN,  4, "g_mainpll_d20_76m_ck"),
	FMCLK(CKGEN,  5, "g_mainpll_d7_215m_ck"),
	FMCLK(CKGEN,  6, "g_univpll_d16_78m_ck"),
	FMCLK(CKGEN,  7, "g_univpll_d24_52m_ck"),
	FMCLK(CKGEN,  8, "csw_mux_nfi2x_ck"),
	FMCLK(ABIST, 11, "AD_SYS_26M_CK"),
	FMCLK(ABIST, 12, "AD_USB_F48M_CK"),
	FMCLK(CKGEN, 13, "csw_gfmux_emi1x_ck"),
	FMCLK(CKGEN, 14, "csw_gfmux_axibus_ck"),
	FMCLK(CKGEN, 15, "csw_mux_smi_ck"),
	FMCLK(CKGEN, 16, "csw_gfmux_uart0_ck"),
	FMCLK(CKGEN, 17, "csw_gfmux_uart1_ck"),
	FMCLK(CKGEN, 18, "hf_faud_intbus_ck"),
	FMCLK(CKGEN, 19, "csw_mux_msdc0_ck"),
	FMCLK(CKGEN, 20, "csw_mux_msdc1_ck"),
	FMCLK(ABIST, 22, "AD_MPPLL_TST_CK"),
	FMCLK(ABIST, 23, "AD_PLLGP_TST_CK"),
	FMCLK(CKGEN, 24, "csw_52m_ck"),
	FMCLK(CKGEN, 25, "mcusys_debug_mon0"),
	FMCLK(ABIST, 26, "csw_32k_ck"),
	FMCLK(ABIST, 27, "AD_MEMPLL_MONCLK"),
	FMCLK(ABIST, 28, "AD_MEMPLL2_MONCLK"),
	FMCLK(ABIST, 29, "AD_MEMPLL3_MONCLK"),
	FMCLK(ABIST, 30, "AD_MEMPLL4_MONCLK"),
	FMCLK(CKGEN, 32, "hf_spis_ck"),
	FMCLK(CKGEN, 33, "hf_gcpu_ck"),
	FMCLK(CKGEN, 34, "csw_mux_pwm_mm_ck"),
	FMCLK(CKGEN, 35, "csw_mux_ddrphycfg_ck"),
	FMCLK(CKGEN, 36, "csw_mux_pmicspi_ck"),
	FMCLK(CKGEN, 37, "csw_mux_spi_ck1"),
	FMCLK(CKGEN, 38, "csw_104m_ck"),
	FMCLK(CKGEN, 39, "csw_78m_ck"),
	FMCLK(CKGEN, 40, "csw_mux_spinor_ck"),
	FMCLK(CKGEN, 41, "csw_mux_eth_ck"),
	FMCLK(CKGEN, 42, "csw_mux_audio_ck"),
	FMCLK(CKGEN, 43, "csw_mux_imgrz_sys_ck"),
	FMCLK(CKGEN, 44, "csw_mux_png_sys_ck"),
	FMCLK(CKGEN, 45, "csw_mux_graph_eclk"),
	FMCLK(CKGEN, 46, "csw_mux_fdbi_ck"),
	FMCLK(CKGEN, 47, "csw_mux_aud1_ck"),
	FMCLK(CKGEN, 48, "csw_mux_aud2_ck"),
	FMCLK(CKGEN, 49, "csw_mux_fa2sys_ck"),
	FMCLK(CKGEN, 50, "csw_mux_fa1sys_ck"),
	FMCLK(CKGEN, 51, "csw_mux_spinfi_bclk_ck"),
	FMCLK(CKGEN, 52, "csw_mux_pwm_infra_ck"),
	FMCLK(CKGEN, 53, "csw_mux_aud_spdif_in_ck"),
	FMCLK(CKGEN, 54, "csw_gfmux_uart2_ck"),
	FMCLK(CKGEN, 56, "csw_mux_dbg_atclk_ck"),
	FMCLK(CKGEN, 57, "csw_mux_fasm_m_ck"),
	FMCLK(ABIST, 58, "csw_mux_fecc_ck"),
	FMCLK(ABIST, 59, "fq_trng_freq_debug_out0"),
	FMCLK(ABIST, 60, "fq_trng_freq_debug_out1"),
	FMCLK(ABIST, 61, "fq_usb20_clk480m"),
	FMCLK(CKGEN, 62, "csw_mux_hf_pe2_mac_ck"),
	FMCLK(CKGEN, 63, "csw_mux_hf_cmsys_ck"),
	FMCLK(ABIST, 64, "AD_ARMPLL_650M_CK"),
	FMCLK(ABIST, 65, "AD_MAINPLL_1501P5M_CK"),
	FMCLK(ABIST, 66, "AD_UNIVPLL_1248M_CK"),
	FMCLK(ABIST, 67, "AD_MMPLL_380M_CK"),
	FMCLK(ABIST, 68, "AD_TVDPLL_594M_CK"),
	FMCLK(ABIST, 69, "AD_AUD1PLL_180P6336M_CK"),
	FMCLK(ABIST, 70, "AD_AUD2PLL_196P608M_CK"),
	FMCLK(ABIST, 71, "AD_ARMPLL_650M_VPROC_CK"),
	FMCLK(ABIST, 72, "AD_USB20_48M_CK"),
	FMCLK(ABIST, 73, "AD_UNIV_48M_CK"),
	FMCLK(ABIST, 74, "AD_TPHY_26M_CK"),
	FMCLK(CKGEN, 75, "fq_hf_fasm_l_ck"),
	FMCLK(ABIST, 77, "AD_MEM_26M_CK"),
	FMCLK(ABIST, 78, "AD_MEMPLL5_MONCLK"),
	FMCLK(ABIST, 79, "AD_MEMPLL6_MONCLK"),
	FMCLK(CKGEN, 80, "csw_mux_msdc2_ck"),
	FMCLK(CKGEN, 81, "csw_mux_msdc0_hclk50_ck"),
	FMCLK(CKGEN, 82, "csw_mux_msdc2_hclk50_ck"),
	FMCLK(CKGEN, 83, "csw_mux_disp_dpi_ck"),
	FMCLK(CKGEN, 84, "csw_mux_spi_ck2"),
	FMCLK(CKGEN, 85, "csw_mux_spi_ck3"),
	FMCLK(CKGEN, 86, "csw_mux_hapll1_ck"),
	FMCLK(CKGEN, 87, "csw_mux_hapll2_ck"),
	FMCLK(CKGEN, 88, "csw_mux_i2c_ck"),
	FMCLK(CKGEN, 89, "csw_sej_13m_ck"),
	{}
};

#define PLL_HP_CON0			(rb[apmixed].virt + 0x014)
#define PLL_TEST_CON1			(rb[apmixed].virt + 0x064)
#define TEST_DBG_CTRL			(rb[topckgen].virt + 0x38)
#define FREQ_MTR_CTRL_REG		(rb[topckgen].virt + 0x10)
#define FREQ_MTR_CTRL_RDATA		(rb[topckgen].virt + 0x14)

#define RG_FQMTR_CKDIV_GET(x)		(((x) >> 28) & 0x3)
#define RG_FQMTR_CKDIV_SET(x)		(((x) & 0x3) << 28)
#define RG_FQMTR_FIXCLK_SEL_GET(x)	(((x) >> 24) & 0x3)
#define RG_FQMTR_FIXCLK_SEL_SET(x)	(((x) & 0x3) << 24)
#define RG_FQMTR_MONCLK_SEL_GET(x)	(((x) >> 16) & 0x7f)
#define RG_FQMTR_MONCLK_SEL_SET(x)	(((x) & 0x7f) << 16)
#define RG_FQMTR_MONCLK_EN_GET(x)	(((x) >> 15) & 0x1)
#define RG_FQMTR_MONCLK_EN_SET(x)	(((x) & 0x1) << 15)
#define RG_FQMTR_MONCLK_RST_GET(x)	(((x) >> 14) & 0x1)
#define RG_FQMTR_MONCLK_RST_SET(x)	(((x) & 0x1) << 14)
#define RG_FQMTR_MONCLK_WINDOW_GET(x)	(((x) >> 0) & 0xfff)
#define RG_FQMTR_MONCLK_WINDOW_SET(x)	(((x) & 0xfff) << 0)

#define RG_FQMTR_CKDIV_DIV_2		0
#define RG_FQMTR_CKDIV_DIV_4		1
#define RG_FQMTR_CKDIV_DIV_8		2
#define RG_FQMTR_CKDIV_DIV_16		3

#define RG_FQMTR_FIXCLK_26MHZ		0
#define RG_FQMTR_FIXCLK_32KHZ		2

#define RG_FQMTR_EN     1
#define RG_FQMTR_RST    1

#define RG_FRMTR_WINDOW     519

static u32 fmeter_freq(enum FMETER_TYPE type, int k1, int clk)
{
	u32 cnt = 0;

	/* reset & reset deassert */
	clk_writel(FREQ_MTR_CTRL_REG, RG_FQMTR_MONCLK_RST_SET(RG_FQMTR_RST));
	clk_writel(FREQ_MTR_CTRL_REG, RG_FQMTR_MONCLK_RST_SET(!RG_FQMTR_RST));

	/* set window and target */
	clk_writel(FREQ_MTR_CTRL_REG,
		RG_FQMTR_MONCLK_WINDOW_SET(RG_FRMTR_WINDOW) |
		RG_FQMTR_MONCLK_SEL_SET(clk) |
		RG_FQMTR_FIXCLK_SEL_SET(RG_FQMTR_FIXCLK_26MHZ) |
		RG_FQMTR_MONCLK_EN_SET(RG_FQMTR_EN));

	udelay(30);

	cnt = clk_readl(FREQ_MTR_CTRL_RDATA);
	/* reset & reset deassert */
	clk_writel(FREQ_MTR_CTRL_REG, RG_FQMTR_MONCLK_RST_SET(RG_FQMTR_RST));
	clk_writel(FREQ_MTR_CTRL_REG, RG_FQMTR_MONCLK_RST_SET(!RG_FQMTR_RST));

	return ((cnt * 26000) / (RG_FRMTR_WINDOW + 1));
}

static u32 measure_stable_fmeter_freq(enum FMETER_TYPE type, int k1, int clk)
{
	u32 last_freq = 0;
	u32 freq = fmeter_freq(type, k1, clk);
	u32 maxfreq = max(freq, last_freq);

	while (maxfreq > 0 && ABS_DIFF(freq, last_freq) * 100 / maxfreq > 10) {
		last_freq = freq;
		freq = fmeter_freq(type, k1, clk);
		maxfreq = max(freq, last_freq);
	}

	return freq;
}

static const struct fmeter_clk *get_all_fmeter_clks(void)
{
	return fclks;
}

struct bak {
	u32 pll_hp_con0;
};

static void *prepare_fmeter(void)
{
	static struct bak regs;

	regs.pll_hp_con0 = clk_readl(PLL_HP_CON0);

	clk_writel(PLL_HP_CON0, 0x0);		/* disable PLL hopping */
	udelay(10);

	return &regs;
}

static void unprepare_fmeter(void *data)
{
	struct bak *regs = data;

	/* restore old setting */
	clk_writel(PLL_HP_CON0, regs->pll_hp_con0);
}

static u32 fmeter_freq_op(const struct fmeter_clk *fclk)
{
	if (fclk->type)
		return measure_stable_fmeter_freq(fclk->type, 0, fclk->id);

	return 0;
}

/*
 * clkdbg dump_state
 */

static const char * const *get_all_clk_names(void)
{
	static const char * const clks[] = {
		/* plls */
		"armpll",
		"mainpll",
		"univpll",
		"mmpll",
		"apll1",
		"apll2",
		"tvdpll",
		/* topckgen */
		"fq_trng_out0",
		"fq_trng_out1",
		"dmpll_ck",
		"mainpll_d4",
		"mainpll_d8",
		"mainpll_d16",
		"mainpll_d11",
		"mainpll_d22",
		"mainpll_d3",
		"mainpll_d6",
		"mainpll_d12",
		"mainpll_d5",
		"mainpll_d10",
		"mainpll_d20",
		"mainpll_d40",
		"mainpll_d7",
		"mainpll_d14",
		"univpll_d2",
		"univpll_d4",
		"univpll_d8",
		"univpll_d16",
		"univpll_d3",
		"univpll_d6",
		"univpll_d12",
		"univpll_d24",
		"univpll_d5",
		"univpll_d20",
		"univpll_d10",
		"mmpll_d2",
		"usb20_48m_ck",
		"apll1_ck",
		"apll1_d4",
		"apll2_ck",
		"apll2_d2",
		"apll2_d3",
		"apll2_d4",
		"apll2_d8",
		"clk26m_ck",
		"clk26m_d2",
		"clk26m_d8",
		"clk26m_d793",
		"tvdpll_ck",
		"tvdpll_d2",
		"tvdpll_d4",
		"tvdpll_d8",
		"tvdpll_d16",
		"usb20_clk480m",
		"rg_apll1_d2",
		"rg_apll1_d4",
		"rg_apll1_d8",
		"rg_apll1_d16",
		"rg_apll1_d3",
		"rg_apll2_d2",
		"rg_apll2_d4",
		"rg_apll2_d8",
		"rg_apll2_d16",
		"rg_apll2_d3",
		"nfi1x_infra_bck",
		"uart0_sel",
		"emi1x_sel",
		"emi_ddrphy_sel",
		"msdc1_sel",
		"pwm_mm_sel",
		"uart1_sel",
		"spm_52m_sel",
		"pmicspi_sel",
		"nfi2x_sel",
		"ddrphycfg_sel",
		"smi_sel",
		"usb_sel",
		"spinor_sel",
		"eth_sel",
		"aud1_sel",
		"aud2_sel",
		"i2c_sel",
		"aud_i2s0_m_sel",
		"aud_i2s3_m_sel",
		"aud_i2s4_m_sel",
		"aud_i2s6_m_sel",
		"pwm_sel",
		"aud_spdifin_sel",
		"uart2_sel",
		"dbg_atclk_sel",
		"png_sys_sel",
		"sej_13m_sel",
		"imgrz_sys_sel",
		"graph_eclk_sel",
		"fdbi_sel",
		"faudio_sel",
		"fa2sys_sel",
		"fa1sys_sel",
		"fasm_m_sel",
		"fasm_h_sel",
		"fasm_l_sel",
		"fecc_ck_sel",
		"pe2_mac_sel",
		"cmsys_sel",
		"gcpu_sel",
		"spis_ck_sel",
		"apll1_ref_sel",
		"apll2_ref_sel",
		"int_32k_sel",
		"apll1_src_sel",
		"apll2_src_sel",
		"faud_intbus_sel",
		"axibus_sel",
		"hapll1_sel",
		"hapll2_sel",
		"spinfi_sel",
		"msdc0_sel",
		"msdc0_clk50_sel",
		"msdc2_sel",
		"msdc2_clk50_sel",
		"disp_dpi_ck_sel",
		"spi1_sel",
		"spi2_sel",
		"spi3_sel",
		"apll12_ck_div0",
		"apll12_ck_div3",
		"apll12_ck_div4",
		"apll12_ck_div6",
		"pwm_mm",
		"spm_52m",
		"smi",
		"spi2",
		"spi3",
		"spinfi",
		"debug_26m",
		"ad_usb_48m",
		"debug_52m",
		"debug_32k",
		"therm",
		"apdma",
		"i2c0",
		"i2c1",
		"auxadc1",
		"nfi",
		"nfiecc",
		"debugsys",
		"pwm",
		"uart0",
		"uart1",
		"usb",
		"flashif_26m",
		"auxadc2",
		"i2c2",
		"msdc0",
		"msdc1",
		"nfi2x",
		"memslp_dlyer",
		"spi",
		"apxgpt",
		"spm",
		"pwrap_md",
		"pwrap_conn",
		"pmic_sysck",
		"aux_adc",
		"aux_tp",
		"rbist",
		"nfi_bus",
		"gce",
		"trng",
		"pwm_b",
		"pwm1_fb",
		"pwm2_fb",
		"pwm3_fb",
		"pwm4_fb",
		"pwm5_fb",
		"flashif_freerun",
		"cqdma",
		"eth_66m",
		"eth_133m",
		"flashif_axi",
		"usbif",
		"uart2",
		"gcpu_b",
		"msdc0_b",
		"msdc1_b",
		"msdc2_b",
		"apll12_div0",
		"apll12_div3",
		"apll12_div4",
		"apll12_div6",
		"spinor",
		"msdc2",
		"eth",
		"aud1",
		"aud2",
		"i2c",
		"pwm_infra",
		"aud_spdif_in",
		"rg_uart2",
		"dbg_at",
		"imgrz_sys",
		"png_sys",
		"graph_e",
		"fdbi",
		"faudio",
		"faud_intbus",
		"hapll1",
		"hapll2",
		"fa2sys",
		"fa1sys",
		"fasm_l",
		"fasm_m",
		"fasm_h",
		"fecc",
		"pe2_mac",
		"cmsys",
		"gcpu",
		"spis",
		"i2c3_bus",
		"spi_slv_b",
		"spi_slv_bus",
		"pcie_mac_bus",
		"cmsys_bus",
		"ecc_b",
		"pcie_phy_bus",
		"pcie_aux",
		"disp_dpi",
		/* mcucfg */
		"mcu_bus_sel",
		/* mmsys */
		"mm_smi_common",
		"mm_smi_larb1",
		"mm_fake",
		"mm_dbi_axi",
		"mm_dpi",
		"mm_disp_dpi",
		/* end */
		NULL
	};

	return clks;
}

/*
 * clkdbg pwr_status
 */

static const char * const *get_pwr_names(void)
{
	static const char * const pwr_names[] = {
		[0]  = "",
		[1]  = "",
		[2]  = "",
		[3]  = "DISP",
		[4]  = "",
		[5]  = "",
		[6]  = "INFRA",
		[7]  = "",
		[8]  = "",
		[9]  = "",
		[10] = "",
		[11] = "",
		[12] = "",
		[13] = "",
		[14] = "",
		[15] = "",
		[16] = "CM4",
		[17] = "",
		[18] = "",
		[19] = "",
		[20] = "AUDSRC",
		[21] = "AUDAFE",
		[22] = "",
		[23] = "",
		[24] = "",
		[25] = "",
		[26] = "",
		[27] = "",
		[28] = "",
		[29] = "",
		[30] = "",
		[31] = "",
	};

	return pwr_names;
}

/*
 * clkdbg dump_clks
 */

static void setup_provider_clk(struct provider_clk *pvdck)
{
	static const struct {
		const char *pvdname;
		u32 pwr_mask;
	} pvd_pwr_mask[] = {
		{"mmsys", BIT(3)},
	};

	size_t i;
	const char *pvdname = pvdck->provider_name;

	if (pvdname == NULL)
		return;

	for (i = 0; i < ARRAY_SIZE(pvd_pwr_mask); i++) {
		if (strcmp(pvdname, pvd_pwr_mask[i].pvdname) == 0) {
			pvdck->pwr_mask = pvd_pwr_mask[i].pwr_mask;
			return;
		}
	}
}

/*
 * init functions
 */

static struct clkdbg_ops clkdbg_mt8518_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = prepare_fmeter,
	.unprepare_fmeter = unprepare_fmeter,
	.fmeter_freq = fmeter_freq_op,
	.get_all_regnames = get_all_regnames,
	.get_all_clk_names = get_all_clk_names,
	.get_pwr_names = get_pwr_names,
	.setup_provider_clk = setup_provider_clk,
};

static void __init init_custom_cmds(void)
{
	static const struct cmd_fn cmds[] = {
		{}
	};

	set_custom_cmds(cmds);
}

static int __init clkdbg_mt8518_init(void)
{
	if (of_machine_is_compatible("mediatek,mt8518") == 0)
		return -ENODEV;

	init_regbase();

	init_custom_cmds();
	set_clkdbg_ops(&clkdbg_mt8518_ops);

#if DUMP_INIT_STATE
	print_regs();
	print_fmeter_all();
#endif /* DUMP_INIT_STATE */

	return 0;
}
device_initcall(clkdbg_mt8518_init);
