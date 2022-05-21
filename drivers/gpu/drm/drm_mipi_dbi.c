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

#include <drm/drm_mipi_dbi.h>

#include <linux/device.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>

#include <video/mipi_display.h>

static const struct dev_pm_ops mipi_dbi_device_pm_ops = {
	.runtime_suspend = pm_generic_runtime_suspend,
	.runtime_resume = pm_generic_runtime_resume,
	.suspend = pm_generic_suspend,
	.resume = pm_generic_resume,
	.freeze = pm_generic_freeze,
	.thaw = pm_generic_thaw,
	.poweroff = pm_generic_poweroff,
	.restore = pm_generic_restore,
};

static int mipi_dbi_device_match(struct device *dev, struct device_driver *drv)
{
	return of_driver_match_device(dev, drv);
}

static int mipi_dbi_uevent(struct device *_dev, struct kobj_uevent_env *env)
{
	struct mipi_dbi_device *dev = to_mipi_dbi_device(_dev);

	add_uevent_var(env, "MODALIAS=%s%s", MIPI_DBI_MODULE_PREFIX,
		       dev->name);
	return 0;
}

static struct bus_type mipi_dbi_bus_type = {
	.name = "mipi-dbi",
	.match = mipi_dbi_device_match,
	.uevent = mipi_dbi_uevent,
	.pm = &mipi_dbi_device_pm_ops,
};

static void mipi_dbi_dev_release(struct device *dev)
{
	struct mipi_dbi_device *dbi = to_mipi_dbi_device(dev);

	of_node_put(dev->of_node);
	kfree(dbi);
}

static const struct device_type mipi_dbi_device_type = {
	.release = mipi_dbi_dev_release,
};

static struct mipi_dbi_device *mipi_dbi_device_alloc(struct mipi_dbi_bus *bus)
{
	struct mipi_dbi_device *dbi;

	dbi = kzalloc(sizeof(*dbi), GFP_KERNEL);
	if (!dbi)
		return ERR_PTR(-ENOMEM);

	dbi->bus = bus;
	dbi->dev.bus = &mipi_dbi_bus_type;
	dbi->dev.parent = bus->dev;
	dbi->dev.type = &mipi_dbi_device_type;

	device_initialize(&dbi->dev);

	return dbi;
}

static int mipi_dbi_device_add(struct mipi_dbi_device *dbi)
{
	struct mipi_dbi_bus *bus = dbi->bus;

	dev_set_name(&dbi->dev, "%s", dev_name(bus->dev));

	return device_add(&dbi->dev);
}

static struct mipi_dbi_device *
of_mipi_dbi_device_add(struct mipi_dbi_bus *bus, struct device_node *node)
{
	struct mipi_dbi_device *dbi;
	struct device *dev = bus->dev;
	int ret;
	const char interface_type[10];
	const char *type = interface_type;

	ret = of_property_read_string(node, "interface_type", &type);
	if (ret) {
		dev_err(dev, "device node %s has no valid property: %d\n",
			node->full_name, ret);
		return ERR_PTR(-EINVAL);
	}

	if (strcmp(type, "mipi-dbi")) {
		dev_err(dev, "device node %s has invalid reg property: %s\n",
			node->full_name, type);
		return ERR_PTR(-EINVAL);
	}

	dbi = mipi_dbi_device_alloc(bus);
	if (IS_ERR(dbi)) {
		dev_err(dev, "failed to allocate dbi device %s: %ld\n",
			node->full_name, PTR_ERR(dbi));
		return dbi;
	}

	dbi->dev.of_node = of_node_get(node);

	ret = mipi_dbi_device_add(dbi);
	if (ret) {
		dev_err(dev, "failed to add dbi device %s: %d\n",
			node->full_name, ret);
		kfree(dbi);
		return ERR_PTR(ret);
	}

	return dbi;
}

int mipi_dbi_host_register(struct mipi_dbi_bus *bus)
{
	struct device_node *node;

	for_each_available_child_of_node(bus->dev->of_node, node) {
		/* skip nodes without reg property */
		if (!of_find_property(node, "interface_type", NULL))
			continue;
		of_mipi_dbi_device_add(bus, node);
	}

	return 0;
}
EXPORT_SYMBOL(mipi_dbi_host_register);

static int mipi_dbi_remove_device_fn(struct device *dev, void *priv)
{
	struct mipi_dbi_device *dbi = to_mipi_dbi_device(dev);

	device_unregister(&dbi->dev);

	return 0;
}

void mipi_dbi_host_unregister(struct mipi_dbi_bus *bus)
{
	device_for_each_child(bus->dev, NULL, mipi_dbi_remove_device_fn);
}
EXPORT_SYMBOL(mipi_dbi_host_unregister);

/**
 * mipi_dbi_attach - attach a DBI device to its DBI host
 * @dbi: DBI peripheral
 */
int mipi_dbi_attach(struct mipi_dbi_device *dbi)
{
	const struct mipi_dbi_bus_ops *ops = dbi->bus->ops;

	if (!ops || !ops->attach)
		return -ENODATA;

	return ops->attach(dbi->bus, dbi);
}
EXPORT_SYMBOL(mipi_dbi_attach);

/**
 * mipi_dbi_detach - detach a DBI device from its DBI host
 * @dbi: DBI peripheral
 */
int mipi_dbi_detach(struct mipi_dbi_device *dbi)
{
	const struct mipi_dbi_bus_ops *ops = dbi->bus->ops;

	if (!ops || !ops->detach)
		return -ENODATA;

	return ops->detach(dbi->bus, dbi);
}
EXPORT_SYMBOL(mipi_dbi_detach);

static ssize_t mipi_dbi_device_transfer(struct mipi_dbi_device *dbi,
					struct mipi_dbi_msg *msg)
{
	const struct mipi_dbi_bus_ops *ops = dbi->bus->ops;

	if (!ops || !ops->transfer)
		return -ENODATA;

	return ops->transfer(dbi->bus, msg);
}

ssize_t mipi_dbi_dcs_write_buffer(struct mipi_dbi_device *dbi,
				  const void *data, size_t len)
{
	struct mipi_dbi_msg msg = {
		.tx_buf = data,
		.tx_len = len
	};

	if (len < 0)
		return -EINVAL;
	else
		return mipi_dbi_device_transfer(dbi, &msg);
}
EXPORT_SYMBOL(mipi_dbi_dcs_write_buffer);

static int mipi_dbi_drv_probe(struct device *dev)
{
	struct mipi_dbi_driver *drv = to_mipi_dbi_driver(dev->driver);
	struct mipi_dbi_device *dbi = to_mipi_dbi_device(dev);

	return drv->probe(dbi);
}

static int mipi_dbi_drv_remove(struct device *dev)
{
	struct mipi_dbi_driver *drv = to_mipi_dbi_driver(dev->driver);
	struct mipi_dbi_device *dbi = to_mipi_dbi_device(dev);

	return drv->remove(dbi);
}

static void mipi_dbi_drv_shutdown(struct device *dev)
{
	struct mipi_dbi_driver *drv = to_mipi_dbi_driver(dev->driver);
	struct mipi_dbi_device *dbi = to_mipi_dbi_device(dev);

	drv->shutdown(dbi);
}

/**
 * mipi_dsi_driver_register_full() - register a driver for DSI devices
 * @drv: DSI driver structure
 * @owner: owner module
 *
 * Return: 0 on success or a negative error code on failure.
 */
int mipi_dbi_driver_register_full(struct mipi_dbi_driver *drv,
				  struct module *owner)
{
	drv->driver.bus = &mipi_dbi_bus_type;
	drv->driver.owner = owner;

	if (drv->probe)
		drv->driver.probe = mipi_dbi_drv_probe;
	if (drv->remove)
		drv->driver.remove = mipi_dbi_drv_remove;
	if (drv->shutdown)
		drv->driver.shutdown = mipi_dbi_drv_shutdown;

	return driver_register(&drv->driver);
}
EXPORT_SYMBOL(mipi_dbi_driver_register_full);

/**
 * mipi_dsi_driver_unregister() - unregister a driver for DSI devices
 * @drv: DSI driver structure
 *
 * Return: 0 on success or a negative error code on failure.
 */
void mipi_dbi_driver_unregister(struct mipi_dbi_driver *drv)
{
	driver_unregister(&drv->driver);
}
EXPORT_SYMBOL(mipi_dbi_driver_unregister);

static int __init mipi_dbi_bus_init(void)
{
	return bus_register(&mipi_dbi_bus_type);
}

postcore_initcall(mipi_dbi_bus_init);

MODULE_AUTHOR("Shaoming Chen <shaoming.chen@mediatek.com>");
MODULE_DESCRIPTION("MIPI DBI Bus");
MODULE_LICENSE("GPL and additional rights");
