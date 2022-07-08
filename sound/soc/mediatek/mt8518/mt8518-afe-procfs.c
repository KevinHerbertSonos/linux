/*
 * mt8518-afe-procfs.c  --  Mediatek 8518 audio procfs
 *
 * Copyright (c) 2021 Sonos Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "mt8518-afe-procfs.h"
#include "mt8518-reg.h"
#include "mt8518-afe-utils.h"
#include "mt8518-afe-common.h"
#include "../common/mtk-base-afe.h"
#include <linux/proc_fs.h>

#define ENUM_TO_STR(enum) #enum

#define PROCFS_DIR		"driver/mt8518-afe-pcm"
#define PROCFS_IRQ_FILE		"irq"
#define PROCFS_SPDIFIN_FILE	"spdifin"

static const char *irq_name[] = {
	ENUM_TO_STR(MT8518_AFE_IRQ1), /* SPDIF OUT */
	ENUM_TO_STR(MT8518_AFE_IRQ2), /* SPDIF IN DETECT */
	ENUM_TO_STR(MT8518_AFE_IRQ3), /* SPDIF IN DATA */
	ENUM_TO_STR(MT8518_AFE_IRQ10),
	ENUM_TO_STR(MT8518_AFE_IRQ11),
	ENUM_TO_STR(MT8518_AFE_IRQ12),
	ENUM_TO_STR(MT8518_AFE_IRQ13),
	ENUM_TO_STR(MT8518_AFE_IRQ14),
	ENUM_TO_STR(MT8518_AFE_IRQ15),
	ENUM_TO_STR(MT8518_AFE_IRQ16),
	ENUM_TO_STR(MT8518_AFE_IRQ17),
	ENUM_TO_STR(MT8518_AFE_IRQ18),
	ENUM_TO_STR(MT8518_AFE_IRQ19),
	ENUM_TO_STR(MT8518_AFE_IRQ20),
	ENUM_TO_STR(MT8518_AFE_IRQ21),
};

static const unsigned int spdifin_stat_mask[] = {
	AFE_SPDIFIN_DEBUG3_PRE_ERR_NON_STS,
	AFE_SPDIFIN_DEBUG3_PRE_ERR_B_STS,
	AFE_SPDIFIN_DEBUG3_PRE_ERR_M_STS,
	AFE_SPDIFIN_DEBUG3_PRE_ERR_W_STS,
	AFE_SPDIFIN_DEBUG3_PRE_ERR_BITCNT_STS,
	AFE_SPDIFIN_DEBUG3_PRE_ERR_PARITY_STS,
	AFE_SPDIFIN_DEBUG3_TIMEOUT_ERR_STS,
	AFE_SPDIFIN_DEBUG1_DATALAT_ERR,
	AFE_SPDIFIN_INT_EXT2_LRCK_CHANGE,
	AFE_SPDIFIN_DEBUG2_FIFO_ERR,
};

#define SPDIFIN_STAT_NUM	ARRAY_SIZE(spdifin_stat_mask)

static const char *spdifin_stat_name[] = {
	ENUM_TO_STR(AFE_SPDIFIN_DEBUG3_PRE_ERR_NON_STS),
	ENUM_TO_STR(AFE_SPDIFIN_DEBUG3_PRE_ERR_B_STS),
	ENUM_TO_STR(AFE_SPDIFIN_DEBUG3_PRE_ERR_M_STS),
	ENUM_TO_STR(AFE_SPDIFIN_DEBUG3_PRE_ERR_W_STS),
	ENUM_TO_STR(AFE_SPDIFIN_DEBUG3_PRE_ERR_BITCNT_STS),
	ENUM_TO_STR(AFE_SPDIFIN_DEBUG3_PRE_ERR_PARITY_STS),
	ENUM_TO_STR(AFE_SPDIFIN_DEBUG3_TIMEOUT_ERR_STS),
	ENUM_TO_STR(AFE_SPDIFIN_DEBUG1_DATALAT_ERR),
	ENUM_TO_STR(AFE_SPDIFIN_INT_EXT2_LRCK_CHANGE),
	ENUM_TO_STR(AFE_SPDIFIN_DEBUG2_FIFO_ERR),
};

struct mt8518_afe_procfs {
	struct proc_dir_entry *dir;
	struct proc_dir_entry *irq;
	struct proc_dir_entry *spdifin;

	u32 irq_counts[MT8518_AFE_IRQ_NUM];
	u32 spdif_stats[SPDIFIN_STAT_NUM];
};

/*
 * "err" is created by spdif_in_detect_irq_handler() and is an OR of
 * error flags from 4 different registers that do not overlap.
 */
void mt8518_afe_spdifin_errors_track(struct mtk_base_afe *afe, unsigned int err)
{
	struct mt8518_afe_private *afe_priv = afe->platform_priv;
	struct mt8518_afe_procfs *afe_procfs = afe_priv->procfs;
	int idx;

	for (idx = 0; idx < SPDIFIN_STAT_NUM; idx++) {
		if (err & spdifin_stat_mask[idx]) afe_procfs->spdif_stats[idx]++;
	}
}

static int procfs_spdifin_show(struct seq_file *m, void *v)
{
	struct mtk_base_afe *afe = m->private;
	struct mt8518_afe_private *afe_priv = afe->platform_priv;
	struct mt8518_afe_procfs *afe_procfs = afe_priv->procfs;
	int idx;

	for (idx = 0; idx < SPDIFIN_STAT_NUM; idx++) {
		seq_printf(m, "%-37s = %d\n", spdifin_stat_name[idx], afe_procfs->spdif_stats[idx]);
	}

	seq_printf(m, "\n");
	seq_printf(m, "rate = %d\n", afe_priv->spdif_in_data.subdata.rate);
	seq_printf(m, "csb  = %*ph\n", SPDIF_CHSTS_NUM, afe_priv->spdif_in_data.subdata.ch_status);

	return 0;
}

static int procfs_spdifin_open(struct inode *inode, struct file *file)
{
	return single_open(file, procfs_spdifin_show, PDE_DATA(inode));
}

static const struct file_operations procfs_spdifin_ops = {
	.owner = THIS_MODULE,
	.open = procfs_spdifin_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void mt8518_afe_irqs_track(struct mtk_base_afe *afe, int irq_id)
{
	struct mt8518_afe_private *afe_priv = afe->platform_priv;
	struct mt8518_afe_procfs *afe_procfs = afe_priv->procfs;

	afe_procfs->irq_counts[irq_id]++;
}

static int procfs_irq_show(struct seq_file *m, void *v)
{
	struct mtk_base_afe *afe = m->private;
	struct mt8518_afe_private *afe_priv = afe->platform_priv;
	struct mt8518_afe_procfs *afe_procfs = afe_priv->procfs;
	int idx;
	int total = 0;

	for (idx = 0; idx < MT8518_AFE_IRQ_NUM; idx++) {
		seq_printf(m, "%-16s = %d\n", irq_name[idx], afe_procfs->irq_counts[idx]);
		total += afe_procfs->irq_counts[idx];
	}
	seq_printf(m, "TOTAL = %d\n", total);

	return 0;
}

static int procfs_irq_open(struct inode *inode, struct file *file)
{
	return single_open(file, procfs_irq_show, PDE_DATA(inode));
}

static const struct file_operations procfs_irq_ops = {
	.owner = THIS_MODULE,
	.open = procfs_irq_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void mt8518_afe_init_procfs(struct mtk_base_afe *afe)
{
	struct mt8518_afe_private *afe_priv = afe->platform_priv;
	struct mt8518_afe_procfs *afe_procfs;

	afe_procfs = devm_kzalloc(afe->dev, sizeof(struct mt8518_afe_procfs), GFP_KERNEL);
	if (!afe_procfs) {
		dev_err(afe->dev, "unable to allocate procfs struct\n");
		return;
	}
	afe_priv->procfs = afe_procfs;

	afe_procfs->dir = proc_mkdir(PROCFS_DIR, NULL);
	if (!afe_procfs->dir) {
		dev_err(afe->dev, "unable to create procfs dir %s\n", PROCFS_DIR);
		return;
	}

	afe_procfs->irq = proc_create_data(PROCFS_IRQ_FILE, (S_IRUSR | S_IRGRP | S_IROTH), afe_procfs->dir, &procfs_irq_ops, afe);
	if (!afe_procfs->irq) {
		dev_err(afe->dev, "unable to create procfs file %s\n", PROCFS_IRQ_FILE);
		return;
	}

	afe_procfs->spdifin = proc_create_data(PROCFS_SPDIFIN_FILE, (S_IRUSR | S_IRGRP | S_IROTH), afe_procfs->dir, &procfs_spdifin_ops, afe);
	if (!afe_procfs->spdifin) {
		dev_err(afe->dev, "unable to create procfs file %s\n", PROCFS_SPDIFIN_FILE);
		return;
	}
}

void mt8518_afe_cleanup_procfs(struct mtk_base_afe *afe)
{
	struct mt8518_afe_private *afe_priv = afe->platform_priv;
	struct mt8518_afe_procfs *afe_procfs = afe_priv->procfs;

	if (afe_procfs->spdifin) {
		remove_proc_entry(PROCFS_SPDIFIN_FILE, afe_procfs->dir);
	}
	if (afe_procfs->irq) {
		remove_proc_entry(PROCFS_IRQ_FILE, afe_procfs->dir);
	}
	if (afe_procfs->spdifin) {
		remove_proc_entry(PROCFS_SPDIFIN_FILE, afe_procfs->dir);
	}
}
