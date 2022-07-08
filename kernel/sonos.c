/*
 * Copyright (c) 2014-2020, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <linux/module.h>
#include "mdp.h"
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/of_fdt.h>
#include <linux/sonos_kernel.h>
#include "sect_upgrade_header.h"
#include <asm-generic/early_ioremap.h>
#include <linux/netdevice.h>
#include <net/netlink.h>
#include <linux/rtnetlink.h>

/* This function provide a common API for ADC voltage read */
int read_adc_voltage(int chan, int *mvolts)
{
#ifdef CONFIG_VF610_ADC
	(void)vf610_read_adc(chan, mvolts);
#elif defined(CONFIG_MEDIATEK_MT6577_AUXADC)
	(void)mt6577_read_adc(chan, mvolts);
#elif defined (CONFIG_AMLOGIC_SARADC)
	(int)sonos_sar_adc_iio_info_read_raw(chan, mvolts);
#endif
	return 0;
}
EXPORT_SYMBOL(read_adc_voltage);

/* In memory copy of the MDP */
struct manufacturing_data_page sys_mdp;
EXPORT_SYMBOL(sys_mdp);
struct manufacturing_data_page2 sys_mdp2;
EXPORT_SYMBOL(sys_mdp2);
struct manufacturing_data_page3 sys_mdp3;
EXPORT_SYMBOL(sys_mdp3);

/* Provides the atheros HAL a way of getting the calibration data from NAND */
int __weak ath_nand_local_read(u_char *cal_part,loff_t from, size_t len,
		size_t *retlen, u_char *buf)
{
	return 0;
}
EXPORT_SYMBOL(ath_nand_local_read);

/* In memory copy of the MDP */
char uboot_version_str[120];
EXPORT_SYMBOL(uboot_version_str);

int sonos_product_id;
EXPORT_SYMBOL(sonos_product_id);

/* move Sonos special kernel code to here */
extern char uboot_version_str[120];

/*
 * copy the mdp from uboot
 */
static int __init early_mdp(char *p)
{
	phys_addr_t mdpAddr = 0;

	/* Build bomb - mdp sizes must be the same for all builds, platforms, controllers */
	BUILD_BUG_ON(sizeof(struct manufacturing_data_page)!=MDP1_BYTES);
	BUILD_BUG_ON(sizeof(struct manufacturing_data_page2)!=MDP2_BYTES);
	BUILD_BUG_ON(sizeof(struct manufacturing_data_page3)!=MDP3_BYTES);

	if (kstrtoul(p, 16, (unsigned long *)&mdpAddr)) {
		printk(KERN_ERR "early_mdp: strtoul returned an error\n");
		goto mdp_err;
	}

	copy_from_early_mem(&sys_mdp, mdpAddr, sizeof(struct manufacturing_data_page));

	if (sys_mdp.mdp_magic == MDP_MAGIC) {
		struct smdp *s_mdp = (struct smdp *)mdpAddr;
		printk("MDP: model %x, submodel %x, rev %x\n",
			sys_mdp.mdp_model, sys_mdp.mdp_submodel, sys_mdp.mdp_revision);
		copy_from_early_mem(&sys_mdp2, (phys_addr_t)&s_mdp->mdp2,
			sizeof(struct manufacturing_data_page2));
		if ( sys_mdp2.mdp2_magic == MDP_MAGIC2_ENC ) {
			printk("got mdp 2\n");
		} else {
			memset(&sys_mdp2, 0, sizeof(struct manufacturing_data_page2));
		}
		copy_from_early_mem(&sys_mdp3, (phys_addr_t)&s_mdp->mdp3,
			sizeof(struct manufacturing_data_page3));
		if ( sys_mdp3.mdp3_magic == MDP_MAGIC3 ) {
			printk("got mdp 3\n");
		} else {
			memset(&sys_mdp3, 0, sizeof(struct manufacturing_data_page3));
		}
		return 0;
	} else {
		printk("MDP: invalid magic: is %08x should be %08x, using default\n",
			sys_mdp.mdp_magic, MDP_MAGIC);
		printk("MDP: flags = %#x\n", sys_mdp.mdp_flags);
	}

mdp_err:
	memset(&sys_mdp, 0, sizeof(struct manufacturing_data_page));
	return 0;
}
early_param("mdpaddr", early_mdp);

int bootgeneration = 0;
static int __init bootgeneration_setup(char *str)
{
	bootgeneration = simple_strtoul(str, NULL, 0);
	return 1;
}
__setup("bootgen=", bootgeneration_setup);

int bootsection = 0;
static int __init bootsection_setup(char *str)
{
	bootsection = simple_strtoul(str, NULL, 0);
	return 1;
}
__setup("bootsect=", bootsection_setup);

void sonos_announce_linkup(struct net_device *dev)
{
	struct net *rtnl;
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	unsigned char *b;
	int size = NLMSG_SPACE(1024);

	if (dev == NULL) {
		printk("%s: dev NULL\n", __func__);
		return;
	}
	rtnl = dev_net(dev);

	printk("%s: announcing link status change\n", __func__);

	skb = nlmsg_new(size, GFP_ATOMIC);
	if (!skb) {
		netlink_set_err(rtnl->rtnl, 0, RTMGRP_Rincon, ENOBUFS);
		return;
	}
	b = skb_tail_pointer(skb);
	nlh = nlmsg_put(skb, 0, 0, RWM_MII, 0, 0);
	if ( !nlh )
		goto nlmsg_failure;
	nla_put(skb, RWA_DEV_NAME, IFNAMSIZ, dev->name);
	nlh->nlmsg_len = skb_tail_pointer(skb) - b;

	NETLINK_CB(skb).dst_group = RTMGRP_Rincon;
	nlmsg_end(skb, nlh);
	rtnl_notify(skb, rtnl, 0, RTMGRP_Rincon, NULL, GFP_ATOMIC);
	return;

nlmsg_failure:
	kfree_skb(skb);
	netlink_set_err(rtnl->rtnl, 0, RTMGRP_Rincon, EINVAL);
	printk("%s: phy announce failed.\n", __func__);
}
EXPORT_SYMBOL(sonos_announce_linkup);

/* These functions are defined to provide a callback interface
 * for getting orientation events from audiodev to the wifi driver.
 *
 * See all/cc/atheros/driver/diversity.c
 */

// Encore only has one wifi device
#define MAX_ORIENTATION_CALLBACKS	1

struct orientation_cback {
	void	(*function)(int orient, void *param);
	void	*param;
};

static struct orientation_cback orientation_cb_tbl[MAX_ORIENTATION_CALLBACKS];
static int orientation_cb_refs = 0;

void *sonos_orientation_register_callback(void (*function)(int orient, void *param), void *param)
{
	int idx;
	struct orientation_cback *new_entry = NULL;

	if (!product_is_encore()) {
		//This piece of code is only for Encore
		return NULL;
	}

	// First register initializes the array
	if (!orientation_cb_refs) {
		memset(orientation_cb_tbl, 0, sizeof(orientation_cb_tbl));
	}
	orientation_cb_refs++;

	// Search table for a free entry
	for (idx = 0; idx < ARRAY_SIZE(orientation_cb_tbl); idx++) {
		if (orientation_cb_tbl[idx].function == NULL) {
			new_entry = &(orientation_cb_tbl[idx]);
			break;
		}
	}

	if (new_entry == NULL) {
		printk(KERN_WARNING "%s: max callbacks registered!\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	// Set callback
	new_entry->function = function;
	new_entry->param = param;

	return new_entry;
}
EXPORT_SYMBOL(sonos_orientation_register_callback);

int sonos_orientation_unregister_callback(void *entry)
{
	struct orientation_cback *del_entry = entry;

	if (!product_is_encore()) {
		//This piece of code is only for Encore
		return 0;
	}

	if (del_entry == NULL) {
		printk(KERN_WARNING "%s: callback entry is null!\n", __func__);
		return -EFAULT;
	}

	if (del_entry->function == NULL) {
		printk(KERN_WARNING "%s: callback already unregistered!\n", __func__);
		return -EINVAL;
	}

	del_entry->function = NULL;
	del_entry->param = NULL;
	orientation_cb_refs--;
	return 0;
}
EXPORT_SYMBOL(sonos_orientation_unregister_callback);

void sonos_orientation_change_event(int orient)
{
	int idx;

	if (!product_is_encore()) {
		//This piece of code is only for Encore
		return;
	}

	// Call all registered callbacks (blank entries in table are silently ignored)
	for (idx = 0; idx < ARRAY_SIZE(orientation_cb_tbl); idx++) {
		if (orientation_cb_tbl[idx].function != NULL) {
			orientation_cb_tbl[idx].function(orient, orientation_cb_tbl[idx].param);
		}
	}
}
EXPORT_SYMBOL(sonos_orientation_change_event);

#ifdef CONFIG_SONOS_SECBOOT
#if defined CONFIG_SONOS_DIAGS && !defined SONOS_STRICT_DIAG_BUILD
int enable_console = 1;
#else
int enable_console;
#endif

static int __init enable_console_setup(char *str)
{
	if(!str) return 1;

	// when uboot passes enable_console it always passes 1
	enable_console = simple_strtoul(str, NULL, 10);

	return 1;
}
__setup("enable_console=", enable_console_setup);
#endif //CONFIG_SONOS_SECBOOT

char sonos_machine_name[128];
EXPORT_SYMBOL(sonos_machine_name);

static int __init sonos_productid_init(void)
{
	const u32 *product_id; /* One byte value in a 32-bit field */
	const char *uboot_version;

	sonos_product_id = 0;

	product_id = of_get_flat_dt_prop(0, "sonos-product-id", NULL);
	if ( product_id ) {
		sonos_product_id = ntohl(*product_id);
	}
	pr_info("Machine sonos_product_id %d\n", sonos_product_id);
	snprintf(sonos_machine_name, sizeof sonos_machine_name, "%s",
			of_flat_dt_get_machine_name());
	pr_info("Machine Name %s\n", sonos_machine_name);

	uboot_version_str[0] = 0;
	memset(uboot_version_str, 0, sizeof(uboot_version_str));
	uboot_version = of_get_flat_dt_prop(0, "uboot-version", NULL);
	if ( uboot_version ) {
		snprintf(uboot_version_str, sizeof uboot_version_str, "%s",
				uboot_version);
		if ( uboot_version_str[0] == 'U' ) {
			pr_info("U-boot revision %s\n", uboot_version_str);
		}
	}
	return 0;
}
arch_initcall(sonos_productid_init);
