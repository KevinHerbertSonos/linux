/*
 * Copyright (c) 2020, Sonos, Inc.
 *
 * SPDX-License-Identifier:     GPL-2.0
 */

/*
 * drivers/net/phy/ter21x3.c
 *
 * Driver for Teridian 21x3 PHY
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

MODULE_DESCRIPTION("Teridian 21x3 PHY driver");
MODULE_LICENSE("GPL");

static struct proc_dir_entry *procdir = NULL;
#define TER21X3_PROC_DIR			"driver/phy"

#define TER21X3_INTR_ENABLE			0x11
#define TER21X3_INTR_STATUS			0x11
#define TER21X3_LINK_INIT			1 << 10
#define TER21X3_DIAG_REG			0x12
#define TER21X3_DIAG_DUPLEX			1 << 11
#define TER21X3_DIAG_SPEED			1 << 10

struct ethtool_cmd ter21x3_status[2] = {{0}};

int phy_power_down(struct phy_device *phydev, int on)
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

static int ter21x3_proc_phydump(struct seq_file *m, void *v)
{
	struct phy_device *phydev = (struct phy_device *)m->private;
	int reg, val;

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
static int ter21x3_phy_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ter21x3_proc_phydump, PDE_DATA(inode));
}

static ssize_t ter21x3_phy_proc_write(struct file *file,
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
static const struct file_operations ter21x3_phy_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= ter21x3_phy_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= ter21x3_phy_proc_write,
};

static void ter21x3_check_link(struct phy_device *phydev)
{
	int link;
	int val;
	unsigned int prev_speed = ter21x3_status[0].speed;

	/* report link as up if either PHY has link */
	link = 0;
	val = phy_read(phydev, MII_BMSR);
	if (val & BMSR_LSTATUS) {
		link = 1;
		val = phy_read(phydev, TER21X3_DIAG_REG);
		if (val & TER21X3_DIAG_SPEED) {
			ter21x3_status[0].speed = SPEED_100;
		} else {
			ter21x3_status[0].speed = SPEED_10;
		}
		if (val & TER21X3_DIAG_DUPLEX) {
			ter21x3_status[0].duplex = DUPLEX_FULL;
		} else {
			ter21x3_status[0].duplex = DUPLEX_HALF;
		}
	} else {
		ter21x3_status[0].duplex = 0;
		ter21x3_status[0].speed = 0;
	}

	if (ter21x3_status[0].speed != prev_speed) {
		if (ter21x3_status[0].speed == 0) {
			printk("ter21x3: port down\n");
			netif_carrier_off(phydev->attached_dev);
		} else {
			printk("ter21x3: port up, speed %d,"
				" %s duplex\n",
				ter21x3_status[0].speed,
				(ter21x3_status[0].duplex ==
					DUPLEX_FULL) ? "full" : "half");
			netif_carrier_on(phydev->attached_dev);
		}
	}

	sonos_announce_linkup(phydev->attached_dev);
}

static int ter21x3_ack_interrupt(struct phy_device *phydev)
{
	int err;

	err = phy_read(phydev, TER21X3_INTR_STATUS);
	ter21x3_check_link(phydev);

	return (err < 0) ? err : 0;
}

static int ter21x3_config_intr(struct phy_device *phydev)
{
	int err;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		err = phy_write(phydev, TER21X3_INTR_ENABLE,
				TER21X3_LINK_INIT);
	else
		err = phy_write(phydev, TER21X3_INTR_ENABLE, 0);

	return err;
}

static int ter21x3_config_init(struct phy_device *phydev)
{
	int val;
	u32 features;
	struct device_node *node = NULL;
	int ethernet_phy_interrupt = -1;

	node = of_find_compatible_node(NULL, NULL, "Sonos,ter21x3_phy");
	if (node) {
		ethernet_phy_interrupt = gpio_to_irq(of_get_named_gpio(node,
			"interrupts", 0));
	} else {
		printk(KERN_ERR "could not find device node for interrupts\n");
	}

	irq_set_irq_type(ethernet_phy_interrupt, IRQ_TYPE_LEVEL_LOW);
	phydev->irq = ethernet_phy_interrupt;
	val = ter21x3_config_intr(phydev);
	if ( val )
		return val;

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

	if ( procdir == NULL ) {
		procdir = proc_mkdir(TER21X3_PROC_DIR, NULL);
		if (procdir == NULL) {
			printk("Couldn't create base dir /proc/%s\n",
				TER21X3_PROC_DIR);
			return -ENOMEM;
		}
		if (! proc_create_data("reg", 0, procdir,
			&ter21x3_phy_proc_fops, phydev)) {
			printk("%s/phy not created\n", TER21X3_PROC_DIR);
			goto cleanup;
		}
	}
	return 0;
cleanup:
	remove_proc_subtree(TER21X3_PROC_DIR, NULL);
	procdir = NULL;
	return 0;
}


static int ter21x3_config_aneg(struct phy_device *phydev)
{
	genphy_config_aneg(phydev);
	return 0;
}

static int ter21x3_read_status(struct phy_device *phydev)
{
	int val;

	val = phy_read(phydev, MII_BMSR);
	if (val & BMSR_LSTATUS) {
		phydev->link = 1;
		val = phy_read(phydev, TER21X3_DIAG_REG);
		if (val & TER21X3_DIAG_SPEED) {
			phydev->speed = SPEED_100;
		} else {
			phydev->speed = SPEED_10;
		}
		if (val & TER21X3_DIAG_DUPLEX) {
			phydev->duplex = DUPLEX_FULL;
		} else {
			phydev->duplex = DUPLEX_HALF;
		}
	} else {
		phydev->link = 0;
		phydev->duplex = 0;
		phydev->speed = 0;
	}

	return 0;
}

static struct phy_driver ter21x3_driver = {
	.phy_id		= 0x000e7237,
	.name		= "Teridian 78Q21x3 ethernet",
	.phy_id_mask	= 0xfffffc00,
	.config_init	= ter21x3_config_init,
	.features	= PHY_BASIC_FEATURES,
	.flags		= PHY_HAS_INTERRUPT,
	.config_aneg	= &ter21x3_config_aneg,
	.read_status	= &ter21x3_read_status,
	.ack_interrupt	= &ter21x3_ack_interrupt,
	.config_intr	= &ter21x3_config_intr,
};

static int __init ter21x3_init(void)
{
	int ret;

	ret = phy_driver_register(&ter21x3_driver, THIS_MODULE);
	return ret;
}

static void __exit ter21x3_exit(void)
{
	phy_driver_unregister(&ter21x3_driver);
	if ( procdir )
		remove_proc_subtree(TER21X3_PROC_DIR, NULL);
	procdir = NULL;
}

module_init(ter21x3_init);
module_exit(ter21x3_exit);

static struct mdio_device_id __maybe_unused ter21x3_tbl[] = {
	{ 0x000e7237, 0xffffffef },
	{ }
};

MODULE_DEVICE_TABLE(mdio, ter21x3_tbl);
