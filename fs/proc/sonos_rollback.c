/*
 * Copyright (c) 2015-2017, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 *
 * linux/fs/proc/sonos_rollback.c
 *
 * /proc entry support to allow control over unlock authorization
 * functionality on secured Sonos products.
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>
#include "mdp.h"
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include "sonos_rollback.h"
// sonos_lock.h needs to know whether this is
// is the source file...
#define SONOS_LOCK_C
#include "sonos_lock.h"

#define DRV_NAME		"sonos-rollback"

// Statics for use in maintaining the fallback state

struct sonos_rollback_dev {
	struct regmap *gpr;
	const char * dev_node;
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry * counter_dir;
	struct proc_dir_entry * fallback_dir;
#endif
	int offset;
	int shift;
	int flag;
	int saved_state;
};

struct sonos_rollback_dev *sonos_rollback = NULL;

static unsigned int read_boot_counter(void)
{
	unsigned int boot_counter;
	regmap_read(sonos_rollback->gpr, sonos_rollback->offset, &boot_counter);
	boot_counter >>= sonos_rollback->shift;
	return boot_counter;
}

int init_sonos_rollback(void)
{
	unsigned int boot_counter;
	BootCounterReg_t *data;

	if ( !sonos_rollback ) {
		return 1;
	}

	if ( sonos_rollback->flag ) {
		return 0;
	}

	sonos_rollback->gpr = syscon_regmap_lookup_by_compatible(sonos_rollback->dev_node);

	if ( IS_ERR(sonos_rollback->gpr) ) {
		printk(KERN_ERR "cannot map %s\n", sonos_rollback->dev_node);
		return 1;
	}
	sonos_rollback->flag = 1;
	boot_counter = read_boot_counter();
	data = (BootCounterReg_t*)&boot_counter;
	sonos_rollback->saved_state = data->boot_state;
	return 0;
}

#ifdef CONFIG_PROC_FS
static void write_boot_counter(unsigned int boot_counter)
{
        unsigned int bc_reserved, bc_temp;

	regmap_read(sonos_rollback->gpr, sonos_rollback->offset, &bc_reserved);
	bc_reserved = bc_reserved & ((1<<sonos_rollback->shift)-1);
	bc_temp = boot_counter << sonos_rollback->shift;
	bc_temp = bc_temp | bc_reserved;

	regmap_write(sonos_rollback->gpr, sonos_rollback->offset, bc_temp);
}

// For reads and writes of the register, the boot counter is treated as
// a uint32.  For changing the boot_counter value, it's treated as a
// BootCounterReg_t.
static unsigned int update_boot_counter(int write, int value)
{
	unsigned int boot_counter;
	BootCounterReg_t *data;

	if ( write ) {
		boot_counter = read_boot_counter();
		data = (BootCounterReg_t*)&boot_counter;

		if ( value == 0 ) {
			data->boot_section = 0;
			data->fallback_flags = 0;
			data->boot_state = 0;
			data->boot_counter = 0;
		}
#if defined CONFIG_SONOS_DIAGS
		else if ( value == -1 ) {
			// HACK #2 - In manufacturing we want to avoid a race condition
			// after the upgrade at the Upgrade Station where the operator
			// may disconnect power while various housekeeping tasks that
			// write to flash partitions are in progress. We accomplish this
			// by setting a flag to stop in uboot after a warm-boot.
			data->boot_section = 0;
			data->fallback_flags = BC_FLAG_STOP_BOOT;
			data->boot_state = 0;
			data->boot_counter = 0;
		}
#endif
		else {
			data->boot_counter = value;
		}
		write_boot_counter(boot_counter);
	} else {
		boot_counter = read_boot_counter();
	}
	return boot_counter;
}

// Set BootCounterReg fields on rootfs digest validation failure:
// fallback flag to BC_FLAG_BAD_SIGNATURE,
// boot counter over the fallback threshold
void sonos_rootfs_failure_notify(void)
{
	unsigned int boot_counter;
	BootCounterReg_t *bc;

	if (init_sonos_rollback()) {
		return;
	}

	boot_counter = read_boot_counter();
	bc = (BootCounterReg_t*)&boot_counter;

	bc->fallback_flags |= BC_FLAG_BAD_SIGNATURE;
	bc->boot_counter += BOOT_COUNTER_FALLBACK_THRESHOLD;

	write_boot_counter(boot_counter);
}
EXPORT_SYMBOL(sonos_rootfs_failure_notify);

static int boot_counter_proc_show(struct seq_file *m, void *v)
{
	unsigned int boot_counter;
	BootCounterReg_t *bc;

	if ( init_sonos_rollback() ) {
		return 0;
	}

	boot_counter = update_boot_counter(0, 0);
	bc = (BootCounterReg_t*)&boot_counter;
	seq_printf(m, "%d.%d.%d.%d\n", bc->boot_counter, bc->boot_state, bc->fallback_flags, bc->boot_section);
	return 0;
}

static int boot_counter_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, boot_counter_proc_show, NULL);
}

//	Allow the device to clear the boot_counter...
static int boot_counter_write_data(struct file *file, const char __user * buf, size_t length, loff_t * offset)
{
	char	buffer[64];
	__s32	new_count;
	int ret;

	if ( init_sonos_rollback() ) {
		return 0;
	}

        memset(buffer, 0, sizeof(buffer));
        if (length > sizeof(buffer) - 1) {
                length = sizeof(buffer) - 1;
	}
        if (copy_from_user(buffer, buf, length)) {
                return -EFAULT;
	}

	// For testing purposes, allow counts of -1 to 10...
	ret = sscanf(buffer, "%d", &new_count);
	if ( ret != 1 ) {
		return -EINVAL;
	}

	if ( new_count >= -1 && new_count <= 10 ) {
		update_boot_counter(1, new_count);
	} else {
		return -EINVAL;
	}
	return length;
}

static const struct file_operations boot_counter_proc_fops = {
	.open		= boot_counter_proc_open,
	.read		= seq_read,
	.write		= boot_counter_write_data,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/*	/proc/sonos-lock/fallback_state
 *		Make the bc->state available to users even if it's been
 *		cleared (which always happens during boot/init).
 *
 *		0 - running from the default partition
 *		1 - fallback due to boot counter
 *		2 - fallback due to sonosboot fallback
 */

static int fallback_state_proc_show(struct seq_file *m, void *v)
{
	if ( init_sonos_rollback() ) {
		return 0;
	}

	seq_printf(m, "%d\n", sonos_rollback->saved_state );
	return 0;
}

static int fallback_state_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, fallback_state_proc_show, NULL);
}

static const struct file_operations fallback_state_proc_fops = {
	.open		= fallback_state_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif

static int sonos_rollback_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	const __be32 * shift;
	const __be32 * offset;
	int err;

	sonos_rollback = devm_kzalloc(&pdev->dev, sizeof(*sonos_rollback), GFP_KERNEL);
	if ( !sonos_rollback ) {
		return -ENOMEM;
	}

	sonos_rollback->flag = 0;

	platform_set_drvdata(pdev, sonos_rollback);

	err = of_property_read_string(np, "wdt-node", &sonos_rollback->dev_node);
	if ( err ) {
		devm_kfree(&pdev->dev, sonos_rollback);
		of_node_put(np);
		sonos_rollback = NULL;
		return err;
	}

	offset = of_get_property(np, "offset", NULL);
	if ( offset ) {
		sonos_rollback->offset = be32_to_cpu(*offset);
	} else {
		pr_debug("%s missing offset property\n", np->full_name);
		devm_kfree(&pdev->dev, sonos_rollback);
		of_node_put(np);
		sonos_rollback = NULL;
		return -EINVAL;
	}

	shift = of_get_property(np, "shift", NULL);
	if ( shift ) {
		sonos_rollback->shift = be32_to_cpu(*shift);
	} else {
		sonos_rollback->shift = 0;
	}
#ifdef CONFIG_PROC_FS
	sonos_rollback->counter_dir = proc_create("sonos-lock/boot_counter",
		0, NULL, &boot_counter_proc_fops);
	if ( !sonos_rollback->counter_dir ) {
		pr_debug("create proc sonos-lock/boot_counter\n");
		devm_kfree(&pdev->dev, sonos_rollback);
		of_node_put(np);
		sonos_rollback = NULL;
		return -ENOMEM;
	}
	sonos_rollback->fallback_dir = proc_create("sonos-lock/fallback_state",
		0, NULL, &fallback_state_proc_fops);
	if ( !sonos_rollback->fallback_dir ) {
		pr_debug("create proc sonos-lock/fallback_state\n");
		proc_remove(sonos_rollback->counter_dir);
		devm_kfree(&pdev->dev, sonos_rollback);
		of_node_put(np);
		sonos_rollback = NULL;
		return -ENOMEM;
	}
#endif
	return 0;
}

static int sonos_rollback_remove(struct platform_device *pdev)
{
#ifdef CONFIG_PROC_FS
	proc_remove(sonos_rollback->fallback_dir);
	proc_remove(sonos_rollback->counter_dir);
#endif
	devm_kfree(&pdev->dev, sonos_rollback);
	sonos_rollback = NULL;
	return 0;
}

static void sonos_rollback_shutdown(struct platform_device *pdev)
{
	return;
}

static const struct of_device_id sonos_rollback_dt_ids[] = {
	{ .compatible = "sonos,sonos-rollback" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sonos_rollback_dt_ids);

static struct platform_driver sonos_rollback_driver = {
	.probe		= sonos_rollback_probe,
	.remove		= sonos_rollback_remove,
	.shutdown	= sonos_rollback_shutdown,
	.driver		= {
		.name		= DRV_NAME,
		.of_match_table	= sonos_rollback_dt_ids,
	},
};
module_platform_driver(sonos_rollback_driver);
