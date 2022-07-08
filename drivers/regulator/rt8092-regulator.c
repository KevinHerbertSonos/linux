/*
 * Regulator driver for the Richtek RT8092 Step-Down DC-DC Converter
 *
 * Copyright (C) 2017 Sonos, Inc.
 * Author: Allen Antony <allen.antony@sonos.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>

/* i2c Registers */
#define RT8092_REG_STATUS			0x01
#define RT8092_REG_CONTROL1			0x10
#define RT8092_REG_CONTROL0			0x11
#define RT8092_REG_DISCHARGE		0x12
#define RT8092_REG_PWM_MODE			0x14
#define RT8092_REG_CURR_LIMIT		0x16
#define RT8092_REG_EVENTS			0x18
#define RT8092_REG_LOW_POWER		0x19
#define RT8092_REG_VOUT_BANK1		0x1C
#define RT8092_REG_VOUT_BANK0		0x1D
#define RT8092_REG_VOUT_LIMIT		0x1E

/* Bit masks */
#define RT8092_MSK_SEN_TSD			BIT(7)
#define RT8092_MSK_SEN_PG			BIT(0)
#define RT8092_MSK_ENSEL			BIT(7)
#define RT8092_MSK_VOUTSEL			0x7F
#define RT8092_MSK_DISCHARGE_EN		BIT(4)
#define RT8092_MSK_PWM0_MODE		BIT(6)
#define RT8092_MSK_PWM1_MODE		BIT(7)
#define RT8092_MSK_IPEAK_SEL		0xC0
#define RT8092_MSK_OCP				BIT(7)
#define RT8092_MSK_SCP				BIT(4)
#define RT8092_MSK_PVIN_UVLO		BIT(1)
#define RT8092_MSK_LPM				BIT(3)
#define RT8092_MSK_VOUT_BANK		0x03
#define RT8092_MSK_VLIMIT_MAX		0xF0
#define RT8092_MSK_VLIMIT_MIN		0x0F

#define RT8092_FORCED_PWM			1
#define RT8092_NUM_STEPS			0x80 /* 7 bits represents the number of voltage steps */
#define RT8092_RAMP_DELAY			300  /* unit is uV/us */


struct rt8092_regulator_info {
	struct device *dev;
	struct regmap *regmap;
	bool Vsel_high;
};

struct rt8092_platform_data {
	struct regulator_init_data *regulator;
};


static int rt8092_get_status(struct regulator_dev *rdev)
{
	struct rt8092_regulator_info *info = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret;
	ret = regmap_read(info->regmap, RT8092_REG_STATUS, &val);
	if (ret != 0) {
		return ret;
	}
	return (int)val;
}


static int rt8092_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct rt8092_regulator_info *info = rdev_get_drvdata(rdev);

	switch (mode) {
		case REGULATOR_MODE_FAST:
			regmap_update_bits(info->regmap, RT8092_REG_PWM_MODE,
					(info->Vsel_high ? RT8092_MSK_PWM1_MODE : RT8092_MSK_PWM0_MODE), RT8092_FORCED_PWM);
			break;
		case REGULATOR_MODE_NORMAL:
			regmap_update_bits(info->regmap, RT8092_REG_PWM_MODE,
					(info->Vsel_high ? RT8092_MSK_PWM1_MODE : RT8092_MSK_PWM0_MODE), !RT8092_FORCED_PWM);
			break;
		default:
			return -EINVAL;
	}
	return 0;
}


static unsigned int rt8092_get_mode(struct regulator_dev *rdev)
{
	struct rt8092_regulator_info *info = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret;

	ret = regmap_read(info->regmap, RT8092_REG_PWM_MODE, &val);
	if (ret != 0) {
		return ret;
	}
	if (val & (info->Vsel_high ? RT8092_MSK_PWM1_MODE : RT8092_MSK_PWM0_MODE)) {
		return REGULATOR_MODE_FAST;
	}
	return REGULATOR_MODE_NORMAL;
}


static const struct regulator_ops rt8092_ops = {
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.list_voltage	= regulator_list_voltage_linear,
	.map_voltage	= regulator_map_voltage_linear,
	.enable		= regulator_enable_regmap,
	.disable	= regulator_disable_regmap,
	.is_enabled	= regulator_is_enabled_regmap,
	.set_mode	= rt8092_set_mode,
	.get_mode	= rt8092_get_mode,
	.get_status	= rt8092_get_status,
};

static struct regulator_desc rt8092_desc = {
	.ops		= &rt8092_ops,
	.name		= "rt8092_reg" ,
	.type		= REGULATOR_VOLTAGE,
	.n_voltages	= RT8092_NUM_STEPS,
	.owner		= THIS_MODULE,
	.vsel_mask	= RT8092_MSK_VOUTSEL,
	.ramp_delay	= RT8092_RAMP_DELAY,
	.enable_mask	= RT8092_MSK_ENSEL,
	.enable_is_inverted	= false,
};

static const struct regmap_config rt8092_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static struct of_regulator_match rt8092_matches[] = {
		{ .name = "rt8092_reg" },
};


static int rt8092_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct rt8092_platform_data pdata;
	struct rt8092_regulator_info *info = NULL;
	struct regulator_dev *regulator;
	struct regulator_config config = { };
	struct of_regulator_match *reg_matches = rt8092_matches;
	struct device_node *node;
	struct device *dev = &client->dev;
	unsigned int val;
	unsigned int reg_num_matches = 1;
	unsigned int Vmax_limit_uV;
	unsigned int Vmin_limit_uV;
	unsigned int min_uV;
	unsigned int max_uV;
	unsigned int uV_step;
	int ret;
	int reg_matched;
	uint8_t vlimit_code_min;
	uint8_t vlimit_code_max;
	uint8_t vlimit_code_byte;

	info = devm_kzalloc(&client->dev, sizeof(struct rt8092_regulator_info), GFP_KERNEL);
	if (IS_ERR(info)) {
		dev_err(&client->dev, "Failed to allocate memory!");
		return -ENOMEM;
	}

	node = client->dev.of_node;
	info->Vsel_high = of_property_read_bool(node, "Vsel-high");
	if (of_property_read_u32(node, "Vout-max", &Vmax_limit_uV)) {
		dev_err(&client->dev, "Failed to find the 'Vout-max' property!");
		return -EINVAL;
	}
	if (of_property_read_u32(node, "Vout-min", &Vmin_limit_uV)) {
		dev_err(&client->dev, "Failed to find the 'Vout-min' property!");
		return -EINVAL;
	}

	node = of_get_child_by_name(dev->of_node, "regulators");
	if (IS_ERR(node)) {
		dev_err(&client->dev, "Missing 'regulators' subnode in DT");
		return -EINVAL;
	}

	reg_matched = of_regulator_match(dev, node, reg_matches, reg_num_matches);
	of_node_put(node);
	if (reg_matched <= 0) {
		dev_err(&client->dev, "Failed to match a regulator entry in DT (%d)", reg_matched);
		return reg_matched;
	} else if (reg_num_matches != 1) {
		dev_err(&client->dev, "There can only be ONE regulator entry in DT since rt8092 only has ONE!");
		return -EINVAL;
	}

	pdata.regulator = rt8092_matches[0].init_data;

	info->regmap = devm_regmap_init_i2c(client, &rt8092_regmap_config);
	if (IS_ERR(info->regmap)) {
		ret = PTR_ERR(info->regmap);
		dev_err(&client->dev, "Failed to allocate register map (%d)", ret);
		return ret;
	}

	info->dev = &client->dev;
	i2c_set_clientdata(client, info);

	if (info->Vsel_high) {
		rt8092_desc.vsel_reg = RT8092_REG_CONTROL1;
		rt8092_desc.enable_reg = RT8092_REG_CONTROL1;
	} else {
		rt8092_desc.vsel_reg = RT8092_REG_CONTROL0;
		rt8092_desc.enable_reg = RT8092_REG_CONTROL0;
	}

	ret = regmap_read(info->regmap, (info->Vsel_high ? RT8092_REG_VOUT_BANK1 : RT8092_REG_VOUT_BANK0), &val);
	val &= RT8092_MSK_VOUT_BANK;
	if ((ret != 0) || (val != (info->Vsel_high ? 2 : 1))) {
		dev_err(info->dev, "Failed to read Vout Bank reg or got invalid defaults! ret(%d) val(%d)", ret, val);
		return ret;
	}
	dev_info(info->dev, "Detected Richtek RT8092 Step-Down DC-DC Converter");

	/* Vout voltage level = (303.125mV + 3.125mV x VoutSELx[6:0]) x 2 ^ (VOUT_BANKx[1:0])
	 * So, min_uV will be the Vout value when VoutSELx[6:0] is 0b0000000, max_uV will be
	 * the Vout value when VoutSELx[6:0] is 0b1111111 and uV_step will be
	 * the (max_uV - min_uV) / (RT8092_NUM_STEPS - 1). The VoutSel uses 7 bits */
	min_uV = (303125 + (3125 * 0)) * (1 << (val & RT8092_MSK_VOUT_BANK));
	max_uV = (303125 + (3125 * 127)) * (1 << (val & RT8092_MSK_VOUT_BANK));
	uV_step = (max_uV - min_uV) / (RT8092_NUM_STEPS - 1);

	/* Since the Voltage min/max hard limits are expressed in 4 bits and the Vout output voltage
	 * is expressed in 7 bits, there may be a delta depending on the values. The max delta for
	 * Vmax_limit_uV will be +(2^3 * uV_step) and the max delta for Vmin_limit_uV will be -(2^3 * uV_step) */

	/* if Register VoutSELx[6:0] > {Vout_High[3:0], 3'b111}, effective
	 * Vout would be limited to be the voltage corresponding to the code {Vout_High[3:0],
	 * 3'b111}. Else effective Vout would follow Register VoutSELx[6:0] setting. */
	vlimit_code_max = (uint8_t)(((Vmax_limit_uV / (1 << (val & RT8092_MSK_VOUT_BANK))) - 303125 ) / 3125 );
	vlimit_code_max = (vlimit_code_max << 1) & RT8092_MSK_VLIMIT_MAX;
	/* if Register VoutSELx[6:0] < {Vout_Low[3:0], 3'b000}, effective
	 * Vout would be limited to be the voltage corresponding to the code {Vout_Low[3:0],
	 * 3'b000}. Else effective Vout would follow Register VoutSELx[6:0] setting. */
	vlimit_code_min = (uint8_t)(((Vmin_limit_uV / (1 << (val & RT8092_MSK_VOUT_BANK))) - 303125 ) / 3125 );
	vlimit_code_min = (vlimit_code_min >> 3) & RT8092_MSK_VLIMIT_MIN;

	vlimit_code_byte = vlimit_code_min | vlimit_code_max;
	regmap_write(info->regmap, RT8092_REG_VOUT_LIMIT, vlimit_code_byte);

	rt8092_desc.min_uV = min_uV;
	rt8092_desc.uV_step = uV_step;

	config.dev = &client->dev;
	config.init_data = pdata.regulator;
	config.driver_data = info;
	config.regmap = info->regmap;

	regulator = devm_regulator_register(dev, &rt8092_desc, &config);
	if (IS_ERR(regulator)) {
		dev_err(info->dev, "Failed to register regulator %s\n", rt8092_desc.name);
		return PTR_ERR(regulator);
	}

	return 0;
}

static struct of_device_id rt8092_ids[] = {
	{ .compatible = "richtek,rt8092"},
	{ }
};

static const struct i2c_device_id rt8092_id[] = {
	{ "rt8092", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, rt8092_id);

static struct i2c_driver rt8092_driver = {
	.probe		= rt8092_probe,
	.driver		= {
		.name	= "rt8092",
		.of_match_table = rt8092_ids
	},
	.id_table	= rt8092_id,
};

static int __init rt8092_init(void)
{
	return i2c_add_driver(&rt8092_driver);
}
subsys_initcall(rt8092_init);

static void __exit rt8092_exit(void)
{
	i2c_del_driver(&rt8092_driver);
}
module_exit(rt8092_exit);

/* Module information */
MODULE_DESCRIPTION("Richtek RT8092 voltage regulator driver");
MODULE_AUTHOR("Sonos, Inc");
MODULE_LICENSE("GPL");
