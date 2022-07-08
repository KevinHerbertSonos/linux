/*
 * mt8518-afe-utils.h  --  Mediatek 8518 audio utility
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Hidalgo Huang <hidalgo.huang@mediatek.com>
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

#ifndef _MT8518_AFE_UTILS_H_
#define _MT8518_AFE_UTILS_H_

#include <linux/types.h>

struct mtk_base_afe;
struct clk;

int mt8518_afe_init_audio_clk(struct mtk_base_afe *afe);

int mt8518_afe_enable_clk(struct mtk_base_afe *afe, struct clk *clk);

void mt8518_afe_disable_clk(struct mtk_base_afe *afe, struct clk *clk);

int mt8518_afe_set_clk_rate(struct mtk_base_afe *afe, struct clk *clk,
			    unsigned int rate);

int mt8518_afe_set_clk_parent(struct mtk_base_afe *afe, struct clk *clk,
			      struct clk *parent);

int mt8518_afe_enable_top_cg(struct mtk_base_afe *afe, unsigned int cg_type);

int mt8518_afe_disable_top_cg(struct mtk_base_afe *afe, unsigned int cg_type);

int mt8518_afe_enable_main_clk(struct mtk_base_afe *afe);

int mt8518_afe_disable_main_clk(struct mtk_base_afe *afe);

int mt8518_afe_enable_reg_rw_clk(struct mtk_base_afe *afe);

int mt8518_afe_disable_reg_rw_clk(struct mtk_base_afe *afe);

int mt8518_afe_enable_afe_on(struct mtk_base_afe *afe);

int mt8518_afe_disable_afe_on(struct mtk_base_afe *afe);

int mt8518_afe_enable_irq(struct mtk_base_afe *afe, unsigned int irq_id);

int mt8518_afe_disable_irq(struct mtk_base_afe *afe, unsigned int irq_id);

int mt8518_afe_handle_spdif_in_port_change(struct mtk_base_afe *afe,
	unsigned int port);

int mt8518_afe_block_dpidle(struct mtk_base_afe *afe);

int mt8518_afe_unblock_dpidle(struct mtk_base_afe *afe);

int mt8518_afe_get_be_idx(int id);

int mt8518_afe_set_be_active(struct mtk_base_afe *afe, int id);

int mt8518_afe_clear_be_active(struct mtk_base_afe *afe, int id);

bool mt8518_afe_is_be_active(struct mtk_base_afe *afe, int id);

#endif
