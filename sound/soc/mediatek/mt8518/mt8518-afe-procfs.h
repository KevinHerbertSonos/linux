/*
 * mt8518-afe-procfs.h  --  Mediatek 8518 audio procfs
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

#ifndef __MT8518_AFE_PROCFS_H__
#define __MT8518_AFE_PROCFS_H__

struct mtk_base_afe;

void mt8518_afe_spdifin_errors_track(struct mtk_base_afe *afe, unsigned int err);

void mt8518_afe_irqs_track(struct mtk_base_afe *afe, int irq_id);

void mt8518_afe_init_procfs(struct mtk_base_afe *afe);

void mt8518_afe_cleanup_procfs(struct mtk_base_afe *afe);

#endif
