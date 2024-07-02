// SPDX-License-Identifier: GPL-2.0+
/*
 * Amlogic Meson-AXG Clock Controller Driver
 *
 * Copyright (c) 2016 BayLibre, SAS.
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 *
 * Copyright (c) 2018 Amlogic, inc.
 * Author: Qiufang Dai <qiufang.dai@amlogic.com>
 * Author: Yixun Lan <yixun.lan@amlogic.com>
 */

#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/module.h>

#include <linux/slab.h>
#include "meson-aoclk.h"

static int meson_aoclk_do_reset(struct reset_controller_dev *rcdev,
			       unsigned long id)
{
	struct meson_aoclk_reset_controller *rstc =
		container_of(rcdev, struct meson_aoclk_reset_controller, reset);

	return regmap_write(rstc->regmap, rstc->data->reset_reg,
			    BIT(rstc->data->reset[id]));
}

static const struct reset_control_ops meson_aoclk_reset_ops = {
	.reset = meson_aoclk_do_reset,
};

int meson_aoclkc_probe(struct platform_device *pdev)
{
	struct meson_aoclk_reset_controller *rstc;
	struct meson_aoclk_data *data;
	struct device *dev = &pdev->dev;
	struct device_node *np;
	struct regmap *regmap;
	int ret, clkid;
	int ao_32k_ignore_unused =
		of_property_read_bool(pdev->dev.of_node,
				      "ao-32k-ignore-unused");

	data = (struct meson_aoclk_data *) of_device_get_match_data(dev);
	if (!data)
		return -ENODEV;

	rstc = devm_kzalloc(dev, sizeof(*rstc), GFP_KERNEL);
	if (!rstc)
		return -ENOMEM;

	np = of_get_parent(dev->of_node);
	regmap = syscon_node_to_regmap(np);
	of_node_put(np);
	if (IS_ERR(regmap)) {
		dev_err(dev, "failed to get regmap\n");
		return PTR_ERR(regmap);
	}

	/* Reset Controller */
	rstc->data = data;
	rstc->regmap = regmap;
	rstc->reset.ops = &meson_aoclk_reset_ops;
	rstc->reset.nr_resets = data->num_reset;
	rstc->reset.of_node = dev->of_node;
	ret = devm_reset_controller_register(dev, &rstc->reset);
	if (ret) {
		dev_err(dev, "failed to register reset controller\n");
		return ret;
	}

	/* Populate regmap */
	for (clkid = 0; clkid < data->num_clks; clkid++)
		data->clks[clkid]->map = regmap;

	/* Register all clks */
	for (clkid = 0; clkid < data->hw_clks.num; clkid++) {
		if (!data->hw_clks.hws[clkid])
			continue;

		/*
		 * Per Amlogic, 32k_by_oscin_pre and 32k_by_oscin_clk
		 * are required to remain enabled for proper scheduling of
		 * tasks by FreeRTOS on the m4.  From Linux's perspective,
		 * however, these clocks appear to be unused.
		 *
		 * Linux's default behavior is to disable unused clocks
		 * after all driver probes have completed.  Add a device
		 * tree property ao-32k-ignore-unused that allows us to
		 * specify that related clocks should remain enabled.
		 */
		if (ao_32k_ignore_unused) {
			if (!strcmp("g12a_ao_32k_by_oscin_pre",
				    data->hw_data->hws[clkid]->init->name)) {
				((struct clk_init_data *)
				 data->hw_data->hws[clkid]->init)->flags |=
							CLK_IGNORE_UNUSED;
			}
			if (!strcmp("g12a_ao_32k_by_oscin",
				    data->hw_data->hws[clkid]->init->name)) {
				((struct clk_init_data *)
				 data->hw_data->hws[clkid]->init)->flags |=
							CLK_IGNORE_UNUSED;
			}
		}

		ret = devm_clk_hw_register(dev, data->hw_data->hws[clkid]);
		if (ret) {
			dev_err(dev, "Clock registration failed\n");
			return ret;
		}
	}

	return devm_of_clk_add_hw_provider(dev, meson_clk_hw_get, (void *)&data->hw_clks);
}
EXPORT_SYMBOL_NS_GPL(meson_aoclkc_probe, CLK_MESON);

MODULE_DESCRIPTION("Amlogic Always-ON Clock Controller helpers");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS(CLK_MESON);
