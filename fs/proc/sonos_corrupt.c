/*
 * Copyright (c) 2018, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 *
 * linux/fs/proc/sonos_corrupt.c
 *
 * Functions for detecting memory corruption.
 * This is specifically implemented to look for a
 * memory corruption related to the MT8521p "conn" domain.
 * See sonos_mtk_disable_conn_pd.patch.
 */

#include <linux/mm.h>
#include <linux/unistd.h>
#include <linux/uaccess.h>
#include <linux/rcutree.h>
#include <linux/kallsyms.h>

#define TARGET1		((volatile void *)0xc0080400)
#define TARGET2		((volatile void *)0xc008001c)
#define TEST_SIZE	(32)
u32 init = 0;
u8 before1_obj[TEST_SIZE];
u8 before2_obj[TEST_SIZE];

int sonos_corrupt_read(void *buff, size_t len, volatile void *loc)
{
	int err;
	mm_segment_t fs;

	fs = get_fs();
	set_fs(KERNEL_DS);

	err = probe_kernel_read(buff, (const void *)loc, len);

	set_fs(fs);
	barrier();

	return err;
}

void sonos_corrupt_init(void)
{
	init = 1;
	if (sonos_corrupt_read(before1_obj, sizeof(before1_obj), TARGET1)) {
		printk("[CORRUPT] failed to init 1\n");
		init = 0;
	}
	if (sonos_corrupt_read(before2_obj, sizeof(before2_obj), TARGET2)) {
		printk("[CORRUPT] failed to init 2\n");
		init = 0;
	}

	if (init) {
		printk("[CORRUPT] store init\n");
	}
}

void sonos_corrupt_test(volatile void *loc, u8 *before_obj)
{
	u8 after_obj[TEST_SIZE];

	if (sonos_corrupt_read(after_obj, sizeof(after_obj), loc)) {
		printk("[CORRUPT] failed to check @ %p\n", loc);
	}
	else {
		if (memcmp(before_obj, after_obj, sizeof(after_obj))) {
			printk("[CORRUPT] ######## MEMORY CORRUPTION @ %p ########\n", loc);
			printk("\t\tBEFORE: %32ph\n", before_obj);
			printk("\t\tAFTER : %32ph\n", after_obj);
			memcpy(before_obj, after_obj, sizeof(after_obj));
		}
	}
}

void sonos_corrupt_check(void)
{
	if (!init) {
		sonos_corrupt_init();
	}
	else {
		sonos_corrupt_test(TARGET1, before1_obj);
		sonos_corrupt_test(TARGET2, before2_obj);
	}
}
