/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Qiqi Wang <Qiqi.wang@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/clk.h>

static int pwm_regulator_clock_probe(struct platform_device *pdev)
{
	int ret;
	struct clk *clk_pwm_infra;
	struct clk *clk_pwm_top;
	struct clk *clk_pwm;
	struct clk *clk_pwmn;

	clk_pwm_infra = devm_clk_get(&pdev->dev, "pwm_infra");
	if (IS_ERR(clk_pwm_infra)) {
		dev_info(&pdev->dev, "failed to get clock: %ld\n",
				PTR_ERR(clk_pwm_infra));
		return PTR_ERR(clk_pwm_infra);
	}
	ret = clk_prepare_enable(clk_pwm_infra);
	if (ret)
		return ret;

	clk_pwm_top = devm_clk_get(&pdev->dev, "pwm_top");
	if (IS_ERR(clk_pwm_top)) {
		dev_info(&pdev->dev, "failed to get clock: %ld\n",
				PTR_ERR(clk_pwm_top));
		return PTR_ERR(clk_pwm_top);
	}
	ret = clk_prepare_enable(clk_pwm_top);
	if (ret)
		return ret;

	clk_pwm = devm_clk_get(&pdev->dev, "pwm");
	if (IS_ERR(clk_pwm)) {
		dev_info(&pdev->dev, "failed to get clock: %ld\n",
				PTR_ERR(clk_pwm));
		return PTR_ERR(clk_pwm);
	}
	ret = clk_prepare_enable(clk_pwm);
	if (ret)
		return ret;

	clk_pwmn = devm_clk_get(&pdev->dev, "pwm0");
	if (IS_ERR(clk_pwmn)) {
		dev_info(&pdev->dev, "failed to get clock: %ld\n",
				PTR_ERR(clk_pwmn));
		return PTR_ERR(clk_pwmn);
	}
	ret = clk_prepare_enable(clk_pwmn);
	if (ret)
		return ret;

	return 0;
}

static const struct of_device_id pwm_clock_of_match[] = {
	{ .compatible = "mediatek, pwm-regulator-clock" },
	{ },
};
MODULE_DEVICE_TABLE(of, pwm_clock_of_match);

static struct platform_driver pwm_regulator_clock_driver = {
	.driver = {
		.name		= "pwm-regulator-mediatek",
		.of_match_table = of_match_ptr(pwm_clock_of_match),
	},
	.probe = pwm_regulator_clock_probe,
};

module_platform_driver(pwm_regulator_clock_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Qiqi Wang <Qiqi.wang@mediatek.com>");
MODULE_DESCRIPTION("PWM Regulator Dummy Clock Driver for Mediatek solution");
MODULE_ALIAS("platform:pwm-regulator-clock");

