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
#include <linux/syscore_ops.h>
#include <linux/version.h>

#define WARN_ON_CHECK_PLL_FAIL		0
#define CLKDBG_CCF_API_4_4	1

#define TAG	"[clkchk] "

#define clk_warn(fmt, args...)	pr_warn(TAG fmt, ##args)

#if !CLKDBG_CCF_API_4_4

/* backward compatible */

static const char *clk_hw_get_name(const struct clk_hw *hw)
{
	return __clk_get_name(hw->clk);
}

static bool clk_hw_is_prepared(const struct clk_hw *hw)
{
	return __clk_is_prepared(hw->clk);
}

static bool clk_hw_is_enabled(const struct clk_hw *hw)
{
	return __clk_is_enabled(hw->clk);
}

#endif /* !CLKDBG_CCF_API_4_4 */

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

static const char *ccf_state(struct clk_hw *hw)
{
	if (__clk_get_enable_count(hw->clk))
		return "enabled";

	if (clk_hw_is_prepared(hw))
		return "prepared";

	return "disabled";
}

static void print_enabled_clks(void)
{
	const char * const *cn = get_all_clk_names();

	clk_warn("enabled clks:\n");

	for (; *cn; cn++) {
		struct clk *c = __clk_lookup(*cn);
		struct clk_hw *c_hw = __clk_get_hw(c);
		struct clk_hw *p_hw;

		if (IS_ERR_OR_NULL(c) || !c_hw)
			continue;

		p_hw = clk_hw_get_parent(c_hw);

		if (!p_hw)
			continue;

		if (!clk_hw_is_prepared(c_hw) && !__clk_get_enable_count(c))
			continue;

		clk_warn("[%-17s: %8s, %3d, %3d, %10ld, %17s]\n",
			clk_hw_get_name(c_hw),
			ccf_state(c_hw),
			clk_hw_is_prepared(c_hw),
			__clk_get_enable_count(c),
			clk_hw_get_rate(c_hw),
			p_hw ? clk_hw_get_name(p_hw) : "- ");
	}
}

static void check_pll_off(void)
{
	static const char * const off_pll_names[] = {
		"univpll",
		"mmpll",
		"apll1",
		"apll2",
		"tvdpll",
		NULL
	};

	static struct clk *off_plls[ARRAY_SIZE(off_pll_names)];

	struct clk **c;
	int invalid = 0;
	char buf[128] = {0};
	int n = 0;

	if (!off_plls[0]) {
		const char * const *pn;

		for (pn = off_pll_names, c = off_plls; *pn; pn++, c++)
			*c = __clk_lookup(*pn);
	}

	for (c = off_plls; *c; c++) {
		struct clk_hw *c_hw = __clk_get_hw(*c);

		if (!c_hw)
			continue;

		if (!clk_hw_is_prepared(c_hw) && !clk_hw_is_enabled(c_hw))
			continue;

		n += snprintf(buf + n, sizeof(buf) - n, "%s ",
				clk_hw_get_name(c_hw));

		invalid++;
	}

	if (invalid) {
		clk_warn("unexpected unclosed PLL: %s\n", buf);
		print_enabled_clks();

#if WARN_ON_CHECK_PLL_FAIL
		WARN_ON(1);
#endif
	}
}

static int clkchk_syscore_suspend(void)
{
	check_pll_off();

	return 0;
}

static void clkchk_syscore_resume(void)
{
}

static struct syscore_ops clkchk_syscore_ops = {
	.suspend = clkchk_syscore_suspend,
	.resume = clkchk_syscore_resume,
};

static int __init clkchk_init(void)
{
	if (!of_machine_is_compatible("mediatek,mt8518"))
		return -ENODEV;

	register_syscore_ops(&clkchk_syscore_ops);

	return 0;
}
subsys_initcall(clkchk_init);
