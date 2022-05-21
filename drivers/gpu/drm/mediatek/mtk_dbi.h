/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MTK_DBI_H_
#define _MTK_DBI_H_

#include <drm/drm_crtc.h>

#include "mtk_drm_ddp_comp.h"

enum mtk_dbi_te_mode {
	DBI_TE_MODE_DISABLED = 0,
	DBI_TE_MODE_VSYNC_ONLY = 1,
	DBI_TE_MODE_VSYNC_OR_HSYNC = 2,
};

enum mtk_dbi_te_edge {
	DBI_TE_RISING = 0,
	DBI_TE_FAILLING = 1,
};

struct mtk_dbi_sif_params {
	u32 sif_port;
	u32 sif_3wire;
	u32 sif_hw_cs;
	u32 sif_1st_pol;
	u32 sif_sck_def;
	u32 sif_div2;
	u32 sif_sdi;

	u32 cs_setup;
	u32 cs_hold;
	u32 rd_1st;
	u32 rd_2nd;
	u32 wr_1st;
	u32 wr_2nd;

	u32 te_mode;
	u32 te_edge;
};

struct mtk_dbi {
	struct mtk_ddp_comp ddp_comp;
	struct device *dev;
	struct drm_encoder encoder;
	struct drm_connector conn;
	struct device_node *panel_node, *device_node;
	struct drm_panel *panel;
	struct drm_bridge *bridge;

	struct pinctrl *dbi_pinctrl;
	struct pinctrl_state *pins_gpio_mode;
	struct pinctrl_state *pins_dbi_mode;

	struct mipi_dbi_bus bus;
	struct mtk_dbi_sif_params sif_params;
	void __iomem *regs;

	struct clk *top_fdbi;
	struct clk *top_fdbi_sel;
	struct clk *top_mainpll_d12;

	u32 data_rate;

	unsigned long mode_flags;

	u32 bus_width;
	u32 data_width;
	enum mipi_dbi_format format;
	struct videomode vm;
	int refcount;
	bool enabled, poweron;
	int irq_num, irq_data;

#if defined(CONFIG_DEBUG_FS)
	struct dentry *debugfs;
#endif
};

static inline struct mtk_dbi *bus_to_dbi(struct mipi_dbi_bus *h)
{
	return container_of(h, struct mtk_dbi, bus);
}

static inline struct mtk_dbi *encoder_to_dbi(struct drm_encoder *e)
{
	return container_of(e, struct mtk_dbi, encoder);
}

static inline struct mtk_dbi *connector_to_dbi(struct drm_connector *c)
{
	return container_of(c, struct mtk_dbi, conn);
}

void mtk_dbi_dump_registers(struct mtk_dbi *dbi);

#endif
