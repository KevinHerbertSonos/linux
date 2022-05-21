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
#include <drm/drmP.h>
#include <drm/drm_gem.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_mipi_dbi.h>
#include <drm/drm_panel.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>

#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/regulator/consumer.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <video/mipi_display.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <video/videomode.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/pinctrl/consumer.h>

#include "mtk_drm_drv.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp.h"
#include "mtk_drm_gem.h"
#include "mtk_dbi.h"
#include "mtk_drm_debugfs.h"

#define DBI_STA	0x00
#define BUSY	BIT(4)

#define DBI_INTEN	0x04
#define DBI_APB_TIMEOUT	BIT(5)
#define DBI_TE	BIT(4)
#define DBI_SYNC	BIT(3)
#define DBI_HTT	BIT(2)
#define DBI_CPL	BIT(0)

#define DBI_INTSTA	0x08

#define DBI_START	0x0c
#define DBI_SRST		BIT(0)

#define DBI_RSTB	0x10
#define DBI_LCM_RST		BIT(0)

#define DBI_SIF_PIX_CON	0x18
#define DBI_SIF_TIMING0	0x1c
#define DBI_SIF_TIMING1	0x20

#define DBI_SCNF	0x28
#define DBI_SCNF_CS	0x2c
#define DBI_PCNF0	0x30
#define DBI_PCNF1	0x34
#define DBI_PCNF2	0x38
#define DBI_PDW	0x3c
#define DBI_TECON	0x40
#define DBI_SEN	BIT(0)
#define DBI_TEG	BIT(1)
#define DBI_SM	BIT(2)

#define DBI_CALC_HTT	0x44
#define DBI_SYNC_LCM_SIZE	0x48
#define DBI_SYNC_CNT	0x4c
#define DBI_ROICON	0x60
#define IF24	BIT(25)

#define DBI_ROI_CADD	0x64
#define DBI_ROI_DADD	0x68
#define DBI_ROI_SIZE	0x6c
#define DBI_ULTRA_CON	0x90
#define DBI_CONSUME_RATE	0x94
#define DBI_DBI_ULTRA_TH	0x98
#define DBI_DBI_LCM	0xa8
#define DBI_SERIAL_CHKSUM	0xe0
#define DBI_PARALLEL_CHKSUM	0xe4
#define DBI_PATTERN	0xe8
#define DBI_SIF_STR_BYTE_CON	0x270
#define DBI_SIF_WR_STR_BYTE	0x278
#define DBI_SIF_RD_STR_BYTE	0x27C
#define DBI_SIF_VDO_SYNC_CON0	0x290
#define DBI_SIF_VDO_SYNC_CON1	0x294
#define DBI_SIF_FR_DURATION	0x298
#define DBI_SIF_VDO_HEADER	0x29C
#define DBI_SERIAL_PAD_SEL	0x300
#define DBI_PAD_DELAY_SEL	0x304
#define DBI_SERIAL_PAD_CON	0x310
#define DBI_SIF_CMD_LOCK	0x320
#define DBI_DITHER_0	0xe00
#define DBI_DITHER_5	0xe14
#define DBI_DITHER_6	0xe18
#define DBI_DITHER_7	0xe1C
#define DBI_DITHER_8	0xe20
#define DBI_DITHER_9	0xe24
#define DBI_DITHER_10	0xe28
#define DBI_DITHER_11	0xe2c
#define DBI_DITHER_12	0xe30
#define DBI_DITHER_13	0xe34
#define DBI_DITHER_14	0xe38
#define DBI_DITHER_15	0xe3c
#define DBI_DITHER_16	0xe40
#define DBI_DITHER_17	0xe44
#define DBI_PCMD0	0xf00
#define DBI_PDAT0	0xf10
#define DBI_PCMD1	0xf20
#define DBI_PDAT1	0xf30
#define DBI_PCMD2	0xf40
#define DBI_PDAT2	0xf50
#define DBI_SCMD0	0xf80
#define DBI_SPE_SCMD0	0xf88
#define DBI_SDAT0	0xf90
#define DBI_SCMD1	0xfa0
#define DBI_SPE_SCMD1	0xfa8
#define DBI_SDAT1	0xfb0
#define DBI_SPE_SDAT1	0xfb8

static void mtk_dbi_mask(struct mtk_dbi *dbi, u32 offset, u32 mask, u32 data)
{
	u32 temp = readl(dbi->regs + offset);

	writel((temp & ~mask) | (data & mask), dbi->regs + offset);
}

static void mtk_dbi_reset_engine(struct mtk_dbi *dbi)
{
	mtk_dbi_mask(dbi, DBI_START, DBI_SRST, DBI_SRST);
	mtk_dbi_mask(dbi, DBI_START, DBI_SRST, 0);
}

static void mtk_dbi_start(struct mtk_dbi *dbi)
{
	writel(0x0, dbi->regs + DBI_START);
	writel(0x8000, dbi->regs + DBI_START);
}

static void mtk_dbi_wait_for_idle(struct mtk_dbi *dbi)
{
	u32 timeout_ms = 500000; /* total 1s ~ 2s timeout */

	while (timeout_ms--) {
		if (!(readl(dbi->regs + DBI_STA) & BUSY))
			break;

		usleep_range(2, 4);
	}

	if (timeout_ms == 0) {
		DRM_INFO("polling dbi wait idle timeout!\n");

		mtk_dbi_start(dbi);
		mtk_dbi_reset_engine(dbi);
	}
}

static void mtk_dbi_interrupt_enable(struct mtk_dbi *dbi, bool enable)
{
	if (!enable)
		writel(0, dbi->regs + DBI_INTEN);
	else
		writel(DBI_CPL | DBI_SYNC, dbi->regs + DBI_INTEN);
}

static void mtk_dbi_te_control(struct mtk_dbi *dbi, bool enable)
{
	if (!enable)
		writel(0, dbi->regs + DBI_TECON);
	else {
		unsigned int  te_edge = 0;
		unsigned int  te_mode = 0;

		te_mode = dbi->sif_params.te_mode;
		te_edge = dbi->sif_params.te_edge ? DBI_TEG : 0;

		if (te_mode & DBI_TE_MODE_VSYNC_ONLY)
			writel(te_edge | DBI_SEN, dbi->regs + DBI_TECON);
		else if (te_mode & DBI_TE_MODE_VSYNC_OR_HSYNC)
			writel(te_edge | DBI_SEN | DBI_SM,
				dbi->regs + DBI_TECON);
		else
			writel(0, dbi->regs + DBI_TECON);
	}
}

static void mtk_dbi_set_roi(struct mtk_dbi *dbi)
{
	unsigned int roi_con = 0;
	unsigned int cmd_addr = 8;
	unsigned int data_addr = 9;

	roi_con = dbi->format << 3 | dbi->bus_width << 6;

	if (dbi->bus_width == MIPI_DBI_DATA_WIDTH_24BITS)
		roi_con |= IF24;

	writel(roi_con, dbi->regs + DBI_ROICON);

	writel(dbi->vm.hactive | dbi->vm.vactive << 16,
		dbi->regs + DBI_ROI_SIZE);

	cmd_addr += dbi->sif_params.sif_port ? 2 : 0;
	data_addr += dbi->sif_params.sif_port ? 2 : 0;

	writel(cmd_addr << 4, dbi->regs + DBI_ROI_CADD);
	writel(data_addr << 4, dbi->regs + DBI_ROI_DADD);
}

static void mtk_dbi_sif_timing(struct mtk_dbi *dbi)
{
	unsigned int offset = 0;
	unsigned int sif_timing = 0, scnf_timing = 0;

	offset = dbi->sif_params.sif_port ? 8 : 0;

	scnf_timing = dbi->data_width << offset |
		dbi->sif_params.sif_3wire << (offset + 3) |
		dbi->sif_params.sif_sdi << (offset + 4) |
		dbi->sif_params.sif_1st_pol << (offset + 5) |
		dbi->sif_params.sif_sck_def << (offset + 6) |
		dbi->sif_params.sif_div2 << (offset + 7) |
		dbi->sif_params.sif_hw_cs << 24;
	writel(scnf_timing, dbi->regs + DBI_SCNF);

	if (dbi->sif_params.sif_hw_cs)
		writel(1 << (dbi->sif_params.sif_port ? 1 : 0),
		dbi->regs + DBI_SCNF_CS);

	sif_timing = dbi->sif_params.wr_2nd | dbi->sif_params.wr_1st << 4 |
		dbi->sif_params.rd_2nd << 8 | dbi->sif_params.rd_1st << 12 |
		dbi->sif_params.cs_hold << 16 | dbi->sif_params.cs_setup << 20;
	writel(sif_timing, dbi->regs +
		(dbi->sif_params.sif_port ? DBI_SIF_TIMING1 : DBI_SIF_TIMING0));
	writel(0x1, dbi->regs + DBI_DITHER_0);
	writel(0x5, dbi->regs + DBI_DITHER_6);
	writel(0x333, dbi->regs + DBI_DITHER_7);
}

static void mtk_dbi_lcm_reset(struct mtk_dbi *dbi)
{
	mtk_dbi_mask(dbi, DBI_RSTB, DBI_LCM_RST, DBI_LCM_RST);
	usleep_range(20000, 40000);

	mtk_dbi_mask(dbi, DBI_RSTB, DBI_LCM_RST, 0);
	usleep_range(20000, 40000);

	mtk_dbi_mask(dbi, DBI_RSTB, DBI_LCM_RST, DBI_LCM_RST);
	usleep_range(20000, 40000);
}

static int mtk_dbi_power_on(struct mtk_dbi *dbi)
{
	int ret;
	void __iomem *mmsys_base;

	if (++dbi->refcount != 1)
		return 0;

	pinctrl_select_state(dbi->dbi_pinctrl, dbi->pins_dbi_mode);

	ret = clk_prepare_enable(dbi->top_fdbi);
	if (ret < 0) {
		DRM_INFO("[mtk-dbi] Failed to enable top_fdbi clock: %d\n",
			ret);
		goto err_disable_top_fdbi_clk;
	}

	ret = clk_set_parent(dbi->top_fdbi_sel, dbi->top_mainpll_d12);
	if (ret < 0) {
		DRM_INFO("[mtk-dbi] failed to clk_set_parent (%d)\n", ret);
		goto err_disable_top_fdbi_clk;
	}

	ret = clk_prepare_enable(dbi->top_fdbi_sel);
	if (ret < 0) {
		DRM_INFO("[mtk-dbi] Failed to enable top_fdbi_sel clock: %d\n",
			ret);
		goto err_disable_top_fdbi_sel_clk;
	}

	mmsys_base = ioremap(0x14000100, 0x4);
	writel(0x0, mmsys_base);
	return 0;

err_disable_top_fdbi_sel_clk:
	clk_disable_unprepare(dbi->top_fdbi_sel);
err_disable_top_fdbi_clk:
	clk_disable_unprepare(dbi->top_fdbi);

	dbi->refcount--;

	return ret;
}

static ssize_t mtk_dbi_bus_transfer(struct mipi_dbi_bus *bus,
				     const struct mipi_dbi_msg *msg)
{
	struct mtk_dbi *dbi = bus_to_dbi(bus);
	const char *tx_buf = msg->tx_buf;
	u32 i = 0, addr;

	if (!tx_buf || !msg->tx_len)
		return 0;

	addr = (dbi->sif_params.sif_port == 0) ? DBI_SCMD0 : DBI_SCMD1;

	switch (dbi->bus_width) {
	case MIPI_DBI_BUS_WIDTH_32_BITS:
		writel(tx_buf[0], dbi->regs + addr);
		break;
	case MIPI_DBI_BUS_WIDTH_16_BITS:
		writew(tx_buf[0], dbi->regs + addr);
		break;
	default:
	case MIPI_DBI_BUS_WIDTH_8_BITS:
		writeb(tx_buf[0], dbi->regs + addr);
		break;
	}

	if (msg->tx_len > 1) {
		addr = (dbi->sif_params.sif_port == 0) ? DBI_SDAT0 : DBI_SDAT1;
		for (i = 1; i < msg->tx_len; i++) {
			switch (dbi->bus_width) {
			case MIPI_DBI_BUS_WIDTH_32_BITS:
				writel(tx_buf[i], dbi->regs + addr);
				break;
			case MIPI_DBI_BUS_WIDTH_16_BITS:
				writew(tx_buf[i], dbi->regs + addr);
				break;
			default:
			case MIPI_DBI_BUS_WIDTH_8_BITS:
				writeb(tx_buf[i], dbi->regs + addr);
				break;
			}
		}
	}

	return i;
}

static int mtk_dbi_bus_attach(struct mipi_dbi_bus *bus,
			       struct mipi_dbi_device *device)
{
	struct mtk_dbi *dbi = bus_to_dbi(bus);
	int ret = 0;

	dbi->format = device->format;
	dbi->data_width = device->data_width;
	dbi->bus_width = device->bus_width;
	dbi->panel = of_drm_find_panel(dbi->device_node);
	if (dbi->panel) {
		ret = drm_panel_attach(dbi->panel, &dbi->conn);
		if (ret) {
			DRM_ERROR("Failed to attact dbi panel to drm\n");
			drm_connector_cleanup(&dbi->conn);
			goto errconnectorreg;
		}
	}

	DRM_INFO("[mtk-dbi] %d,%d,%d\n", dbi->format, dbi->data_width,
			dbi->bus_width);
	if (dbi->conn.dev)
		drm_helper_hpd_irq_event(dbi->conn.dev);

	return 0;

errconnectorreg:
	drm_connector_cleanup(&dbi->conn);
	return ret;
}

static int mtk_dbi_bus_detach(struct mipi_dbi_bus *bus,
			       struct mipi_dbi_device *device)
{
	struct mtk_dbi *dbi = bus_to_dbi(bus);

	if (dbi->conn.dev)
		drm_helper_hpd_irq_event(dbi->conn.dev);

	return 0;
}

static const struct mipi_dbi_bus_ops mtk_dbi_ops = {
	.attach = mtk_dbi_bus_attach,
	.detach = mtk_dbi_bus_detach,
	.transfer = mtk_dbi_bus_transfer,
};

static void mtk_dbi_stop(struct mtk_dbi *dbi)
{
	writel(0x0, dbi->regs + DBI_START);
	writel(0x0, dbi->regs + DBI_INTEN);
}

static void mtk_dbi_power_off(struct mtk_dbi *dbi)
{
	if (WARN_ON(dbi->refcount == 0))
		return;

	if (--dbi->refcount != 0)
		return;

	if (dbi->panel) {
		if (drm_panel_unprepare(dbi->panel)) {
			DRM_ERROR("failed to unprepare the panel\n");
			return;
		}
	}

	pinctrl_select_state(dbi->dbi_pinctrl, dbi->pins_gpio_mode);

	mtk_dbi_reset_engine(dbi);
	mtk_dbi_stop(dbi);

	clk_disable_unprepare(dbi->top_fdbi_sel);
	clk_disable_unprepare(dbi->top_fdbi);
}

static void mtk_dbi_output_disable(struct mtk_dbi *dbi)
{
	if (!dbi->enabled)
		return;

	if (dbi->panel) {
		if (drm_panel_disable(dbi->panel)) {
			DRM_ERROR("failed to disable the dbi panel\n");
			return;
		}
	}

	mtk_dbi_power_off(dbi);

	dbi->enabled = false;
}

static void mtk_dbi_encoder_destroy(struct drm_encoder *encoder)
{
	drm_encoder_cleanup(encoder);
}

static const struct drm_encoder_funcs mtk_dbi_encoder_funcs = {
	.destroy = mtk_dbi_encoder_destroy,
};

static void mtk_dbi_output_enable(struct mtk_dbi *dbi)
{
	int ret;

	if (dbi->enabled)
		return;

	ret = mtk_dbi_power_on(dbi);
	if (ret < 0) {
		DRM_ERROR("failed to power on dbi\n");
		return;
	}

	mtk_dbi_reset_engine(dbi);
	mtk_dbi_wait_for_idle(dbi);
	mtk_dbi_sif_timing(dbi);
	mtk_dbi_set_roi(dbi);
	mtk_dbi_te_control(dbi, true);
	mtk_dbi_interrupt_enable(dbi, true);

	if (dbi->panel) {
		mtk_dbi_lcm_reset(dbi);
		if (drm_panel_prepare(dbi->panel)) {
			DRM_ERROR("failed to prepare the panel\n");
			goto err_dbi_power_off;
		}
	}

	/* dbi pattern for debug */
	/* writel(0x00ff0041, dbi->regs + DBI_PATTERN); */
	/* mtk_dbi_start(dbi); */

	if (dbi->panel) {
		if (drm_panel_enable(dbi->panel)) {
			DRM_ERROR("failed to enable the dbi panel\n");
			goto err_dbi_power_off;
		}
	}

	dbi->enabled = true;

	return;
err_dbi_power_off:
	mtk_dbi_power_off(dbi);
}

static void mtk_dbi_encoder_dpms(struct drm_encoder *encoder, int mode)
{
	struct mtk_dbi *dbi = encoder_to_dbi(encoder);

	dev_info(dbi->dev, "mtk_dbi_encoder_dpms 0x%x\n", mode);

	switch (mode) {
	case DRM_MODE_DPMS_ON:
		mtk_dbi_output_enable(dbi);
		break;
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_OFF:
		mtk_dbi_output_disable(dbi);
		break;
	default:
		break;
	}
}

static bool mtk_dbi_encoder_mode_fixup(
	struct drm_encoder *encoder,
	const struct drm_display_mode *mode,
	struct drm_display_mode *adjusted_mode
	)
{
	return true;
}

static void mtk_dbi_encoder_prepare(struct drm_encoder *encoder)
{
	struct mtk_dbi *dbi = encoder_to_dbi(encoder);

	mtk_dbi_output_disable(dbi);
}

static void mtk_dbi_encoder_mode_set(
		struct drm_encoder *encoder,
		struct drm_display_mode *mode,
		struct drm_display_mode *adjusted
		)
{
	struct mtk_dbi *dbi = encoder_to_dbi(encoder);

	dbi->vm.pixelclock = adjusted->clock;
	dbi->vm.hactive = adjusted->hdisplay;
	dbi->vm.hback_porch = adjusted->htotal - adjusted->hsync_end;
	dbi->vm.hfront_porch = adjusted->hsync_start - adjusted->hdisplay;
	dbi->vm.hsync_len = adjusted->hsync_end - adjusted->hsync_start;

	dbi->vm.vactive = adjusted->vdisplay;
	dbi->vm.vback_porch = adjusted->vtotal - adjusted->vsync_end;
	dbi->vm.vfront_porch = adjusted->vsync_start - adjusted->vdisplay;
	dbi->vm.vsync_len = adjusted->vsync_end - adjusted->vsync_start;
}

static void mtk_dbi_encoder_disable(struct drm_encoder *encoder)
{
	struct mtk_dbi *dbi = encoder_to_dbi(encoder);

	mtk_dbi_output_disable(dbi);
}

static void mtk_dbi_encoder_enable(struct drm_encoder *encoder)
{
	struct mtk_dbi *dbi = encoder_to_dbi(encoder);

	mtk_dbi_output_enable(dbi);
}

static void mtk_dbi_encoder_commit(struct drm_encoder *encoder)
{
	struct mtk_dbi *dbi = encoder_to_dbi(encoder);

	mtk_dbi_output_enable(dbi);
}

static const struct drm_encoder_helper_funcs mtk_dbi_encoder_helper_funcs = {
	.dpms = mtk_dbi_encoder_dpms,
	.mode_fixup = mtk_dbi_encoder_mode_fixup,
	.prepare = mtk_dbi_encoder_prepare,
	.mode_set = mtk_dbi_encoder_mode_set,
	.commit = mtk_dbi_encoder_commit,
	.disable = mtk_dbi_encoder_disable,
	.enable = mtk_dbi_encoder_enable,
};

static enum drm_connector_status
mtk_dbi_connector_detect(struct drm_connector *connector, bool force)
{
	return connector_status_connected;
}

static void mtk_dbi_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static const struct drm_connector_funcs mtk_dbi_connector_funcs = {
	.dpms = drm_atomic_helper_connector_dpms,
	.detect = mtk_dbi_connector_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = mtk_dbi_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static int mtk_dbi_connector_get_modes(struct drm_connector *connector)
{
	struct mtk_dbi *dbi = connector_to_dbi(connector);

	return drm_panel_get_modes(dbi->panel);
}

static struct drm_encoder
*mtk_dbi_connector_best_encoder(struct drm_connector *connector)
{
	struct mtk_dbi *dbi = connector_to_dbi(connector);

	return &dbi->encoder;
}

static const struct drm_connector_helper_funcs
	mtk_dbi_connector_helper_funcs = {
	.get_modes = mtk_dbi_connector_get_modes,
	.best_encoder = mtk_dbi_connector_best_encoder,
};

static int mtk_dbi_create_conn_enc(struct drm_device *drm, struct mtk_dbi *dbi)
{
	int ret;

	ret = drm_encoder_init(drm, &dbi->encoder, &mtk_dbi_encoder_funcs,
			       DRM_MODE_ENCODER_DBI, NULL);
	if (ret) {
		DRM_ERROR("Failed to dbi encoder init to drm\n");
		return ret;
	}

	drm_encoder_helper_add(&dbi->encoder, &mtk_dbi_encoder_helper_funcs);

	dbi->encoder.possible_crtcs = BIT(0) | BIT(1);

	ret = drm_connector_init(drm, &dbi->conn, &mtk_dbi_connector_funcs,
				 DRM_MODE_CONNECTOR_DBI);
	if (ret) {
		DRM_ERROR("Failed to dbi connector init to drm\n");
		goto errconnector;
	}

	drm_connector_helper_add(&dbi->conn, &mtk_dbi_connector_helper_funcs);

	dbi->conn.dpms = DRM_MODE_DPMS_OFF;
	drm_mode_connector_attach_encoder(&dbi->conn, &dbi->encoder);

	if (dbi->panel) {
		ret = drm_panel_attach(dbi->panel, &dbi->conn);
		if (ret) {
			DRM_ERROR("Failed to attach panel to drm\n");
			goto errconnector;
		}
	}

	return 0;

errconnector:
	drm_encoder_cleanup(&dbi->encoder);
	return ret;
}

static void mtk_dbi_destroy_conn_enc(struct mtk_dbi *dbi)
{
	drm_encoder_cleanup(&dbi->encoder);
	drm_connector_unregister(&dbi->conn);
	drm_connector_cleanup(&dbi->conn);
}

static int mtk_dbi_bind(struct device *dev, struct device *master, void *data)
{
	int ret;
	struct drm_device *drm = data;
	struct mtk_dbi *dbi = dev_get_drvdata(dev);

	MTK_DRM_DEBUG(dev, "\n");

	ret = mtk_ddp_comp_register(drm, &dbi->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	ret = mtk_dbi_create_conn_enc(drm, dbi);
	if (ret) {
		DRM_ERROR("Encoder create failed with %d\n", ret);
		goto err_unregister;
	}

	MTK_DRM_DEBUG(dev, " end\n");
	return 0;

err_unregister:
	return ret;
}

static void mtk_dbi_unbind(struct device *dev, struct device *master,
			   void *data)
{
	struct mtk_dbi *dbi;

	dbi = platform_get_drvdata(to_platform_device(dev));
	mtk_dbi_destroy_conn_enc(dbi);
	mipi_dbi_host_unregister(&dbi->bus);
}

static const struct component_ops mtk_dbi_component_ops = {
	.bind = mtk_dbi_bind,
	.unbind = mtk_dbi_unbind,
};

static void mtk_dbi_parse_dt(struct mtk_dbi *dbi)
{
	struct device_node *np = dbi->dev->of_node;
	int ret = 0;


	dbi->dbi_pinctrl = devm_pinctrl_get(dbi->dev);
	if (IS_ERR(dbi->dbi_pinctrl)) {
		ret = PTR_ERR(dbi->dbi_pinctrl);
		DRM_INFO("[mtk-dbi] fail to get dbi pins\n");
	} else {
		dbi->pins_dbi_mode = pinctrl_lookup_state(dbi->dbi_pinctrl,
					"dbi_pin_mode");
		if (IS_ERR(dbi->pins_dbi_mode)) {
			ret = PTR_ERR(dbi->pins_dbi_mode);
			DRM_INFO("[mtk-dbi] cannot get dbi_pins_mode\n");
		}

		dbi->pins_gpio_mode = pinctrl_lookup_state(dbi->dbi_pinctrl,
					"dbi_pin_gpio_mode");
		if (IS_ERR(dbi->pins_gpio_mode)) {
			ret = PTR_ERR(dbi->pins_gpio_mode);
			DRM_INFO("[mtk-dbi] cannot get dbi_pins_gpio_mode\n");
		}
	}

	of_property_read_u32(np, "sif-port", &dbi->sif_params.sif_port);
	of_property_read_u32(np, "three-wire", &dbi->sif_params.sif_3wire);
	of_property_read_u32(np, "hw-cs", &dbi->sif_params.sif_hw_cs);
	of_property_read_u32(np, "sdi", &dbi->sif_params.sif_sdi);
	of_property_read_u32(np, "1st-pol", &dbi->sif_params.sif_1st_pol);
	of_property_read_u32(np, "sck-def", &dbi->sif_params.sif_sck_def);
	of_property_read_u32(np, "div2", &dbi->sif_params.sif_div2);

	of_property_read_u32(np, "cs-setup", &dbi->sif_params.cs_setup);
	of_property_read_u32(np, "cs-hold", &dbi->sif_params.cs_hold);
	of_property_read_u32(np, "rd-1st", &dbi->sif_params.rd_1st);
	of_property_read_u32(np, "rd-2nd", &dbi->sif_params.rd_2nd);
	of_property_read_u32(np, "wr-1st", &dbi->sif_params.wr_1st);
	of_property_read_u32(np, "wr-2nd", &dbi->sif_params.wr_2nd);

	of_property_read_u32(np, "te-mode", &dbi->sif_params.te_mode);
	of_property_read_u32(np, "te-edge", &dbi->sif_params.te_edge);

	DRM_INFO("[mtk-dbi] sif_3wire: %d, sif_hw_cs: %d, sif_sdi: %d\n",
		dbi->sif_params.sif_3wire, dbi->sif_params.sif_hw_cs,
		dbi->sif_params.sif_sdi);
	DRM_INFO("[mtk-dbi] sif_1st_pol: %d, sif_sck_def: %d, sif_div2: %d\n",
		dbi->sif_params.sif_1st_pol, dbi->sif_params.sif_sck_def,
		dbi->sif_params.sif_div2);

	dbi->sif_params.sif_3wire &= 0x01;
	dbi->sif_params.sif_hw_cs &= 0x01;
	dbi->sif_params.sif_sdi &= 0x01;
	dbi->sif_params.sif_1st_pol &= 0x01;
	dbi->sif_params.sif_sck_def &= 0x01;
	dbi->sif_params.sif_div2 &= 0x01;

	DRM_INFO("[mtk-dbi] setup: %d, hold: %d, rd_1st: %d, rd_2nd: %d\n",
			dbi->sif_params.cs_setup, dbi->sif_params.cs_hold,
			dbi->sif_params.rd_1st, dbi->sif_params.rd_2nd);
	DRM_INFO("[mtk-dbi] wr_1st: %d, wr_2nd: %d\n",
		dbi->sif_params.wr_1st, dbi->sif_params.wr_2nd);

	dbi->sif_params.cs_setup &= 0x0f;
	dbi->sif_params.cs_hold &= 0x0f;
	dbi->sif_params.rd_1st &= 0x0f;
	dbi->sif_params.rd_2nd &= 0x0f;
	dbi->sif_params.wr_1st &= 0x0f;
	dbi->sif_params.wr_2nd &= 0x0f;

	DRM_INFO("[mtk-dbi] te_mode: %d, te_edge: %d\n",
		dbi->sif_params.te_mode, dbi->sif_params.te_edge);

	dbi->sif_params.te_mode &= 0x01;
	dbi->sif_params.te_edge &= 0x01;
}

static void mtk_dbi_ddp_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_dbi *dbi = container_of(comp, struct mtk_dbi, ddp_comp);

	mtk_dbi_power_on(dbi);
}

static void mtk_dbi_ddp_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_dbi *dbi = container_of(comp, struct mtk_dbi, ddp_comp);

	mtk_dbi_power_off(dbi);
}

static void mtk_dbi_ddp_trigger_update(struct mtk_ddp_comp *comp)
{
	struct mtk_dbi *dbi = container_of(comp, struct mtk_dbi, ddp_comp);

	mtk_dbi_start(dbi);
}

static const struct mtk_ddp_comp_funcs mtk_dbi_funcs = {
	.prepare = mtk_dbi_ddp_prepare,
	.unprepare = mtk_dbi_ddp_unprepare,
	.trigger_update = mtk_dbi_ddp_trigger_update,
};

static int mtk_dbi_probe(struct platform_device *pdev)
{
	struct mtk_dbi *dbi;
	struct device *dev = &pdev->dev;
	struct device_node *remote_node, *endpoint;
	struct resource *regs;
	int ret;
	int comp_id;

	MTK_DRM_DEBUG(dev, "\n");

	dbi = devm_kzalloc(dev, sizeof(*dbi), GFP_KERNEL);
	dbi->dev = dev;

	dbi->bus.ops = &mtk_dbi_ops;
	dbi->bus.dev = dev;
	ret = mipi_dbi_host_register(&dbi->bus);
	if (ret < 0) {
		DRM_INFO("[mtk-dbi] failed to register DBI host: %d\n", ret);
		return -EPROBE_DEFER;
	}

	dbi->top_fdbi = devm_clk_get(dev, "top_fdbi");
	if (IS_ERR(dbi->top_fdbi_sel)) {
		ret = PTR_ERR(dbi->top_fdbi);
		DRM_INFO("[mtk-dbi] Failed to get top_fdbi clock: %d\n", ret);
		return ret;
	}

	dbi->top_fdbi_sel = devm_clk_get(dev, "top_fdbi_sel");
	if (IS_ERR(dbi->top_fdbi_sel)) {
		ret = PTR_ERR(dbi->top_fdbi_sel);
		DRM_INFO("[mtk-dbi] Failed to get top_fdbi_sel clock: %d\n",
			ret);
		return ret;
	}

	dbi->top_mainpll_d12 = devm_clk_get(dev, "top_mainpll_d12");
	if (IS_ERR(dbi->top_mainpll_d12)) {
		ret = PTR_ERR(dbi->top_mainpll_d12);
		DRM_INFO("[mtk-dbi] Failed to get top_mainpll_d12 clock: %d\n",
			ret);
		return ret;
	}

	mtk_dbi_parse_dt(dbi);

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dbi->regs = devm_ioremap_resource(dev, regs);
	if (IS_ERR(dbi->regs)) {
		ret = PTR_ERR(dbi->regs);
		DRM_INFO("[mtk-dbi] Failed to ioremap memory: %d\n", ret);
		return ret;
	}

	endpoint = of_graph_get_next_endpoint(dev->of_node, NULL);
	if (endpoint) {
		remote_node = of_graph_get_remote_port_parent(endpoint);
		of_node_put(endpoint);

		if (!remote_node) {
			DRM_INFO("[mtk-dbi] No panel connected\n");
			return -ENODEV;
		}

		dbi->bridge = of_drm_find_bridge(remote_node);
		dbi->panel = of_drm_find_panel(remote_node);

		dbi->device_node = remote_node;
		of_node_put(remote_node);

		if (!dbi->bridge && !dbi->panel) {
			DRM_INFO(
				"[mtk-dbi] Waiting for bridge or panel driver\n"
				);
			ret = -EPROBE_DEFER;
			goto err_host_unregister;
		}
	}

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DBI);
	if (comp_id < 0) {
		DRM_INFO("[mtk-dbi] Failed to identify by alias: %d\n",
			comp_id);
		ret = comp_id;
		goto err_host_unregister;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &dbi->ddp_comp, comp_id,
				&mtk_dbi_funcs);
	if (ret) {
		DRM_INFO("[mtk-dbi] Failed to initialize component: %d\n", ret);
		goto err_host_unregister;
	}

	dbi->irq_data = 0;
	dev_info(dev, "dbi irq num is 0x%x\n", dbi->irq_num);

	platform_set_drvdata(pdev, dbi);

	ret = component_add(&pdev->dev, &mtk_dbi_component_ops);
	MTK_DRM_DEBUG(dev, " end\n");
	return ret;
err_host_unregister:
	mipi_dbi_host_unregister(&dbi->bus);
	return ret;
}

static int mtk_dbi_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM
static int mtk_dbi_suspend(struct device *dev)
{
	return 0;
}

static int mtk_dbi_resume(struct device *dev)
{
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(mtk_dbi_pm_ops, mtk_dbi_suspend, mtk_dbi_resume);

static const struct of_device_id mtk_dbi_of_match[] = {
	{ .compatible = "mediatek,mt8518-dbi" },
	{ },
};

struct platform_driver mtk_dbi_driver = {
	.probe = mtk_dbi_probe,
	.remove = mtk_dbi_remove,
	.driver = {
		.name = "mtk-dbi",
		.of_match_table = mtk_dbi_of_match,
		.pm = &mtk_dbi_pm_ops,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};
