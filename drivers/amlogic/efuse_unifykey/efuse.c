// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/clk.h>
#include "efuse.h"
#include <linux/amlogic/efuse.h>
#include <linux/io.h>
#include <linux/amlogic/secmon.h>
#include <linux/amlogic/cpu_info.h>

#include <linux/compat.h>
#define EFUSE_DEVICE_NAME   "efuse"
#define EFUSE_CLASS_NAME    "efuse"

struct aml_efuse_dev {
	struct platform_device *pdev;
	struct class           cls;
	struct cdev            cdev;
	dev_t                  devno;
	char name[EFUSE_CHECK_NAME_LEN];
};

static struct aml_efuse_key efuse_key = {
	.num      = 0,
	.infos    = NULL,
};

struct aml_efuse_cmd efuse_cmd;
void __iomem *sharemem_input_base;
void __iomem *sharemem_output_base;
unsigned int efuse_obj_cmd_status;

struct SonosFuseEntry {
	u32 index;
	const char *name;
	u32 expectedLen;
};

#define SONOS_MAX_FUSE_SIZE 32

static const struct SonosFuseEntry fuseMapping[] = {
	{
		(u32)EFUSE_OBJ_LICENSE_ENABLE_SECURE_BOOT,
		"ENABLE_SECURE_BOOT", 1
	},
	{
		(u32)EFUSE_OBJ_LICENSE_ENABLE_ENCRYPTION,
		"ENABLE_ENCRYPTION", 1
	},
	{
		(u32)EFUSE_OBJ_LICENSE_REVOKE_KPUB_0,
		"REVOKE_KPUB_0", 1
	},
	{
		(u32)EFUSE_OBJ_LICENSE_REVOKE_KPUB_1,
		"REVOKE_KPUB_1", 1
	},
	{
		(u32)EFUSE_OBJ_LICENSE_REVOKE_KPUB_2,
		"REVOKE_KPUB_2", 1
	},
	{
		(u32)EFUSE_OBJ_LICENSE_REVOKE_KPUB_3,
		"REVOKE_KPUB_3", 1
	},
	{
		(u32)EFUSE_OBJ_LICENSE_ENABLE_ANTIROLLBACK,
		"ENABLE_ANTIROLLBACK", 1
	},
	{
		(u32)EFUSE_OBJ_LICENSE_ENABLE_JTAG_PASSWORD,
		"ENABLE_JTAG_PASSWORD", 1
	},
	{
		(u32)EFUSE_OBJ_LICENSE_ENABLE_SCAN_PASSWORD,
		"ENABLE_SCAN_PASSWORD", 1
	},
	{
		(u32)EFUSE_OBJ_LICENSE_DISABLE_JTAG,
		"DISABLE_JTAG", 1
	},
	{
		(u32)EFUSE_OBJ_LICENSE_DISABLE_SCAN,
		"DISABLE_SCAN", 1
	},
	{
		(u32)EFUSE_OBJ_LICENSE_ENABLE_USB_BOOT_PASSWORD,
		"ENABLE_USB_BOOT_PASSWORD", 1
	},
	{
		(u32)EFUSE_OBJ_LICENSE_DISABLE_USB_BOOT,
		"DISABLE_USB_BOOT", 1
	},
#if defined(SONOS_ARCH_ATTR_SOC_IS_S767)
	{
		(u32)EFUSE_OBJ_LICENSE_ENABLE_M3_SECURE_BOOT,
		"ENABLE_M3_SECURE_BOOT", 1
	},
	{
		(u32)EFUSE_OBJ_LICENSE_ENABLE_M3_ENCRYPTION,
		"ENABLE_M3_ENCRYPTION", 1
	},
	{
		(u32)EFUSE_OBJ_LICENSE_ENABLE_M4_SECURE_BOOT,
		"ENABLE_M4_SECURE_BOOT", 1
	},
	{
		(u32)EFUSE_OBJ_LICENSE_ENABLE_M4_ENCRYPTION,
		"ENABLE_M4_ENCRYPTION", 1
	},
	{
		(u32)EFUSE_OBJ_LICENSE_REVOKE_M4_KPUB_0,
		"REVOKE_M4_KPUB_0", 1
	},
	{
		(u32)EFUSE_OBJ_LICENSE_REVOKE_M4_KPUB_1,
		"REVOKE_M4_KPUB_1", 1
	},
	{
		(u32)EFUSE_OBJ_LICENSE_ENABLE_M4_JTAG_PASSWORD,
		"ENABLE_M4_JTAG_PASSWORD", 1
	},
	{
		(u32)EFUSE_OBJ_LICENSE_DISABLE_M3_JTAG,
		"DISABLE_M3_JTAG", 1
	},
	{
		(u32)EFUSE_OBJ_LICENSE_DISABLE_M4_JTAG,
		"DISABLE_M4_JTAG", 1
	},
#endif
	{
		(u32)EFUSE_OBJ_SBOOT_KPUB_SHA,
		"SBOOT_KPUB_SHA", 32
	},
	{
		(u32)EFUSE_OBJ_SBOOT_AES256_SHA2,
		"SBOOT_AES256_SHA2", 32
	},
	{
		(u32)EFUSE_OBJ_AUDIO_CUSTOMER_ID,
		"AUDIO_CUSTOMER_ID", 4
	},
#if defined(SONOS_ARCH_ATTR_SOC_IS_S767)
	{
		(u32)EFUSE_OBJ_M4_SBOOT_AES256_SHA2,
		"M4_SBOOT_AES256_SHA2", 32
	},
#endif
	{
		(u32)EFUSE_OBJ_GP_REE,
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

int efuse_getinfo(char *item, struct efusekey_info *info)
{
	int i;
	int ret = -EINVAL;

	for (i = 0; i < efuse_key.num; i++) {
		if (strcmp(efuse_key.infos[i].keyname, item) == 0) {
			strcpy(info->keyname, efuse_key.infos[i].keyname);
			info->offset = efuse_key.infos[i].offset;
			info->size = efuse_key.infos[i].size;
			ret = 0;
			break;
		}
	}
	if (ret < 0)
		pr_err("%s item not found.\n", item);
	return ret;
}

ssize_t efuse_user_attr_store(char *name, const char *buf, size_t count)
{
#ifndef EFUSE_READ_ONLY
	char *op = NULL;
	ssize_t ret;
	int i;
	const char *c, *s;
	struct efusekey_info info;
	unsigned int uint_val;
	loff_t pos;

	if (efuse_getinfo(name, &info) < 0) {
		ret = -EINVAL;
		pr_err("efuse: %s is not found\n", name);
		goto exit;
	}

	op = kzalloc(sizeof(char) * count, GFP_KERNEL);
	if (!op) {
		ret = -ENOMEM;
		pr_err("efuse: failed to allocate memory\n");
		goto exit;
	}

	memset(op, 0, count);
	memcpy(op, buf, count);

	c = ":";
	s = op;
	if (strstr(s, c)) {
		for (i = 0; i < info.size; i++) {
			uint_val = 0;
			if (i != info.size - 1) {
				ret = sscanf(s, "%x:", &uint_val);
				if (ret != 1  || uint_val > 0xff)
					ret = -EINVAL;
			} else {
				ret = kstrtou8(s, 16,
					       (unsigned char *)&uint_val);
			}

			if (ret < 0) {
				kfree(op);
				pr_err("efuse: get key input data fail\n");
				goto exit;
			}

			op[i] = uint_val;

			s += 2;
			if (!strncmp(s, c, 1))
				s++;
		}
	} else if ((op[count - 1] != 0x0A && count != info.size) ||
		   count - 1 > info.size || count < info.size) {
		kfree(op);
		ret = -EINVAL;
		pr_err("efuse: key data length not match\n");
		goto exit;
	}

	pos = ((loff_t)(info.offset)) & 0xffffffff;
	ret = efuse_write_usr(op, info.size, &pos);
	kfree(op);

	if (ret < 0) {
		pr_err("efuse: write user area %d bytes data fail\n",
		       (unsigned int)info.size);
		goto exit;
	}

	pr_info("efuse: write user area %d bytes data OK\n",
		(unsigned int)ret);

	ret = count;

exit:
	return ret;
#else
	pr_err("no permission to write!!\n");
	return -EPERM;
#endif
}
EXPORT_SYMBOL(efuse_user_attr_store);

ssize_t efuse_user_attr_read(char *name, char *buf)
{
	char *op = NULL;
	ssize_t ret;
	struct efusekey_info info;
	loff_t pos;

	if (efuse_getinfo(name, &info) < 0) {
		ret = -EINVAL;
		pr_err("efuse: %s is not found\n", name);
		goto exit;
	}

	op = kzalloc(sizeof(char) * info.size, GFP_KERNEL);
	if (!op) {
		ret = -ENOMEM;
		pr_err("efuse: failed to allocate memory\n");
		goto exit;
	}

	memset(op, 0, info.size);

	pos = ((loff_t)(info.offset)) & 0xffffffff;
	ret = efuse_read_usr(op, info.size, &pos);
	if (ret < 0) {
		kfree(op);
		pr_err("efuse: read user data fail!\n");
		goto exit;
	}

	memcpy(buf, op, info.size);
	kfree(op);

	ret = (ssize_t)info.size;

exit:
	return ret;
}

static int key_item_parse_dt(const struct device_node *np_efusekey,
			     int index, struct efusekey_info *infos)
{
	const phandle *phandle;
	struct device_node *np_key;
	char *propname;
	const char *keyname;
	int ret;
	int name_size;

	propname = kasprintf(GFP_KERNEL, "key%d", index);

	phandle = of_get_property(np_efusekey, propname, NULL);
	if (!phandle) {
		ret = -EINVAL;
		pr_err("failed to find match %s\n", propname);
		goto exit;
	}
	np_key = of_find_node_by_phandle(be32_to_cpup(phandle));
	if (!np_key) {
		ret = -EINVAL;
		pr_err("failed to find device node %s\n", propname);
		goto exit;
	}

	ret = of_property_read_string(np_key, "keyname", &keyname);
	if (ret < 0) {
		pr_err("failed to get keyname item\n");
		goto exit;
	}

	name_size = EFUSE_KEY_NAME_LEN - 1;
	memcpy(infos[index].keyname, keyname,
	       strlen(keyname) > name_size ? name_size : strlen(keyname));

	ret = of_property_read_u32(np_key, "offset",
				   &infos[index].offset);
	if (ret < 0) {
		pr_err("failed to get key offset item\n");
		goto exit;
	}

	ret = of_property_read_u32(np_key, "size",
				   &infos[index].size);
	if (ret < 0) {
		pr_err("failed to get key size item\n");
		goto exit;
	}

	pr_debug("efusekey name: %15s\toffset: %5d\tsize: %5d\n",
		infos[index].keyname,
		infos[index].offset,
		infos[index].size);

	ret = 0;

exit:
	kfree(propname);
	return ret;
}

static int get_efusekey_info(struct device_node *np)
{
	const phandle *phandle;
	struct device_node *np_efusekey = NULL;
	int index;
	int ret;

	phandle = of_get_property(np, "key", NULL);
	if (!phandle) {
		ret = -EINVAL;
		pr_err("failed to find match efuse key\n");
		goto exit;
	}
	np_efusekey = of_find_node_by_phandle(be32_to_cpup(phandle));
	if (!np_efusekey) {
		ret = -EINVAL;
		pr_err("failed to find device node efusekey\n");
		goto exit;
	}

	ret = of_property_read_u32(np_efusekey, "keynum", &efuse_key.num);
	if (ret < 0) {
		pr_err("failed to get efusekey num item\n");
		goto exit;
	}

	if (efuse_key.num <= 0) {
		ret = -EINVAL;
		pr_err("efusekey num config error\n");
		goto exit;
	}
	pr_debug("efusekey num: %d\n", efuse_key.num);

	efuse_key.infos = kzalloc((sizeof(struct efusekey_info))
		* efuse_key.num, GFP_KERNEL);
	if (!efuse_key.infos) {
		ret = -ENOMEM;
		pr_err("fail to alloc enough mem for efusekey_infos\n");
		goto exit;
	}

	for (index = 0; index < efuse_key.num; index++) {
		ret = key_item_parse_dt(np_efusekey, index, efuse_key.infos);
		if (ret < 0) {
			kfree(efuse_key.infos);
			goto exit;
		}
	}

	return 0;

exit:
	return ret;
}

static ssize_t efuse_obj_store(struct class *cla,
		struct class_attribute *attr, const char *buf, size_t count)
{
	const struct SonosFuseEntry *fe;
	int rc;
	u32 len;

	fe = sonosFuseGetEntry(attr->attr.name);
	if (!fe || fe->expectedLen != count || !buf) {
		return -EINVAL;
	}

	len = fe->expectedLen;
	rc = meson_efuse_obj_write_sonos(fe->index, (char *)buf, &len);

	if (rc == EFUSE_OBJ_SUCCESS) {
		return count;
	} else {
		return -EINVAL;
	}
}

static ssize_t efuse_obj_show(struct class *cla, struct class_attribute *attr, char *buf)
{
	unsigned char read_buf[SONOS_MAX_FUSE_SIZE];
	const struct SonosFuseEntry *fe;
	int rc;
	u32 len;

	fe = sonosFuseGetEntry(attr->attr.name);
	if (!fe || !buf) {
		return -EINVAL;
	}

	len = fe->expectedLen;
	rc = meson_efuse_obj_read_sonos(fe->index, read_buf, &len);

	if (rc == EFUSE_OBJ_SUCCESS && len == fe->expectedLen) {
		memcpy(buf, read_buf, len);
		return len;
	} else {
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

#define EFUSE_OBJ_CLASS_ATTR_RO(_name) \
	struct class_attribute class_attr_##_name = __ATTR(_name, 0444, efuse_obj_show, NULL)

#define EFUSE_OBJ_CLASS_ATTR_RW(_name) \
	struct class_attribute class_attr_##_name = __ATTR(_name, 0644, efuse_obj_show, efuse_obj_store)

static EFUSE_OBJ_CLASS_ATTR_RO(ENABLE_SECURE_BOOT);
static EFUSE_OBJ_CLASS_ATTR_RO(ENABLE_ENCRYPTION);
static EFUSE_OBJ_CLASS_ATTR_RO(REVOKE_KPUB_0);
static EFUSE_OBJ_CLASS_ATTR_RO(REVOKE_KPUB_1);
static EFUSE_OBJ_CLASS_ATTR_RO(REVOKE_KPUB_2);
static EFUSE_OBJ_CLASS_ATTR_RO(REVOKE_KPUB_3);
static EFUSE_OBJ_CLASS_ATTR_RO(ENABLE_ANTIROLLBACK);
static EFUSE_OBJ_CLASS_ATTR_RO(ENABLE_JTAG_PASSWORD);
static EFUSE_OBJ_CLASS_ATTR_RO(ENABLE_SCAN_PASSWORD);
static EFUSE_OBJ_CLASS_ATTR_RO(DISABLE_JTAG);
static EFUSE_OBJ_CLASS_ATTR_RO(DISABLE_SCAN);
static EFUSE_OBJ_CLASS_ATTR_RO(ENABLE_USB_BOOT_PASSWORD);
static EFUSE_OBJ_CLASS_ATTR_RO(DISABLE_USB_BOOT);
static EFUSE_OBJ_CLASS_ATTR_RO(SBOOT_KPUB_SHA);
static EFUSE_OBJ_CLASS_ATTR_RO(SBOOT_AES256_SHA2);
static EFUSE_OBJ_CLASS_ATTR_RW(GP_REE);
static EFUSE_OBJ_CLASS_ATTR_RO(AUDIO_CUSTOMER_ID);
static struct class_attribute class_attr_CPUID =  __ATTR(CPUID, 0444, cpuid_show, NULL);
#if defined(SONOS_ARCH_ATTR_SOC_IS_S767)
static EFUSE_OBJ_CLASS_ATTR_RO(ENABLE_M3_SECURE_BOOT);
static EFUSE_OBJ_CLASS_ATTR_RO(ENABLE_M3_ENCRYPTION);
static EFUSE_OBJ_CLASS_ATTR_RO(ENABLE_M4_SECURE_BOOT);
static EFUSE_OBJ_CLASS_ATTR_RO(ENABLE_M4_ENCRYPTION);
static EFUSE_OBJ_CLASS_ATTR_RO(REVOKE_M4_KPUB_0);
static EFUSE_OBJ_CLASS_ATTR_RO(REVOKE_M4_KPUB_1);
static EFUSE_OBJ_CLASS_ATTR_RO(ENABLE_M4_JTAG_PASSWORD);
static EFUSE_OBJ_CLASS_ATTR_RO(DISABLE_M3_JTAG);
static EFUSE_OBJ_CLASS_ATTR_RO(DISABLE_M4_JTAG);
static EFUSE_OBJ_CLASS_ATTR_RO(M4_SBOOT_AES256_SHA2);
#endif

static struct attribute *efuse_class_attrs[] = {
	&class_attr_ENABLE_SECURE_BOOT.attr,
	&class_attr_ENABLE_ENCRYPTION.attr,
	&class_attr_REVOKE_KPUB_0.attr,
	&class_attr_REVOKE_KPUB_1.attr,
	&class_attr_REVOKE_KPUB_2.attr,
	&class_attr_REVOKE_KPUB_3.attr,
	&class_attr_ENABLE_ANTIROLLBACK.attr,
	&class_attr_ENABLE_JTAG_PASSWORD.attr,
	&class_attr_ENABLE_SCAN_PASSWORD.attr,
	&class_attr_DISABLE_JTAG.attr,
	&class_attr_DISABLE_SCAN.attr,
	&class_attr_ENABLE_USB_BOOT_PASSWORD.attr,
	&class_attr_DISABLE_USB_BOOT.attr,
	&class_attr_SBOOT_KPUB_SHA.attr,
	&class_attr_SBOOT_AES256_SHA2.attr,
	&class_attr_GP_REE.attr,
	&class_attr_AUDIO_CUSTOMER_ID.attr,
	&class_attr_CPUID.attr,
#if defined(SONOS_ARCH_ATTR_SOC_IS_S767)
	&class_attr_ENABLE_M3_SECURE_BOOT.attr,
	&class_attr_ENABLE_M3_ENCRYPTION.attr,
	&class_attr_ENABLE_M4_SECURE_BOOT.attr,
	&class_attr_ENABLE_M4_ENCRYPTION.attr,
	&class_attr_REVOKE_M4_KPUB_0.attr,
	&class_attr_REVOKE_M4_KPUB_1.attr,
	&class_attr_ENABLE_M4_JTAG_PASSWORD.attr,
	&class_attr_DISABLE_M3_JTAG.attr,
	&class_attr_DISABLE_M4_JTAG.attr,
	&class_attr_M4_SBOOT_AES256_SHA2.attr,
#endif
	NULL
};
ATTRIBUTE_GROUPS(efuse_class);

static int efuse_probe(struct platform_device *pdev)
{
	int ret;
	struct device *devp;
	struct device_node *np = pdev->dev.of_node;
	struct clk *efuse_clk;
	struct aml_efuse_dev *efuse_dev;

	efuse_dev = devm_kzalloc(&pdev->dev, sizeof(*efuse_dev), GFP_KERNEL);
	if (!efuse_dev) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "failed to alloc enough mem for efuse_dev\n");
		goto out;
	}

	efuse_dev->pdev = pdev;
	platform_set_drvdata(pdev, efuse_dev);

	efuse_clk = devm_clk_get(&pdev->dev, "efuse_clk");
	if (IS_ERR(efuse_clk)) {
		dev_err(&pdev->dev, "can't get efuse clk gate, use default clk\n");
	} else {
		ret = clk_prepare_enable(efuse_clk);
		if (ret) {
			dev_err(&pdev->dev, "failed to enable efuse clk gate\n");
			goto error1;
		}
	}

	of_node_get(np);

	ret = of_property_read_u32(np, "read_cmd", &efuse_cmd.read_cmd);
	if (ret) {
		dev_err(&pdev->dev, "please config read_cmd\n");
		goto error1;
	}

	ret = of_property_read_u32(np, "write_cmd", &efuse_cmd.write_cmd);
	if (ret) {
		dev_err(&pdev->dev, "please config write_cmd\n");
		goto error1;
	}

	ret = of_property_read_u32(np, "read_obj_cmd", &efuse_cmd.read_obj_cmd);
	if (ret) {
		dev_err(&pdev->dev, "please config read_obj\n");
		goto error1;
	}

	ret = of_property_read_u32(np, "write_obj_cmd", &efuse_cmd.write_obj_cmd);
	if (ret) {
		dev_err(&pdev->dev, "please config write_obj_cmd\n");
		goto error1;
	}

	ret = of_property_read_u32(np, "get_max_cmd", &efuse_cmd.get_max_cmd);
	if (ret) {
		dev_err(&pdev->dev, "please config get_max_cmd\n");
		goto error1;
	}

	ret = of_property_read_u32(np, "mem_in_base_cmd",
				   &efuse_cmd.mem_in_base_cmd);
	if (ret) {
		dev_err(&pdev->dev, "please config to mem_in_base_cmd\n");
		goto error1;
	}

	ret = of_property_read_u32(np, "mem_out_base_cmd",
				   &efuse_cmd.mem_out_base_cmd);
	if (ret) {
		dev_err(&pdev->dev, "please config mem_out_base_cmd\n");
		goto error1;
	}

	ret = of_property_read_u32(np, "efuse_obj_cmd_status", &efuse_obj_cmd_status);
	if (ret)
		efuse_obj_cmd_status = 0;

	get_efusekey_info(np);

	ret = alloc_chrdev_region(&efuse_dev->devno, 0, 1, EFUSE_DEVICE_NAME);
	if (ret < 0) {
		dev_err(&pdev->dev, "fail to allocate major number\n ");
		goto error1;
	}

	efuse_dev->cls.name = EFUSE_CLASS_NAME;
	efuse_dev->cls.owner = THIS_MODULE;
	efuse_dev->cls.class_groups = efuse_class_groups;
	ret = class_register(&efuse_dev->cls);
	if (ret)
		goto error2;

	efuse_dev->cdev.owner = THIS_MODULE;

	ret = cdev_add(&efuse_dev->cdev, efuse_dev->devno, 1);
	if (ret) {
		dev_err(&pdev->dev, "failed to add device\n");
		goto error3;
	}

	devp = device_create(&efuse_dev->cls, NULL,
			     efuse_dev->devno, NULL, "efuse");
	if (IS_ERR(devp)) {
		dev_err(&pdev->dev, "failed to create device node\n");
		ret = PTR_ERR(devp);
		goto error4;
	}

	sharemem_input_base = get_meson_sm_input_base();
	sharemem_output_base = get_meson_sm_output_base();

	dev_info(&pdev->dev, "device %s created OK\n", EFUSE_DEVICE_NAME);
	return 0;

error4:
	cdev_del(&efuse_dev->cdev);
error3:
	class_unregister(&efuse_dev->cls);
error2:
	unregister_chrdev_region(efuse_dev->devno, 1);
error1:
	devm_kfree(&pdev->dev, efuse_dev);
out:
	return ret;
}

static int efuse_remove(struct platform_device *pdev)
{
	struct aml_efuse_dev *efuse_dev = platform_get_drvdata(pdev);

	unregister_chrdev_region(efuse_dev->devno, 1);
	device_destroy(&efuse_dev->cls, efuse_dev->devno);
	cdev_del(&efuse_dev->cdev);
	class_unregister(&efuse_dev->cls);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id efuse_dt_match[] = {
	{	.compatible = "amlogic, efuse",
	},
	{},
};

static struct platform_driver efuse_driver = {
	.probe = efuse_probe,
	.remove = efuse_remove,
	.driver = {
		.name = EFUSE_DEVICE_NAME,
		.of_match_table = efuse_dt_match,
	.owner = THIS_MODULE,
	},
};

int __init aml_efuse_init(void)
{
	return platform_driver_register(&efuse_driver);
}

void aml_efuse_exit(void)
{
	platform_driver_unregister(&efuse_driver);
}
