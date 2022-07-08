/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <linux/io.h>
#include "mt8521p-aud-global.h"
#include "mt8521p-afe-clk.h"
#include "mt8521p-afe-reg.h"
#include "mt8521p-afe.h"
#include "mt8521p-dai.h"
#include "mt8521p-private.h"

static void *bitstream_sram;
static void *code_sram, *code_sram_store;
static void *afe_internal_sram;
static u32 hw_offset;
static int clock_started;
struct snd_pcm_substream *lp_substream;
enum audio_irq_id lp_irq_id = IRQ_NUM;

#define BITSTREAM_SRAM_ADDR_PHYS ((u32)0x11340000)
#define BITSTREAM_SRAM_SIZE ((u32)128 * 1024)
#define CODE_SRAM_ADDR_PHYS 0x11300000
#define CODE_SRAM_SIZE 0x30000

static void enter_region(volatile struct lp_region *r, enum lp_cpu_id id)
{
	enum lp_cpu_id other = 1 - id;

	r->interested[id] = 1;
	r->turn = id;
	while (r->turn == id && r->interested[other])
		pr_debug("[AP]%s() waiting ...\n", __func__);
}

static void leave_region(volatile struct lp_region *r, enum lp_cpu_id id)
{
	r->interested[id] = 0;
}

/* extern to deep-idle */
int lp_switch_mode(unsigned int mode)
{
	volatile struct lp_mode *m;

	if (!bitstream_sram)
		return -ENODEV;
	m = &((volatile struct lp_info *)bitstream_sram)->m;
	pr_debug("%s() enter mode=%u\n", __func__, mode);
	enter_region(&m->region, LP_CPU_AP);
	m->mode = mode;
	leave_region(&m->region, LP_CPU_AP);
	pr_debug("%s() leave\n", __func__);
	return 0;
}

static const struct snd_pcm_hardware mt8521p_lp_pcm_hardware = {
	.info             = SNDRV_PCM_INFO_INTERLEAVED,
	.formats          = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S32_LE,
	.period_bytes_min = 3072,
	.period_bytes_max = 3072 * 10,
	.periods_min      = 8,
	.periods_max      = 1024,
	.buffer_bytes_max = 2 * 1024 * 1024,
};
static int itrcon_i2s1out(int on)
{
	/* prevent overflow when mixing with immsnd */
	itrcon_rightshift1bit(O15, on);
	itrcon_rightshift1bit(O16, on);

	itrcon_connect(I12, O15, on);
	itrcon_connect(I13, O16, on);
	return 0;
}

static int itrcon_i2s2out(int on)
{
	itrcon_connect(I14, O17, on);
	itrcon_connect(I15, O18, on);
	return 0;
}

static int itrcon_i2s1in(int on)
{
	itrcon_connect(I00, O00, on);
	itrcon_connect(I01, O01, on);
	return 0;
}

static int itrcon_i2s2in(int on)
{
	itrcon_connect(I02, O02, on);
	itrcon_connect(I03, O03, on);
	return 0;
}

static const itrcon_action itrcon[2][2] = {
	{ itrcon_i2s1out, itrcon_i2s2out },
	{ itrcon_i2s1in, itrcon_i2s2in },
};

static int mt8521p_lp_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int dai_id = rtd->cpu_dai->id;

	pr_debug("%s()\n", __func__);
	if (dai_id != MT_DAI_I2S1_ID && dai_id != MT_DAI_I2S2_ID) {
		pr_err("%s() error: invalid i2s id %d\n", __func__, dai_id);
		return -ENODEV;
	}
	if (lp_substream) {
		pr_err("%s() error: lp_substream exists\n", __func__);
		return -EPERM;
	}
	lp_irq_id = asys_irq_acquire();
	if (lp_irq_id == IRQ_NUM) {
		pr_err("%s() error: no more asys irq\n", __func__);
		return -ENODEV;
	}
	substream->runtime->private_data = bitstream_sram;
	lp_substream = substream;
	snd_soc_set_runtime_hwparams(substream, &mt8521p_lp_pcm_hardware);
	afe_power_mode(LOW_POWER_MODE);
	afe_i2s_out_asrc_clock_sel(dai_id - MT_DAI_I2S1_ID, CLK_A1SYS);
	a1sys_start(itrcon[substream->stream][dai_id - MT_DAI_I2S1_ID], 1);
	mt_afe_apdma_clk_on();
	return 0;
}

static int mt8521p_lp_pcm_close(struct snd_pcm_substream *substream)
{
	if (lp_substream == substream) {
		struct snd_soc_pcm_runtime *rtd = substream->private_data;
		int dai_id = rtd->cpu_dai->id;

	pr_debug("%s()\n", __func__);
		mt_afe_apdma_clk_off();
		a1sys_start(itrcon[substream->stream][dai_id - MT_DAI_I2S1_ID], 0);
		afe_i2s_out_asrc_clock_sel(dai_id - MT_DAI_I2S1_ID, CLK_ASM_HIGH);
		afe_power_mode(NORMAL_POWER_MODE);
		asys_irq_release(lp_irq_id);
		lp_irq_id = IRQ_NUM;
	lp_substream = NULL;
	}
	return 0;
}

static int mt8521p_lp_pcm_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params)
{
	int ret;

	ret = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(params));
	if (ret < 0) {
		pr_err("%s() error: allocation of memory failed\n", __func__);
		return ret;
	}
	/* lp-audio use SRAM as alsa-buffer, So, to set 0 data */
	/* prevent ulp playback from white noise causing to uninitialized data */
	memset(afe_internal_sram, 0x00, AFE_INTERNAL_SRAM_SIZE);
	return 0;
}

static int mt8521p_lp_pcm_hw_free(struct snd_pcm_substream *substream)
{
	pr_debug("%s()\n", __func__);
	return snd_pcm_lib_free_pages(substream);
}

static int mt8521p_lp_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	enum afe_sampling_rate fs = fs_enum(runtime->rate);
	struct mt_lp_private *priv = snd_soc_platform_get_drvdata(rtd->platform);

	pr_debug("%s() period_size=%lu, periods=%u, buffer_size=%lu\n",
		 __func__, runtime->period_size, runtime->periods, runtime->buffer_size);

	lp_configurate(
		(volatile struct lp_info *)runtime->private_data
		, runtime->dma_addr
		, frames_to_bytes(runtime, runtime->buffer_size)
		, runtime->rate
		, runtime->channels
		, snd_pcm_format_width(runtime->format)
		, rtd->cpu_dai->id == MT_DAI_I2S1_ID ? 0 : 1
		, lp_irq_id
		, priv->use_i2s_slave_clock
	);

	hw_offset = 0;
	{
		struct audio_irq_config config = {
			.mode = fs,
			.init_val = runtime->period_size
		};
		if (fs == FS_384000HZ || fs == FS_352800HZ)
			config.init_val /= 2;
		audio_irq_configurate(IRQ_AFE_IRQ1, &config);
	}
	return 0;
}

static int mt8521p_lp_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	volatile struct lp_info *lp = (volatile struct lp_info *)runtime->private_data;

	unsigned int lp_cmd[2][2] = {
		[SNDRV_PCM_STREAM_PLAYBACK] = {
			[SNDRV_PCM_TRIGGER_STOP] = LP_CMD_PLAYBACK_STOP,
			[SNDRV_PCM_TRIGGER_START] = LP_CMD_PLAYBACK_START
		},
		[SNDRV_PCM_STREAM_CAPTURE] = {
			[SNDRV_PCM_TRIGGER_STOP] = LP_CMD_CAPTURE_STOP,
			[SNDRV_PCM_TRIGGER_START] = LP_CMD_CAPTURE_START
		}
	};

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		pr_debug("%s() SNDRV_PCM_TRIGGER_START\n", __func__);
		/* silence the buffer that has been processed by hw */
		runtime->silence_threshold = 0;
		runtime->silence_size = runtime->boundary;
		if (!clock_started) {
			pr_debug("%s() enable clocks\n", __func__);

			audio_irq_enable(IRQ_AFE_IRQ1, 1);
			clock_started = 1;
		}
		lp_cmd_excute(lp, lp_cmd[substream->stream][SNDRV_PCM_TRIGGER_START]);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		pr_debug("%s() SNDRV_PCM_TRIGGER_STOP\n", __func__);
		lp_cmd_excute(lp, lp_cmd[substream->stream][SNDRV_PCM_TRIGGER_STOP]);
		break;
	default:
		pr_err("%s() error: bad cmd %d\n", __func__, cmd);
		return -EINVAL;
	}
	return 0;
}

static snd_pcm_uframes_t mt8521p_lp_pcm_pointer(
		struct snd_pcm_substream *substream)
{
	snd_pcm_uframes_t offset;
	struct snd_pcm_runtime *runtime = substream->runtime;

	offset = bytes_to_frames(runtime, hw_offset);
	if (unlikely(offset >= runtime->buffer_size))
		offset = 0;
	return offset;
}

static void lp_update_appl_ptr(volatile struct lp_info *lp, struct snd_pcm_runtime *runtime)
{
		enter_region(&lp->buf.appl_region, LP_CPU_AP);
		lp->buf.appl_ofs = frames_to_bytes(runtime,
			runtime->control->appl_ptr % runtime->buffer_size);
		if (lp->buf.appl_ofs == 0) {
			lp->buf.appl_is_in_buf_end =
			(runtime->control->appl_ptr == runtime->hw_ptr_base) ? 0 : 1;
		}
		leave_region(&lp->buf.appl_region, LP_CPU_AP);
}

void lp_audio_isr(void)
{
	if (!IS_ERR_OR_NULL(lp_substream)) {
		struct snd_pcm_runtime *runtime = lp_substream->runtime;
		volatile struct lp_info *lp =
			(volatile struct lp_info *)runtime->private_data;

		lp->draining = (runtime->status->state == SNDRV_PCM_STATE_DRAINING);
		hw_offset = lp_hw_offset(lp);
		snd_pcm_period_elapsed(lp_substream);
	}
}

static int mt8521p_lp_pcm_ack(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	volatile struct lp_info *lp = (volatile struct lp_info *)runtime->private_data;

	lp_update_appl_ptr(lp, runtime);
	return 0;
}

static struct snd_pcm_ops mt8521p_lp_pcm_ops = {
	.open = mt8521p_lp_pcm_open,
	.close = mt8521p_lp_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mt8521p_lp_pcm_hw_params,
	.hw_free = mt8521p_lp_pcm_hw_free,
	.prepare = mt8521p_lp_pcm_prepare,
	.trigger = mt8521p_lp_pcm_trigger,
	.pointer = mt8521p_lp_pcm_pointer,
	.ack = mt8521p_lp_pcm_ack,
};

static int mt8521p_lp_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	pr_debug("%s()\n", __func__);
	return snd_pcm_lib_preallocate_pages_for_all(
		rtd->pcm,
		SNDRV_DMA_TYPE_DEV,
		NULL,
		2 * 1024 * 1024,
		2 * 1024 * 1024);
}

static void mt8521p_lp_pcm_free(struct snd_pcm *pcm)
{
	pr_debug("%s()\n", __func__);
	snd_pcm_lib_preallocate_free_for_all(pcm);
}

static int mt8521p_lp_pcm_probe(struct snd_soc_platform *platform)
{
	struct mt_lp_private *priv;

	pr_debug("%s()\n", __func__);
	mt_afe_unipll_clk_on();
	code_sram_store = kmalloc(CODE_SRAM_SIZE, GFP_KERNEL);
	code_sram = (void *)ioremap_nocache(CODE_SRAM_ADDR_PHYS, CODE_SRAM_SIZE);
	if (!code_sram || !code_sram_store) {
		pr_err("Failed to allocate buf or ioremap\n");
		mt_afe_unipll_clk_off();
		return -ENOMEM;
	}
	memcpy(code_sram_store, code_sram, CODE_SRAM_SIZE); /* backup CM4 code sram before run */
	cmsys_msk_write(CMSYS_REG00, CPU_RST_SW_RESET, CPU_RST_SW_MASK);
	cmsys_msk_write(CMSYS_REG02, CPUCK_DISABLE, CPUCK_EN_MASK);
	mt_afe_cmsys_clk_on();
	cmsys_msk_write(CMSYS_REG02, CPUCK_ENABLE, CPUCK_EN_MASK);
	cmsys_msk_write(CMSYS_REG00, CPU_RST_SW_RELEASE_RESET, CPU_RST_SW_MASK);
	mt_afe_unipll_clk_off();
	bitstream_sram = ioremap(BITSTREAM_SRAM_ADDR_PHYS, BITSTREAM_SRAM_SIZE);
	priv = devm_kzalloc(platform->dev, sizeof(struct mt_lp_private), GFP_KERNEL);
	if (!priv) {
		dev_err(platform->dev, "%s() can't allocate memory\n", __func__);
		return -ENOMEM;
	}
	afe_internal_sram = ioremap(AFE_INTERNAL_SRAM_PHYS_BASE, AFE_INTERNAL_SRAM_SIZE);
	if (!afe_internal_sram) {
		dev_err(platform->dev, "afe_internal_sram ioremap failed\n");
		return -ENOMEM;
	}
	snd_soc_platform_set_drvdata(platform, priv);
	return 0;
}

static int mt8521p_lp_pcm_remove(struct snd_soc_platform *platform)
{
	struct mt_lp_private *priv;

	pr_debug("%s()\n", __func__);
	priv = snd_soc_platform_get_drvdata(platform);
	iounmap(afe_internal_sram);
	devm_kfree(platform->dev, priv);
	iounmap(bitstream_sram);
	bitstream_sram = NULL;
	return 0;
}

static struct snd_soc_platform_driver mt8521p_lp_soc_platform_driver = {
	.probe = mt8521p_lp_pcm_probe,
	.remove = mt8521p_lp_pcm_remove,
	.pcm_new = mt8521p_lp_pcm_new,
	.pcm_free = mt8521p_lp_pcm_free,
	.ops = &mt8521p_lp_pcm_ops,
};

static int mt8521p_lp_audio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pr_debug("%s()\n", __func__);
	#ifdef CONFIG_OF
	if (dev->of_node) {
		dev_set_name(dev, "%s", "mt8590-lp-audio");
		pr_notice("%s set dev name %s\n", __func__, dev_name(dev));
	}
	#endif
	return snd_soc_register_platform(&pdev->dev,
					 &mt8521p_lp_soc_platform_driver);
}

static int mt8521p_lp_audio_remove(struct platform_device *pdev)
{
	pr_debug("%s()\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	cmsys_msk_write(CMSYS_REG00, CPU_RST_SW_RESET, CPU_RST_SW_MASK);
	cmsys_msk_write(CMSYS_REG02, CPUCK_DISABLE, CPUCK_EN_MASK);
	kfree(code_sram_store);
	iounmap(code_sram);
	return 0;
}

static int mt8521p_lp_audio_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	/*
	* cmsys_msk_write(CMSYS_REG00, CPU_RST_SW_RESET, CPU_RST_SW_MASK);
	* cmsys_msk_write(CMSYS_REG02, CPUCK_DISABLE, CPUCK_EN_MASK);
	*/
	return 0;
}

static int mt8521p_lp_audio_resume(struct platform_device *pdev)
{
	/*
	* mt_afe_unipll_clk_on();
	* memcpy(code_sram, code_sram_store, CODE_SRAM_SIZE);
	* cmsys_msk_write(CMSYS_REG00, CPU_RST_SW_RESET, CPU_RST_SW_MASK);
	* cmsys_msk_write(CMSYS_REG02, CPUCK_DISABLE, CPUCK_EN_MASK);
	* mt_afe_cmsys_clk_on();
	* cmsys_msk_write(CMSYS_REG02, CPUCK_ENABLE, CPUCK_EN_MASK);
	* cmsys_msk_write(CMSYS_REG00, CPU_RST_SW_RELEASE_RESET, CPU_RST_SW_MASK);
	* mt_afe_unipll_clk_off();
	*/
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt8521p_lp_audio_dt_match[] = {
	{.compatible = "mediatek,mt8521p-lp-audio",},
	{}
};
#endif
static struct platform_driver mt8521p_lp_audio = {
	.driver = {
		.name = "mt8521p-lp-audio",
		.owner = THIS_MODULE,
		#ifdef CONFIG_OF
		.of_match_table = mt8521p_lp_audio_dt_match,
		#endif
		},
	.probe = mt8521p_lp_audio_probe,
	.remove = mt8521p_lp_audio_remove,
	.suspend = mt8521p_lp_audio_suspend,
	.resume = mt8521p_lp_audio_resume
};

module_platform_driver(mt8521p_lp_audio);

MODULE_DESCRIPTION("mt8521p low-power audio driver");
MODULE_LICENSE("GPL");
