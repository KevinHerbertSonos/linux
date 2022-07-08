/*
 * drivers/amlogic/media/vout/backlight/bl_extern/i2c_ad7535.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/amlogic/i2c-amlogic.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/amlogic/media/vout/lcd/aml_bl_extern.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include "bl_extern.h"


#define BL_EXTERN_INDEX			0
#define BL_EXTERN_NAME			"i2c_ad7535"
#define BL_EXTERN_TYPE			BL_EXTERN_I2C

#define BL_EXTERN_CMD_SIZE		4

static unsigned int bl_status;

typedef struct {
	unsigned char addr;
	unsigned char reg;
	unsigned char val;
	unsigned char delay;
} i2c_config;

i2c_config init_on_table[] = {
	{0x39, 0x41, 0x10, 0x00},
	{0x39, 0x16, 0x20, 0x00},
	{0x39, 0x9a, 0xe0, 0x00},
	{0x39, 0xba, 0x70, 0x00},
	{0x39, 0xde, 0x82, 0x00},
	{0x39, 0xe4, 0x40, 0x00},
	{0x39, 0xe5, 0x80, 0x00},
	{0x3c, 0x15, 0xd0, 0x00},
	{0x3c, 0x17, 0xd0, 0x00},
	{0x3c, 0x24, 0x20, 0x00},
	{0x3c, 0x57, 0x11, 0x00},
	{0x3c, 0x1c, 0x40, 0x00},
	{0x3c, 0x16, 0x00, 0x00},
	{0x3c, 0x27, 0xcb, 0x00},
	{0x3c, 0x28, 0x89, 0x00},
	{0x3c, 0x29, 0x80, 0x00},
	{0x3c, 0x2a, 0x02, 0x00},
	{0x3c, 0x2b, 0xc0, 0x00},
	{0x3c, 0x2c, 0x05, 0x00},
	{0x3c, 0x2d, 0x80, 0x00},
	{0x3c, 0x2e, 0x09, 0x00},
	{0x3c, 0x2f, 0x40, 0x00},
	{0x3c, 0x30, 0x46, 0x00},
	{0x3c, 0x31, 0x50, 0x00},
	{0x3c, 0x32, 0x00, 0x00},
	{0x3c, 0x33, 0x50, 0x00},
	{0x3c, 0x34, 0x00, 0x00},
	{0x3c, 0x35, 0x40, 0x00},
	{0x3c, 0x36, 0x02, 0x00},
	{0x3c, 0x37, 0x40, 0x00},
	{0x3c, 0x27, 0xcb, 0x00},
	{0x3c, 0x27, 0x8b, 0x00},
	{0x3c, 0x27, 0xcb, 0x00},
	{0x39, 0xaf, 0x16, 0x00},
	{0x39, 0x55, 0x10, 0x00},
	{0x39, 0x56, 0x28, 0x00},
	{0x39, 0x40, 0x80, 0x00},
	{0x39, 0x4c, 0x04, 0x00},
	{0x39, 0x49, 0x00, 0x00},
	{0x3c, 0x05, 0xc8, 0x00},
	{0x39, 0x01, 0x00, 0x00},
	{0x39, 0x02, 0x18, 0x00},
	{0x39, 0x03, 0x00, 0x00},
	{0x39, 0x0a, 0x41, 0x00},
	{0x39, 0x0c, 0xbc, 0x00},
	{0x39, 0x15, 0x20, 0x00},
	{0x3c, 0xbe, 0x3d, 0x00},
	{0x3c, 0x03, 0x89, 0x00},
	{0x00, 0x00, 0x00, 0x00},
};

static int i2c_ad7535_write(unsigned char addr,
		unsigned char *buff, unsigned int len)
{
	int ret = 0;
	struct aml_bl_extern_i2c_dev_s *i2c_dev = aml_bl_extern_i2c_get_dev();

	struct i2c_msg msg[] = {
		{
			.addr = addr,
			.flags = 0,
			.len = len,
			.buf = buff,
		}
	};

	if (i2c_dev == NULL) {
		BLEXERR("invalid i2c device\n");
		return -1;
	}

	ret = i2c_transfer(i2c_dev->client->adapter, msg, 1);
	if (ret < 0)
		BLEXERR("i2c write failed [addr 0x%02x]\n",
			i2c_dev->client->addr);

	return ret;
}

static int i2c_ad7535_power_cmd(i2c_config *init_table)
{
	int ret = 0;
	int i = 0;

	while ( 1 ) {
		if (init_table[i].addr  == 0 ) {
			break;
		}
		i2c_ad7535_write(init_table[i].addr, &init_table[i].reg, 2);
		if ( init_table[i].delay ) {
			mdelay(init_table[i].delay);
		}
		i++;
	}

	return ret;
}

static int i2c_ad7535_power_ctrl(int flag)
{
	int ret = 0;
	struct aml_bl_extern_driver_s *bl_extern = aml_bl_extern_get_driver();

	ret = i2c_ad7535_power_cmd(init_on_table);

	BLEX("%s(%d: %s): %d\n",
		__func__, bl_extern->config.index,
		bl_extern->config.name, flag);
	return ret;
}

int i2c_ad7535_power_on(void)
{
	int ret;

	bl_status = 1;
	ret = i2c_ad7535_power_ctrl(1);

	return ret;

}

int i2c_ad7535_power_off(void)
{
	int ret;

	bl_status = 0;
	ret = i2c_ad7535_power_ctrl(0);
	return ret;

}

int i2c_ad7535_set_level(unsigned int level)
{
	int ret = 0;
	struct aml_bl_drv_s *bl_drv = aml_bl_get_driver();
	struct aml_bl_extern_driver_s *bl_extern = aml_bl_extern_get_driver();
	unsigned int level_max, level_min;
	unsigned int dim_max, dim_min;

	if (bl_drv == NULL)
		return -1;
	level_max = bl_drv->bconf->level_max;
	level_min = bl_drv->bconf->level_min;
	dim_max = bl_extern->config.dim_max;
	dim_min = bl_extern->config.dim_min;
	level = dim_min - ((level - level_min) * (dim_min - dim_max)) /
			(level_max - level_min);
	level &= 0xff;

	return ret;
}

static int i2c_ad7535_update(void)
{
	struct aml_bl_extern_driver_s *bl_extern = aml_bl_extern_get_driver();

	if (bl_extern == NULL) {
		BLEXERR("%s driver is null\n", BL_EXTERN_NAME);
		return -1;
	}

	if (bl_extern->config.type == BL_EXTERN_MAX) {
		bl_extern->config.index = BL_EXTERN_INDEX;
		bl_extern->config.type = BL_EXTERN_TYPE;
		strcpy(bl_extern->config.name, BL_EXTERN_NAME);
	}

	return 0;
}

int i2c_ad7535_probe(void)
{
	int ret = 0;

	ret = i2c_ad7535_update();

	i2c_ad7535_power_on();

	return ret;
}

int i2c_ad7535_remove(void)
{
	return 0;
}
