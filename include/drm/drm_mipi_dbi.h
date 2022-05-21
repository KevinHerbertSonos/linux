/*
 * MIPI DBI Bus
 *
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/module.h>

#ifndef __MIPI_DBI_BUS_H__
#define __MIPI_DBI_BUS_H__

#include <linux/device.h>

struct mipi_dbi_bus;
struct mipi_dbi_device;

struct mipi_dbi_msg {
	u8 type;
	u16 flags;

	size_t tx_len;
	const void *tx_buf;

	size_t rx_len;
	void *rx_buf;
};

struct mipi_dbi_bus_ops {
	int (*attach)(struct mipi_dbi_bus *bus, struct mipi_dbi_device *dbi);
	int (*detach)(struct mipi_dbi_bus *bus, struct mipi_dbi_device *dbi);
	ssize_t (*transfer)(struct mipi_dbi_bus *bus,
		const struct mipi_dbi_msg *msg);
/*
 *	int (*write_command) (struct mipi_dbi_bus *bus, u16 cmd);
 *	int (*write_data) (struct mipi_dbi_bus *bus, const u8 *data,
 *			size_t len);
 *	int (*read_data) (struct mipi_dbi_bus *bus, u8 *data, size_t len);
 */
};

struct mipi_dbi_bus {
	struct device *dev;
	const struct mipi_dbi_bus_ops *ops;
};

#define MIPI_DBI_MODULE_PREFIX		"mipi-dbi:"
#define MIPI_DBI_NAME_SIZE		32

enum mipi_dbi_format {
	MIPI_DBI_FORMAT_RGB332 = 0,
	MIPI_DBI_FORMAT_RGB444 = 1,
	MIPI_DBI_FORMAT_RGB565 = 2,
	MIPI_DBI_FORMAT_RGB666 = 3,
	MIPI_DBI_FORMAT_RGB888 = 4
};

enum mipi_dbi_data_width {
	MIPI_DBI_DATA_WIDTH_8BITS  = 0,
	MIPI_DBI_DATA_WIDTH_9BITS  = 1,
	MIPI_DBI_DATA_WIDTH_16BITS = 2,
	MIPI_DBI_DATA_WIDTH_18BITS = 3,
	MIPI_DBI_DATA_WIDTH_24BITS = 4,
};

enum mipi_dbi_interface_type {
	MIPI_DBI_INTERFACE_TYPE_A,
	MIPI_DBI_INTERFACE_TYPE_B,
	MIPI_DBI_INTERFACE_TYPE_C,
};

enum mipi_dbi_bus_width {
	MIPI_DBI_BUS_WIDTH_8_BITS  = 8,
	MIPI_DBI_BUS_WIDTH_16_BITS = 16,
	MIPI_DBI_BUS_WIDTH_32_BITS = 32,
};

struct mipi_dbi_device {
	const char *name;
	int id;
	struct device dev;

	struct mipi_dbi_bus *bus;
	enum mipi_dbi_interface_type type;
	enum mipi_dbi_format format;
	unsigned int bus_width;
	unsigned int data_width;
};

#define to_mipi_dbi_device(d)	container_of(d, struct mipi_dbi_device, dev)
int mipi_dbi_device_register(struct mipi_dbi_device *dev,
		struct mipi_dbi_bus *bus);
void mipi_dbi_device_unregister(struct mipi_dbi_device *dev);
int mipi_dbi_attach(struct mipi_dbi_device *dbi);
int mipi_dbi_detach(struct mipi_dbi_device *dbi);
int mipi_dbi_host_register(struct mipi_dbi_bus *bus);
void mipi_dbi_host_unregister(struct mipi_dbi_bus *bus);

struct mipi_dbi_driver {
	int (*probe)(struct mipi_dbi_device *);
	int (*remove)(struct mipi_dbi_device *);
	void (*shutdown)(struct mipi_dbi_device *);
	struct device_driver driver;
};

#define to_mipi_dbi_driver(d)	container_of(d, struct mipi_dbi_driver, driver)

int mipi_dbi_driver_register(struct mipi_dbi_driver *drv);
void mipi_dbi_driver_unregister(struct mipi_dbi_driver *drv);

static inline void *mipi_dbi_get_drvdata(const struct mipi_dbi_device *dev)
{
	return dev_get_drvdata(&dev->dev);
}

static inline void mipi_dbi_set_drvdata(struct mipi_dbi_device *dev, void *data)
{
	dev_set_drvdata(&dev->dev, data);
}

int mipi_dbi_driver_register_full(struct mipi_dbi_driver *drv,
		struct module *owner);
void mipi_dbi_driver_unregister(struct mipi_dbi_driver *drv);

#define mipi_dbi_driver_register(driver) \
	mipi_dbi_driver_register_full(driver, THIS_MODULE)

#define module_mipi_dbi_driver(__mipi_dbi_driver) \
	 module_driver(__mipi_dbi_driver, mipi_dbi_driver_register, \
			 mipi_dbi_driver_unregister)

int mipi_dbi_set_data_width(struct mipi_dbi_device *dev, unsigned int width);
int mipi_dbi_write_command(struct mipi_dbi_device *dev, u16 cmd);
int mipi_dbi_read_data(struct mipi_dbi_device *dev, u8 *data, size_t len);
int mipi_dbi_write_data(struct mipi_dbi_device *dev, const u8 *data,
		size_t len);
ssize_t mipi_dbi_dcs_write_buffer(struct mipi_dbi_device *dbi,
				  const void *data, size_t len);
#endif				/* __MIPI_DBI_BUS__ */
