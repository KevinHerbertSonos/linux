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

#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include "mt8518-asrc-reg.h"
#include "mt8518-asrc-clk.h"

static struct clk *asrc_clocks[MT8518_CLK_NUM];

static const char *aud_clks_8518[MT8518_CLK_NUM] = {
	[MT8518_CLK_FASM_L] = "fasm_l",
	[MT8518_CLK_APLL1] =  "apll1",
	[MT8518_CLK_APLL2] =  "apll2",
};

int asrc_init_clock(struct device *dev)
{
	int i = 0;

	for (i = 0; i < (int)MT8518_CLK_NUM; i++) {
		asrc_clocks[i] = devm_clk_get(dev, aud_clks_8518[i]);
		if (IS_ERR(aud_clks_8518[i])) {
			pr_debug("%s devm_clk_get %s fail\n",
			__func__, aud_clks_8518[i]);
		return (int)PTR_ERR(aud_clks_8518[i]);
		}
	}

	return 0;
}

int asrc_turn_on_asrc_clock(void)
{
	int ret = 0;

	/* Set Mux */
	ret = clk_prepare_enable(asrc_clocks[MT8518_CLK_FASM_L]);
	if (ret != 0) {
		pr_debug("%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks_8518[MT8518_CLK_FASM_L], ret);
		clk_disable_unprepare(asrc_clocks[MT8518_CLK_FASM_L]);
	}
	// has set default parent in dtsi. so dose not call clk_set_parent.

	return ret;
}

void asrc_turn_off_asrc_clock(void)
{
	clk_disable_unprepare(asrc_clocks[MT8518_CLK_FASM_L]);
}

unsigned long asrc_get_clock_rate(enum audio_system_clock_type_8518 clk_type)
{
	if (clk_type > MT8518_CLK_NUM)
		return 0;
	return clk_get_rate(asrc_clocks[clk_type]);
}

