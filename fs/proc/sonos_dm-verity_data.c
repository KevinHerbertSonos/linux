/*
 * Copyright (c) 2018, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 *
 * linux/fs/proc/dm-verity_data.c
 *
 * Exposes files in /proc with information necessary to set up live integrity
 * checking for the rootfs with dm-verity's `veritysetup create'.
 *
 * Because verity metadata is built after the kernel is built in the Sonos build
 * system, the values in these files need to be inserted after kernel build
 * time. See all/cc/upgrade/app/gen_dm-verity_data.c
 *
 * These procfiles are intended to be read by the initramfs as part of rootfs
 * mounting.
 *
 * NOTE: The 4.9 kernel doesn't have a kernel decompressor for arm64, so if
 * this code is ever used with an ARCH that uses a kernel decompressor, it
 * should be adapted so that the kernel decompressor has a copy of this struct
 * and the magic numbers marking it, and the decompressor should populate this
 * struct after decompressing this part of the kernel. See the scheme in the
 * 3.10 and 4.4 kernels for inserting the rootfs digest as a reference if you're
 * trying to make this implementation compatible with a kernel decompressor.
 */

#ifndef CONFIG_SONOS_USES_DM_VERITY
#error "This code should only be used to expose values for dm-verity."
#endif

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/module.h>

#include "sonos_dm-verity_data.h"

sonos_dmverity_data_t dmverity_data = {
	.magic = SONOS_DMVERITY_MAGIC,
  /* Other fields left uninitialized -- will be inserted by build system */
};

/*	/proc/sonos-dmverity/hash_offset
 *		(read-only)
 *
 *	The build system installs the dm-verity metadata and hash tree data on
 *	device blocks just after the root filesystem squashfs image. In order to
 *	find dm-verity data in userspace, the build system will insert the offset
 *	from the start of the eMMC block to the start of the hash tree data, and
 *	this value will be read in the initramfs.
 *
 *	This value should be passed to veritysetup utilities with the
 *	--hash-offset option.
 */
static int hash_offset_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", be32_to_cpu(dmverity_data.hash_offset_be));
	return 0;
}

static int hash_offset_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, hash_offset_proc_show, NULL);
}

static const struct file_operations hash_offset_proc_fops = {
	.open = hash_offset_proc_open,
	.read = seq_read,
	.write = NULL,
	.llseek = seq_lseek,
	.release = single_release,
};

/*	/proc/sonos-dmverity/root_hash
 *		(read-only)
 *
 *	The root hash is needed to "unlock" dm-verity hash tree data for use
 *	in verification of the filesystem. The build system will insert the
 *	root hash to the kernel, and the kernel publishes it to be read in the
 *	initramfs.
 */
static int root_hash_proc_show(struct seq_file *m, void *v)
{
	size_t i;
	for (i = 0; i < SONOS_DMVERITY_ROOT_HASH_LENGTH; i++) {
		seq_printf(m, "%c", dmverity_data.root_hash[i]);
	}
	seq_printf(m, "\n");
	return 0;
}

static int root_hash_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, root_hash_proc_show, NULL);
}

static const struct file_operations root_hash_proc_fops = {
	.open = root_hash_proc_open,
	.read = seq_read,
	.write = NULL,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init proc_dmverity_init(void)
{
	printk(KERN_CRIT "proc_dmverity_init\n");
	proc_create("sonos-dmverity/hash_offset", 0, NULL, &hash_offset_proc_fops);
	proc_create("sonos-dmverity/root_hash", 0, NULL, &root_hash_proc_fops);
	return 0;
}
module_init(proc_dmverity_init);
