/*
 *  drivers/mtd/nand/nand_rincon.c
 *
 *  Copyright (c) 2003 Texas Instruments
 *
 *  Derived from drivers/mtd/autcpu12.c
 *
 *  Copyright (c) 2002 Thomas Gleixner <tgxl@linutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Overview:
 *   This is a device driver for the NAND flash device found on the
 *   TI fido board. It supports 32MiB and 64MiB cards
 *
 * $Id: nand_rincon.c,v 1.19.16.1 2006/09/12 19:38:28 tober Exp $
 */

#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand-gpio.h>

#include "mdp.h"
#include "sonos_ptable.h"

#define NAND_RINCON_NUM_PARTITIONS	10
#define NAND_RINCON_OOB_OFFSET		8
#define NAND_RINCON_OOB_LEN		8

extern struct manufacturing_data_page sys_mdp;

struct nand_badmap_ent {
	int pbase;
	int lbase;
	int nblocks;
} nand_rincon_badmap[36];

int nand_rincon_badmap_ents = -1;

int bootsection = 0;
int bootgeneration = 0;

char nandroot[64];

static struct mtd_partition nand_rincon_partition_info[NAND_RINCON_NUM_PARTITIONS] = {
	{ .name =	"whole" },
	{ .name =	"reserved" },
	{ .name =	"kernel0" },
	{ .name =	"rootfs0" },
	{ .name =	"jffs" },
	{ .name =	"kernel1" },
	{ .name =	"rootfs1" },
	{ .name =	"<unused>" },
	{ .name =	"<unused>" },
	{ .name =	"<unused>" },
};

struct ptable sys_pt = {
	0, /*magic*/
	0, /*flags*/
	{ { 0, 0, 0 } }
};

extern struct manufacturing_data_page sys_mdp;

#ifdef CONFIG_SONOS
#define GPIO_NCE 0x5c
#define GPIO_ALE 0x17
#define GPIO_CLE 0x16
#define GPIO_RDY 0x5d

int nand_init_alecle(void)
{
	int ret;

	ret = gpio_request(GPIO_NCE, "NAND NCE");
	if (ret)
		goto err;
	gpio_direction_output(GPIO_NCE, 1);

	ret = gpio_request(GPIO_ALE, "NAND ALE");
	if (ret)
		goto err;
	gpio_direction_output(GPIO_ALE, 0);

	ret = gpio_request(GPIO_CLE, "NAND CLE");
	if (ret)
		goto err;
	gpio_direction_output(GPIO_CLE, 0);

	ret = gpio_request(GPIO_RDY, "NAND RDY");
	if (ret)
		goto err;
	gpio_direction_input(GPIO_RDY);

	printk("alecle init done\n");
	return 0;
err:
	printk("gpio init failed\n");
	return 1;
}

void nand_free_alecle(void)
{
	if (gpio_is_valid(GPIO_NCE))
		gpio_free(GPIO_NCE);
	if (gpio_is_valid(GPIO_ALE))
		gpio_free(GPIO_ALE);
	if (gpio_is_valid(GPIO_CLE))
		gpio_free(GPIO_CLE);
	if (gpio_is_valid(GPIO_RDY))
		gpio_free(GPIO_RDY);
}

void nand_ctrl(unsigned int ctrl)
{
	gpio_set_value(GPIO_NCE, !(ctrl & NAND_NCE));
	udelay(1);
	gpio_set_value(GPIO_CLE, !!(ctrl & NAND_CLE));
	udelay(1);
	gpio_set_value(GPIO_ALE, !!(ctrl & NAND_ALE));
}

static int nand_dev_ready(void)
{
	return gpio_get_value(GPIO_RDY);
}

void early_read_mdp(void)
{
	unsigned char buf[528];
	int x;
	int ret;
	void __iomem *io_addr;

	io_addr = ioremap(0xF8000000, 32);

	ret = nand_init_alecle();
	if (ret)
		goto err;

	nand_ctrl(NAND_NCE | NAND_CLE);
	udelay(1);
	writeb(0,io_addr);
	udelay(1);
	nand_ctrl(NAND_NCE | NAND_ALE);
	udelay(1);
	writeb(0,io_addr);
	ndelay(50);
	writeb(0,io_addr);
	ndelay(50);
	writeb(0,io_addr);
	ndelay(50);
	writeb(0,io_addr);
	udelay(1);
	nand_ctrl(NAND_NCE);
	udelay(1);
	while (!nand_dev_ready()) ;
	for (x = 0; x < 528; x ++) {
		buf[x]=readb(io_addr);
		ndelay(50);
	}
	nand_ctrl(0);
	memcpy(&sys_mdp, buf, sizeof(sys_mdp));

	printk("early_read_mdp: success, model num %d\n", sys_mdp.mdp_model);
err:
	iounmap(io_addr);
	nand_free_alecle();
}
#endif

int nand_rincon_ltop(int b)
{
	int x;
	for ( x = 0; x <= nand_rincon_badmap_ents; x ++ ) {
		if ( b < (nand_rincon_badmap[x].lbase+nand_rincon_badmap[x].nblocks) )
			return nand_rincon_badmap[x].pbase + b - nand_rincon_badmap[x].lbase;
	}
	return -1;
}

int nand_rincon_check_partition(struct mtd_info *mtd, int start, int end)
{
	u_char oobbuf[NAND_RINCON_OOB_LEN];
	struct mtd_oob_ops ops;
	int g = 0, h, page;
	int err = 0;

	memset(&ops, 0, sizeof(ops));

	ops.mode      = MTD_OOB_RAW;
	ops.ooblen    = NAND_RINCON_OOB_LEN;
	ops.ooboffs   = NAND_RINCON_OOB_OFFSET;
	ops.oobbuf    = oobbuf;

	page = start << 5;
	while ( page <= ((end << 5) + 31) ) {
		if ( (page & 31) == 0 ) {
			if ( mtd->block_isbad(mtd, page << 9) ) {
				printk("nand_rincon_check_partition: block %d is bad\n", page >> 5);
				page += 32;
				continue;
			}
		}
		err = mtd->read_oob(mtd, page << 9, &ops);
		if ( err ) {
			printk("nand_rincon_check_partition: read failed %d\n", err);
			return -1;
		}
		if ( !(((oobbuf[0] == 0x19) && (oobbuf[1] == 0x74)) ||
		       ((oobbuf[0] == 0x74) && (oobbuf[1] == 0x19))) ) {
			printk("nand_rincon_check_partition: bad magic\n");
			return -1;
		}
		h = oobbuf[2] | (oobbuf[3] << 8);
		if ( h == 0 ) {
			printk("nand_rincon_check_partition: bad generation(0)\n");
			return -1;
		}
		if ( g == 0 )
			g = h;
		if ( g != h) {
			printk("nand_rincon_check_partition: generation mismatch\n");
			return -1;
		}

		if ( (oobbuf[0] == 0x74) && (oobbuf[1] == 0x19) ) {
			printk("nand_rincon_check_partition: success (generation %u)\n",g);
			return g;
		}

		if ( sys_pt.pt_magic != PT_MAGIC )
			return g;

		if ( page == ((end << 5) + 31) )
			break;
		page = (end << 5) + 31;
	}
	printk("nand_rincon_check_partition: reached the end of the partition before the end of the file (%d %d %d)\n", start, end, page);
	return -1;
}

int nand_rincon_scan(struct mtd_info *mtd)
{
	int page, lblock = 0;
	int lastbad = 1;
	int r, x;
	int section = 0;
	size_t rl;
	u_char buf[mtd->writesize];
	struct ptable *pt;
	int n[2] = {0, 0};
	int g[10];
	int skip = 0;
	int s, e;

	nand_rincon_badmap_ents = -1;
	nand_rincon_badmap[0].lbase = 0;
	nand_rincon_badmap[0].nblocks = 0;

	for ( page = 0; page < ((mtd->size) >> 9); page += 32 ) {
		if ( (mtd->block_isbad(mtd, page << 9)) ) {
			lastbad = 1;
		} else {
			if ( lastbad ) {
				nand_rincon_badmap_ents ++;
				nand_rincon_badmap[nand_rincon_badmap_ents].pbase = page >> 5;
				nand_rincon_badmap[nand_rincon_badmap_ents].lbase = lblock;
				nand_rincon_badmap[nand_rincon_badmap_ents].nblocks = 0;
			}
			lastbad = 0;
			nand_rincon_badmap[nand_rincon_badmap_ents].nblocks ++;
			lblock ++;
		}
	}

	for ( x = 0; x <= nand_rincon_badmap_ents; x ++ ) {
		printk("NAND badmap entry %d: lbase %d pbase %d nblocks %d\n", x, nand_rincon_badmap[x].lbase, nand_rincon_badmap[x].pbase, nand_rincon_badmap[x].nblocks);
	}

	r = mtd->read(mtd, 0, mtd->writesize, &rl, buf);
	if ( (r != 0) || (rl != mtd->writesize) ) {
		printk("read MDP failed\n");
		goto read_pt;
	}

	memcpy(&sys_mdp, buf, sizeof(sys_mdp));

	if( sys_mdp.mdp_magic != MDP_MAGIC ) {
		printk("It looks like this system has no MDP\n");
		goto read_pt;
	}

read_pt:
	r = mtd->read(mtd, nand_rincon_ltop(1) << 14, mtd->writesize, &rl, buf);
	if ( (r != 0) || (rl != mtd->writesize) ) {
		printk("Egads, read partition table failed.\n");
		goto fake_it;
	}

	pt = ( struct ptable * ) buf;

	if ( pt->pt_magic != PT_MAGIC ) {
		printk("It looks like this system has no partition table\n");
		goto fake_it;
	}
	memcpy(&sys_pt, pt, sizeof(sys_pt));

fake_it:
	for ( x = 1; x < NAND_RINCON_NUM_PARTITIONS; x ++ ) {
		if ( sys_pt.pt_entries[x - 1].pe_type == PE_TYPE_END ) break;
		if ( sys_pt.pt_flags & PTF_REL ) {
			s = nand_rincon_ltop(sys_pt.pt_entries[x - 1].pe_start);
			e = nand_rincon_ltop(sys_pt.pt_entries[x - 1].pe_start + sys_pt.pt_entries[x - 1].pe_nblocks - 1) - s + 1;
		} else {
			s = sys_pt.pt_entries[x - 1].pe_start;
			e = sys_pt.pt_entries[x - 1].pe_nblocks;
		}
		nand_rincon_partition_info[x].offset = s * mtd->erasesize;
		nand_rincon_partition_info[x].size = e * mtd->erasesize;
		printk("Partition %d: offset %llu size %llu\n", x, nand_rincon_partition_info[x].offset, nand_rincon_partition_info[x].size);
	}

	if ( sys_pt.pt_magic != PT_MAGIC ) {
		printk("Skipping partition check\n");
		bootgeneration = nand_rincon_check_partition(mtd, 16, 63);
		goto skip_partition_check;
	}

	for ( x = 0; x < NAND_RINCON_NUM_PARTITIONS; x ++ ) {
		if ( sys_pt.pt_entries[x].pe_type == PE_TYPE_END )
			break;
		if ( sys_pt.pt_entries[x].pe_type == PE_TYPE_JFFS) {
			section ++;
			skip = 0;
			continue;
		}
		if ( skip )
			continue;
		if ( (sys_pt.pt_entries[x].pe_type != PE_TYPE_KERNEL)&&
		     (sys_pt.pt_entries[x].pe_type != PE_TYPE_ROOT) )
			continue;

		if ( sys_pt.pt_flags & PTF_REL ) {
			s = nand_rincon_ltop(sys_pt.pt_entries[x].pe_start);
			e = nand_rincon_ltop(sys_pt.pt_entries[x].pe_start + sys_pt.pt_entries[x].pe_nblocks - 1);
		} else {
			s = sys_pt.pt_entries[x].pe_start;
			e = sys_pt.pt_entries[x].pe_start + sys_pt.pt_entries[x].pe_nblocks - 1;
		}

		g[x] = nand_rincon_check_partition(mtd, s, e);
		if ( g[x] <= 0 ) {
			n[section] = 0;
			skip = 1;
			continue;
		}

		if ( n[section]==0 )
			n[section] = g[x];
		if ( g[x] != n[section]) {
			n[section] = 0;
			skip = 1;
			continue;
		}
	}

	if ( n[1] > n[0] ) {
		printk("Selected boot section 1 (generation %d)\n",n[1]);
		bootsection = 1;
		bootgeneration = n[1];
        } else if ( n[0] > 0 ) {
                printk("Selected boot section 0 (generation %d)\n",n[0]);
		bootsection = 0;
		bootgeneration = n[0];
        } else {
                panic("No good boot section\n");
        }

skip_partition_check:
	if ( bootsection ) {
		snprintf(nandroot, sizeof(nandroot), "/dev/mtdblock6");
	} else {
		snprintf(nandroot, sizeof(nandroot), "/dev/mtdblock3");
	}
	printk("Using %s for rootfs\n", nandroot);

	return 0;
}

void nand_rincon_adjust_parts(struct gpio_nand_platdata * plat, size_t size)
{
	nand_rincon_partition_info[0].offset = 0;
	nand_rincon_partition_info[0].size = size;
	plat->parts = nand_rincon_partition_info;
	plat->num_parts = NAND_RINCON_NUM_PARTITIONS;
}
