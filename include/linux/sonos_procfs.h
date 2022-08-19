/*
 * Copyright (c) 2014-2020, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#ifndef _SONOS_PROCFS_H
#define _SONOS_PROCFS_H

#include <generated/autoconf.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define SONOS_PROCFS_PERM_READ	(S_IRUSR | S_IRGRP | S_IROTH)
#define SONOS_PROCFS_PERM_WRITE	(S_IWUSR | S_IWGRP | S_IWOTH)

#define SONOS_PROCFS_INIT_WITH_SIZE(procReadFunc, procWriteFunc, fopsStr, size)	        \
	static int open_ ## procReadFunc(struct inode *inode, struct file *file)	\
	{										\
		return single_open_size(file, procReadFunc, PDE_DATA(inode), size);	\
	}										\
	static const struct file_operations fopsStr =					\
	{										\
		.owner = THIS_MODULE,							\
		.open = open_ ## procReadFunc,						\
		.write = procWriteFunc,							\
		.read = seq_read,							\
		.llseek = seq_lseek,							\
		.release = single_release,						\
	};

#define SONOS_PROCFS_INIT(procReadFunc, procWriteFunc, fopsStr)	\
    SONOS_PROCFS_INIT_WITH_SIZE(procReadFunc, procWriteFunc, fopsStr, PAGE_SIZE)

#endif // ifndef _SONOS_PROCFS_H
