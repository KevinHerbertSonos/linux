/*
 * Copyright (C) 2018 Renesas Electronics
 *
 * Copyright (C) 2016 Atmel
 *		      Bo Shen <voice.shen@atmel.com>
 *
 * Authors:	      Bo Shen <voice.shen@atmel.com>
 *		      Boris Brezillon <boris.brezillon@free-electrons.com>
 *		      Wu, Songjun <Songjun.Wu@atmel.com>
 *
 *
 * Copyright (C) 2010-2011 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/debugfs.h>
#include <linux/time.h>
#include <linux/workqueue.h>
#include <linux/atomic.h>
#include <linux/kthread.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c-mux.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/of_irq.h>

#include <drm/drmP.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_edid.h>
#include <linux/types.h>
#include <media/cec.h>
#include "siHdmiTx_902x_TPI.h"
#include "si_apiCpi.h"
#include <si_apiCEC.h>
#include "si_cpi_regs.h"
#include "si_cec_enums.h"

#define SII902X_TPI_VIDEO_DATA			0x0

#define SII902X_TPI_PIXEL_REPETITION		0x8
#define SII902X_TPI_AVI_PIXEL_REP_BUS_24BIT     BIT(5)
#define SII902X_TPI_AVI_PIXEL_REP_RISING_EDGE   BIT(4)
#define SII902X_TPI_AVI_PIXEL_REP_4X		3
#define SII902X_TPI_AVI_PIXEL_REP_2X		1
#define SII902X_TPI_AVI_PIXEL_REP_NONE		0
#define SII902X_TPI_CLK_RATIO_HALF		(0 << 6)
#define SII902X_TPI_CLK_RATIO_1X		(1 << 6)
#define SII902X_TPI_CLK_RATIO_2X		(2 << 6)
#define SII902X_TPI_CLK_RATIO_4X		(3 << 6)

#define SII902X_TPI_AVI_IN_FORMAT		0x9
#define SII902X_TPI_AVI_INPUT_BITMODE_12BIT	BIT(7)
#define SII902X_TPI_AVI_INPUT_DITHER		BIT(6)
#define SII902X_TPI_AVI_INPUT_RANGE_LIMITED	(2 << 2)
#define SII902X_TPI_AVI_INPUT_RANGE_FULL	(1 << 2)
#define SII902X_TPI_AVI_INPUT_RANGE_AUTO	(0 << 2)
#define SII902X_TPI_AVI_INPUT_COLORSPACE_BLACK	(3 << 0)
#define SII902X_TPI_AVI_INPUT_COLORSPACE_YUV422	(2 << 0)
#define SII902X_TPI_AVI_INPUT_COLORSPACE_YUV444	(1 << 0)
#define SII902X_TPI_AVI_INPUT_COLORSPACE_RGB	(0 << 0)

#define SII902X_TPI_AVI_INFOFRAME		0x0c

#define SII902X_SYS_CTRL_DATA			0x1a
#define SII902X_SYS_CTRL_PWR_DWN		BIT(4)
#define SII902X_SYS_CTRL_AV_MUTE		BIT(3)
#define SII902X_SYS_CTRL_DDC_BUS_REQ		BIT(2)
#define SII902X_SYS_CTRL_DDC_BUS_GRTD		BIT(1)
#define SII902X_SYS_CTRL_OUTPUT_MODE		BIT(0)
#define SII902X_SYS_CTRL_OUTPUT_HDMI		1
#define SII902X_SYS_CTRL_OUTPUT_DVI		0

#define SII902X_REG_CHIPID(n)			(0x1b + (n))

#define SII902X_PWR_STATE_CTRL			0x1e
#define SII902X_AVI_POWER_STATE_MSK		GENMASK(1, 0)
#define SII902X_AVI_POWER_STATE_D(l)		\
	((l) & SII902X_AVI_POWER_STATE_MSK)

#define SII902X_INT_ENABLE			0x3c
#define SII902X_INT_STATUS			0x3d
#define SII902X_HOTPLUG_EVENT			BIT(0)
#define SII902X_PLUGGED_STATUS			BIT(2)
#define SII902X_CEC_EVENT			BIT(3)

#define SII902X_REG_TPI_RQB			0xc7

#define SII902X_I2C_BUS_ACQUISITION_TIMEOUT_MS	500

#define Sii902x_Debug_LOG(fmt, arg...)    \
	do {                                    \
		if (1) {                                          \
			pr_info("[hdmi_sii9022]%s,%d\n", \
			__func__, __LINE__);  \
			pr_info(fmt, ##arg);                                  \
		}                                                              \
	} while (0)

struct sii902x {
	struct i2c_client *i2c;
	struct regmap *regmap;
	struct drm_bridge bridge;
	struct drm_connector connector;
	struct gpio_desc *reset_gpio;
	struct i2c_mux_core *i2cmux;
	struct mutex mutex;
};

struct sii902x_cec {
	struct SI_CpiData_t cecFrame;
	struct SI_CpiStatus_t cecStatus;
	struct cec_adapter *adap;
};

struct sii902x_cec *cec;
struct regmap *sii902x_cec_regmap;
struct sii902x *sii902x_for_i2c;
static char debug_buffer[2048];
struct sii902x *sii902x_debug;
struct sii902x *sii902x_mutex;

static void process_dbg_opt(const char *opt)
{
	unsigned int vadr_regstart, val_temp;
	struct regmap *regmap = sii902x_debug->regmap;
	unsigned int val;
	int ret;
	int retry_cnt = 0;
	unsigned int i = 0;

	if (strncmp(opt, "read:", 5) == 0) {
		ret = sscanf(opt + 5, "%x", &vadr_regstart);
		do {
			ret = regmap_read(regmap, vadr_regstart, &val);
		} while (ret);
		Sii902x_Debug_LOG("r:0x%08x = 0x%x\n", vadr_regstart, val);
	}

	if (strncmp(opt, "readmany:", 9) == 0) {
		ret = sscanf(opt + 9, "%x=%d", &vadr_regstart, &val_temp);
		for (i = 0; i < val_temp; i++) {
			do {
				ret = regmap_read(regmap,
					vadr_regstart + i, &val);
			} while (ret);
			Sii902x_Debug_LOG("r:0x%08x = 0x%x\n",
				vadr_regstart + i, val);
		}
	}

	if (strncmp(opt, "write:", 6) == 0) {
		ret = sscanf(opt + 6, "%x=%x", &vadr_regstart, &val_temp);
		val = (unsigned int)val_temp;
		do {
			ret = regmap_write(regmap, vadr_regstart, val);
		} while (ret);
		do {
			ret = regmap_read(regmap, vadr_regstart, &val);
		} while (ret);
		Sii902x_Debug_LOG("w:0x%08x = 0x%x\n", vadr_regstart, val);
	}

	if (strncmp(opt, "cec_write:", 10) == 0) {
		ret = sscanf(opt + 10, "%x=%x", &vadr_regstart, &val_temp);
		val = (unsigned int)val_temp;
		do {
			ret =
				regmap_write(sii902x_cec_regmap,
				vadr_regstart, val);
			if (ret) {
				retry_cnt++;
				if (retry_cnt >= 5)
					retry_cnt = 0;
			}
		} while (ret && retry_cnt);

		Sii902x_Debug_LOG("cec_w:0x%08x = 0x%x\n", vadr_regstart, val);
	}

	/*cec*/
	if (strncmp(opt, "cec_read:", 9) == 0) {
		ret = sscanf(opt + 9, "%x", &vadr_regstart);
		do {
			ret =
				regmap_read(sii902x_cec_regmap,
				vadr_regstart, &val);
			if (ret) {
				retry_cnt++;
				if (retry_cnt >= 5)
					retry_cnt = 0;
			}
		} while (ret && retry_cnt);
		Sii902x_Debug_LOG("cec_r:0x%08x = 0x%x\n", vadr_regstart, val);
	}

	if (strncmp(opt, "cec_init", 8) == 0)
		SI_CpiInit();

	if (strncmp(opt, "cec_send_test", 13) == 0) {
		struct SI_CpiData_t cecFrame;

	    cecFrame.opcode        = CECOP_USER_CONTROL_PRESSED;
	    cecFrame.srcDestAddr   = MAKE_SRCDEST(g_cecAddress, 0);
	    cecFrame.args[0]       = 1;
	    cecFrame.argCount      = 1;
	    SI_CpiWrite(&cecFrame);
		Sii902x_Debug_LOG("cec send test\n");
	}
}

static void process_dbg_cmd(char *cmd)
{
	char *tok;

	pr_debug("[extd] %s\n", cmd);

	while ((tok = strsep(&cmd, " ")) != NULL)
		process_dbg_opt(tok);
}

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t debug_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(debug_buffer) - 1;
	size_t ret;

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&debug_buffer, ubuf, count))
		return -EFAULT;

	debug_buffer[count] = 0;

	process_dbg_cmd(debug_buffer);

	return ret;
}

static const char STR_HELP[] =
"\nUSAGE\nHDMI power on:\n		echo power_on>hdmi_test\n";

static ssize_t debug_read(struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(debug_buffer) - 1;
	int n = 0;

	n += scnprintf(debug_buffer + n, debug_bufmax - n, STR_HELP);
	debug_buffer[n++] = 0;

	return simple_read_from_buffer(ubuf, count, ppos, debug_buffer, n);
}

static const struct file_operations debug_fops = {
	.read = debug_read,
	.write = debug_write,
	.open = debug_open,
};

struct dentry *sii902x_dbgfs;

void Sii902x_DBG_Init(void)
{
	sii902x_dbgfs = debugfs_create_file("hdmi_test",
					    S_IFREG | 444, NULL,
					    (void *)0, &debug_fops);
}

void sii9022_cec_handler(void)
{
	struct SI_CpiStatus_t  cecStatus;
	struct cec_msg  receive_msg;

	/* Get the CEC transceiver status and pass it to the    */
	/* current task.  The task will send any required       */
	/* CEC commands.                                        */
	SI_CpiStatus(&cecStatus);

	/*add by mtk*/
	if (cecStatus.txState == SI_TX_SENDACKED) {
		cec_transmit_done(cec->adap, CEC_TX_STATUS_OK, 0, 0, 0, 0);
		pr_debug("cec tx msg send ok\n");
	}
#if 1
	if (cecStatus.txState == SI_TX_SENDFAILED) {
		cec_transmit_done(cec->adap, CEC_TX_STATUS_NACK, 0, 0, 0, 1);
		pr_debug("cec tx msg nack\n");
	}
#endif
	/* Now look to see if any CEC commands were received.   */
	if (cecStatus.rxState) {
		unsigned char frameCount;
		struct SI_CpiData_t cecFrame;

		/* Get CEC frames until no more are present.    */
		cecStatus.rxState = 0;      // Clear activity flag
		frameCount = ((SiIRegioRead(REG_CEC_RX_COUNT) & 0xF0) >> 4);
 
		while (frameCount) {
			pr_debug("\n%d frames in Rx Fifo\n", (int)frameCount);

			if (SI_CpiRead(&cecFrame)) {
				pr_info("Error in Rx Fsi_CecRxMsgHandlerifo\n");
				break;
			}

			if (1) {
				int i;
				struct cec_adapter *new_adap;

				pr_debug("cec msg received\n");
				pr_debug("src dest addr:0x%x\n",
					cecFrame.srcDestAddr);
				pr_debug("opcode:0x%x\n", cecFrame.opcode);
				pr_debug("oprand count:0x%x\n",
					cecFrame.argCount);
				for (i = 0; i < cecFrame.argCount; i++)
					pr_debug(
					"oprand[%d]:0x%x\n",
					i, cecFrame.args[i]);
				pr_debug(
					"cec msg end\n");

				receive_msg.len = cecFrame.argCount + 2;
				receive_msg.msg[0] = cecFrame.srcDestAddr;
				receive_msg.msg[1] = cecFrame.opcode;
				for (i = 0; i < cecFrame.argCount; i++)
					receive_msg.msg[i+2] = cecFrame.args[i];

				new_adap =
					dev_get_drvdata(
					&cec->adap->devnode.dev);
				if (new_adap != NULL)
					cec_received_msg(new_adap,
					&receive_msg);
			}
			frameCount =
				((SiIRegioRead(REG_CEC_RX_COUNT) &
				0xF0) >> 4);
		}
	}
}

/*i2c api for hdmi audio*/
u8 ReadByteTPI(u8 RegOffset)
{
	int ret;

	unsigned int status = 0;
    //return I2CReadByte(TX_SLAVE_ADDR, RegOffset);

	mutex_lock(&sii902x_mutex->mutex);
	ret = regmap_read(sii902x_for_i2c->regmap, RegOffset, &status);
	mutex_unlock(&sii902x_mutex->mutex);

	if (ret) {
		pr_info("i2c read err:%d\n", ret);
		return 0;
	}
	return (status & 0xff);
}

//------------------------------------------------------------------------------
// Function Name: WriteByteTPI()
// Function Description: I2C write
//------------------------------------------------------------------------------
int WriteByteTPI(u8 RegOffset, u8 Data)
{
	int ret;

    //I2CWriteByte(TX_SLAVE_ADDR, RegOffset, Data);
	mutex_lock(&sii902x_mutex->mutex);
	ret = regmap_write(sii902x_for_i2c->regmap, RegOffset, Data);
	mutex_unlock(&sii902x_mutex->mutex);

	if (ret) {
		pr_info("i2c write err:%d\n", ret);
		return ret;
	}
	return 0;
}

u8 I2CWriteBlock(u8 SlaveAddr, u8 RegAddr, u8 NBytes, u8 *Data)
{
	int ret;

	mutex_lock(&sii902x_mutex->mutex);
	ret = regmap_bulk_write(sii902x_for_i2c->regmap, RegAddr, Data, NBytes);
	mutex_unlock(&sii902x_mutex->mutex);

	if (ret) {
		pr_info("I2CWriteBlock err:%d\n", ret);
		return 0;
	}
	return 1;
}

u8 I2CReadBlock(u8 SlaveAddr, u8 RegAddr, u8 NBytes, u8 *Data)
{
	int ret;

	mutex_lock(&sii902x_mutex->mutex);
	ret =
		regmap_bulk_read(sii902x_for_i2c->regmap,
		RegAddr, Data,
		NBytes);
	mutex_unlock(&sii902x_mutex->mutex);

	if (ret) {
		pr_info("I2CWriteBlock err:%d\n", ret);
		return 0;
	}
	return 1;
}

/**i2c for cec write/read **/
void SiIRegioReadBlock(unsigned short regAddr,
unsigned char *buffer, unsigned short length)
{
	int ret;
	int retry_cnt = 0;

	do {
		mutex_lock(&sii902x_mutex->mutex);
		ret =
			regmap_bulk_read(sii902x_cec_regmap,
			(unsigned char)regAddr,
			buffer, length);
		mutex_unlock(&sii902x_mutex->mutex);

		if (ret) {
			retry_cnt++;
			if (retry_cnt >= 5)
				retry_cnt = 0;
			pr_info(
				"cec I2C read block err:%d cnt:%d\n",
				ret, retry_cnt);
		}
	} while (ret && retry_cnt);
}

void SiIRegioWriteBlock(unsigned short regAddr,
	unsigned char *buffer, unsigned short length)
{
	int ret;
	int retry_cnt = 0;

	do {
		mutex_lock(&sii902x_mutex->mutex);
		ret =
			regmap_bulk_write(sii902x_cec_regmap,
			(unsigned char)regAddr,
			buffer, length);
		mutex_unlock(&sii902x_mutex->mutex);

		if (ret) {
			retry_cnt++;
			if (retry_cnt >= 5)
				retry_cnt = 0;
			pr_info(
				"cec I2C WriteBlock err:%d cnt:%d\n",
				ret, retry_cnt);
		}
	} while (ret && retry_cnt);
}

unsigned char SiIRegioRead(unsigned short regAddr)
{
	int ret;
	unsigned int status = 0;

	mutex_lock(&sii902x_mutex->mutex);
	ret = regmap_read(sii902x_cec_regmap, (unsigned char)regAddr, &status);
	mutex_unlock(&sii902x_mutex->mutex);

	if (ret) {
		pr_info("cec i2c read err:%d\n", ret);
		return 0;
	}
	return (status & 0xff);
}

void SiIRegioWrite(unsigned short regAddr, unsigned char value)
{
	int ret;

	mutex_lock(&sii902x_mutex->mutex);
	ret =
		regmap_write(sii902x_cec_regmap,
		(unsigned char)regAddr, value);
	mutex_unlock(&sii902x_mutex->mutex);

	if (ret)
		pr_info("cec i2c write err:%d\n", ret);
}

static int sii902x_read_unlocked(struct i2c_client *i2c, u8 reg, u8 *val)
{
	union i2c_smbus_data data;
	int ret;

	ret = __i2c_smbus_xfer(i2c->adapter, i2c->addr, i2c->flags,
			       I2C_SMBUS_READ, reg, I2C_SMBUS_BYTE_DATA, &data);

	if (ret < 0)
		return ret;

	*val = data.byte;
	return 0;
}

static int sii902x_write_unlocked(struct i2c_client *i2c, u8 reg, u8 val)
{
	union i2c_smbus_data data;

	data.byte = val;

	return __i2c_smbus_xfer(i2c->adapter, i2c->addr, i2c->flags,
				I2C_SMBUS_WRITE, reg, I2C_SMBUS_BYTE_DATA,
				&data);
}

static int sii902x_update_bits_unlocked(struct i2c_client *i2c, u8 reg, u8 mask,
					u8 val)
{
	int ret;
	u8 status;

	ret = sii902x_read_unlocked(i2c, reg, &status);
	if (ret)
		return ret;
	status &= ~mask;
	status |= val & mask;
	return sii902x_write_unlocked(i2c, reg, status);
}

static inline struct sii902x *bridge_to_sii902x(struct drm_bridge *bridge)
{
	return container_of(bridge, struct sii902x, bridge);
}

static inline struct sii902x *connector_to_sii902x(struct drm_connector *con)
{
	return container_of(con, struct sii902x, connector);
}

static void sii902x_reset(struct sii902x *sii902x)
{
	if (!sii902x->reset_gpio)
		return;

	gpiod_set_value(sii902x->reset_gpio, 1);

	/* The datasheet says treset-min = 100us. Make it 150us to be sure. */
	usleep_range(150, 200);

	gpiod_set_value(sii902x->reset_gpio, 0);
}

static enum drm_connector_status
sii902x_connector_detect(struct drm_connector *connector, bool force)
{
	struct sii902x *sii902x = connector_to_sii902x(connector);
	unsigned int status;

	mutex_lock(&sii902x->mutex);
	regmap_read(sii902x->regmap, SII902X_INT_STATUS, &status);
	mutex_unlock(&sii902x->mutex);

	return (status & SII902X_PLUGGED_STATUS) ?
	    connector_status_connected : connector_status_disconnected;
}

static const struct drm_connector_funcs sii902x_connector_funcs = {
	.dpms = drm_atomic_helper_connector_dpms,
	.detect = sii902x_connector_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static int sii902x_get_modes(struct drm_connector *connector)
{
	struct sii902x *sii902x = connector_to_sii902x(connector);
	u32 bus_format = MEDIA_BUS_FMT_RGB888_1X24;
	u8 output_mode = SII902X_SYS_CTRL_OUTPUT_DVI;
	struct edid *edid;
	int num = 0, ret;
	u16 phy_addr = CEC_PHYS_ADDR_INVALID;

	mutex_lock(&sii902x->mutex);

	edid = drm_get_edid(connector, sii902x->i2cmux->adapter[0]);
	drm_mode_connector_update_edid_property(connector, edid);
	if (edid) {
		if (drm_detect_hdmi_monitor(edid))
			output_mode = SII902X_SYS_CTRL_OUTPUT_HDMI;

		num = drm_add_edid_modes(connector, edid);

		phy_addr = cec_get_edid_phys_addr((u8 *)edid,
						  EDID_LENGTH * (1 + (edid->extensions)),
						  NULL);

		kfree(edid);
	}

	ret = drm_display_info_set_bus_formats(&connector->display_info,
					       &bus_format, 1);
	if (ret)
		goto error_out;

	ret = regmap_update_bits(sii902x->regmap, SII902X_SYS_CTRL_DATA,
				 SII902X_SYS_CTRL_OUTPUT_MODE, output_mode);
	if (ret)
		goto error_out;

	ret = num;

error_out:
	mutex_unlock(&sii902x->mutex);

	/* Setting physical address must be outside the lock as it may call back into this driver */
	cec_s_phys_addr(cec->adap, phy_addr, false);

	return ret;
}

static enum drm_mode_status sii902x_mode_valid(struct drm_connector *connector,
					       struct drm_display_mode *mode)
{
	/* TODO: check mode */

	return MODE_OK;
}

static const struct drm_connector_helper_funcs
	sii902x_connector_helper_funcs = {
	.get_modes = sii902x_get_modes,
	.mode_valid = sii902x_mode_valid,
};

static void sii902x_bridge_disable(struct drm_bridge *bridge)
{
	struct sii902x *sii902x = bridge_to_sii902x(bridge);

	mutex_lock(&sii902x->mutex);
	regmap_update_bits(sii902x->regmap, SII902X_SYS_CTRL_DATA,
			   SII902X_SYS_CTRL_PWR_DWN, SII902X_SYS_CTRL_PWR_DWN);
	mutex_unlock(&sii902x->mutex);
}

static void sii902x_bridge_enable(struct drm_bridge *bridge)
{
	struct sii902x *sii902x = bridge_to_sii902x(bridge);
	int ret = 0;
	int i = 0;

	mutex_lock(&sii902x->mutex);
	regmap_update_bits(sii902x->regmap, SII902X_PWR_STATE_CTRL,
			   SII902X_AVI_POWER_STATE_MSK,
			   SII902X_AVI_POWER_STATE_D(0));
	regmap_update_bits(sii902x->regmap, SII902X_SYS_CTRL_DATA,
		SII902X_SYS_CTRL_PWR_DWN, 0);
	for (i = 0; i < 2; i++)
		ret = regmap_write(sii902x->regmap, 0x19, 0);
	mutex_unlock(&sii902x->mutex);
}

static void sii902x_bridge_mode_set(struct drm_bridge *bridge,
				    struct drm_display_mode *mode,
				    struct drm_display_mode *adj)
{
	struct sii902x *sii902x = bridge_to_sii902x(bridge);
	struct regmap *regmap = sii902x->regmap;
	u8 buf[HDMI_INFOFRAME_SIZE(AVI)];
	struct hdmi_avi_infoframe frame;
	u16 pixel_clock_10kHz = adj->clock / 10;
	int ret;
	int i = 0;

	buf[0] = pixel_clock_10kHz & 0xff;
	buf[1] = pixel_clock_10kHz >> 8;
	buf[2] = adj->vrefresh;
	buf[3] = 0x00;
	buf[4] = adj->hdisplay;
	buf[5] = adj->hdisplay >> 8;
	buf[6] = adj->vdisplay;
	buf[7] = adj->vdisplay >> 8;
	buf[8] = SII902X_TPI_CLK_RATIO_1X | SII902X_TPI_AVI_PIXEL_REP_NONE |
	    SII902X_TPI_AVI_PIXEL_REP_RISING_EDGE;
	buf[9] = SII902X_TPI_AVI_INPUT_RANGE_AUTO |
		SII902X_TPI_AVI_INPUT_COLORSPACE_RGB;

	mutex_lock(&sii902x->mutex);
	for (i = 0; i < 2; i++)
		ret = regmap_bulk_write(regmap,
			SII902X_TPI_VIDEO_DATA, buf, 10);
	if (ret) {
		mutex_unlock(&sii902x->mutex);
		return;
	}

	ret = drm_hdmi_avi_infoframe_from_display_mode(&frame, adj);

	if (ret < 0) {
		DRM_ERROR("couldn't fill AVI infoframe\n");
		mutex_unlock(&sii902x->mutex);

		return;
	}
	ret = hdmi_avi_infoframe_pack(&frame, buf, sizeof(buf));
	if (ret < 0) {
		DRM_ERROR("failed to pack AVI infoframe: %d\n", ret);
		mutex_unlock(&sii902x->mutex);

		return;
	}

	/* Do not send the infoframe header, but keep the CRC field. */
	for (i = 0; i < 2; i++)
		ret = regmap_bulk_write(regmap, SII902X_TPI_AVI_INFOFRAME,
					buf + HDMI_INFOFRAME_HEADER_SIZE - 1,
					HDMI_AVI_INFOFRAME_SIZE + 1);
	mutex_unlock(&sii902x->mutex);

	siHdmiTx_AudioSet();

}

static int sii902x_bridge_attach(struct drm_bridge *bridge)
{
	struct sii902x *sii902x = bridge_to_sii902x(bridge);
	struct drm_device *drm = bridge->dev;
	int ret;

	drm_connector_helper_add(&sii902x->connector,
		&sii902x_connector_helper_funcs);

	if (!drm_core_check_feature(drm, DRIVER_ATOMIC)) {
		dev_info(&sii902x->i2c->dev,
			"sii902x driver is only compatible with DRM devices supporting atomic updates");
		return -ENOTSUPP;
	}

	ret = drm_connector_init(drm, &sii902x->connector,
				 &sii902x_connector_funcs,
				 DRM_MODE_CONNECTOR_HDMIA);
	if (ret)
		return ret;

	if (sii902x->i2c->irq > 0)
		sii902x->connector.polled = DRM_CONNECTOR_POLL_HPD;
	else
		sii902x->connector.polled = DRM_CONNECTOR_POLL_CONNECT;

	drm_mode_connector_attach_encoder(&sii902x->connector, bridge->encoder);

	return 0;
}

static const struct drm_bridge_funcs sii902x_bridge_funcs = {
	.attach = sii902x_bridge_attach,
	.mode_set = sii902x_bridge_mode_set,
	.disable = sii902x_bridge_disable,
	.enable = sii902x_bridge_enable,
};

static const struct regmap_range sii902x_volatile_ranges[] = {
	{ .range_min = 0, .range_max = 0xff },
};

static const struct regmap_access_table sii902x_volatile_table = {
	.yes_ranges = sii902x_volatile_ranges,
	.n_yes_ranges = ARRAY_SIZE(sii902x_volatile_ranges),
};

static const struct regmap_config sii902x_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xff,
	.volatile_table = &sii902x_volatile_table,
	.cache_type = REGCACHE_NONE,
};

static const struct regmap_range sii902x_cec_volatile_ranges[] = {
	{ .range_min = 0, .range_max = 0xff },
};

static const struct regmap_access_table sii902x_cec_volatile_table = {
	.yes_ranges = sii902x_cec_volatile_ranges,
	.n_yes_ranges = ARRAY_SIZE(sii902x_cec_volatile_ranges),
};

static const struct regmap_config sii902x_cec_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xff,
	.volatile_table = &sii902x_cec_volatile_table,
	.cache_type = REGCACHE_NONE,
};
static irqreturn_t sii902x_interrupt(int irq, void *data)
{
	struct sii902x *sii902x = data;
	unsigned int status = 0;

	mutex_lock(&sii902x->mutex);
	regmap_read(sii902x->regmap, SII902X_INT_STATUS, &status);
	regmap_write(sii902x->regmap, SII902X_INT_STATUS, status);
	mutex_unlock(&sii902x->mutex);

	if(status & SII902X_CEC_EVENT)
	{
		sii9022_cec_handler();
	}

	if ((status & SII902X_HOTPLUG_EVENT) && sii902x->bridge.dev)
		drm_helper_hpd_irq_event(sii902x->bridge.dev);

	return IRQ_HANDLED;
}

/*
 * The purpose of sii902x_i2c_bypass_select is to enable the pass through
 * mode of the HDMI transmitter. Do not use regmap from within this function,
 * only use sii902x_*_unlocked functions to read/modify/write registers.
 * We are holding the parent adapter lock here, keep this in mind before
 * adding more i2c transactions.
 *
 * Also, since SII902X_SYS_CTRL_DATA is used with regmap_update_bits elsewhere
 * in this driver, we need to make sure that we only touch 0x1A[2:1] from
 * within sii902x_i2c_bypass_select and sii902x_i2c_bypass_deselect, and that
 * we leave the remaining bits as we have found them.
 */
static int sii902x_i2c_bypass_select(struct i2c_mux_core *mux, u32 chan_id)
{
	struct sii902x *sii902x = i2c_mux_priv(mux);
	struct device *dev = &sii902x->i2c->dev;
	unsigned long timeout;
	u8 status;
	int ret;

	ret = sii902x_update_bits_unlocked(sii902x->i2c, SII902X_SYS_CTRL_DATA,
					   SII902X_SYS_CTRL_DDC_BUS_REQ,
					   SII902X_SYS_CTRL_DDC_BUS_REQ);
	if (ret)
		return ret;

	timeout = jiffies +
		  msecs_to_jiffies(SII902X_I2C_BUS_ACQUISITION_TIMEOUT_MS);
	do {
		ret = sii902x_read_unlocked(sii902x->i2c, SII902X_SYS_CTRL_DATA,
					    &status);
		if (ret)
			return ret;
	} while (!(status & SII902X_SYS_CTRL_DDC_BUS_GRTD) &&
		 time_before(jiffies, timeout));

	if (!(status & SII902X_SYS_CTRL_DDC_BUS_GRTD)) {
		dev_err(dev, "Failed to acquire the i2c bus\n");
		return -ETIMEDOUT;
	}

	return sii902x_write_unlocked(sii902x->i2c, SII902X_SYS_CTRL_DATA,
				      status);
}

/*
 * The purpose of sii902x_i2c_bypass_deselect is to disable the pass through
 * mode of the HDMI transmitter. Do not use regmap from within this function,
 * only use sii902x_*_unlocked functions to read/modify/write registers.
 * We are holding the parent adapter lock here, keep this in mind before
 * adding more i2c transactions.
 *
 * Also, since SII902X_SYS_CTRL_DATA is used with regmap_update_bits elsewhere
 * in this driver, we need to make sure that we only touch 0x1A[2:1] from
 * within sii902x_i2c_bypass_select and sii902x_i2c_bypass_deselect, and that
 * we leave the remaining bits as we have found them.
 */
static int sii902x_i2c_bypass_deselect(struct i2c_mux_core *mux, u32 chan_id)
{
	struct sii902x *sii902x = i2c_mux_priv(mux);
	struct device *dev = &sii902x->i2c->dev;
	unsigned long timeout;
	unsigned int retries;
	u8 status;
	int ret;

	/*
	 * When the HDMI transmitter is in pass through mode, we need an
	 * (undocumented) additional delay between STOP and START conditions
	 * to guarantee the bus won't get stuck.
	 */
	udelay(30);

	/*
	 * Sometimes the I2C bus can stall after failure to use the
	 * EDID channel. Retry a few times to see if things clear
	 * up, else continue anyway.
	 */
	retries = 5;
	do {
		ret = sii902x_read_unlocked(sii902x->i2c, SII902X_SYS_CTRL_DATA,
					    &status);
		retries--;
	} while (ret && retries);
	if (ret) {
		dev_err(dev, "failed to read status (%d)\n", ret);
		return ret;
	}

	ret = sii902x_update_bits_unlocked(sii902x->i2c, SII902X_SYS_CTRL_DATA,
					   SII902X_SYS_CTRL_DDC_BUS_REQ |
					   SII902X_SYS_CTRL_DDC_BUS_GRTD, 0);
	if (ret)
		return ret;

	timeout = jiffies +
		  msecs_to_jiffies(SII902X_I2C_BUS_ACQUISITION_TIMEOUT_MS);
	do {
		ret = sii902x_read_unlocked(sii902x->i2c, SII902X_SYS_CTRL_DATA,
					    &status);
		if (ret)
			return ret;
	} while (status & (SII902X_SYS_CTRL_DDC_BUS_REQ |
			   SII902X_SYS_CTRL_DDC_BUS_GRTD) &&
		 time_before(jiffies, timeout));

	if (status & (SII902X_SYS_CTRL_DDC_BUS_REQ |
		      SII902X_SYS_CTRL_DDC_BUS_GRTD)) {
		dev_err(dev, "failed to release the i2c bus\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int sii9022_cec_adap_enable(struct cec_adapter *adap, bool enable)
{
	// Turn off CEC auto response to <Abort> command.
	//SiIRegioWrite(CEC_OP_ABORT_31, CLEAR_BITS);
	return 0;
}

static void __maybe_unused sii9022_print_cec_frame(struct cec_msg *frame)
{
	struct cec_msg *msg = frame;
	unsigned char header = msg->msg[0];

	pr_info("\ncec message initiator is %d\n", (header & 0xf0) >> 4);
	pr_info("cec message follower is %d\n", header & 0x0f);
	pr_info("cec message length is %d\n", msg->len);
	pr_info("cec message opcode is 0x%x\n", msg->msg[1]);
}

static int sii9022_cec_adap_log_addr(struct cec_adapter *adap, u8 logical_addr)
{
	pr_debug("cec message opcode is\n");

	SiIRegioWrite(CEC_OP_ABORT_31, CLEAR_BITS);
	if (!SI_CpiSetLogicalAddr(logical_addr))
		DEBUG_PRINT(MSG_ALWAYS, ("\n Cannot init CPI/CEC"));
	return 0;
}

static int sii9022_cec_adap_transmit(struct cec_adapter *adap, u8 attempts,
				 u32 signal_free_time, struct cec_msg *msg)
{
	struct SI_CpiData_t cecFrame;

	//~ sii9022_print_cec_frame(msg);

	if (msg->len == 1) {
		pr_debug("cec msg header only\n");
		SI_CpiSendPing(msg->msg[0] & 0xf);//send polling msg
	} else if (msg->len == 2) {
		pr_debug("cec msg header + opcode\n");
		cecFrame.srcDestAddr   = msg->msg[0];
	    cecFrame.opcode        = msg->msg[1];
	    cecFrame.argCount      = 0;
	    SI_CpiWrite(&cecFrame);
	} else if (msg->len >= 3) {
		int i;

		cecFrame.srcDestAddr   = msg->msg[0];
	    cecFrame.opcode        = msg->msg[1];
	    cecFrame.argCount      = msg->len - 2;
		for (i = 0; i < cecFrame.argCount; i++)
			cecFrame.args[i]   = msg->msg[i+2];
	    SI_CpiWrite(&cecFrame);
	}

	return 0;
}

static const struct cec_adap_ops sii9022_hdmi_cec_adap_ops = {
	.adap_enable = sii9022_cec_adap_enable,
	.adap_log_addr = sii9022_cec_adap_log_addr,
	.adap_transmit = sii9022_cec_adap_transmit,
};

static int sii902x_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	struct device *dev = &client->dev;

	if (strcmp(client->name, "sii9022") == 0) {
		unsigned int status = 0;
		struct sii902x *sii902x;
		u8 chipid[4];
 
		ret = i2c_check_functionality(client->adapter,
					      I2C_FUNC_SMBUS_BYTE_DATA);
		if (!ret) {
			dev_err(dev, "I2C adapter not suitable\n");
			return -EIO;
		}

		siHdmiTx_AudioSel(AFS_44K1);
		sii902x = devm_kzalloc(dev, sizeof(*sii902x), GFP_KERNEL);
		if (!sii902x)
			return -ENOMEM;

		sii902x_debug = sii902x;
		sii902x_mutex = sii902x;
		sii902x_for_i2c = sii902x;

		sii902x->i2c = client;
		sii902x->regmap =
			devm_regmap_init_i2c(client,
			&sii902x_regmap_config);
		if (IS_ERR(sii902x->regmap))
			return PTR_ERR(sii902x->regmap);

		sii902x->reset_gpio = devm_gpiod_get_optional(dev, "reset",
			GPIOD_OUT_LOW);
		if (IS_ERR(sii902x->reset_gpio)) {
			dev_info(dev, "Failed to retrieve/request reset gpio: %ld\n",
				PTR_ERR(sii902x->reset_gpio));
			return PTR_ERR(sii902x->reset_gpio);
		}

		/*9022 mutex init*/
		mutex_init(&sii902x->mutex);

		sii902x_reset(sii902x);

		sii902x_for_i2c->regmap =
			sii902x->regmap;

		ret = regmap_write(sii902x->regmap, SII902X_REG_TPI_RQB, 0x0);
		if (ret)
			return ret;

		ret = regmap_bulk_read(sii902x->regmap,
			SII902X_REG_CHIPID(0), &chipid, 4);
		if (ret) {
			dev_info(dev, "regmap_read failed %d\n", ret);
			return ret;
		}

		if (chipid[0] != 0xb0) {
			dev_info(dev, "Invalid chipid: %02x (expecting 0xb0)\n",
					chipid[0]);
			return -EINVAL;
		}
		/* Clear all pending interrupts */
		regmap_read(sii902x->regmap, SII902X_INT_STATUS, &status);
		regmap_write(sii902x->regmap, SII902X_INT_STATUS, status);

		if (client->irq > 0) {
			regmap_write(sii902x->regmap,
				SII902X_INT_ENABLE,
				SII902X_HOTPLUG_EVENT | SII902X_CEC_EVENT);

			ret = devm_request_threaded_irq(dev, client->irq, NULL,
						sii902x_interrupt,
						IRQF_TRIGGER_LOW |
						IRQF_ONESHOT,
						"hdmi-irq",
						sii902x);
			if (ret)
				return ret;
		}

		sii902x->bridge.funcs = &sii902x_bridge_funcs;
		sii902x->bridge.of_node = dev->of_node;
		ret = drm_bridge_add(&sii902x->bridge);
		if (ret) {
			dev_info(dev, "Failed to add drm_bridge\n");
			return ret;
		}

		i2c_set_clientdata(client, sii902x);
		Sii902x_DBG_Init();

		sii902x->i2cmux = i2c_mux_alloc(client->adapter, dev,
					1, 0, I2C_MUX_GATE,
					sii902x_i2c_bypass_select,
					sii902x_i2c_bypass_deselect);
		if (!sii902x->i2cmux)
			return -ENOMEM;

		sii902x->i2cmux->priv = sii902x;
		return i2c_mux_add_adapter(sii902x->i2cmux, 0, 0, 0);

	} else if (strcmp(client->name, "sii9022-cec") == 0) {

		cec = devm_kzalloc(dev, sizeof(*cec), GFP_KERNEL);
		if (!cec)
			return -ENOMEM;

		sii902x_cec_regmap =
			devm_regmap_init_i2c(client, &sii902x_regmap_config);
				if (IS_ERR(sii902x_cec_regmap))
					return PTR_ERR(sii902x_cec_regmap);
#if 1
		cec->adap = cec_allocate_adapter(&sii9022_hdmi_cec_adap_ops,
					cec, "sii-hdmi-cec",
					CEC_CAP_TRANSMIT | CEC_CAP_PASSTHROUGH |
					CEC_CAP_LOG_ADDRS | CEC_CAP_PHYS_ADDR, 1);

		ret = PTR_ERR_OR_ZERO(cec->adap);
		if (ret < 0) {
			dev_info(dev,
				"Failed to allocate cec adapter %d\n", ret);
			return ret;
		}

		ret = cec_register_adapter(cec->adap, dev);
		if (ret) {
			dev_info(dev, "Fail to register cec adapter\n");
			cec_delete_adapter(cec->adap);
			return ret;
		}
		SiIRegioWrite(REG_CEC_INT_ENABLE_0, 0x27);
		SiIRegioWrite(REG_CEC_INT_ENABLE_1, 0x0F);
		SiIRegioWrite(CEC_OP_ABORT_31, CLEAR_BITS);
		//~ if (!SI_CpiSetLogicalAddr(0x50))
			//~ DEBUG_PRINT(MSG_ALWAYS, ("\n Cannot init CPI/CEC"));
#endif
	}
	return ret;
}

static int sii902x_remove(struct i2c_client *client)
{
	struct sii902x *sii902x = i2c_get_clientdata(client);

	i2c_mux_del_adapters(sii902x->i2cmux);
	drm_bridge_remove(&sii902x->bridge);

	return 0;
}

static const struct of_device_id sii902x_dt_ids[] = {
	{ .compatible = "sil,sii9022", },
	{ .compatible = "sil,sii9022-cec", },
	{ }
};

MODULE_DEVICE_TABLE(of, sii902x_dt_ids);

static const struct i2c_device_id sii902x_i2c_ids[] = {
	{ "sii9022", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, sii902x_i2c_ids);

static struct i2c_driver sii902x_driver = {
	.probe = sii902x_probe,
	.remove = sii902x_remove,
	.driver = {
		   .name = "sii902x",
		   .of_match_table = sii902x_dt_ids,
		   },
	.id_table = sii902x_i2c_ids,
};

module_i2c_driver(sii902x_driver);

MODULE_AUTHOR("Boris Brezillon <boris.brezillon@free-electrons.com>");
MODULE_DESCRIPTION("SII902x RGB -> HDMI bridges");
MODULE_LICENSE("GPL");
