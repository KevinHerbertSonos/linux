/*
 * drivers/net/phy/ter21x3.c
 *
 * Driver for Teridian 21x3 PHY
 *
 * Author: SONOS
 *	modification of at803x.c by Matus Ujhelyi <ujhelyi.m@gmail.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/phy.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <net/netlink.h>
#include <linux/rtnetlink.h>

void ter21x3_announce_linkup(struct net_device *dev)
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
	b = skb->tail;
	nlh = nlmsg_put(skb, 0, 0, RWM_MII, 0, 0);
	if ( !nlh )
		goto nlmsg_failure;
	nla_put(skb, RWA_DEV_NAME, IFNAMSIZ, dev->name);
	nlh->nlmsg_len = skb->tail - b;

	NETLINK_CB(skb).dst_group = RTMGRP_Rincon;
	nlmsg_end(skb, nlh);
	rtnl_notify(skb, rtnl, 0, RTMGRP_Rincon, NULL, GFP_ATOMIC);
	return;

nlmsg_failure:
	kfree_skb(skb);
	netlink_set_err(rtnl->rtnl, 0, RTMGRP_Rincon, EINVAL);
	printk("%s: phy announce failed.\n", __func__);
}

static int ter21x3_proc_phydump(struct seq_file *m, void *v)
{
	struct phy_device *phydev = (struct phy_device *)m->private;
	int reg, val;

	for (reg = 0; reg <= 24; reg++) {
		val = phy_read(phydev, reg);
		seq_printf(m, "%04x ", val);
		if ((reg & 0x0f) == 0x0f)
			seq_printf(m, "\n");
		else if ((reg & 0x07) == 0x07)
			seq_printf(m, "  ");
	}
	seq_printf(m, "\n");
	return 0;
}

static int ter21x3_phy_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ter21x3_proc_phydump, PDE(inode)->data);
}

static ssize_t ter21x3_phy_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	struct phy_device *phydev = PDE(file->f_path.dentry->d_inode)->data;
	char buf[200];
	char *peq;
	int result = 0;
	long longtemp;
	u32  regnum;
	u16  val;

	if (count > 200)
		result = -EIO;
	else if (copy_from_user(buf, buffer, count)) {
		result = -EFAULT;
	}

	if ( result == 0 ) {
		buf[count] = '\0';
		peq = strchr(buf, '=');
		if (peq != NULL) {
			*peq = 0;
			if ( kstrtol(peq+1, 16, &longtemp) != 0 )
				return -EIO;
			val = longtemp;
			if (strncmp(buf, "reg", 3) == 0) {
				if (kstrtol(buf+3, 16, &longtemp) != 0 )
					return -EIO;
				regnum = longtemp;
				phy_write(phydev, regnum, val);
			}
		}
		result = count;
	}
	return result;
}

static const struct file_operations ter21x3_phy_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= ter21x3_phy_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= ter21x3_phy_proc_write,
};

static int ter21x3_config_init(struct phy_device *phydev)
{
	int val;
	u32 features;
	static int port = 0;
	char name[40];

	val = phy_read(phydev, 0x13);
	phy_write(phydev, 0x13, val & 0x3fff );

	features = SUPPORTED_TP | SUPPORTED_MII | SUPPORTED_AUI |
		   SUPPORTED_FIBRE | SUPPORTED_BNC;

	val = phy_read(phydev, MII_BMSR);
	if (val < 0)
		return val;

	if (val & BMSR_ANEGCAPABLE)
		features |= SUPPORTED_Autoneg;
	if (val & BMSR_100FULL)
		features |= SUPPORTED_100baseT_Full;
	if (val & BMSR_100HALF)
		features |= SUPPORTED_100baseT_Half;
	if (val & BMSR_10FULL)
		features |= SUPPORTED_10baseT_Full;
	if (val & BMSR_10HALF)
		features |= SUPPORTED_10baseT_Half;

	if (val & BMSR_ESTATEN) {
		val = phy_read(phydev, MII_ESTATUS);
		if (val < 0)
			return val;

		if (val & ESTATUS_1000_TFULL)
			features |= SUPPORTED_1000baseT_Full;
		if (val & ESTATUS_1000_THALF)
			features |= SUPPORTED_1000baseT_Half;
	}

	phydev->supported = features;
	phydev->advertising = features;

	snprintf(name, sizeof(name), "driver/phy%d", port);
	if (! proc_create_data(name, 0, NULL,
			&ter21x3_phy_proc_fops, phydev)) {
		printk("%s not created\n", name);
	}
	port++;

	return 0;
}

static int ter21x3_config_aneg(struct phy_device *phydev)
{
	genphy_config_aneg(phydev);
	return 0;
}

static int ter21x3_read_status(struct phy_device *phydev)
{
	int old_link;

	old_link = phydev->link;
	genphy_read_status(phydev);

	if ( phydev->link == 0 ) {
		phydev->speed = 0;
	}

	if ( old_link != phydev->link ) {
		ter21x3_announce_linkup(phydev->attached_dev);
	}

	return 0;
}

static struct phy_driver ter21x3_driver = {
	.phy_id		= 0x000e7237,
	.name		= "Teridian 78Q21x3 ethernet",
	.phy_id_mask	= 0xfffffc00,
	.config_init	= ter21x3_config_init,
	.features	= PHY_BASIC_FEATURES,
	.config_aneg	= &ter21x3_config_aneg,
	.read_status	= &ter21x3_read_status,
	.driver		= {
		.owner = THIS_MODULE,
	},
};


static int __init ter21x3_init(void)
{
	return phy_driver_register(&ter21x3_driver);
}

static void __exit ter21x3_exit(void)
{
	phy_driver_unregister(&ter21x3_driver);
}

module_init(ter21x3_init);
module_exit(ter21x3_exit);

static struct mdio_device_id __maybe_unused ter21x3_tbl[] = {
	{ 0x000e7237, 0xffffffef },
	{ }
};

MODULE_DEVICE_TABLE(mdio, ter21x3_tbl);

/* Module information */
MODULE_DESCRIPTION("Teridian 21x3 PHY driver");
MODULE_AUTHOR("Sonos, Inc");
MODULE_LICENSE("GPL");
