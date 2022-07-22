/*
 * Copyright (c) 2015-2019, Sonos, Inc.
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include "sonos_rollback.h"
// sonos_lock.h needs to know whether this is
// is the source file...
#define SONOS_LOCK_C
#include "sonos_lock.h"

#define DRV_NAME		"sonos-rollback"

/* dm-verity spawns multiple threads to check integrity of blocks, so it's
 * possible multiple threads will discover corrupted data before the kernel
 * manages to restart. If this happens, it's important to not increase the boot
 * counter multiple times, or else it will be possible to go from a boot counter
 * below BOOT_COUNTER_FALLBACK_THRESHOLD to one above
 * BOOT_COUNTER_FAILURE_THRESHOLD without actually trying the fallback
 * partition.
 */

// Statics for use in maintaining the fallback state

typedef struct _sonos_rollback_dev {
	void __iomem *base;
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry * counter_dir;
	struct proc_dir_entry * fallback_dir;
#endif
	int offset;
	int shift;
	int saved_state;
	struct mutex corruption_mutex;
	int num_rootfs_failures;
} sonos_rollback_dev;

/* used by sonos_rootfs_failure_notify */
sonos_rollback_dev *global_data = NULL;

static unsigned int read_boot_counter(sonos_rollback_dev *sonos_rollback)
{
	unsigned int boot_counter;
	boot_counter = readl(sonos_rollback->base + sonos_rollback->offset);
	boot_counter >>= sonos_rollback->shift;
	return boot_counter;
}

int init_sonos_rollback(void)
{
	return 0;
}

#ifdef CONFIG_PROC_FS
static void write_boot_counter(sonos_rollback_dev *sonos_rollback,
	unsigned int boot_counter)
{
	unsigned int bc_reserved, bc_temp;

	bc_reserved = readl(sonos_rollback->base + sonos_rollback->offset);
	bc_reserved = bc_reserved & ((1<<sonos_rollback->shift)-1);
	bc_temp = boot_counter << sonos_rollback->shift;
	bc_temp = bc_temp | bc_reserved;

	writel(bc_temp, sonos_rollback->base + sonos_rollback->offset);
}

// For reads and writes of the register, the boot counter is treated as
// a uint32.  For changing the boot_counter value, it's treated as a
// BootCounterReg_t.
static unsigned int update_boot_counter(sonos_rollback_dev *sonos_rollback,
	int write, int value)
{
	unsigned int boot_counter;
	BootCounterReg_t *data;

	if ( write ) {
		boot_counter = read_boot_counter(sonos_rollback);
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
		write_boot_counter(sonos_rollback, boot_counter);
	} else {
		boot_counter = read_boot_counter(sonos_rollback);
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
	sonos_rollback_dev *sonos_rollback = global_data;

	if ( !sonos_rollback ) {
		return;
	}

	boot_counter = read_boot_counter(sonos_rollback);
	bc = (BootCounterReg_t*)&boot_counter;

	mutex_lock(&sonos_rollback->corruption_mutex);
	sonos_rollback->num_rootfs_failures++;
	if (sonos_rollback->num_rootfs_failures == 1) {
		bc->fallback_flags |= BC_FLAG_BAD_SIGNATURE;
		bc->boot_counter += BOOT_COUNTER_FALLBACK_THRESHOLD;
		if (bc->boot_counter > BOOT_COUNTER_FAILURE_THRESHOLD) {
			printk(KERN_DEBUG "Wrote to boot counter to cause failure on next boot.");
		}
		else {
			printk(KERN_DEBUG "Wrote to boot counter to cause fallback on next boot.");
		}
		write_boot_counter(sonos_rollback, boot_counter);
	}
	mutex_unlock(&sonos_rollback->corruption_mutex);
}
EXPORT_SYMBOL(sonos_rootfs_failure_notify);

static int boot_counter_proc_show(struct seq_file *m, void *v)
{
	unsigned int boot_counter;
	BootCounterReg_t *bc;
	sonos_rollback_dev *sonos_rollback = m->private;

	boot_counter = update_boot_counter(sonos_rollback, 0, 0);
	bc = (BootCounterReg_t*)&boot_counter;
	seq_printf(m, "%d.%d.%d.%d\n", bc->boot_counter, bc->boot_state, bc->fallback_flags, bc->boot_section);
	return 0;
}

static int boot_counter_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, boot_counter_proc_show, PDE_DATA(inode));
}

//	Allow the device to clear the boot_counter...
static ssize_t boot_counter_write_data(struct file *file, const char __user * buf, size_t length, loff_t * offset)
{
	char	buffer[64];
	__s32	new_count;
	int ret;
	sonos_rollback_dev *sonos_rollback = PDE_DATA(file_inode(file));

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
		update_boot_counter(sonos_rollback, 1, new_count);
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
	sonos_rollback_dev *sonos_rollback = m->private;
	seq_printf(m, "%d\n", sonos_rollback->saved_state );
	return 0;
}

static int fallback_state_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, fallback_state_proc_show, PDE_DATA(inode));
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
	struct resource *res;
	const __be32 * shift;
	const __be32 * offset;
	unsigned int boot_counter;
	BootCounterReg_t *data;
	sonos_rollback_dev *sonos_rollback;

	sonos_rollback = devm_kzalloc(&pdev->dev,
		sizeof(*sonos_rollback), GFP_KERNEL);
	if ( !sonos_rollback ) {
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if ( !res ) {
		dev_err(&pdev->dev, "unable to obtain rollback base\n");
		goto fail_get_resource;
	}

	sonos_rollback->base = devm_ioremap(&pdev->dev, res->start,
					resource_size(res));
	if ( !sonos_rollback->base ) {
		dev_err(&pdev->dev, "unable to remap rollback base\n");
		goto fail_ioremap;
	}

	platform_set_drvdata(pdev, sonos_rollback);

	offset = of_get_property(np, "offset", NULL);
	if ( offset ) {
		sonos_rollback->offset = be32_to_cpu(*offset);
	} else {
		pr_debug("%s missing offset property\n", np->full_name);
		goto fail_get_offset;
	}

	if ( resource_size(res) < sonos_rollback->offset ) {
		pr_debug("%s offset and size mismatch\n", np->full_name);
		goto fail_get_offset;
	}

	shift = of_get_property(np, "shift", NULL);
	if ( shift ) {
		sonos_rollback->shift = be32_to_cpu(*shift);
	} else {
		sonos_rollback->shift = 0;
	}

	boot_counter = read_boot_counter(sonos_rollback);
	data = (BootCounterReg_t*)&boot_counter;
	sonos_rollback->saved_state = data->boot_state;

	sonos_rollback->num_rootfs_failures = 0;
	mutex_init(&sonos_rollback->corruption_mutex);

#ifdef CONFIG_PROC_FS
	sonos_rollback->counter_dir = proc_create_data("sonos-lock/boot_counter",
		0, NULL, &boot_counter_proc_fops, sonos_rollback);
	if ( !sonos_rollback->counter_dir ) {
		pr_debug("create proc sonos-lock/boot_counter\n");
		goto fail_counter_dir;
	}
	sonos_rollback->fallback_dir = proc_create_data("sonos-lock/fallback_state",
		0, NULL, &fallback_state_proc_fops, sonos_rollback);
	if ( !sonos_rollback->fallback_dir ) {
		pr_debug("create proc sonos-lock/fallback_state\n");
		goto fail_fallback_dir;
	}
#endif
	global_data = sonos_rollback;
	return 0;

#ifdef CONFIG_PROC_FS
fail_fallback_dir:
	proc_remove(sonos_rollback->counter_dir);
fail_counter_dir:
#endif
fail_get_offset:
	devm_iounmap(&pdev->dev, sonos_rollback->base);
fail_ioremap:
fail_get_resource:
	devm_kfree(&pdev->dev, sonos_rollback);
	return -EINVAL;
}

static int sonos_rollback_remove(struct platform_device *pdev)
{
	sonos_rollback_dev * sonos_rollback = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

#ifdef CONFIG_PROC_FS
	proc_remove(sonos_rollback->fallback_dir);
	proc_remove(sonos_rollback->counter_dir);
#endif

	devm_iounmap(dev, sonos_rollback->base);
	platform_set_drvdata(pdev, NULL);
	devm_kfree(dev, sonos_rollback);
	global_data = NULL;
	return 0;
}

static const struct of_device_id sonos_rollback_dt_ids[] = {
	{ .compatible = "sonos,sonos-rollback" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sonos_rollback_dt_ids);

static struct platform_driver sonos_rollback_driver = {
	.probe		= sonos_rollback_probe,
	.remove		= sonos_rollback_remove,
	.driver		= {
		.name		= DRV_NAME,
		.of_match_table	= sonos_rollback_dt_ids,
	},
};
module_platform_driver(sonos_rollback_driver);
