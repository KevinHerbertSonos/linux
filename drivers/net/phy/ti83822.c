/*
 * drivers/net/phy/ti83822.c
 *
 * Driver for TI83822 PHY
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
#include <linux/string.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <net/netlink.h>
#include <linux/rtnetlink.h>

MODULE_DESCRIPTION("TI 83822 PHY driver");
MODULE_LICENSE("GPL");

static struct proc_dir_entry *procdir = NULL;
#define TI83822_PROC_DIR	"driver/phy"

/* Defines for the TI83822 PHY used on Bootleg */
#define TI83822_PHYSTS				0x10
#define TI83822_PHY_SPECIFIC_CONTROL		0x11
#define TI83822_INTR_STAT1			0x12
#define TI83822_INTR_STAT2			0x13
#define TI83822_INTERRUPT_ENABLE		0x02
#define TI83822_INTERRUPT_OUTPUT		0x01
#define TI83822_LINK_INIT			(TI83822_INTERRUPT_ENABLE | TI83822_INTERRUPT_OUTPUT)
#define TI83822_PHY_ID				0x2000a240
#define TI83822_PHYSTS_LINK_STATUS		(1 << 0)
#define TI83822_PHYSTS_SPEED			(1 << 1)
#define TI83822_PHYSTS_DUPLEX			(1 << 2)

struct ethtool_cmd ti83822_status[2] = {{0}};
static struct phy_device *ti83822_phydev = NULL;
static struct net_device *ti83822_mac_device = NULL;
void (*ti83822_link_up)(struct net_device *ndev) = NULL;

void ti83822_announce_linkup(struct net_device *dev)
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

int ti83822_phy_power_down(struct phy_device *phydev, int on)
{
	int err = 0;
	if ( on ) {
		if ( !(phy_read(phydev, MII_BMCR) & BMCR_PDOWN )) {
			err = phy_write(phydev, MII_BMCR, phy_read(phydev,
				MII_BMCR) | BMCR_PDOWN);
		}
	} else {
		if ( phy_read(phydev, MII_BMCR) & BMCR_PDOWN ) {
			err = phy_write(phydev, MII_BMCR, phy_read(phydev,
				MII_BMCR) | ~BMCR_PDOWN);
		}
	}
	return err;
}

static int ti83822_suspend(struct phy_device *phydev)
{
	/* FIXME later if need care of eth power save in suspend
	return ti83822_phy_power_down(phydev, 0); */
	return 0;
}

static int ti83822_resume(struct phy_device *phydev)
{
	/* FIXME later if need care of eth power save in suspend
	return ti83822_phy_power_down(phydev, 1); */
	return 0;
}

static int ti83822_proc_phydump(struct seq_file *m, void *v)
{
	struct phy_device *phydev = (struct phy_device *)m->private;
	int i, reg, val;
	u32 offset;

	struct ti83822_ranges {
                u32 start;
                u32 end;
        } ranges[] = {
                {0x25, 0x25},
                {0x27, 0x27},
                {0x3e, 0x3f},
                {0x42, 0x42},
                {0x155, 0x155},
                {0x170, 0x171},
                {0x173, 0x173},
                {0x177, 0x177},
                {0x180, 0x18a},
                {0x215, 0x215},
                {0x21d, 0x21d},
                {0x403, 0x403},
                {0x428, 0x428},
                {0x456, 0x456},
                {0x460, 0x463},
                {0x465, 0x465},
                {0x467, 0x469},
                {0x4a0, 0x4d1},
                {0x3000, 0x3001},
                {0x3014, 0x3014},
                {0x3016, 0x3016},
                {0x703c, 0x703d},
	};

	for (i = 0 ; i < ARRAY_SIZE(ranges) ; i++) {
		for (offset = ranges[i].start; offset <= ranges[i].end; offset++ ) {
			seq_printf(m, "reg %04x = %04x\n", offset,
				phy_read(phydev, offset));
		}
	}
	for (reg = 0; reg <= 0x1f; reg++) {
		val = phy_read(phydev, reg);
		seq_printf(m, "%04x ", val);
		if ((reg & 0x0f) == 0x0f)
			seq_printf(m, "\n");
		else if ((reg & 0x07) == 0x07)
			seq_printf(m, "  ");
	}
	return 0;
}

static int ti83822_phy_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ti83822_proc_phydump, PDE_DATA(inode));
}

static ssize_t ti83822_phy_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	struct phy_device *phydev = (struct phy_device *)PDE_DATA(file_inode(file));
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
static const struct file_operations ti83822_phy_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= ti83822_phy_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= ti83822_phy_proc_write,
};

static void ti83822_check_link(void)
{
	int link;
	struct phy_device *phydev;
	int val;
	unsigned int prev_speed = ti83822_status[0].speed;
	if (ti83822_phydev == NULL) {
		printk("ti83822_phydev NULL\n");
		return;
	}

	phydev = ti83822_phydev;
	/* report link as up if either PHY has link */
	link = 0;
	val = phy_read(phydev, MII_BMSR);
	if (val & BMSR_LSTATUS) {
		link = 1;
		val = phy_read(phydev, TI83822_PHYSTS);
		if (val & TI83822_PHYSTS_SPEED) {
			ti83822_status[0].speed = SPEED_10;
		} else {
			ti83822_status[0].speed = SPEED_100;
		}
		if (val & TI83822_PHYSTS_DUPLEX) {
			ti83822_status[0].duplex = DUPLEX_FULL;
		} else {
			ti83822_status[0].duplex = DUPLEX_HALF;
		}
	} else {
		ti83822_status[0].duplex = 0;
		ti83822_status[0].speed = 0;
	}

	if (ti83822_status[0].speed != prev_speed) {
		if (ti83822_status[0].speed == 0) {
			printk("ti83822: port down\n");
		} else {
			printk("ti83822: port up, speed %d,"
				" %s duplex\n",
				ti83822_status[0].speed,
				(ti83822_status[0].duplex ==
					DUPLEX_FULL) ? "full" : "half");
		}
	}


	if (ti83822_link_up != NULL) {
		(*ti83822_link_up)(ti83822_mac_device);
	}
}


static int ti83822_ack_interrupt(struct phy_device *phydev)
{
	int err;
	int val;

	err = phy_read(phydev, TI83822_INTR_STAT1);
	err = phy_read(phydev, TI83822_INTR_STAT2);
	// Disable the interrupt
	val = phy_read(phydev, TI83822_PHY_SPECIFIC_CONTROL);
	err = phy_write(phydev, TI83822_PHY_SPECIFIC_CONTROL, (val & ~TI83822_INTERRUPT_ENABLE));
	// and re-enable it.
	val = phy_read(phydev, TI83822_PHY_SPECIFIC_CONTROL);
	err = phy_write(phydev, TI83822_PHY_SPECIFIC_CONTROL, (val | TI83822_INTERRUPT_ENABLE));

	ti83822_check_link();

	return (err < 0) ? err : 0;
}

static int ti83822_config_intr(struct phy_device *phydev)
{
	int err;
	int val;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED) {
		// Link status change - nothing else...
		err = phy_write(phydev, TI83822_INTR_STAT1, 0x20);
		val = phy_read(phydev, TI83822_PHY_SPECIFIC_CONTROL);
		err = phy_write(phydev, TI83822_PHY_SPECIFIC_CONTROL,
				val | TI83822_LINK_INIT);
	} else {
		val = phy_read(phydev, TI83822_PHY_SPECIFIC_CONTROL);
		err = phy_write(phydev, TI83822_PHY_SPECIFIC_CONTROL, (val & ~TI83822_INTERRUPT_ENABLE));
	}

	return err;
}

static int ti83822_config_init(struct phy_device *phydev)
{
	int val;
	u32 features;
	struct device_node *node = NULL;
	int ethernet_phy_interrupt;

	ti83822_phydev = phydev;
	ti83822_link_up = ti83822_announce_linkup;
	ti83822_mac_device = phydev->attached_dev;

	node = of_find_compatible_node(NULL, NULL, "Sonos,ti83822_phy");
	if (node) {
		ethernet_phy_interrupt = gpio_to_irq(of_get_named_gpio(node, "interrupts", 0));
	} else {
		printk(KERN_ERR "could not find device node to get interrupts\n");
	}

	irq_set_irq_type(ethernet_phy_interrupt, IRQ_TYPE_LEVEL_LOW);
	phydev->irq = ethernet_phy_interrupt;

	val = ti83822_config_intr(phydev);
	if ( val )
		return val;

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

	phydev->supported = features;
	phydev->advertising = features;

	if ( procdir == NULL ) {
		procdir = proc_mkdir(TI83822_PROC_DIR, NULL);
		if (procdir == NULL) {
			printk("Couldn't create base dir /proc/%s\n",
				TI83822_PROC_DIR);
			return -ENOMEM;
		}
		if (! proc_create_data("reg", 0, procdir,
			&ti83822_phy_proc_fops, phydev)) {
			printk("%s/phy not created\n", TI83822_PROC_DIR);
			goto cleanup;
		}
	}

	return 0;
cleanup:
	remove_proc_subtree(TI83822_PROC_DIR, NULL);
	procdir = NULL;
	return 0;
}

static int ti83822_config_aneg(struct phy_device *phydev)
{
	genphy_config_aneg(phydev);
	return 0;
}

static int ti83822_read_status(struct phy_device *phydev)
{
	int status;

	status = phy_read(phydev, TI83822_PHYSTS);
	if (status < 0)
		return status;

	phydev->speed = 0;
	phydev->duplex = DUPLEX_HALF;
	if ( status & TI83822_PHYSTS_LINK_STATUS ) {
		phydev->link = 1;

		if ( status & TI83822_PHYSTS_SPEED )  {
			phydev->speed = SPEED_10;
		} else {
			phydev->speed = SPEED_100;
		}
		if (status & TI83822_PHYSTS_DUPLEX) {
			phydev->duplex = DUPLEX_FULL;
		}
	} else {
		phydev->link = 0;
	}

	return 0;
}

void sonos_get_port_status(unsigned int port, struct ethtool_cmd *cmd)
{
	*cmd = ti83822_status[port];
}

void sonos_phy_connect(void (*link_up)(struct net_device *), struct net_device *ndev)
{
	ti83822_link_up = link_up;
	ti83822_mac_device = ndev;
	ti83822_check_link();
}

void sonos_phy_disconnect(void)
{
	ti83822_status[0].duplex = 0;
	ti83822_status[0].speed = 0;
	if (ti83822_link_up != NULL)
		(*ti83822_link_up)(ti83822_mac_device);
	ti83822_link_up = NULL;
	ti83822_mac_device = NULL;
	if ( procdir )
		remove_proc_subtree(TI83822_PROC_DIR, NULL);
	procdir = NULL;
	ti83822_phydev = NULL;
}

static struct phy_driver ti83822_driver = {
	.phy_id		= 0x2000a240,
	.name		= "TI DP83822 ethernet",
	.phy_id_mask	= 0xfffffff0,
	.config_init	= ti83822_config_init,
	.features	= PHY_BASIC_FEATURES,
	.flags		= PHY_HAS_INTERRUPT,
	.config_aneg	= &ti83822_config_aneg,
	.read_status	= &ti83822_read_status,
	.ack_interrupt	= &ti83822_ack_interrupt,
	.config_intr	= &ti83822_config_intr,
	.suspend	= &ti83822_suspend,
	.resume		= &ti83822_resume,
	.driver		= {
		.owner = THIS_MODULE,
	},
};


static int __init ti83822_init(void)
{
	int ret;

	ret = phy_driver_register(&ti83822_driver);
	if (ret)
		goto err;

	return 0;

err:
	phy_driver_unregister(&ti83822_driver);

	return ret;
}

static void __exit ti83822_exit(void)
{
	phy_driver_unregister(&ti83822_driver);
	if ( procdir )
		remove_proc_subtree(TI83822_PROC_DIR, NULL);
	procdir = NULL;
}

module_init(ti83822_init);
module_exit(ti83822_exit);

static struct mdio_device_id __maybe_unused ti83822_tbl[] = {
	{ 0x2000a240, 0xffffffef },
	{ }
};

MODULE_DEVICE_TABLE(mdio, ti83822_tbl);
