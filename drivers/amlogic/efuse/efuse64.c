/*
 * drivers/amlogic/efuse/efuse64.c
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

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>

#include <linux/amlogic/secmon.h>
#include "efuse.h"
#ifdef CONFIG_ARM64
#include <linux/amlogic/efuse.h>
#endif

#include <linux/amlogic/cpu_version.h>

#define EFUSE_MODULE_NAME   "efuse"
#define EFUSE_DRIVER_NAME	"efuse"
#define EFUSE_DEVICE_NAME   "efuse"
#define EFUSE_CLASS_NAME    "efuse"
#define EFUSE_IS_OPEN           (0x01)

struct efusekey_info *efusekey_infos;
int efusekeynum =  -1;

struct efuse_dev_t {
	struct cdev         cdev;
	unsigned int        flags;
};

/* static struct class *efuse_clsp; */
static dev_t efuse_devno;

void __iomem *sharemem_input_base;
void __iomem *sharemem_output_base;
unsigned int efuse_read_cmd;
unsigned int efuse_write_cmd;
unsigned int efuse_read_obj_cmd;
unsigned int efuse_write_obj_cmd;
unsigned int efuse_get_max_cmd;


struct SonosFuseEntry {
	uint32_t index;
	const char *name;
	uint32_t expectedLen;
};

#define SONOS_MAX_FUSE_SIZE 32

static const struct SonosFuseEntry fuseMapping[] =
{
	{
		(uint32_t)EFUSE_OBJ_LICENSE_ENABLE_SECURE_BOOT,
		"ENABLE_SECURE_BOOT", 1
	},
	{
		(uint32_t)EFUSE_OBJ_LICENSE_ENABLE_ENCRYPTION,
		"ENABLE_ENCRYPTION", 1
	},
	{
		(uint32_t)EFUSE_OBJ_LICENSE_REVOKE_KPUB_0,
		"REVOKE_KPUB_0", 1
	},
	{
		(uint32_t)EFUSE_OBJ_LICENSE_REVOKE_KPUB_1,
		"REVOKE_KPUB_1", 1
	},
	{
		(uint32_t)EFUSE_OBJ_LICENSE_REVOKE_KPUB_2,
		"REVOKE_KPUB_2", 1
	},
	{
		(uint32_t)EFUSE_OBJ_LICENSE_REVOKE_KPUB_3,
		"REVOKE_KPUB_3", 1
	},
	{
		(uint32_t)EFUSE_OBJ_LICENSE_ENABLE_ANTIROLLBACK,
		"ENABLE_ANTIROLLBACK", 1
	},
	{
		(uint32_t)EFUSE_OBJ_LICENSE_ENABLE_JTAG_PASSWORD,
		"ENABLE_JTAG_PASSWORD", 1
	},
	{
		(uint32_t)EFUSE_OBJ_LICENSE_ENABLE_SCAN_PASSWORD,
		"ENABLE_SCAN_PASSWORD", 1
	},
	{
		(uint32_t)EFUSE_OBJ_LICENSE_DISABLE_JTAG,
		"DISABLE_JTAG", 1
	},
	{
		(uint32_t)EFUSE_OBJ_LICENSE_DISABLE_SCAN,
		"DISABLE_SCAN", 1
	},
	{
		(uint32_t)EFUSE_OBJ_LICENSE_ENABLE_USB_BOOT_PASSWORD,
		"ENABLE_USB_BOOT_PASSWORD", 1
	},
	{
		(uint32_t)EFUSE_OBJ_LICENSE_DISABLE_USB_BOOT,
		"DISABLE_USB_BOOT", 1
	},
	{
		(uint32_t)EFUSE_OBJ_SBOOT_KPUB_SHA,
		"SBOOT_KPUB_SHA", 32
	},
	{
		(uint32_t)EFUSE_OBJ_JTAG_PASSWD_SHA_SALT,
		"JTAG_PASSWD_SHA_SALT", 32
	},
	{
		(uint32_t)EFUSE_OBJ_SCAN_PASSWD_SHA_SALT,
		"SCAN_PASSWD_SHA_SALT", 32
	},
	{
		(uint32_t)EFUSE_OBJ_SBOOT_AES256_SHA2,
		"SBOOT_AES256_SHA2", 32
	},
	{
		/*
		 * fixidn TODO: integrate Amlogic patch that defines
		 *              EFUSE_OBJ_AUDIO_CUSTOMER_ID
		 *
		 * The current expression here is ugly but is also
		 * correct and stable (this value can't change).
		 */
		((uint32_t)EFUSE_OBJ_SBOOT_AES256_SHA2) + 1,
		"AUDIO_CUSTOMER_ID", 4
	},
	{
		(uint32_t)EFUSE_OBJ_GP_REE,
		"GP_REE", 16
	}
};

const struct SonosFuseEntry *sonosFuseGetEntry(const char *name)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(fuseMapping); i++) {
		if (strcmp(name, fuseMapping[i].name) == 0) {
			return &fuseMapping[i];
		}
	}
	return NULL;
}

static int meson_efuse_obj_rw_fn(uint32_t cmd, uint32_t obj_id, uint8_t *buff, uint32_t *size)
{
	uint32_t rc = EFUSE_OBJ_ERR_UNKNOWN;
	uint32_t len = 0;

	register uint64_t x0 asm("x0");
	register uint64_t x1 asm("x1");
	register uint64_t x2 asm("x2");

	if (!sharemem_input_base || !sharemem_output_base || !buff || !size) {
		return -1;
	}

	/* pr_info("%s cmd:0x%x, obj_id:0x%x[%d] size:%d\n", __func__, cmd, obj_id, obj_id, *size); */

	sharemem_mutex_lock();
	if (cmd == efuse_write_obj_cmd) {
		if (obj_id == EFUSE_OBJ_GP_REE && buff && *size == 16) {
			memcpy((void *)sharemem_input_base, buff, *size);
			asm __volatile__("" : : : "memory");
		}
		else {
			sharemem_mutex_unlock();
			return -EFUSE_OBJ_ERR_INVALID_DATA;
		}
	}

	x0 = cmd;
	x1 = obj_id;
	x2 = *size;

	do {
		asm volatile(
				__asmeq("%0", "x0")
				__asmeq("%1", "x1")
				__asmeq("%2", "x0")
				__asmeq("%3", "x1")
				__asmeq("%4", "x2")
				"smc    #0\n"
				: "+r" (x0), "+r" (x1)
				: "r" (x0), "r" (x1), "r" (x2));
	} while (0);

	rc = x0;

	if (cmd == efuse_read_obj_cmd) {
		len = x1;
		/* pr_info("%s read rc:%d len:%d\n", __func__, rc, len); */
		if (rc == EFUSE_OBJ_SUCCESS) {
			if (*size >= len) {
				memcpy(buff, (const void *)sharemem_output_base, len);
				*size = len;
			}
			else {
				rc = -EFUSE_OBJ_ERR_SIZE;
			}
		}
		else {
			rc = -rc;
		}
	}
	else {
		/* pr_info("%s write rc:%d\n", __func__, rc); */
		if (rc != EFUSE_OBJ_SUCCESS) {
			rc = -rc;
		}
	}
	sharemem_mutex_unlock();

	return rc;
}

int meson_efuse_obj_read(uint32_t obj_id, uint8_t *buff, uint32_t *size)
{
	int rc = meson_efuse_obj_rw_fn(efuse_read_obj_cmd, obj_id, buff, size);
	if (rc != EFUSE_OBJ_SUCCESS) {
		pr_err("%s failed:%d\n", __func__, rc);
	}
	return rc;
}

int meson_efuse_obj_write(uint32_t obj_id, uint8_t *buff, uint32_t *size)
{
	int rc = meson_efuse_obj_rw_fn(efuse_write_obj_cmd, obj_id, buff, size);
	if (rc != EFUSE_OBJ_SUCCESS) {
		pr_err("%s failed:%d\n", __func__, rc);
	}
	return rc;
}

static ssize_t efuse_obj_store(struct class *cla,
		struct class_attribute *attr, const char *buf, size_t count)
{
	const struct SonosFuseEntry *fe;
	int rc;
	uint32_t len;

	fe = sonosFuseGetEntry(attr->attr.name);
	if (!fe || fe->expectedLen != count || !buf) {
		return -EINVAL;
	}

	len = fe->expectedLen;
	rc = meson_efuse_obj_write(fe->index, (char *)buf, &len);

	if (rc == EFUSE_OBJ_SUCCESS) {
		return count;
	}
	else {
		return -EINVAL;
	}
}

static ssize_t efuse_obj_show(struct class *cla, struct class_attribute *attr, char *buf)
{
        unsigned char read_buf[SONOS_MAX_FUSE_SIZE];
	const struct SonosFuseEntry *fe;
	int rc;
	uint32_t len;

	fe = sonosFuseGetEntry(attr->attr.name);
	if (!fe || !buf) {
		return -EINVAL;
	}

	len = fe->expectedLen;
	rc = meson_efuse_obj_read(fe->index, read_buf, &len);

	if (rc == EFUSE_OBJ_SUCCESS && len == fe->expectedLen) {
		memcpy(buf, read_buf, len);
		return len;
	}
	else {
		return -EINVAL;
	}
}

// "fake" fuse, show it in /sys/class/efuse with the real fuses
static ssize_t cpuid_show(struct class *cla, struct class_attribute *attr, char *buf)
{
	if (!buf) {
		return -EINVAL;
	}

	cpuinfo_get_chipid(buf, CHIPID_LEN);
	return CHIPID_LEN;
}

static struct class_attribute efuse_class_attrs[] = {
	__ATTR(ENABLE_SECURE_BOOT, S_IRUGO, efuse_obj_show, NULL),
	__ATTR(ENABLE_ENCRYPTION, S_IRUGO, efuse_obj_show, NULL),
	__ATTR(REVOKE_KPUB_0, S_IRUGO, efuse_obj_show, NULL),
	__ATTR(REVOKE_KPUB_1, S_IRUGO, efuse_obj_show, NULL),
	__ATTR(REVOKE_KPUB_2, S_IRUGO, efuse_obj_show, NULL),
	__ATTR(REVOKE_KPUB_3, S_IRUGO, efuse_obj_show, NULL),
	__ATTR(ENABLE_ANTIROLLBACK, S_IRUGO, efuse_obj_show, NULL),
	__ATTR(ENABLE_JTAG_PASSWORD, S_IRUGO, efuse_obj_show, NULL),
	__ATTR(ENABLE_SCAN_PASSWORD, S_IRUGO, efuse_obj_show, NULL),
	__ATTR(DISABLE_JTAG, S_IRUGO, efuse_obj_show, NULL),
	__ATTR(DISABLE_SCAN, S_IRUGO, efuse_obj_show, NULL),
	__ATTR(ENABLE_USB_BOOT_PASSWORD, S_IRUGO, efuse_obj_show, NULL),
	__ATTR(DISABLE_USB_BOOT, S_IRUGO, efuse_obj_show, NULL),
	__ATTR(SBOOT_KPUB_SHA, S_IRUGO, efuse_obj_show, NULL),
	__ATTR(JTAG_PASSWD_SHA_SALT, S_IRUGO, efuse_obj_show, NULL),
	__ATTR(SCAN_PASSWD_SHA_SALT, S_IRUGO, efuse_obj_show, NULL),
	__ATTR(SBOOT_AES256_SHA2, S_IRUGO, efuse_obj_show, NULL),
	__ATTR(GP_REE, S_IWUSR | S_IRUGO, efuse_obj_show, efuse_obj_store),
	__ATTR(AUDIO_CUSTOMER_ID, S_IRUGO, efuse_obj_show, NULL),
	__ATTR(CPUID, S_IRUGO, cpuid_show, NULL),
	__ATTR_NULL

};

static struct class efuse_class = {

	.name = EFUSE_CLASS_NAME,

	.class_attrs = efuse_class_attrs,

};

static int efuse_probe(struct platform_device *pdev)
{
	int ret;
	struct device *devp;
	struct device_node *np = pdev->dev.of_node;
	struct clk *efuse_clk;

	/* open clk gate HHI_GCLK_MPEG0 bit62*/
	efuse_clk = devm_clk_get(&pdev->dev, "efuse_clk");
	if (IS_ERR(efuse_clk))
		dev_err(&pdev->dev, " open efuse clk gate error!!\n");
	else{
		ret = clk_prepare_enable(efuse_clk);
		if (ret)
			dev_err(&pdev->dev, "enable efuse clk gate error!!\n");
	}

	if (pdev->dev.of_node) {
		of_node_get(np);

		ret = of_property_read_u32(np, "read_cmd", &efuse_read_cmd);
		if (ret) {
			dev_err(&pdev->dev, "please config read_cmd item\n");
			return -1;
		}

		ret = of_property_read_u32(np, "write_cmd", &efuse_write_cmd);
		if (ret) {
			dev_err(&pdev->dev, "please config write_cmd item\n");
			return -1;
		}

		ret = of_property_read_u32(np, "read_obj_cmd", &efuse_read_obj_cmd);
		if (ret) {
			dev_err(&pdev->dev, "please config read_obj_cmd item\n");
			efuse_read_obj_cmd = 0;;
		}

		ret = of_property_read_u32(np, "write_obj_cmd", &efuse_write_obj_cmd);
		if (ret) {
			dev_err(&pdev->dev, "please config write_obj_cmd item\n");
			efuse_write_obj_cmd = 0;
		}

	  ret = of_property_read_u32(np, "get_max_cmd", &efuse_get_max_cmd);
		if (ret) {
			dev_err(&pdev->dev, "please config get_max_cmd\n");
			return -1;
		}

	}

	ret = alloc_chrdev_region(&efuse_devno, 0, 1, EFUSE_DEVICE_NAME);
	if (ret < 0) {
		ret = -ENODEV;
		goto out;
	}

	ret = class_register(&efuse_class);
	if (ret)
		goto error1;

	devp = device_create(&efuse_class, NULL, efuse_devno, NULL, "efuse");
	if (IS_ERR(devp)) {
		dev_err(&pdev->dev, "failed to create device node\n");
		ret = PTR_ERR(devp);
		goto error2;
	}
	dev_dbg(&pdev->dev, "device %s created\n", EFUSE_DEVICE_NAME);

	sharemem_input_base = get_secmon_sharemem_input_base();
	sharemem_output_base = get_secmon_sharemem_output_base();
	dev_info(&pdev->dev, "probe OK!\n");
	return 0;

error2:
	/* class_destroy(efuse_clsp); */
	class_unregister(&efuse_class);
error1:
	unregister_chrdev_region(efuse_devno, 1);
out:
	return ret;
}

static int efuse_remove(struct platform_device *pdev)
{
	unregister_chrdev_region(efuse_devno, 1);
	/* device_destroy(efuse_clsp, efuse_devno); */
	device_destroy(&efuse_class, efuse_devno);
	/* class_destroy(efuse_clsp); */
	class_unregister(&efuse_class);
	return 0;
}

static const struct of_device_id amlogic_efuse_dt_match[] = {
	{	.compatible = "amlogic, efuse",
	},
	{},
};

static struct platform_driver efuse_driver = {
	.probe = efuse_probe,
	.remove = efuse_remove,
	.driver = {
		.name = EFUSE_DEVICE_NAME,
		.of_match_table = amlogic_efuse_dt_match,
	.owner = THIS_MODULE,
	},
};

static int __init efuse_init(void)
{
	int ret = -1;

	ret = platform_driver_register(&efuse_driver);
	if (ret != 0) {
		pr_err("failed to register efuse driver, error %d\n", ret);
		return -ENODEV;
	}

	return ret;
}

static void __exit efuse_exit(void)
{
	platform_driver_unregister(&efuse_driver);
}

module_init(efuse_init);
module_exit(efuse_exit);

MODULE_DESCRIPTION("AMLOGIC eFuse driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Cai Yun <yun.cai@amlogic.com>");

