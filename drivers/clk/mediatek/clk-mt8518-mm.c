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
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt8518-clk.h>

static const struct mtk_gate_regs mm_cg_regs = {
	.set_ofs = 0x100,
	.clr_ofs = 0x100,
	.sta_ofs = 0x100,
};

#define GATE_MM(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

static const struct mtk_gate mm_clks[] = {
	GATE_MM(CLK_MM_SMI_COMMON, "mm_smi_common", "smi_sel", 0),
	GATE_MM(CLK_MM_SMI_LARB1, "mm_smi_larb1", "smi_sel", 1),
	GATE_MM(CLK_MM_FAKE, "mm_fake", "smi_sel", 2),
	GATE_MM(CLK_MM_DISP_DBI, "mm_disp_dbi", "smi_sel", 4),
	GATE_MM(CLK_MM_DBI_AXI, "mm_dbi_axi", "fdbi_sel", 15),
	GATE_MM(CLK_MM_DPI, "mm_dpi", "smi_sel", 18),
	GATE_MM(CLK_MM_DISP_DPI, "mm_disp_dpi", "smi_sel", 19),
};

static int clk_mt8518_mm_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_MM_NR_CLK);

	mtk_clk_register_gates(node, mm_clks, ARRAY_SIZE(mm_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

	return r;
}

static const struct of_device_id of_match_clk_mt8518_mm[] = {
	{ .compatible = "mediatek,mt8518-mmsys", },
	{}
};

static struct platform_driver clk_mt8518_mm_drv = {
	.probe = clk_mt8518_mm_probe,
	.driver = {
		.name = "clk-mt8518-mm",
		.of_match_table = of_match_clk_mt8518_mm,
	},
};

builtin_platform_driver(clk_mt8518_mm_drv);
