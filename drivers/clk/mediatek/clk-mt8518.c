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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt8518-clk.h>

static DEFINE_SPINLOCK(mt8518_clk_lock);

static const struct mtk_fixed_clk top_fixed_clks[] = {
	FIXED_CLK(CLK_TOP_CLK_NULL, "clk_null", NULL, 0),
	FIXED_CLK(CLK_TOP_FQ_TRNG_OUT0, "fq_trng_out0", "clk_null", 500000000),
	FIXED_CLK(CLK_TOP_FQ_TRNG_OUT1, "fq_trng_out1", "clk_null", 500000000),
};

static const struct mtk_fixed_factor top_early_divs[] = {
	FACTOR(CLK_TOP_CLK26M, "clk26m_ck", "clk26m", 1, 1),
	FACTOR(CLK_TOP_CLK26M_D2, "clk26m_d2", "clk26m", 1, 2),
};

static const struct mtk_fixed_factor top_divs[] = {
	FACTOR(CLK_TOP_DMPLL, "dmpll_ck", "clk26m", 1, 1),
	FACTOR(CLK_TOP_MAINPLL_D4, "mainpll_d4", "mainpll", 1, 4),
	FACTOR(CLK_TOP_MAINPLL_D8, "mainpll_d8", "mainpll", 1, 8),
	FACTOR(CLK_TOP_MAINPLL_D16, "mainpll_d16", "mainpll", 1, 16),
	FACTOR(CLK_TOP_MAINPLL_D11, "mainpll_d11", "mainpll", 1, 11),
	FACTOR(CLK_TOP_MAINPLL_D22, "mainpll_d22", "mainpll", 1, 22),
	FACTOR(CLK_TOP_MAINPLL_D3, "mainpll_d3", "mainpll", 1, 3),
	FACTOR(CLK_TOP_MAINPLL_D6, "mainpll_d6", "mainpll", 1, 6),
	FACTOR(CLK_TOP_MAINPLL_D12, "mainpll_d12", "mainpll", 1, 12),
	FACTOR(CLK_TOP_MAINPLL_D5, "mainpll_d5", "mainpll", 1, 5),
	FACTOR(CLK_TOP_MAINPLL_D10, "mainpll_d10", "mainpll", 1, 10),
	FACTOR(CLK_TOP_MAINPLL_D20, "mainpll_d20", "mainpll", 1, 20),
	FACTOR(CLK_TOP_MAINPLL_D40, "mainpll_d40", "mainpll", 1, 40),
	FACTOR(CLK_TOP_MAINPLL_D7, "mainpll_d7", "mainpll", 1, 7),
	FACTOR(CLK_TOP_MAINPLL_D14, "mainpll_d14", "mainpll", 1, 14),
	FACTOR(CLK_TOP_UNIVPLL_D2, "univpll_d2", "univpll", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL_D4, "univpll_d4", "univpll", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL_D8, "univpll_d8", "univpll", 1, 8),
	FACTOR(CLK_TOP_UNIVPLL_D16, "univpll_d16", "univpll", 1, 16),
	FACTOR(CLK_TOP_UNIVPLL_D3, "univpll_d3", "univpll", 1, 3),
	FACTOR(CLK_TOP_UNIVPLL_D6, "univpll_d6", "univpll", 1, 6),
	FACTOR(CLK_TOP_UNIVPLL_D12, "univpll_d12", "univpll", 1, 12),
	FACTOR(CLK_TOP_UNIVPLL_D24, "univpll_d24", "univpll", 1, 24),
	FACTOR(CLK_TOP_UNIVPLL_D5, "univpll_d5", "univpll", 1, 5),
	FACTOR(CLK_TOP_UNIVPLL_D20, "univpll_d20", "univpll", 1, 20),
	FACTOR(CLK_TOP_UNIVPLL_D10, "univpll_d10", "univpll", 1, 10),
	FACTOR(CLK_TOP_MMPLL_D2, "mmpll_d2", "mmpll", 1, 2),
	FACTOR(CLK_TOP_USB20_48M, "usb20_48m_ck", "univpll", 1, 26),
	FACTOR(CLK_TOP_APLL1, "apll1_ck", "apll1", 1, 1),
	FACTOR(CLK_TOP_APLL1_D4, "apll1_d4", "apll1_ck", 1, 4),
	FACTOR(CLK_TOP_APLL2, "apll2_ck", "apll2", 1, 1),
	FACTOR(CLK_TOP_APLL2_D2, "apll2_d2", "apll2_ck", 1, 2),
	FACTOR(CLK_TOP_APLL2_D3, "apll2_d3", "apll2_ck", 1, 3),
	FACTOR(CLK_TOP_APLL2_D4, "apll2_d4", "apll2_ck", 1, 4),
	FACTOR(CLK_TOP_APLL2_D8, "apll2_d8", "apll2_ck", 1, 8),
	FACTOR(CLK_TOP_CLK26M_D4, "clk26m_d4", "clk26m", 1, 4),
	FACTOR(CLK_TOP_CLK26M_D8, "clk26m_d8", "clk26m", 1, 8),
	FACTOR(CLK_TOP_CLK26M_D793, "clk26m_d793", "clk26m", 1, 793),
	FACTOR(CLK_TOP_TVDPLL, "tvdpll_ck", "tvdpll", 1, 1),
	FACTOR(CLK_TOP_TVDPLL_D2, "tvdpll_d2", "tvdpll_ck", 1, 2),
	FACTOR(CLK_TOP_TVDPLL_D4, "tvdpll_d4", "tvdpll_ck", 1, 4),
	FACTOR(CLK_TOP_TVDPLL_D8, "tvdpll_d8", "tvdpll_ck", 1, 8),
	FACTOR(CLK_TOP_TVDPLL_D16, "tvdpll_d16", "tvdpll_ck", 1, 16),
	FACTOR(CLK_TOP_USB20_CLK480M, "usb20_clk480m", "clk_null", 1, 1),
	FACTOR(CLK_TOP_RG_APLL1_D2, "rg_apll1_d2", "apll1_src_sel", 1, 2),
	FACTOR(CLK_TOP_RG_APLL1_D4, "rg_apll1_d4", "apll1_src_sel", 1, 4),
	FACTOR(CLK_TOP_RG_APLL1_D8, "rg_apll1_d8", "apll1_src_sel", 1, 8),
	FACTOR(CLK_TOP_RG_APLL1_D16, "rg_apll1_d16", "apll1_src_sel", 1, 16),
	FACTOR(CLK_TOP_RG_APLL1_D3, "rg_apll1_d3", "apll1_src_sel", 1, 3),
	FACTOR(CLK_TOP_RG_APLL2_D2, "rg_apll2_d2", "apll2_src_sel", 1, 2),
	FACTOR(CLK_TOP_RG_APLL2_D4, "rg_apll2_d4", "apll2_src_sel", 1, 4),
	FACTOR(CLK_TOP_RG_APLL2_D8, "rg_apll2_d8", "apll2_src_sel", 1, 8),
	FACTOR(CLK_TOP_RG_APLL2_D16, "rg_apll2_d16", "apll2_src_sel", 1, 16),
	FACTOR(CLK_TOP_RG_APLL2_D3, "rg_apll2_d3", "apll2_src_sel", 1, 3),
	FACTOR(CLK_TOP_NFI1X_INFRA_BCLK, "nfi1x_infra_bck", "nfi2x_sel", 1, 2),
	FACTOR(CLK_TOP_AHB_INFRA_D2, "ahb_infra_d2", "axibus_sel", 1, 2),
};

static const char * const uart0_parents[] = {
	"clk26m_ck",
	"univpll_d24"
};

static const char * const emi1x_parents[] = {
	"clk26m_ck",
	"dmpll_ck"
};

static const char * const emi_ddrphy_parents[] = {
	"emi1x_sel",
	"emi1x_sel"
};

static const char * const msdc1_parents[] = {
	"clk26m_ck",
	"univpll_d6",
	"mainpll_d8",
	"univpll_d8",
	"mainpll_d16",
	"mmpll_d2",
	"mainpll_d12"
};

static const char * const pwm_mm_parents[] = {
	"clk26m_ck",
	"univpll_d12"
};

static const char * const pmicspi_parents[] = {
	"univpll_d20",
	"usb20_48m_ck",
	"univpll_d16",
	"clk26m_ck",
	"clk26m_d2"
};

static const char * const nfi2x_parents[] = {
	"clk26m_ck",
	"mainpll_d4",
	"mainpll_d5",
	"mainpll_d6",
	"mainpll_d7",
	"mainpll_d8",
	"mainpll_d10",
	"mainpll_d12"
};

static const char * const ddrphycfg_parents[] = {
	"clk26m_ck",
	"mainpll_d16"
};

static const char * const smi_parents[] = {
	"clk_null",
	"clk26m_ck",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"univpll_d4",
	"mainpll_d7",
	"clk_null",
	"mainpll_d14"
};

static const char * const usb_parents[] = {
	"clk_null",
	"clk26m_ck",
	"univpll_d16",
	"clk_null",
	"mainpll_d20"
};

static const char * const spinor_parents[] = {
	"clk26m_d2",
	"clk26m_ck",
	"mainpll_d40",
	"univpll_d24",
	"univpll_d20",
	"mainpll_d20",
	"mainpll_d16",
	"univpll_d12"
};

static const char * const eth_parents[] = {
	"clk26m_ck",
	"mainpll_d40",
	"univpll_d24",
	"univpll_d20",
	"mainpll_d20"
};

static const char * const aud1_parents[] = {
	"clk26m_ck",
	"apll1_src_sel"
};

static const char * const aud2_parents[] = {
	"clk26m_ck",
	"apll2_src_sel"
};

static const char * const i2c_parents[] = {
	"clk26m_ck",
	"usb20_48m_ck",
	"univpll_d12",
	"univpll_d10",
	"univpll_d8"
};

static const char * const aud_i2s0_m_parents[] = {
	"aud1",
	"aud2"
};

static const char * const aud_spdifin_parents[] = {
	"clk26m_ck",
	"univpll_d2",
	"tvdpll_ck"
};

static const char * const dbg_atclk_parents[] = {
	"clk_null",
	"clk26m_ck",
	"mainpll_d5",
	"clk_null",
	"univpll_d5"
};

static const char * const png_sys_parents[] = {
	"clk26m_ck",
	"univpll_d8",
	"mainpll_d7",
	"mainpll_d6",
	"mainpll_d5",
	"univpll_d3"
};

static const char * const sej_13m_parents[] = {
	"clk26m_ck",
	"clk26m_d2"
};

static const char * const imgrz_sys_parents[] = {
	"clk26m_ck",
	"mainpll_d6",
	"mainpll_d7",
	"mainpll_d5",
	"univpll_d4",
	"univpll_d10",
	"univpll_d5",
	"univpll_d6"
};

static const char * const graph_eclk_parents[] = {
	"clk26m_ck",
	"mainpll_d6",
	"univpll_d8",
	"univpll_d16",
	"mainpll_d7",
	"univpll_d4",
	"univpll_d10",
	"univpll_d24",
	"mainpll_d8"
};

static const char * const fdbi_parents[] = {
	"clk26m_ck",
	"mainpll_d12",
	"mainpll_d14",
	"mainpll_d16",
	"univpll_d10",
	"univpll_d12",
	"univpll_d16",
	"univpll_d24",
	"tvdpll_d2",
	"tvdpll_d4",
	"tvdpll_d8",
	"tvdpll_d16"
};

static const char * const faudio_parents[] = {
	"clk26m_ck",
	"univpll_d24",
	"apll1_d4",
	"apll2_d4"
};

static const char * const fa2sys_parents[] = {
	"clk26m_ck",
	"apll1_src_sel",
	"rg_apll1_d2",
	"rg_apll1_d4",
	"rg_apll1_d8",
	"rg_apll1_d16",
	"clk26m_d2",
	"rg_apll1_d3"
};

static const char * const fa1sys_parents[] = {
	"clk26m_ck",
	"apll2_src_sel",
	"rg_apll2_d2",
	"rg_apll2_d4",
	"rg_apll2_d8",
	"rg_apll2_d16",
	"clk26m_d2",
	"rg_apll2_d3"
};

static const char * const fasm_m_parents[] = {
	"clk26m_ck",
	"univpll_d12",
	"univpll_d6",
	"mainpll_d7"
};

static const char * const fecc_ck_parents[] = {
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk26m_ck",
	"univpll_d6",
	"clk_null",
	"univpll_d4",
	"clk_null",
	"clk_null",
	"clk_null",
	"univpll_d3",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"mainpll_d3"
};

static const char * const pe2_mac_parents[] = {
	"clk26m_ck",
	"mainpll_d11",
	"mainpll_d16",
	"univpll_d12",
	"univpll_d10"
};

static const char * const cmsys_parents[] = {
	"clk26m_ck",
	"univpll_d5",
	"univpll_d6",
	"mainpll_d5",
	"apll2_ck",
	"apll2_d2",
	"apll2_d4",
	"apll2_d3"
};

static const char * const gcpu_parents[] = {
	"clk26m_ck",
	"mainpll_d4",
	"mainpll_d5",
	"mainpll_d6",
	"mainpll_d7",
	"univpll_d4",
	"univpll_d10",
	"univpll_d3"
};

static const char * const spis_ck_parents[] = {
	"clk26m_ck",
	"univpll_d12",
	"univpll_d8",
	"univpll_d6",
	"univpll_d5",
	"univpll_d4",
	"mainpll_d4",
	"univpll_d3"
};

static const char * const apll1_ref_parents[] = {
	"aud_extck_i0",
	"aud_extck_i1",
	"i2sin_mclk_i",
	"i2so_mclk_i",
	"tdmin_mclk_i",
	"tdmo_mclk_i"
};

static const char * const int_32k_parents[] = {
	"clk32k",
	"clk26m_d793"
};

static const char * const apll1_src_parents[] = {
	"apll1_ck",
	"aud_extck_i1",
	"i2sin_mclk_i",
	"i2so_mclk_i"
};

static const char * const apll2_src_parents[] = {
	"apll2_ck",
	"aud_extck_i0",
	"i2sin_mclk_i",
	"i2so_mclk_i"
};

static const char * const faud_intbus_parents[] = {
	"clk26m_ck",
	"mainpll_d11",
	"clk26m_ck",
	"univpll_d10",
	"rg_apll2_d8",
	"clk26m_d2",
	"rg_apll1_d8",
	"univpll_d20"
};

static const char * const axibus_parents[] = {
	"clk26m_ck",
	"mainpll_d11",
	"mainpll_d12",
	"univpll_d10",
	"clk26m_d2",
	"apll2_d8"
};

static const char * const hapll1_parents[] = {
	"clk26m_ck",
	"apll1_src_sel",
	"rg_apll1_d2",
	"rg_apll1_d4",
	"rg_apll1_d8",
	"rg_apll1_d16",
	"clk26m_d2",
	"clk26m_d8",
	"rg_apll1_d3"
};

static const char * const hapll2_parents[] = {
	"clk26m_ck",
	"apll2_src_sel",
	"rg_apll2_d2",
	"rg_apll2_d4",
	"rg_apll2_d8",
	"rg_apll2_d16",
	"clk26m_d2",
	"clk26m_d4",
	"rg_apll2_d3"
};

static const char * const spinfi_parents[] = {
	"clk26m_ck",
	"univpll_d24",
	"univpll_d20",
	"mainpll_d22",
	"univpll_d16",
	"mainpll_d16",
	"univpll_d12",
	"univpll_d10",
	"mainpll_d11"
};

static const char * const msdc0_parents[] = {
	"clk26m_ck",
	"univpll_d6",
	"mainpll_d8",
	"univpll_d8",
	"mainpll_d16",
	"mainpll_d12",
	"mmpll",
	"mmpll_d2"
};

static const char * const msdc0_clk50_parents[] = {
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk26m_ck",
	"univpll_d6",
	"clk_null",
	"mainpll_d8",
	"clk_null",
	"clk_null",
	"clk_null",
	"univpll_d8",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"mainpll_d6"
};

static const char * const msdc2_parents[] = {
	"clk26m_ck",
	"univpll_d6",
	"mainpll_d8",
	"univpll_d8",
	"mainpll_d16",
	"mmpll_d2",
	"mainpll_d12",
	"mmpll"
};

static const char * const disp_dpi_ck_parents[] = {
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk26m_ck",
	"tvdpll_d2",
	"clk_null",
	"tvdpll_d4",
	"clk_null",
	"clk_null",
	"clk_null",
	"tvdpll_d8",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"tvdpll_d16"
};

static struct mtk_composite top_muxes[] = {
	/* CLK_MUX_SEL0 */
	MUX(CLK_TOP_UART0_SEL, "uart0_sel", uart0_parents,
	    0x000, 0, 1),
	MUX_FLAGS(CLK_TOP_EMI1X_SEL, "emi1x_sel", emi1x_parents,
	    0x000, 1, 1, CLK_IS_CRITICAL, 0),
	MUX(CLK_TOP_EMI_DDRPHY_SEL, "emi_ddrphy_sel", emi_ddrphy_parents,
	    0x000, 2, 1),
	MUX_FLAGS(CLK_TOP_MSDC1_SEL, "msdc1_sel", msdc1_parents,
	    0x000, 4, 8, CLK_SET_RATE_PARENT, CLK_MUX_INDEX_BIT),
	MUX(CLK_TOP_PWM_MM_SEL, "pwm_mm_sel", pwm_mm_parents,
	    0x000, 18, 1),
	MUX(CLK_TOP_UART1_SEL, "uart1_sel", uart0_parents,
	    0x000, 19, 1),
	MUX(CLK_TOP_SPM_52M_SEL, "spm_52m_sel", uart0_parents,
	    0x000, 22, 1),
	MUX(CLK_TOP_PMICSPI_SEL, "pmicspi_sel", pmicspi_parents,
	    0x000, 23, 3),
	/* CLK_MUX_SEL1 */
	MUX(CLK_TOP_NFI2X_SEL, "nfi2x_sel", nfi2x_parents,
	    0x004, 0, 3),
	MUX(CLK_TOP_DDRPHYCFG_SEL, "ddrphycfg_sel", ddrphycfg_parents,
	    0x004, 15, 1),
	MUX(CLK_TOP_SMI_SEL, "smi_sel", smi_parents,
	    0x004, 16, 4),
	MUX(CLK_TOP_USB_SEL, "usb_sel", usb_parents,
	    0x004, 20, 3),
	/* CLK_MUX_SEL8 */
	MUX(CLK_TOP_SPINOR_SEL, "spinor_sel", spinor_parents,
	    0x040, 0, 3),
	MUX(CLK_TOP_ETH_SEL, "eth_sel", eth_parents,
	    0x040, 6, 3),
	MUX(CLK_TOP_AUD1_SEL, "aud1_sel", aud1_parents,
	    0x040, 22, 1),
	MUX(CLK_TOP_AUD2_SEL, "aud2_sel", aud2_parents,
	    0x040, 23, 1),
	MUX(CLK_TOP_I2C_SEL, "i2c_sel", i2c_parents,
	    0x040, 28, 3),
	/* CLK_SEL_9 */
	MUX(CLK_TOP_AUD_I2S0_M_SEL, "aud_i2s0_m_sel", aud_i2s0_m_parents,
	    0x044, 12, 1),
	MUX(CLK_TOP_AUD_I2S3_M_SEL, "aud_i2s3_m_sel", aud_i2s0_m_parents,
	    0x044, 15, 1),
	MUX(CLK_TOP_AUD_I2S4_M_SEL, "aud_i2s4_m_sel", aud_i2s0_m_parents,
	    0x044, 16, 1),
	MUX(CLK_TOP_AUD_I2S6_M_SEL, "aud_i2s6_m_sel", aud_i2s0_m_parents,
	    0x044, 18, 1),
	/* CLK_MUX_SEL13 */
	MUX(CLK_TOP_PWM_SEL, "pwm_sel", pwm_mm_parents,
	    0x07c, 0, 1),
	MUX(CLK_TOP_AUD_SPDIFIN_SEL, "aud_spdifin_sel", aud_spdifin_parents,
	    0x07c, 2, 2),
	MUX(CLK_TOP_UART2_SEL, "uart2_sel", uart0_parents,
	    0x07c, 4, 1),
	MUX(CLK_TOP_DBG_ATCLK_SEL, "dbg_atclk_sel", dbg_atclk_parents,
	    0x07c, 7, 3),
	MUX(CLK_TOP_PNG_SYS_SEL, "png_sys_sel", png_sys_parents,
	    0x07c, 16, 3),
	MUX(CLK_TOP_SEJ_13M_SEL, "sej_13m_sel", sej_13m_parents,
	    0x07c, 22, 1),
	/* CLK_MUX_SEL14 */
	MUX(CLK_TOP_IMGRZ_SYS_SEL, "imgrz_sys_sel", imgrz_sys_parents,
	    0xc0, 0, 3),
	MUX(CLK_TOP_GRAPH_ECLK_SEL, "graph_eclk_sel", graph_eclk_parents,
	    0xc0, 8, 4),
	MUX(CLK_TOP_FDBI_SEL, "fdbi_sel", fdbi_parents,
	    0xc0, 12, 4),
	MUX(CLK_TOP_FAUDIO_SEL, "faudio_sel", faudio_parents,
	    0xc0, 16, 2),
	MUX(CLK_TOP_FA2SYS_SEL, "fa2sys_sel", fa2sys_parents,
	    0xc0, 24, 3),
	MUX(CLK_TOP_FA1SYS_SEL, "fa1sys_sel", fa1sys_parents,
	    0xc0, 27, 3),
	MUX(CLK_TOP_FASM_M_SEL, "fasm_m_sel", fasm_m_parents,
	    0xc0, 30, 2),
	/* CLK_MUX_SEL15 */
	MUX(CLK_TOP_FASM_H_SEL, "fasm_h_sel", fasm_m_parents,
	    0XC4, 0, 2),
	MUX(CLK_TOP_FASM_L_SEL, "fasm_l_sel", fasm_m_parents,
	    0XC4, 2, 2),
	MUX(CLK_TOP_FECC_CK_SEL, "fecc_ck_sel", fecc_ck_parents,
	    0XC4, 18, 6),
	MUX(CLK_TOP_PE2_MAC_SEL, "pe2_mac_sel", pe2_mac_parents,
	    0XC4, 24, 3),
	MUX(CLK_TOP_CMSYS_SEL, "cmsys_sel", cmsys_parents,
	    0XC4, 28, 3),
	/* CLK_MUX_SEL16 */
	MUX(CLK_TOP_GCPU_SEL, "gcpu_sel", gcpu_parents,
	    0XC8, 0, 3),
	MUX_FLAGS(CLK_TOP_SPIS_CK_SEL, "spis_ck_sel", spis_ck_parents,
	    0XC8, 4, 8, 0, CLK_MUX_INDEX_BIT),
	/* CLK_MUX_SEL17 */
	MUX(CLK_TOP_APLL1_REF_SEL, "apll1_ref_sel", apll1_ref_parents,
	    0XCC, 6, 3),
	MUX(CLK_TOP_APLL2_REF_SEL, "apll2_ref_sel", apll1_ref_parents,
	    0XCC, 9, 3),
	MUX_FLAGS(CLK_TOP_INT_32K_SEL, "int_32k_sel", int_32k_parents,
	    0XCC, 12, 1, CLK_IS_CRITICAL, 0),
	MUX(CLK_TOP_APLL1_SRC_SEL, "apll1_src_sel", apll1_src_parents,
	    0XCC, 13, 2),
	MUX(CLK_TOP_APLL2_SRC_SEL, "apll2_src_sel", apll2_src_parents,
	    0XCC, 15, 2),
	/* CLK_MUX_SEL19 */
	MUX_FLAGS(CLK_TOP_FAUD_INTBUS_SEL, "faud_intbus_sel",
	    faud_intbus_parents, 0XD4, 8, 8, 0, CLK_MUX_INDEX_BIT),
	MUX_FLAGS(CLK_TOP_AXIBUS_SEL, "axibus_sel", axibus_parents,
	    0XD4, 24, 8, CLK_IS_CRITICAL, CLK_MUX_INDEX_BIT),
	/* CLK_MUX_SEL21 */
	MUX(CLK_TOP_HAPLL1_SEL, "hapll1_sel", hapll1_parents,
	    0XDC, 0, 4),
	MUX(CLK_TOP_HAPLL2_SEL, "hapll2_sel", hapll2_parents,
	    0XDC, 4, 4),
	MUX(CLK_TOP_SPINFI_SEL, "spinfi_sel", spinfi_parents,
	    0XDC, 8, 4),
	/* CLK_MUX_SEL22 */
	MUX_FLAGS(CLK_TOP_MSDC0_SEL, "msdc0_sel", msdc0_parents,
	    0XF4, 0, 8, CLK_SET_RATE_PARENT, CLK_MUX_INDEX_BIT),
	MUX(CLK_TOP_MSDC0_CLK50_SEL, "msdc0_clk50_sel", msdc0_clk50_parents,
	    0XF4, 8, 6),
	MUX_FLAGS(CLK_TOP_MSDC2_SEL, "msdc2_sel", msdc2_parents,
	    0XF4, 15, 8, CLK_SET_RATE_PARENT, CLK_MUX_INDEX_BIT),
	MUX(CLK_TOP_MSDC2_CLK50_SEL, "msdc2_clk50_sel", msdc0_clk50_parents,
	    0XF4, 23, 6),
	/* CLK_MUX_SEL23 */
	MUX(CLK_TOP_DISP_DPI_CK_SEL, "disp_dpi_ck_sel", disp_dpi_ck_parents,
	    0XF8, 0, 6),
	MUX_FLAGS(CLK_TOP_SPI1_SEL, "spi1_sel", spis_ck_parents,
	    0XF8, 6, 8, 0, CLK_MUX_INDEX_BIT),
	MUX_FLAGS(CLK_TOP_SPI2_SEL, "spi2_sel", spis_ck_parents,
	    0XF8, 14, 8, 0, CLK_MUX_INDEX_BIT),
	MUX_FLAGS(CLK_TOP_SPI3_SEL, "spi3_sel", spis_ck_parents,
	    0XF8, 22, 8, 0, CLK_MUX_INDEX_BIT),
};

static const char * const mcu_bus_parents[] = {
	"clk26m_ck",
	"armpll",
	"univpll_d2",
	"mainpll_d3"
};

static struct mtk_composite mcu_muxes[] = {
	/* bus_pll_divider_cfg */
	MUX_GATE_FLAGS(CLK_MCU_BUS_SEL, "mcu_bus_sel", mcu_bus_parents,
	    0x7C0, 9, 2, -1, CLK_IS_CRITICAL),
};

static const struct mtk_clk_divider top_adj_divs[] = {
	DIV_ADJ(CLK_TOP_APLL12_CK_DIV0, "apll12_ck_div0",
	    "aud_i2s0_m_sel", 0x048, 0, 8),
	DIV_ADJ(CLK_TOP_APLL12_CK_DIV3, "apll12_ck_div3",
	    "aud_i2s3_m_sel", 0x048, 24, 8),
	DIV_ADJ(CLK_TOP_APLL12_CK_DIV4, "apll12_ck_div4",
	    "aud_i2s4_m_sel", 0x04c, 0, 8),
	DIV_ADJ(CLK_TOP_APLL12_CK_DIV6, "apll12_ck_div6",
	    "aud_i2s6_m_sel", 0x078, 0, 8),
};

static const struct mtk_gate_regs top0_cg_regs = {
	.set_ofs = 0x50,
	.clr_ofs = 0x80,
	.sta_ofs = 0x20,
};

static const struct mtk_gate_regs top1_cg_regs = {
	.set_ofs = 0x54,
	.clr_ofs = 0x84,
	.sta_ofs = 0x24,
};

static const struct mtk_gate_regs top2_cg_regs = {
	.set_ofs = 0x6c,
	.clr_ofs = 0x9c,
	.sta_ofs = 0x3c,
};

static const struct mtk_gate_regs top3_cg_regs = {
	.set_ofs = 0x44,
	.clr_ofs = 0x44,
	.sta_ofs = 0x44,
};

static const struct mtk_gate_regs top4_cg_regs = {
	.set_ofs = 0xa0,
	.clr_ofs = 0xb0,
	.sta_ofs = 0x70,
};

static const struct mtk_gate_regs top5_cg_regs = {
	.set_ofs = 0x120,
	.clr_ofs = 0x140,
	.sta_ofs = 0xe0,
};

static const struct mtk_gate_regs top6_cg_regs = {
	.set_ofs = 0x128,
	.clr_ofs = 0x148,
	.sta_ofs = 0xe8,
};

static const struct mtk_gate_regs top7_cg_regs = {
	.set_ofs = 0x12c,
	.clr_ofs = 0x14c,
	.sta_ofs = 0xec,
};

#define GATE_TOP0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_TOP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_TOP2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_TOP2_I(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_TOP3(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top3_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_TOP4(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top4_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_TOP5(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top5_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_TOP5_I(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top5_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_TOP6(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top6_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_TOP7(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top7_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

static const struct mtk_gate top_early_clks[] = {
	GATE_TOP1(CLK_TOP_APXGPT, "apxgpt", "clk26m_ck", 24),
};

static const struct mtk_gate top_clks[] = {
	/* TOP0 */
	GATE_TOP0(CLK_TOP_PWM_MM, "pwm_mm", "pwm_mm_sel", 0),
	GATE_TOP0(CLK_TOP_SMI, "smi", "smi_sel", 9),
	GATE_TOP0(CLK_TOP_SPI2, "spi2", "spi2_sel", 10),
	GATE_TOP0(CLK_TOP_SPI3, "spi3", "spi3_sel", 11),
	GATE_TOP0(CLK_TOP_SPINFI, "spinfi", "spinfi_sel", 12),
	GATE_TOP0(CLK_TOP_26M_DEBUG, "debug_26m", "clk26m_ck", 16),
	GATE_TOP0(CLK_TOP_USB_48M_DEBUG, "ad_usb_48m", "usb20_48m_ck", 17),
	GATE_TOP0(CLK_TOP_52M_DEBUG, "debug_52m", "univpll_d24", 18),
	GATE_TOP0(CLK_TOP_32K_DEBUG, "debug_32k", "int_32k_sel", 19),
	/* TOP1 */
	GATE_TOP1(CLK_TOP_THERM, "therm", "axibus_sel", 1),
	GATE_TOP1(CLK_TOP_APDMA, "apdma", "axibus_sel", 2),
	GATE_TOP1(CLK_TOP_I2C0, "i2c0", "ahb_infra_d2", 3),
	GATE_TOP1(CLK_TOP_I2C1, "i2c1", "ahb_infra_d2", 4),
	GATE_TOP1(CLK_TOP_AUXADC1, "auxadc1", "clk26m_ck", 5),
	GATE_TOP1(CLK_TOP_NFI, "nfi", "nfi1x_infra_bck", 6),
	GATE_TOP1(CLK_TOP_NFIECC, "nfiecc", "axibus_sel", 7),
	GATE_TOP1(CLK_TOP_DEBUGSYS, "debugsys", "dbg_atclk_sel", 8),
	GATE_TOP1(CLK_TOP_PWM, "pwm", "axibus_sel", 9),
	GATE_TOP1(CLK_TOP_UART0, "uart0", "uart0_sel", 10),
	GATE_TOP1(CLK_TOP_UART1, "uart1", "uart1_sel", 11),
	GATE_TOP1(CLK_TOP_USB, "usb", "usb_b", 13),
	GATE_TOP1(CLK_TOP_FLASHIF_26M, "flashif_26m", "clk26m_ck", 14),
	GATE_TOP1(CLK_TOP_AUXADC2, "auxadc2", "clk26m_ck", 15),
	GATE_TOP1(CLK_TOP_I2C2, "i2c2", "ahb_infra_d2", 16),
	GATE_TOP1(CLK_TOP_MSDC0, "msdc0", "msdc0_sel", 17),
	GATE_TOP1(CLK_TOP_MSDC1, "msdc1", "msdc1_sel", 18),
	GATE_TOP1(CLK_TOP_NFI2X, "nfi2x", "nfi2x_sel", 19),
	GATE_TOP1(CLK_TOP_MEMSLP_DLYER, "memslp_dlyer", "clk26m_ck", 22),
	GATE_TOP1(CLK_TOP_SPI, "spi", "spi1_sel", 23),
	GATE_TOP1(CLK_TOP_PMICWRAP_MD, "pwrap_md", "clk26m_ck", 27),
	GATE_TOP1(CLK_TOP_PMICWRAP_CONN, "pwrap_conn", "pmicspi_sel", 28),
	GATE_TOP1(CLK_TOP_PMIC_SYSCK, "pmic_sysck", "clk26m_ck", 29),
	GATE_TOP1(CLK_TOP_AUX_ADC, "aux_adc", "clk26m_ck", 30),
	GATE_TOP1(CLK_TOP_AUX_TP, "aux_tp", "clk26m_ck", 31),
	/* TOP2 */
	GATE_TOP2(CLK_TOP_RBIST, "rbist", "univpll_d12", 1),
	GATE_TOP2(CLK_TOP_NFI_BUS, "nfi_bus", "axibus_sel", 2),
	GATE_TOP2(CLK_TOP_GCE, "gce", "axibus_sel", 4),
	GATE_TOP2(CLK_TOP_TRNG, "trng", "axibus_sel", 5),
	GATE_TOP2(CLK_TOP_PWM_B, "pwm_b", "pwm_sel", 8),
	GATE_TOP2(CLK_TOP_PWM1_FB, "pwm1_fb", "pwm_sel", 9),
	GATE_TOP2(CLK_TOP_PWM2_FB, "pwm2_fb", "pwm_sel", 10),
	GATE_TOP2(CLK_TOP_PWM3_FB, "pwm3_fb", "pwm_sel", 11),
	GATE_TOP2(CLK_TOP_PWM4_FB, "pwm4_fb", "pwm_sel", 12),
	GATE_TOP2(CLK_TOP_PWM5_FB, "pwm5_fb", "pwm_sel", 13),
	GATE_TOP2(CLK_TOP_FLASHIF_FREERUN, "flashif_freerun", "axibus_sel", 15),
	GATE_TOP2(CLK_TOP_CQDMA, "cqdma", "axibus_sel", 17),
	GATE_TOP2(CLK_TOP_66M_ETH, "eth_66m", "axibus_sel", 19),
	GATE_TOP2(CLK_TOP_133M_ETH, "eth_133m", "axibus_sel", 20),
	GATE_TOP2(CLK_TOP_FLASHIF_AXI, "flashif_axi", "spi1_sel", 23),
	GATE_TOP2(CLK_TOP_USBIF, "usbif", "axibus_sel", 24),
	GATE_TOP2(CLK_TOP_UART2, "uart2", "rg_uart2", 25),
	GATE_TOP2(CLK_TOP_GCPU_B, "gcpu_b", "axibus_sel", 27),
	GATE_TOP2_I(CLK_TOP_MSDC0_B, "msdc0_b", "msdc0", 28),
	GATE_TOP2_I(CLK_TOP_MSDC1_B, "msdc1_b", "msdc1", 29),
	GATE_TOP2_I(CLK_TOP_MSDC2_B, "msdc2_b", "msdc2", 30),
	GATE_TOP2(CLK_TOP_USB_B, "usb_b", "usb_sel", 31),
	/* TOP3 */
	GATE_TOP3(CLK_TOP_APLL12_DIV0, "apll12_div0", "apll12_ck_div0", 0),
	GATE_TOP3(CLK_TOP_APLL12_DIV3, "apll12_div3", "apll12_ck_div3", 3),
	GATE_TOP3(CLK_TOP_APLL12_DIV4, "apll12_div4", "apll12_ck_div4", 4),
	GATE_TOP3(CLK_TOP_APLL12_DIV6, "apll12_div6", "apll12_ck_div6", 8),
	/* TOP4 */
	GATE_TOP4(CLK_TOP_SPINOR, "spinor", "spinor_sel", 0),
	GATE_TOP4(CLK_TOP_MSDC2, "msdc2", "msdc2_sel", 1),
	GATE_TOP4(CLK_TOP_ETH, "eth", "eth_sel", 2),
	GATE_TOP4(CLK_TOP_AUD1, "aud1", "aud1_sel", 8),
	GATE_TOP4(CLK_TOP_AUD2, "aud2", "aud2_sel", 9),
	GATE_TOP4(CLK_TOP_I2C, "i2c", "i2c_sel", 12),
	GATE_TOP4(CLK_TOP_PWM_INFRA, "pwm_infra", "pwm_sel", 13),
	GATE_TOP4(CLK_TOP_AUD_SPDIF_IN, "aud_spdif_in", "aud_spdifin_sel", 14),
	GATE_TOP4(CLK_TOP_RG_UART2, "rg_uart2", "uart2_sel", 15),
	GATE_TOP4(CLK_TOP_DBG_AT, "dbg_at", "dbg_atclk_sel", 17),
	/* TOP5 */
	GATE_TOP5_I(CLK_TOP_IMGRZ_SYS, "imgrz_sys", "imgrz_sys_sel", 0),
	GATE_TOP5_I(CLK_TOP_PNG_SYS, "png_sys", "png_sys_sel", 1),
	GATE_TOP5_I(CLK_TOP_GRAPH_E, "graph_e", "graph_eclk_sel", 2),
	GATE_TOP5_I(CLK_TOP_FDBI, "fdbi", "fdbi_sel", 3),
	GATE_TOP5_I(CLK_TOP_FAUDIO, "faudio", "faudio_sel", 4),
	GATE_TOP5_I(CLK_TOP_FAUD_INTBUS, "faud_intbus", "faud_intbus_sel", 5),
	GATE_TOP5_I(CLK_TOP_HAPLL1, "hapll1", "hapll1_sel", 6),
	GATE_TOP5_I(CLK_TOP_HAPLL2, "hapll2", "hapll2_sel", 7),
	GATE_TOP5_I(CLK_TOP_FA2SYS, "fa2sys", "fa2sys_sel", 8),
	GATE_TOP5_I(CLK_TOP_FA1SYS, "fa1sys", "fa1sys_sel", 9),
	GATE_TOP5_I(CLK_TOP_FASM_L, "fasm_l", "fasm_l_sel", 10),
	GATE_TOP5_I(CLK_TOP_FASM_M, "fasm_m", "fasm_m_sel", 11),
	GATE_TOP5_I(CLK_TOP_FASM_H, "fasm_h", "fasm_h_sel", 12),
	GATE_TOP5_I(CLK_TOP_FECC, "fecc", "fecc_ck_sel", 23),
	GATE_TOP5_I(CLK_TOP_PE2_MAC, "pe2_mac", "pe2_mac_sel", 24),
	GATE_TOP5_I(CLK_TOP_CMSYS, "cmsys", "cmsys_sel", 25),
	GATE_TOP5_I(CLK_TOP_GCPU, "gcpu", "gcpu_sel", 26),
	GATE_TOP5(CLK_TOP_SPIS, "spis", "spis_ck_sel", 27),
	/* TOP6 */
	GATE_TOP6(CLK_TOP_I2C3, "i2c3", "ahb_infra_d2", 0),
	GATE_TOP6(CLK_TOP_SPI_SLV_B, "spi_slv_b", "spis_ck_sel", 1),
	GATE_TOP6(CLK_TOP_SPI_SLV_BUS, "spi_slv_bus", "axibus_sel", 2),
	GATE_TOP6(CLK_TOP_PCIE_MAC_BUS, "pcie_mac_bus", "axibus_sel", 3),
	GATE_TOP6(CLK_TOP_CMSYS_BUS, "cmsys_bus", "axibus_sel", 4),
	GATE_TOP6(CLK_TOP_ECC_B, "ecc_b", "axibus_sel", 5),
	GATE_TOP6(CLK_TOP_PCIE_PHY_BUS, "pcie_phy_bus", "clk26m_ck", 6),
	GATE_TOP6(CLK_TOP_PCIE_AUX, "pcie_aux", "clk26m_ck", 7),
	/* TOP7 */
	GATE_TOP7(CLK_TOP_DISP_DPI, "disp_dpi", "disp_dpi_ck_sel", 0),
};


#define MT8518_PLL_FMAX		(3000UL * MHZ)
#define MT8518_PLL_ARMPLL_FMAX	(1800UL * MHZ)
#define MT8518_PLL_MAINPLL_FMAX	(1501500000UL)
#define MT8518_PLL_UNIVPLL_FMAX	(1248UL * MHZ)
#define MT8518_PLL_APLL1_FMAX	(180633600UL)
#define MT8518_PLL_APLL2_FMAX	(196608000UL)
#define MT8518_PLL_TVDPLL_FMAX	(594UL * MHZ)
#define MT8518_PLL_MMPLL_FMAX	(730UL * MHZ)

#define CON0_MT8518_RST_BAR	BIT(27)

#define PLL_B(_id, _name, _reg, _pwr_reg, _en_mask, _flags, _fmax, _pcwbits,	\
			_pd_reg, _pd_shift, _tuner_reg, _pcw_reg,	\
			_pcw_shift, _div_table) {			\
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.pwr_reg = _pwr_reg,					\
		.en_mask = _en_mask,					\
		.flags = _flags,					\
		.rst_bar_mask = CON0_MT8518_RST_BAR,			\
		.fmax = _fmax,				\
		.pcwbits = _pcwbits,					\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.div_table = _div_table,				\
	}

#define PLL(_id, _name, _reg, _pwr_reg, _en_mask, _flags, _pcwbits,	\
			_pd_reg, _pd_shift, _tuner_reg, _pcw_reg,	\
			_pcw_shift)					\
		PLL_B(_id, _name, _reg, _pwr_reg, _en_mask, _flags, MT8518_PLL_FMAX, _pcwbits, \
			_pd_reg, _pd_shift, _tuner_reg, _pcw_reg, _pcw_shift, \
			NULL)

static const struct mtk_pll_div_table armpll_div_table[] = {
	{ .div = 0, .freq = MT8518_PLL_ARMPLL_FMAX },
	{ .div = 1, .freq = 1013500000 },
	{ .div = 2, .freq = 506500000 },
	{ .div = 3, .freq = 253500000 },
	{ .div = 4, .freq = 126750000 },
	{ } /* sentinel */
};

static const struct mtk_pll_data plls[] = {
	PLL_B(CLK_APMIXED_ARMPLL, "armpll", 0x0100, 0x0110, 0x00000001,
	    0, MT8518_PLL_ARMPLL_FMAX, 21, 0x0104, 24, 0, 0x0104, 0, armpll_div_table),
	PLL_B(CLK_APMIXED_MAINPLL, "mainpll", 0x0120, 0x0130, 0x00000001,
	    HAVE_RST_BAR, MT8518_PLL_MAINPLL_FMAX, 21, 0x0124, 24, 0, 0x0124, 0, NULL),
	PLL_B(CLK_APMIXED_UNIVPLL, "univpll", 0x0140, 0x0150, 0x30000001,
	    HAVE_RST_BAR, MT8518_PLL_UNIVPLL_FMAX, 7, 0x0144, 24, 0, 0x0144, 0, NULL),
	PLL_B(CLK_APMIXED_MMPLL, "mmpll", 0x0160, 0x0170, 0x00000001,
	    0, MT8518_PLL_MMPLL_FMAX, 21, 0x0164, 24, 0, 0x0164, 0, NULL),
	PLL_B(CLK_APMIXED_APLL1, "apll1", 0x0180, 0x0190, 0x00000001,
	    0, MT8518_PLL_APLL1_FMAX, 31, 0x0180, 1, 0x0194, 0x0184, 0, NULL),
	PLL_B(CLK_APMIXED_APLL2, "apll2", 0x01A0, 0x01B0, 0x00000001,
	    0, MT8518_PLL_APLL2_FMAX, 31, 0x01A0, 1, 0x01B4, 0x01A4, 0, NULL),
	PLL_B(CLK_APMIXED_TVDPLL, "tvdpll", 0x01C0, 0x01D0, 0x00000001,
	    0, MT8518_PLL_TVDPLL_FMAX, 21, 0x01C4, 24, 0, 0x01C4, 0, NULL),
};

static int clk_mt8518_apmixed_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;
	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_err("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(CLK_APMIXED_NR_CLK);

	mtk_clk_register_plls(node, plls, ARRAY_SIZE(plls), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

	return r;
}

struct clk_onecell_data *top_clk_data;

static void clk_mt8518_top_init_early(struct device_node *node)
{
	int r, i;

	if (!top_clk_data) {
		top_clk_data = mtk_alloc_clk_data(CLK_TOP_NR_CLK);

		for (i = 0; i < CLK_TOP_NR_CLK; i++)
			top_clk_data->clks[i] = ERR_PTR(-EPROBE_DEFER);
	}

	mtk_clk_register_factors(top_early_divs, ARRAY_SIZE(top_early_divs),
			top_clk_data);
	mtk_clk_register_gates(node, top_early_clks, ARRAY_SIZE(top_early_clks),
			top_clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, top_clk_data);
	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);
}
CLK_OF_DECLARE_DRIVER(mt8518_topckgen, "mediatek,mt8518-topckgen",
			clk_mt8518_top_init_early);

static int clk_mt8518_top_probe(struct platform_device *pdev)
{
	int r, i;
	struct device_node *node = pdev->dev.of_node;
	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_err("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	if (!top_clk_data) {
		top_clk_data = mtk_alloc_clk_data(CLK_TOP_NR_CLK);
	} else {
		for (i = 0; i < CLK_TOP_NR_CLK; i++) {
			if (top_clk_data->clks[i] == ERR_PTR(-EPROBE_DEFER))
				top_clk_data->clks[i] = ERR_PTR(-ENOENT);
		}
	}

	mtk_clk_register_fixed_clks(top_fixed_clks,
	    ARRAY_SIZE(top_fixed_clks), top_clk_data);
	mtk_clk_register_factors(top_divs, ARRAY_SIZE(top_divs), top_clk_data);
	mtk_clk_register_composites(top_muxes, ARRAY_SIZE(top_muxes),
	    base, &mt8518_clk_lock, top_clk_data);
	mtk_clk_register_dividers(top_adj_divs, ARRAY_SIZE(top_adj_divs),
	    base, &mt8518_clk_lock, top_clk_data);
	mtk_clk_register_gates(node, top_clks, ARRAY_SIZE(top_clks),
		top_clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, top_clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

	return r;
}

static int clk_mt8518_mcu_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;
	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_err("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(CLK_MCU_NR_CLK);

	mtk_clk_register_composites(mcu_muxes, ARRAY_SIZE(mcu_muxes),
	    base, &mt8518_clk_lock, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

	return r;
}

static const struct of_device_id of_match_clk_mt8518[] = {
	{
		.compatible = "mediatek,mt8518-apmixedsys",
		.data = clk_mt8518_apmixed_probe,
	}, {
		.compatible = "mediatek,mt8518-topckgen",
		.data = clk_mt8518_top_probe,
	}, {
		.compatible = "mediatek,mt8518-mcucfg",
		.data = clk_mt8518_mcu_probe,
	}, {
		/* sentinel */
	}
};

static int clk_mt8518_probe(struct platform_device *pdev)
{
	int (*clk_probe)(struct platform_device *);
	int r;

	clk_probe = of_device_get_match_data(&pdev->dev);
	if (!clk_probe)
		return -EINVAL;

	r = clk_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

	return r;
}

static struct platform_driver clk_mt8518_drv = {
	.probe = clk_mt8518_probe,
	.driver = {
		.name = "clk-mt8518",
		.owner = THIS_MODULE,
		.of_match_table = of_match_clk_mt8518,
	},
};

static int __init clk_mt8518_init(void)
{
	return platform_driver_register(&clk_mt8518_drv);
}

arch_initcall(clk_mt8518_init);
