/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/completion.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/signal.h>
#include <sound/memalloc.h>
#include <linux/of_device.h>
#include <linux/dma-mapping.h>
#include <linux/bitops.h>
#include <linux/string.h>
#include "mt8518-asrc-hw.h"
#include "mt8518-asrc-clk.h"
#include "mt8518-asrc-reg.h"
#include <linux/pm_runtime.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include "mtk_asrc_common.h"

#define ASRC_SIGNAL_REASON_IBUF_EMPTY  (0U)
#define ASRC_SIGNAL_REASON_OBUF_FULL   (1U)

#define ASRC_BUFFER_SIZE_MAX   (1024 * 12)
#define INPUT  (0)
#define OUTPUT (1)

#define ASRC_IRQ_ENABLE (1)
#define ASRC_IRQ_DISABLE (!ASRC_IRQ_ENABLE)

#define DUMP_REG_ENTRY(reg) {reg, #reg}
#define toAsrcDev(x, id) container_of(mdev, \
		struct asrc_devices_info, miscdev[id])

struct regmap *asrc_regmap;
static unsigned int asrc_irq[ASRC_MAX_NUM];
static const char *isr_names[ASRC_MAX_NUM] = {
	"asrc0-isr", "asrc1-isr", "asrc2-isr", "asrc3-isr"
};

static struct asrc_devices_info *asrc_dev_info;

struct mt8518_asrc_debug_fs {
	char *fs_name;
	const struct file_operations *fops;
};

struct asrc_dump_reg_attr {
	uint32_t offset;
	char *name;
};

static const struct asrc_dump_reg_attr asrc_dump_regs[] = {
	DUMP_REG_ENTRY(MASRC_TOP_CON),
	DUMP_REG_ENTRY(MASRC_ASM_CON2),
	DUMP_REG_ENTRY(REG_ASRC_GEN_CONF),
	DUMP_REG_ENTRY(REG_ASRC_IER),
	DUMP_REG_ENTRY(REG_ASRC_IFR),
	DUMP_REG_ENTRY(REG_ASRC_IBUF_INTR_CNT0),
	DUMP_REG_ENTRY(REG_ASRC_OBUF_INTR_CNT0),
	DUMP_REG_ENTRY(REG_ASRC_CH01_CNFG),
	DUMP_REG_ENTRY(REG_ASRC_IBUF_SADR),
	DUMP_REG_ENTRY(REG_ASRC_IBUF_SIZE),
	DUMP_REG_ENTRY(REG_ASRC_CH01_IBUF_RDPNT),
	DUMP_REG_ENTRY(REG_ASRC_CH01_IBUF_WRPNT),
	DUMP_REG_ENTRY(REG_ASRC_OBUF_SADR),
	DUMP_REG_ENTRY(REG_ASRC_OBUF_SIZE),
	DUMP_REG_ENTRY(REG_ASRC_CH01_OBUF_WRPNT),
	DUMP_REG_ENTRY(REG_ASRC_CH01_OBUF_RDPNT),
	DUMP_REG_ENTRY(REG_ASRC_CH01_CNFG),
	DUMP_REG_ENTRY(REG_ASRC_FREQUENCY_0),
	DUMP_REG_ENTRY(REG_ASRC_FREQUENCY_1),
	DUMP_REG_ENTRY(REG_ASRC_FREQ_CALI_CTRL),
	DUMP_REG_ENTRY(REG_ASRC_FREQ_CALI_CYC),
	DUMP_REG_ENTRY(REG_ASRC_CALI_DENOMINATOR),
	DUMP_REG_ENTRY(REG_ASRC_PRD_CALI_RESULT),
	DUMP_REG_ENTRY(REG_ASRC_FREQ_CALI_RESULT),
};

struct asrc_buf {
	unsigned char *area;	/* virtual pointer */
	dma_addr_t addr;		/* physical address */
	size_t bytes;		/* buffer size in bytes */
};

struct asrc_private {
	enum afe_mem_asrc_id id;
	int running;
	int stereo;
	struct asrc_buf input_dmab;
	u32 input_freq;
	u32 input_bitwidth;
	struct asrc_buf output_dmab;
	u32 output_freq;
	u32 output_bitwidth;
	struct fasync_struct *fasync_queue;
	unsigned long signal_reason;
	struct completion irq_fill_buf_finish;
	enum afe_mem_asrc_tracking_mode tracking_mode;
	enum afe_mem_asrc_tracking_source tracking_src;
	u32 cali_cycle;
	struct device *dev;
};

struct asrc_devices_info {
	struct device *dev;
	//miscdev: asrc1 ~ 4.
	struct miscdevice miscdev[ASRC_MAX_NUM];
	struct asrc_private *asrc_priv[ASRC_MAX_NUM];
	spinlock_t asrc_ctrl_lock[ASRC_MAX_NUM];
	u32 cali_ref_count[ASRC_MAX_NUM];
};

// ASRC_BUF_DIR : in and out.
#define ASRC_BUF_DIR 2
static struct asrc_buf asrc_buffers[ASRC_MAX_NUM][ASRC_BUF_DIR];

static int asrc_allocate_buffer(enum afe_mem_asrc_id id, struct device *dev,
				int dir, size_t bytes)
{
	int ret = 0;

	if (unlikely(id >= ASRC_MAX_NUM))
		return -EINVAL;
	if (unlikely(dir != INPUT && dir != OUTPUT))
		return -EINVAL;

	asrc_buffers[id][dir].area = dma_alloc_coherent(dev, bytes,
		&(asrc_buffers[id][dir].addr), GFP_KERNEL);
	if (asrc_buffers[id][dir].area == NULL) {
		ret = -ENOMEM;
		dev_dbg(dev, "%s() error: dma_alloc_coherent failed for asrc[%d]'s %s_buffer, ret=%d\n",
			__func__, id, (dir != 0) ? "output" : "input", ret);
	}
	asrc_buffers[id][dir].bytes = bytes;
	dev_dbg(dev, "asrc-%d, dir:%d, area:%p, byte:%ld\n", id, dir,
		asrc_buffers[id][dir].area, (long int)bytes);
	return ret;
}

static void asrc_free_buffer(enum afe_mem_asrc_id id,
			     struct device *dev, int dir)
{
	if (unlikely(id >= ASRC_MAX_NUM))
		return;
	if (unlikely(dir != INPUT && dir != OUTPUT))
		return;

	dma_free_coherent(dev, asrc_buffers[id][dir].bytes,
		asrc_buffers[id][dir].area, asrc_buffers[id][dir].addr);
	memset(&asrc_buffers[id][dir], 0, sizeof(struct asrc_buf));
}

static int asrc_register_irq(enum afe_mem_asrc_id id, irq_handler_t isr,
	irq_handler_t thread_isr, const char *name, void *dev)
{
	int ret;
	struct asrc_private *priv = dev;

	if (unlikely(id >= ASRC_MAX_NUM)) {
		dev_info(priv->dev, "%s() error: invalid id %u\n",
			__func__, id);
		return -EINVAL;
	}
	ret = request_threaded_irq(asrc_irq[id], isr, thread_isr,
			  IRQF_TRIGGER_LOW | IRQF_ONESHOT, name, priv);
	if (ret != 0)
		dev_info(priv->dev, "%s() can't register ISR for mem asrc[%u] (ret=%i)\n",
			 __func__, id, ret);
	return ret;
}

static int asrc_unregister_irq(enum afe_mem_asrc_id id, void *dev)
{
	struct asrc_private *priv = dev;

	if (unlikely(id >= ASRC_MAX_NUM)) {
		dev_info(priv->dev, "%s() error: invalid id %u\n",
			__func__, id);
		return -EINVAL;
	}
	free_irq(asrc_irq[id], dev);
	return 0;
}

#define ASRC_MAX_REG 0x1090U
//static int asrc_regmap_init(struct device *dev)
static const struct regmap_config mt8518_asrc_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = ASRC_MAX_REG,
	.cache_type = REGCACHE_NONE,
};

static int asrc_regmap_init(struct platform_device *pdev)
{
	struct resource *res;
	void __iomem *asrc_base_addr;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	asrc_base_addr = devm_ioremap_resource(&pdev->dev, res);

	if (IS_ERR(asrc_base_addr))
		return PTR_ERR(asrc_base_addr);

	asrc_regmap = devm_regmap_init_mmio(&pdev->dev, asrc_base_addr,
	&mt8518_asrc_regmap_config);

	if (!asrc_regmap) {
		dev_dbg(&pdev->dev, "could not get regmap from parent\n");
		return -ENODEV;
	}
	dev_dbg(&pdev->dev, "%s - platform name:%s, dev name:%s ...\n",
		__func__, pdev->name, pdev->dev.init_name);

	return 0;
}

static irqreturn_t asrc_isr(int irq, void *dev)
{
	struct asrc_private *priv = dev;
	enum afe_mem_asrc_id id = priv->id;
	u32 status = afe_mem_asrc_irq_status(id);

	if ((afe_mem_asrc_irq_is_enabled(id, IBUF_EMPTY_INT) != 0)
	    && ((status & IBUF_EMPTY_INT) != 0U))
		set_bit((int)ASRC_SIGNAL_REASON_IBUF_EMPTY,
			&priv->signal_reason);
	if ((afe_mem_asrc_irq_is_enabled(id, OBUF_OV_INT) != 0)
	    && ((status & OBUF_OV_INT) != 0U))
		set_bit((int)ASRC_SIGNAL_REASON_OBUF_FULL,
			&priv->signal_reason);
	if (priv->fasync_queue != NULL && priv->signal_reason)
		kill_fasync(&priv->fasync_queue, SIGIO, POLL_IN);

	return IRQ_WAKE_THREAD;
}


static irqreturn_t asrc_thread_isr(int irq, void *dev)
{
	struct asrc_private *priv = dev;
	enum afe_mem_asrc_id id = priv->id;
	long ret;

	ret = wait_for_completion_killable_timeout(&priv->irq_fill_buf_finish,
						   msecs_to_jiffies(1000));
	if (ret > 0) {
		if (test_and_clear_bit((int)ASRC_SIGNAL_REASON_IBUF_EMPTY,
					&priv->signal_reason))
			afe_mem_asrc_irq_clear(id, IBUF_EMPTY_INT);
		if (test_and_clear_bit((int)ASRC_SIGNAL_REASON_OBUF_FULL,
					&priv->signal_reason))
			afe_mem_asrc_irq_clear(id, OBUF_OV_INT);
	} else if (ret == 0) {
		dev_dbg(priv->dev, "timeout in asrc %d irq\n", id);
		priv->signal_reason = 0;
	} else {
		dev_dbg(priv->dev, "wait_for_completion failed (killed?), %d irq, ret: %lu\n",
			 id, ret);
	}

	return IRQ_HANDLED;
}


static int asrc_fasync(int fd, struct file *filp, int on)
{
	struct asrc_private *priv = filp->private_data;

	if (priv == NULL)
		return -EFAULT;
	dev_dbg(priv->dev, "%s()\n", __func__);
	return fasync_helper(fd, filp, on, &priv->fasync_queue);
}

static inline int asrc_srate_map(int srate)
{
	switch (srate) {
	case 8000:
		return 0x050000;
	case 12000:
		return 0x078000;
	case 16000:
		return 0x0A0000;
	case 24000:
		return 0x0F0000;
	case 32000:
		return 0x140000;
	case 48000:
		return 0x1E0000;
	case 96000:
		return 0x3C0000;
	case 192000:
		return 0x780000;
	case 384000:
		return 0xF00000;
	case 7350:
		return 0x049800;
	case 11025:
		return 0x06E400;
	case 14700:
		return 0x093000;
	case 22050:
		return 0x0DC800;
	case 29400:
		return 0x126000;
	case 44100:
		return 0x1B9000;
	case 88200:
		return 0x372000;
	case 176400:
		return 0x6E4000;
	case 352800:
		return 0xDC8000;
	default:
		return 0;
	}
}

int mtk_asrc_cali_open(enum afe_mem_asrc_id id)
{
	unsigned long flags;
	bool first_open = false;

	if (id >= ASRC_MAX_NUM)
		return -EINVAL;
	if (asrc_dev_info == NULL)
		return -EINVAL;

	/* asrc-id doesn't be used. */
	spin_lock_irqsave(&asrc_dev_info->asrc_ctrl_lock[id], flags);
	first_open = (asrc_dev_info->asrc_priv[id] == NULL);
	asrc_dev_info->cali_ref_count[id]++;
	spin_unlock_irqrestore(&asrc_dev_info->asrc_ctrl_lock[id], flags);
	if (first_open == true &&
		asrc_dev_info->cali_ref_count[id] == 1) {
		asrc_turn_on_asrc_clock();
		asrc_dev_info->asrc_priv[id] = kzalloc(
						sizeof(struct asrc_private),
						GFP_KERNEL);
		afe_power_on_mem_asrc_brg(ASRC_ENABLE);
		afe_power_on_mem_asrc(id, ASRC_ENABLE);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_asrc_cali_open);

int mtk_asrc_cali_release(enum afe_mem_asrc_id id)
{
	unsigned long flags;
	struct asrc_private *asrc_priv;

	if (id >= ASRC_MAX_NUM)
		return -EINVAL;
	if (asrc_dev_info == NULL)
		return -EINVAL;

	asrc_priv = asrc_dev_info->asrc_priv[id];
	if (asrc_priv == NULL)
		return -EINVAL;

	spin_lock_irqsave(&asrc_dev_info->asrc_ctrl_lock[id], flags);
	asrc_dev_info->cali_ref_count[id]--;
	spin_unlock_irqrestore(&asrc_dev_info->asrc_ctrl_lock[id], flags);
	if (asrc_dev_info->cali_ref_count[id] == 0) {
		afe_power_on_mem_asrc(id, ASRC_DISABLE);
		afe_power_on_mem_asrc_brg(ASRC_DISABLE);
		kzfree(asrc_priv);
		asrc_priv = NULL;
		asrc_dev_info->asrc_priv[id] = NULL;
		asrc_turn_off_asrc_clock();
	}
	if (asrc_dev_info->cali_ref_count[id] < 0) {
		asrc_dev_info->cali_ref_count[id] = 0;
		dev_info(asrc_priv->dev, "%s() cali_ref_count < 0\n", __func__);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_asrc_cali_release);

int mtk_asrc_cali_start(enum afe_mem_asrc_id id,
	enum afe_mem_asrc_tracking_mode tracking_mode,
	enum afe_mem_asrc_tracking_source tracking_src,
	u32 cali_cycle,
	u32 input_freq,
	u32 output_freq)
{
	int res;
	struct asrc_private *asrc_priv;

	if (asrc_dev_info == NULL)
		return -EINVAL;

	asrc_priv = asrc_dev_info->asrc_priv[id];
	if (asrc_priv == NULL)
		return -EINVAL;

	asrc_priv->cali_cycle = cali_cycle;
	asrc_priv->input_freq = input_freq = asrc_srate_map(input_freq);
	asrc_priv->output_freq = output_freq = asrc_srate_map(output_freq);
	asrc_priv->tracking_mode = tracking_mode;
	asrc_priv->tracking_src = tracking_src;

	res = afe_mem_asrc_cali_setup(id,
		tracking_mode,
		tracking_src,
		cali_cycle,
		input_freq,
		output_freq);
	if (res == 0)
		afe_mem_asrc_cali_enable(id,
			ASRC_CALI_ENABLE);

	return res;
}
EXPORT_SYMBOL_GPL(mtk_asrc_cali_start);

u64 mtk_asrc_cali_get_result(enum afe_mem_asrc_id id)
{
	u64 res = 0;
	u32 freq;
	struct asrc_private *asrc_priv;

	if (id >= ASRC_MAX_NUM)
		return 0;
	if (asrc_dev_info == NULL)
		return 0;

	asrc_priv = asrc_dev_info->asrc_priv[id];
	if (asrc_priv == NULL)
		return 0;

	freq = asrc_priv->output_freq;
	res = afe_mem_asrc_calibration_result(id, freq);

	return res;
}
EXPORT_SYMBOL_GPL(mtk_asrc_cali_get_result);

int mtk_asrc_cali_stop(enum afe_mem_asrc_id id)
{
	struct asrc_private *asrc_priv;

	if (id >= ASRC_MAX_NUM)
		return -EINVAL;
	if (asrc_dev_info == NULL)
		return -EINVAL;

	asrc_priv = asrc_dev_info->asrc_priv[id];
	if (asrc_priv == NULL)
		return -EINVAL;

	return afe_mem_asrc_cali_enable(id, ASRC_CALI_DISABLE);
}
EXPORT_SYMBOL_GPL(mtk_asrc_cali_stop);

static enum afe_mem_asrc_id get_asrc_id(const struct file *filp);

static int asrc_open(struct inode *node, struct file *filp)
{
	struct asrc_private *priv;
	enum afe_mem_asrc_id id = get_asrc_id(filp);
	int ret;
	struct miscdevice *mdev = (struct miscdevice *)filp->private_data;
	struct asrc_devices_info *asrc_dev_inf = toAsrcDev(mdev, id);
	struct device *dev = asrc_dev_inf->dev;

	dev_info(dev, "%s() id=%u\n", __func__, id);

	mtk_asrc_cali_open(id);
	priv = asrc_dev_inf->asrc_priv[id];
	if (priv == NULL)
		goto open_error;

	init_completion(&priv->irq_fill_buf_finish);

	if (asrc_register_irq(id, asrc_isr, asrc_thread_isr,
			      isr_names[id], priv) != 0)
		goto register_irq_error;

	priv->dev = asrc_dev_inf->dev;
	priv->id = id;
	filp->private_data = priv;

	priv->input_dmab.area = asrc_buffers[id][INPUT].area;
	priv->input_dmab.addr = asrc_buffers[id][INPUT].addr;
	priv->input_dmab.bytes = asrc_buffers[id][INPUT].bytes;
	priv->output_dmab.area = asrc_buffers[id][OUTPUT].area;
	priv->output_dmab.addr = asrc_buffers[id][OUTPUT].addr;
	priv->output_dmab.bytes = asrc_buffers[id][OUTPUT].bytes;

	ret = afe_mem_asrc_irq_enable(id, IBUF_EMPTY_INT, ASRC_IRQ_DISABLE);
	ret = afe_mem_asrc_irq_enable(id, OBUF_OV_INT, ASRC_IRQ_DISABLE);

	return 0;
register_irq_error:
	mtk_asrc_cali_release(id);
open_error:
	return -EINVAL;
}


static int asrc_release(struct inode *node, struct file *filp)
{
	enum afe_mem_asrc_id id;
	struct asrc_private *priv = filp->private_data;
	int ret = 0;

	dev_dbg(priv->dev, "%s()\n", __func__);
	if (priv == NULL)
		return -ENOMEM;
	id = priv->id;
	if (id >= ASRC_MAX_NUM) {
		dev_info(priv->dev, "%s() error: invalid id %u\n",
			__func__, id);
		return -EINVAL;
	}
	ret = afe_mem_asrc_irq_enable(id, IBUF_EMPTY_INT, ASRC_IRQ_DISABLE);
	ret = afe_mem_asrc_irq_enable(id, OBUF_OV_INT, ASRC_IRQ_DISABLE);
	ret = afe_mem_asrc_enable(id, ASRC_DISABLE);

	ret = asrc_unregister_irq(id, priv);
	ret = asrc_fasync(-1, filp, 0);
	filp->private_data = NULL;
	mtk_asrc_cali_release(id);

	return ret;
}

static int is_ibuf_empty(enum afe_mem_asrc_id id)
{
	u32 wp, rp;

	wp = afe_mem_asrc_get_ibuf_wp(id);
	rp = afe_mem_asrc_get_ibuf_rp(id);
	return (int) (wp == rp);
}

static int is_obuf_empty(enum afe_mem_asrc_id id, size_t bytes)
{
	u32 wp, rp;

	wp = afe_mem_asrc_get_obuf_wp(id);
	rp = afe_mem_asrc_get_obuf_rp(id);
	if (wp > rp)
		return (int)(wp - rp == 16U);
	else
		return (int)(wp + bytes - rp == 16U);
}

static long asrc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct asrc_private *priv = filp->private_data;
	enum afe_mem_asrc_id id;
	long res = 0;

	dev_dbg(priv->dev, "%s() cmd=0x%08x, arg=0x%lx\n", __func__, cmd, arg);
	if (priv == NULL)
		return -ENOMEM;
	id = priv->id;
	if (id >= ASRC_MAX_NUM) {
		dev_dbg(priv->dev, "%s() error: invalid id %u\n", __func__, id);
		return -EINVAL;
	}
	switch ((u64)cmd) {
	case ASRC_CMD_CHANNELS:
		priv->stereo = (arg == 1UL) ? 0 : 1;
		break;
	case ASRC_CMD_INPUT_FREQ:
		priv->input_freq = (u32)arg;
		afe_mem_asrc_set_ibuf_freq(id, (u32)arg);
		break;
	case ASRC_CMD_INPUT_BITWIDTH:
		priv->input_bitwidth = (u32)arg;
		break;
	case ASRC_CMD_INPUT_BUFSIZE: {
		struct asrc_buf *b;

		if (priv->running != 0) {
			dev_dbg(priv->dev, "%s() error: can't set input buffer size while running\n",
				 __func__);
			return -EPERM;
		}
		b = &asrc_buffers[id][INPUT];
		priv->input_dmab.area = b->area;
		priv->input_dmab.addr = b->addr;
		if (arg + 16 > b->bytes) {
			dev_dbg(priv->dev, "%s() error: too large input buffer size 0x%lx (MAX=0x%lx)\n",
				 __func__, arg, (long int)b->bytes);
			priv->input_dmab.bytes = b->bytes;
			return -ENOMEM;
		}
		priv->input_dmab.bytes = arg + 16;
		break;
	}
	case ASRC_CMD_OUTPUT_FREQ:
		priv->output_freq = (u32)arg;
		afe_mem_asrc_set_obuf_freq(id, (u32)arg);
		break;
	case ASRC_CMD_OUTPUT_BITWIDTH:
		priv->output_bitwidth = (u32)arg;
		break;
	case ASRC_CMD_OUTPUT_BUFSIZE: {
		struct asrc_buf *b;

		if (priv->running != 0) {
			dev_dbg(priv->dev, "%s() error: can't set output buffer size while running\n",
				 __func__);
			return -EPERM;
		}
		b = &asrc_buffers[id][OUTPUT];
		priv->output_dmab.area = b->area;
		priv->output_dmab.addr = b->addr;
		if (arg + 16 > b->bytes) {
			dev_dbg(priv->dev, "%s() error: too large output buffer size 0x%lx (MAX=0x%lx)\n",
				 __func__, arg, (long int)b->bytes);
			priv->output_dmab.bytes = b->bytes;
			return -ENOMEM;
		}
		priv->output_dmab.bytes = arg + 16;
		break;
	}
	case ASRC_CMD_TRIGGER: {
		int ret;

		if (arg != 0UL) {
			uintptr_t clean_ptr;
			struct afe_mem_asrc_config config = {
				.input_buffer = {
					.base = (u32)priv->input_dmab.addr,
					.size = (u32)priv->input_dmab.bytes,
					.freq = priv->input_freq,
					.bitwidth = priv->input_bitwidth
				},
				.output_buffer = {
					.base = (u32)priv->output_dmab.addr,
					.size = (u32)priv->output_dmab.bytes,
					.freq = priv->output_freq,
					.bitwidth = priv->output_bitwidth
				},
				.stereo = priv->stereo,
				.tracking_mode = priv->tracking_mode,
				.tracking_src = priv->tracking_src,
				.cali_cycle = priv->cali_cycle
			};
			ret = afe_mem_asrc_configurate(id, &config);
			if (ret != 0) {
				dev_dbg(priv->dev, "%s() error: configurate asrc return %d\n",
					 __func__, ret);
				return -EINVAL;
			}

			/* clean write buffer ptr to zero to prevent noise */
			clean_ptr = (uintptr_t)priv->output_dmab.area;
			clean_ptr = clean_ptr + priv->output_dmab.bytes - 16;
			memset((void *)clean_ptr, 0, 16);

			if (priv->tracking_mode != MEM_ASRC_NO_TRACKING) {
				ret = afe_mem_asrc_cali_enable(id,
							ASRC_CALI_ENABLE);
				if (ret != 0) {
					dev_dbg(priv->dev, "%s() error: disable asrc cali return %d\n",
						__func__, ret);
					return -EINVAL;
				}
			}

			ret = afe_mem_asrc_enable(id, ASRC_ENABLE);
			if (ret != 0) {
				dev_dbg(priv->dev, "%s() error: enable asrc return %d\n",
					 __func__, ret);
				return -EINVAL;
			}

			priv->running = 1;
		} else {
			unsigned long flags;
			u32 ref_cnt;

			priv->running = 0;
			if (priv->tracking_mode != MEM_ASRC_NO_TRACKING) {
				spin_lock_irqsave(
					&asrc_dev_info->asrc_ctrl_lock[id],
					flags);
				ref_cnt = --asrc_dev_info->cali_ref_count[id];
				spin_unlock_irqrestore(
					&asrc_dev_info->asrc_ctrl_lock[id],
					flags);
				if (ref_cnt == 0) {
					ret = afe_mem_asrc_cali_enable(id,
							ASRC_CALI_DISABLE);
					if (ret != 0) {
						dev_dbg(priv->dev, "%s() error: disable asrc cali return %d\n",
						 __func__, ret);
						return -EINVAL;
					}
				}
			}

			ret = afe_mem_asrc_enable(id, ASRC_DISABLE);
			if (ret != 0) {
				dev_dbg(priv->dev, "%s() error: disable asrc return %d\n",
					 __func__, ret);
				return -EINVAL;
			}
		}
		break;
	}
	case ASRC_CMD_IS_DRAIN: {
		int is_drain;

		if (arg == 0UL)
			return -EFAULT;
		if (priv->running != 0) {
			if ((is_ibuf_empty(id) != 0) &&
			    (is_obuf_empty(id, priv->output_dmab.bytes) != 0))
				is_drain = 1;
			else
				is_drain = 0;
		} else
			is_drain = 1;
		if (copy_to_user((void __user *)(arg), (void *)(&is_drain),
		    sizeof(is_drain)) != 0U)
			return -EFAULT;
		break;
	}
	case ASRC_CMD_SIGNAL_REASON: {
		unsigned long reason;

		if (arg == 0UL)
			return -EFAULT;
		reason = priv->signal_reason;
		if (copy_to_user((void __user *)(arg), (void *)(&reason),
		    sizeof(reason)) != 0U)
			return -EFAULT;
		break;
	}
	case ASRC_CMD_SIGNAL_CLEAR:
		complete(&priv->irq_fill_buf_finish);
		break;
	case ASRC_CMD_TRACK_MODE:
		priv->tracking_mode = (u32)arg;
		break;
	case ASRC_CMD_TRACK_SRC:
		priv->tracking_src = (u32)arg;
		break;
	case ASRC_CMD_CALI_CYC:
		priv->cali_cycle = (u32)arg;
		break;
	case ASRC_CMD_CALI_MODE_SET:
		res = afe_mem_asrc_cali_setup(id,
			priv->tracking_mode,
			priv->tracking_src,
			priv->cali_cycle,
			priv->input_freq,
			priv->output_freq);
		break;
	case ASRC_CMD_CALI_MODE_GET: {
		u64 cali_result_rate;
		struct CALI_FS cali_fs;

		cali_result_rate = afe_mem_asrc_calibration_result(id,
				priv->output_freq);
		if (cali_result_rate < 0)
			return -EINVAL;
		cali_fs.integer = div_u64_rem(cali_result_rate, 100000,
				&cali_fs.decimal);
		if (copy_to_user((void __user *)(arg), (void *)(&cali_fs),
		    sizeof(cali_fs)) != 0U)
			return -EFAULT;

		dev_dbg(priv->dev, "%s() cali_result_rate:%llu\n", __func__,
			cali_result_rate);
		dev_dbg(priv->dev, "%s() cali_result_rate:%d.%d\n", __func__,
			cali_fs.integer, cali_fs.decimal);
		break;
	}
	case ASRC_CMD_CALI_MODE_TRIGGER:
		/* cali enable */
		if (arg != 0UL)
			afe_mem_asrc_cali_enable(id,
				ASRC_CALI_ENABLE);
		else
			afe_mem_asrc_cali_enable(id, ASRC_CALI_DISABLE);

		break;
	default:
		dev_info(priv->dev, "%s() error: unknown asrc cmd 0x%08x\n",
			 __func__, cmd);
		res = -EINVAL;
		break;
	}
	return res;
}

static ssize_t asrc_read(struct file *filp, char __user *buf,
			 size_t count, loff_t *offset)
{
	struct asrc_private *priv = filp->private_data;
	enum afe_mem_asrc_id id;
	struct asrc_buf *dmab;
	u32 wp, rp, copy;
	unsigned char *copy_start;

	dev_dbg(priv->dev, "%s()\n", __func__);
	if (unlikely(priv == NULL))
		return 0;
	id = priv->id;
	if (unlikely(id >= ASRC_MAX_NUM)) {
		dev_info(priv->dev, "%s() error: invalid id %u\n",
			__func__, id);
		return 0;
	}
	if (priv->running == 0) {
		dev_info(priv->dev, "%s() warning: asrc[%u] is not running\n",
			 __func__, id);
		return 0;
	}
	dmab = &priv->output_dmab;
	wp = afe_mem_asrc_get_obuf_wp(id);
	rp = afe_mem_asrc_get_obuf_rp(id);
	copy_start = dmab->area + (rp - dmab->addr);
	count = count / 16UL * 16UL;
	dev_dbg(priv->dev, "%s() wp=0x%08x, rp=0x%08x\n",
		__func__, wp, rp);
	if (rp < wp) {
		u32 delta = wp - rp;

		copy = delta - 16U < (u32)count ? delta - 16U : (u32)count;
		if (copy == 0U)
			return 0;

		if (copy_to_user(buf, copy_start, copy) != 0UL) {
			dev_info(priv->dev, "%s() error: L%d copy_to_user\n",
				 __func__, __LINE__);
			return 0;
		}
		afe_mem_asrc_set_obuf_rp(id, rp + copy);
		dev_dbg(priv->dev, "%s() copy=%u (line %d)\n",
			__func__, copy, __LINE__);
	} else {		/* rp >= wp */
		u32 delta = wp + (u32)dmab->bytes - rp;

		copy = delta - 16U < (u32)count ? delta - 16U : (u32)count;
		if (copy == 0U)
			return 0;
		if ((u64)rp + (u64)copy < dmab->addr + dmab->bytes) {
			if (copy_to_user(buf, copy_start, copy) != 0UL) {
				dev_info(priv->dev, "%s() error: L%d copy_to_user\n",
					 __func__, __LINE__);
				return 0;
			}
			afe_mem_asrc_set_obuf_rp(id, rp + copy);
			dev_dbg(priv->dev, "%s() copy=%u (line %d)\n", __func__,
				 copy, __LINE__);
			return (ssize_t)copy;

		} else {
			u32 s1 = (u32)(dmab->addr + dmab->bytes - rp);
			u32 s2 = copy - s1;

			if (copy_to_user(buf, copy_start, s1) != 0UL) {
				dev_info(priv->dev, "%s() error: L%d copy_to_user\n",
					 __func__, __LINE__);
				return 0;
			}
			if (s2 != 0U) {
				if (copy_to_user(buf + s1, dmab->area, s2)
				    != 0UL) {
					dev_dbg(priv->dev, "%s() error: L%d copy_to_user\n",
						 __func__, __LINE__);
					afe_mem_asrc_set_obuf_rp(id,
						(u32)dmab->addr);
					dev_dbg(priv->dev, "%s() s1=%u (line %d)\n",
						 __func__, s1, __LINE__);
					return (ssize_t)s1;
				}
			}
			afe_mem_asrc_set_obuf_rp(id, (u32)dmab->addr + s2);
			dev_dbg(priv->dev, "%s() copy=%u (line %d)\n", __func__,
				 copy, __LINE__);
			return (ssize_t)copy;
		}
	}
	return (ssize_t)copy;
}

static ssize_t asrc_write(struct file *filp, const char __user *buf,
			  size_t count, loff_t *offset)
{
	struct asrc_private *priv = filp->private_data;
	enum afe_mem_asrc_id id;
	struct asrc_buf *dmab;
	u32 wp, rp, copy;
	unsigned char *copy_start;

	dev_dbg(priv->dev, "%s()\n", __func__);
	if (unlikely(priv == NULL))
		return 0;
	id = priv->id;
	if (unlikely(id >= ASRC_MAX_NUM)) {
		dev_info(priv->dev, "%s() error: invalid id %u\n",
			__func__, id);
		return 0;
	}
	if (priv->running == 0) {
		dev_info(priv->dev, "%s() warning: asrc[%u] is not running\n",
			 __func__, id);
		return 0;
	}
	dmab = &priv->input_dmab;
	wp = afe_mem_asrc_get_ibuf_wp(id);
	rp = afe_mem_asrc_get_ibuf_rp(id);
	dev_dbg(priv->dev, "%s() wp=0x%08x, rp=0x%08x\n", __func__, wp, rp);
	if (wp == 0 || rp == 0) {
		dev_dbg(priv->dev, "%s() wp or rp is 0. May get some problem!\n",
			 __func__);
	}

	copy_start = dmab->area + (wp - dmab->addr);

	dev_dbg(priv->dev, "%s() copy_start=%p, dmab->area=%p, dmab->addr=0x%08llx\n",
		__func__, copy_start, dmab->area, (long long int)dmab->addr);

	count = count / 16UL * 16UL;
	if (wp < rp) {
		u32 delta = rp - wp;

		copy = delta - 16U < (u32)count ? delta - 16U : (u32)count;
		if (copy == 0U)
			return 0;
		if (copy_from_user(copy_start, buf, copy) != 0UL) {
			dev_info(priv->dev, "%s() error: L%d copy_from_user\n",
				 __func__, __LINE__);
			return 0;
		}

		afe_mem_asrc_set_ibuf_wp(id, wp + copy);
		dev_dbg(priv->dev, "%s() copy=%u (line %d)\n", __func__,
			 copy, __LINE__);
	} else {		/* wp >= rp */
		u32 delta = rp + (u32)dmab->bytes - wp;

		copy = delta - 16U < (u32)count ? delta - 16U : (u32)count;
		if (copy == 0U)
			return 0;

		if ((u64)wp + (u64)copy < dmab->addr + dmab->bytes) {
			if (copy_from_user(copy_start, buf, copy) != 0UL) {
				dev_info(priv->dev, "%s() error: L%d copy_from_user\n",
					 __func__, __LINE__);
				return 0;
			}
			afe_mem_asrc_set_ibuf_wp(id, wp + copy);
			dev_dbg(priv->dev, "%s() copy=%u (line %d)\n", __func__,
				 copy, __LINE__);
		} else {
			u32 s1 = (u32)dmab->addr + (u32)dmab->bytes - wp;
			u32 s2 = copy - s1;

			if (copy_from_user(copy_start, buf, s1) != 0UL) {
				dev_info(priv->dev, "%s() error: L%d copy_from_user\n",
					 __func__, __LINE__);
				return 0;
			}

			if (s2 != 0U) {
				if (copy_from_user(dmab->area,
						buf + s1, s2) != 0UL) {
					dev_dbg(priv->dev, "%s() error: L%d copy_from_user\n",
						 __func__, __LINE__);
					afe_mem_asrc_set_ibuf_wp(id,
						(u32)dmab->addr);
					dev_dbg(priv->dev, "%s() s1=%u (line %d)\n",
						__func__, s1, __LINE__);
					return (ssize_t)s1;
				}
			}

			afe_mem_asrc_set_ibuf_wp(id, (u32)dmab->addr + s2);
			dev_dbg(priv->dev, "%s() copy=%u (line %d)\n", __func__,
				 copy, __LINE__);
		}
	}

	// enable the irq when first write. now we only use output full irq
	if (copy != 0 && afe_mem_asrc_irq_is_enabled(id, OBUF_OV_INT) == 0)
		afe_mem_asrc_irq_enable(id, OBUF_OV_INT, ASRC_IRQ_ENABLE);

	return (ssize_t)copy;
}

static const struct file_operations asrc_fops[ASRC_MAX_NUM] = {
	[MEM_ASRC_1 ... MEM_ASRC_4] = {
		.owner = THIS_MODULE,
		.open = asrc_open,
		.release = asrc_release,
		.unlocked_ioctl = asrc_ioctl,
		.write = asrc_write,
		.read = asrc_read,
		.flush = NULL,
		.fasync = asrc_fasync,
		.mmap = NULL
	}
};

static enum afe_mem_asrc_id get_asrc_id(const struct file *filp)
{
	if (filp != NULL) {
		enum afe_mem_asrc_id id;
		int i;

		for (i = (int)MEM_ASRC_1; i < (int)ASRC_MAX_NUM; i++) {
			id = (enum afe_mem_asrc_id) i;
			if (filp->f_op == &asrc_fops[id])
				return id;
		}
	}
	return ASRC_MAX_NUM;
}

static ssize_t asrc_dump_registers(char __user *user_buf,
					size_t count,
					loff_t *pos,
					const struct asrc_dump_reg_attr *regs,
					size_t regs_len)
{
	ssize_t ret, i;
	char *buf;
	unsigned int reg_value;
	int n = 0;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	asrc_turn_on_asrc_clock();

	for (i = 0; i < regs_len; i++) {
		if (regmap_read(asrc_regmap, regs[i].offset, &reg_value))
			n += scnprintf(buf + n, count - n, "%s = N/A\n",
				       regs[i].name);
		else
			n += scnprintf(buf + n, count - n, "%s = 0x%x\n",
				       regs[i].name, reg_value);
	}

	asrc_turn_off_asrc_clock();

	ret = simple_read_from_buffer(user_buf, count, pos, buf, n);
	kfree(buf);

	return ret;
}

static ssize_t asrc_debug_fs_read_file(struct file *file,
				    char __user *user_buf,
				    size_t count,
				    loff_t *pos)
{
	ssize_t ret;

	if (*pos < 0 || !count)
		return -EINVAL;

	ret = asrc_dump_registers(user_buf, count, pos,
				asrc_dump_regs,
				ARRAY_SIZE(asrc_dump_regs));

	return ret;
}

static ssize_t asrc_debug_fs_write_file(struct file *file,
				     const char __user *user_buf,
				     size_t count,
				     loff_t *pos)
{
	char buf[64];
	size_t buf_size;
	char *start = buf;
	char *reg_str;
	char *value_str;
	const char delim[] = " ,";
	unsigned long reg, value;

	buf_size = min(count, (sizeof(buf) - 1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	buf[buf_size] = 0;

	reg_str = strsep(&start, delim);
	if (!reg_str || !strlen(reg_str))
		return -EINVAL;

	value_str = strsep(&start, delim);
	if (!value_str || !strlen(value_str))
		return -EINVAL;

	if (kstrtoul(reg_str, 16, &reg))
		return -EINVAL;

	if (kstrtoul(value_str, 16, &value))
		return -EINVAL;

	asrc_turn_on_asrc_clock();
	regmap_write(asrc_regmap, reg, value);
	asrc_turn_off_asrc_clock();

	return buf_size;
}

static const struct file_operations asrc_debug_fops = {
	.open = simple_open,
	.read = asrc_debug_fs_read_file,
	.write = asrc_debug_fs_write_file,
	.llseek = default_llseek,
};


struct mt8518_asrc_debug_fs asrc_debug_fs[] = {
	{"mtksocaudiomasrc", &asrc_debug_fops},
};

void asrc_init_debugfs(struct platform_device *pdev)
{
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_dentry = NULL;

	int i;

	for (i = 0; i < ARRAY_SIZE(asrc_debug_fs); i++) {
		debugfs_dentry =
			debugfs_create_file(asrc_debug_fs[i].fs_name,
				0644, NULL, NULL, asrc_debug_fs[i].fops);
		if (!debugfs_dentry)
			dev_info(&pdev->dev, "%s create %s debugfs failed\n",
				 __func__, asrc_debug_fs[i].fs_name);
	}
#endif
}

static int asrc_dev_probe(struct platform_device *pdev)
{
	int ret, i;
	enum afe_mem_asrc_id id;
	struct device *dev = &pdev->dev;
	char name[32];
	struct asrc_devices_info *asrc_dev_inf;

	asrc_dev_inf = devm_kzalloc(dev, sizeof(struct asrc_devices_info),
				GFP_KERNEL);
	if (!asrc_dev_inf)
		return -ENOMEM;
	asrc_dev_inf->dev = dev;
	dev_dbg(&pdev->dev, "%s()\n", __func__);

	ret = asrc_regmap_init(pdev);
	if (ret != 0)
		return ret;

	ret = asrc_init_clock(&pdev->dev);

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		dev_dbg(&pdev->dev, "%s pm_runtime not enabled\n", __func__);
		goto err_pm_disable;
	}

	pm_runtime_get_sync(&pdev->dev);
	platform_set_drvdata(pdev, asrc_dev_inf);

	for (i = (int)MEM_ASRC_1; i < (int)ASRC_MAX_NUM; ++i) {
		id = (enum afe_mem_asrc_id) i;
		sprintf(name, "asrc%d", id);
		asrc_dev_inf->miscdev[id].minor = MISC_DYNAMIC_MINOR;
		asrc_dev_inf->miscdev[id].name = name;
		asrc_dev_inf->miscdev[id].fops = &asrc_fops[id];

		ret = misc_register(&asrc_dev_inf->miscdev[id]);
		if (ret != 0)
			dev_dbg(&pdev->dev, "%s() error: misc_register asrc[%u] failed %d\n",
				 __func__, id, ret);
		else {
			ret = asrc_allocate_buffer(id, &pdev->dev, INPUT,
						   ASRC_BUFFER_SIZE_MAX);
			if (ret != 0)
				continue;
			ret = asrc_allocate_buffer(id, &pdev->dev, OUTPUT,
						   ASRC_BUFFER_SIZE_MAX);
			if (ret != 0)
				continue;
		}

		asrc_irq[id] = (u32)platform_get_irq(pdev, (u32)id);
		if (asrc_irq[id] == 0U) {
			dev_info(&pdev->dev, "%d no irq found\n", id);
			return -ENXIO;
		}
		dev_dbg(&pdev->dev, "platform device name:%s, asrc_irq[%d]: %d\n",
			pdev->name, id, asrc_irq[id]);
		//init spin lock
		spin_lock_init(&asrc_dev_inf->asrc_ctrl_lock[id]);
	}

	asrc_init_debugfs(pdev);
	asrc_dev_info = asrc_dev_inf;

	return 0;

err_pm_disable:
	pm_runtime_disable(&pdev->dev);

	return ret;
}

static int asrc_dev_remove(struct platform_device *pdev)
{
	enum afe_mem_asrc_id id;
	int i;
	struct asrc_devices_info *asrc_dev_inf = platform_get_drvdata(pdev);

	for (i = (int)MEM_ASRC_1; i < (int)ASRC_MAX_NUM; ++i) {
		id = (enum afe_mem_asrc_id) i;
		misc_deregister(&asrc_dev_inf->miscdev[id]);
		asrc_free_buffer(id, &pdev->dev, INPUT);
		asrc_free_buffer(id, &pdev->dev, OUTPUT);
	}

	asrc_turn_off_asrc_clock();
	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	asrc_dev_info = NULL;

	return 0;
}

static const struct of_device_id asrc_dt_match[] = {
	{ .compatible = "mediatek,mt8518-audio-asrc"},
};

static struct platform_driver asrc_platform_driver = {
	.driver = {
		.name = "mt8518-audio-asrc",
		.of_match_table = asrc_dt_match,
	},
	.probe = asrc_dev_probe,
	.remove = asrc_dev_remove,
};

module_platform_driver(asrc_platform_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("memory asrc driver");
