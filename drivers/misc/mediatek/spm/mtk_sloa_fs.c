// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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

#include <linux/arm-smccc.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/string.h>

#define MTK_SIP_SLOA_FS_READ	0x82000406
#define MTK_SIP_SLOA_FS_WRITE	0x82000407

#define FS_COUNT		40
#define FS_SIZE			32
#define WAKEUPSRC_COUNT		32
#define WAKEUPSRC_SIZE		32

/* MT2712 Platform Data Start Here */

#define MT2712_WAKESRC_REG	0xb6c
enum {
	FS_WRITE_PCM_FLAGS = 0,
	FS_WRITE_PCM_FLAGS_CUST,
	FS_WRITE_PCM_RESERVE,
	FS_WRITE_TIMER_VAL,
	FS_WRITE_TIMER_VAL_CUST,
	FS_WRITE_WAKE_SRC,
	FS_WRITE_WAKE_SRC_CUST,
	FS_WRITE_R0_CTRL_EN,
	FS_WRITE_R7_CTRL_EN,
	FS_WRITE_INFRA_DCM_LOCK,
	FS_WRITE_PCM_APSRC_REQ,
	FS_WRITE_PCM_F26M_REQ,
	FS_WRITE_MCUSYS_IDLE_MASK,
	FS_WRITE_CA15TOP_IDLE_MASK,
	FS_WRITE_CA7TOP_IDLE_MASK,
	FS_WRITE_WFI_OP,
	FS_WRITE_CA15_WFI0_EN,
	FS_WRITE_CA15_WFI1_EN,
	FS_WRITE_CA15_WFI2_EN,
	FS_WRITE_CA15_WFI3_EN,
	FS_WRITE_CA7_WFI0_EN,
	FS_WRITE_CA7_WFI1_EN,
	FS_WRITE_CA7_WFI2_EN,
	FS_WRITE_CA7_WFI3_EN,
	FS_WRITE_CONN_MASK,
	FS_WRITE_GCE_REQ_MASK,
	FS_WRITE_DISP0_REQ_MASK,
	FS_WRITE_DISP1_REQ_MASK,
	FS_WRITE_MFG_REQ_MASK,
	FS_WRITE_VDEC_REQ_MASK,
	FS_WRITE_MM_DDR_REQ_MASK,
	FS_WRITE_SYSPWREQ_MASK,
	FS_WRITE_SRCLKENAI_MASK,
	FS_WRITE_PARAM1,
	FS_WRITE_PARAM2,
	FS_WRITE_PARAM3,
	NR_FS_WRITE_CMDS,
};

static char mt2712_fs_write_cmd[FS_COUNT][FS_SIZE] = {
	"pcm_flags",
	"pcm_flags_cust",
	"pcm_reserve",
	"timer_val",
	"timer_val_cust",
	"wake_src",
	"wake_src_cust",
	"r0_ctrl_en",
	"r7_ctrl_en",
	"infra_dcm_lock",
	"pcm_apsrc_req",
	"pcm_f26m_req",
	"mcusys_idle_mask",
	"ca15top_idle_mask",
	"ca7top_idle_mask",
	"wfi_op",
	"ca15_wfi0_en",
	"ca15_wfi1_en",
	"ca15_wfi2_en",
	"ca15_wfi3_en",
	"ca7_wfi0_en",
	"ca7_wfi1_en",
	"ca7_wfi2_en",
	"ca7_wfi3_en",
	"conn_mask",
	"gce_req_mask",
	"disp0_req_mask",
	"disp1_req_mask",
	"mfg_req_mask",
	"vdec_req_mask",
	"mm_ddr_req_mask",
	"syspwreq_mask",
	"srclkenai_mask",
	"param1",
	"param2",
	"param3",
};

static char mt2712_wakesrc_str[WAKEUPSRC_COUNT][WAKEUPSRC_SIZE] = {
	"PCM_TIMER_LSB",
	"R12_BIT1",
	"KP_IRQ_LSB",
	"APWDT_EVENT_LSB",
	"APXGPT_EVENT_LSB",
	"APXGPT_MD32_EVENT_LSB",
	"EINT_EVENT_LSB",
	"EINT_MD32_EVENT_LSB",
	"PE2_P0_LSB",
	"PE2_P1_LSB",
	"MD32_SPM_IRQ_LSB",
	"26M_WAKE_LSB",
	"26M_SLEEP_LSB",
	"PCM_WDT_WAKE_LSB",
	"RTC_LSB",
	"USB0_POWERDWN_LSB",
	"PMIC_EINT0_CPU_LSB",
	"IRRX_LSB",
	"PMIC_EINT_MD32_LSB",
	"UART0_IRQ_LSB",
	"AFE_IRQ_MCU_LSB",
	"THERM_CTRL_EVENT_LSB",
	"SYS_CIRQ_IRQ_LSB",
	"AUD_MD32_IRQ_LSB",
	"CSYSPWREQ_LSB",
	"MD_WDT_LSB",
	"USB_INIT_D_LSB",
	"SEJ_LSB",
	"ALL_MD32_WAKEUP_LSB",
	"ALL_CPU_IRQ_LSB",
	"APSRC_WAKE_LSB",
	"APSRC_SLEEP_LSB",
};
/* End of 2712 Platform Data. */

/* MT8518 Platform Data Start Here */

#define MT8518_WAKESRC_REG	0xb68

static char mt8518_wakesrc_str[WAKEUPSRC_COUNT][WAKEUPSRC_SIZE] = {
	"WAKE_SRC_SPM_MERGE",
	"WAKE_SRC_MCUSYS",
	"WAKE_SRC_EINT_CM4",
	"WAKE_SRC_WDT",
	"WAKE_SRC_GPT",
	"WAKE_SRC_EINT",
	"WAKE_SRC_CM4",
	"WAKE_SRC_IR_WAKE_CM4",
	"WAKE_SRC_ACAO_IRQ",
	"WAKE_SRC_LOW_BAT",
	"WAKE_SRC_CONN2AP",
	"WAKE_SRC_RESERVE_WAKE",
	"WAKE_SRC_RESERVE_SLEEP",
	"WAKE_SRC_PCM_WDT",
	"WAKE_SRC_USB_CD",
	"WAKE_SRC_USB_PDN",
	"WAKE_SRC_ETHERNET",
	"WAKE_SRC_SYSTIMER",
	"WAKE_SRC_SYSTEM",
	"WAKE_SRC_AFE",
	"WAKE_SRC_THERM",
	"WAKE_SRC_CIRQ",
	"WAKE_SRC_SEJ",
	"WAKE_SRC_SYSPWREQ",
	"WAKE_SRC_IRRX",
	"WAKE_SRC_CPU0_IRQ",
	"WAKE_SRC_CPU1_IRQ",
	"WAKE_SRC_CPU2_IRQ",
	"WAKE_SRC_CPU3_IRQ",
	"WAKE_SRC_APSRC_WAKE",
	"WAKE_SRC_APSRC_SLEEP",
};
/* End of 8518 Platform Data. */

enum {
	FS_READ_PCM_DESC = 0,
	FS_READ_PWR_CTRL,
	FS_DUMP_PCM_CODE,
	NR_FS_READ_CMDS,
};

struct mtk_sloa_data {
	const int wakesrc_reg;
	char (*wakesrc_str)[WAKEUPSRC_SIZE];
	char (*fs_write_cmd_name)[FS_SIZE];
};

struct mtk_sloa {
	void __iomem *spm_base;
	struct device *dev;
	const struct mtk_sloa_data *conf;
};

static unsigned long invoke_psci_fn(unsigned long function_id,
	unsigned long arg0, unsigned long arg1,
	unsigned long arg2)
{
	struct arm_smccc_res res;

	arm_smccc_smc(function_id, arg0, arg1, arg2, 0, 0, 0, 0, &res);
	return res.a0;
};

enum SCENE {
	SCN_TYPE_SLP = 0,
	SCN_TYPE_DP = 1,
	SCN_TYPE_SO = 2,
	NR_SCN_TYPES = 3,
};

static ssize_t show_pcm_desc(const enum SCENE scene)
{
	invoke_psci_fn(MTK_SIP_SLOA_FS_READ, scene, FS_READ_PCM_DESC, 0);
	return 0;
}

static ssize_t suspend_pcm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return show_pcm_desc(SCN_TYPE_SLP);
}

static ssize_t dpidle_pcm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return show_pcm_desc(SCN_TYPE_DP);
}

static ssize_t sodi_pcm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return show_pcm_desc(SCN_TYPE_SO);
}

static ssize_t dump_pcm_code_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	invoke_psci_fn(MTK_SIP_SLOA_FS_READ, SCN_TYPE_SLP, FS_DUMP_PCM_CODE, 0);
	return 0;
}

static ssize_t show_pwr_ctrl(const enum SCENE scene)
{
	invoke_psci_fn(MTK_SIP_SLOA_FS_READ, scene, FS_READ_PWR_CTRL, 0);
	return 0;
}

static ssize_t suspend_ctrl_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return show_pwr_ctrl(SCN_TYPE_SLP);
}

static ssize_t dpidle_ctrl_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return show_pwr_ctrl(SCN_TYPE_DP);
}

static ssize_t sodi_ctrl_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return show_pwr_ctrl(SCN_TYPE_SO);
}

static ssize_t store_pwr_ctrl(struct device *dev,
	const enum SCENE scene, const char *buf, size_t count)
{
	int val, i;
	char cmd[32];
	struct platform_device *pdev = to_platform_device(dev);
	struct mtk_sloa *mt = platform_get_drvdata(pdev);

	if (sscanf(buf, "%31s %x", cmd, &val) != 2)
		return -EPERM;

	pr_info("pwr_ctrl: cmd = %s, val = 0x%x\n", cmd, val);

	for (i = 0; i < NR_FS_WRITE_CMDS; i++) {
		if (!strcmp(cmd, mt->conf->fs_write_cmd_name[i]))
			break;
	}

	if (i < NR_FS_WRITE_CMDS)
		invoke_psci_fn(MTK_SIP_SLOA_FS_WRITE, scene, i, val);

	return count;
}

static ssize_t suspend_ctrl_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return store_pwr_ctrl(dev, SCN_TYPE_SLP, buf, count);
}

static ssize_t dpidle_ctrl_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return store_pwr_ctrl(dev, SCN_TYPE_DP, buf, count);
}

static ssize_t sodi_ctrl_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return store_pwr_ctrl(dev, SCN_TYPE_SO, buf, count);
}

static DEVICE_ATTR(suspend_pcm, 0444, suspend_pcm_show, NULL);
static DEVICE_ATTR(dpidle_pcm, 0444, dpidle_pcm_show, NULL);
static DEVICE_ATTR(sodi_pcm, 0444, sodi_pcm_show, NULL);
static DEVICE_ATTR(dump_pcm_code, 0444, dump_pcm_code_show, NULL);
static DEVICE_ATTR(suspend_ctrl, 0644, suspend_ctrl_show, suspend_ctrl_store);
static DEVICE_ATTR(dpidle_ctrl, 0644, dpidle_ctrl_show, dpidle_ctrl_store);
static DEVICE_ATTR(sodi_ctrl, 0644, sodi_ctrl_show, sodi_ctrl_store);

static struct attribute *spm_attrs[] = {
	/* for spm_lp_scen.pcmdesc */
	&dev_attr_suspend_pcm.attr,
	&dev_attr_dpidle_pcm.attr,
	&dev_attr_sodi_pcm.attr,
	&dev_attr_dump_pcm_code.attr,

	/* for spm_lp_scen.pwrctrl */
	&dev_attr_suspend_ctrl.attr,
	&dev_attr_dpidle_ctrl.attr,
	&dev_attr_sodi_ctrl.attr,

	/* must */
	NULL,
};

static struct attribute_group spm_attr_group = {
	.name = "spm",
	.attrs = spm_attrs,
};

static int __maybe_unused mtk_sloafs_resume(struct device *dev)
{
	unsigned int i, wakesrc;
	struct platform_device *pdev = to_platform_device(dev);
	struct mtk_sloa *mt = platform_get_drvdata(pdev);

	wakesrc = readl(mt->spm_base + mt->conf->wakesrc_reg);

	pr_debug("%s wakesrc: 0x%x\n", __func__, wakesrc);

	for (i = 0; i < 32; i++)
		if (wakesrc & (1U << i))
			pr_info("wakeup reason: %s\n",
				mt->conf->wakesrc_str[i]);

	return 0;
}

static const struct mtk_sloa_data mt2712_sloa_data = {
	.wakesrc_reg = MT2712_WAKESRC_REG,
	.wakesrc_str = mt2712_wakesrc_str,
	.fs_write_cmd_name = mt2712_fs_write_cmd,
};

static const struct mtk_sloa_data mt8518_sloa_data = {
	.wakesrc_reg = MT8518_WAKESRC_REG,
	.wakesrc_str = mt8518_wakesrc_str,
	.fs_write_cmd_name = mt2712_fs_write_cmd,
};

static const struct of_device_id sloa_fs_of_match[] = {
	{
		.compatible = "mediatek,mt2712-scpsys_debug",
		.data = (void *)&mt2712_sloa_data,
	},
	{
		.compatible = "mediatek,mt8518-scpsys_debug",
		.data = (void *)&mt8518_sloa_data,
	},
};
MODULE_DEVICE_TABLE(of, sloa_fs_of_match);

static int sloa_fs_probe(struct platform_device *pdev)
{
	struct mtk_sloa *mt;
	const struct of_device_id *of_id;
	struct device_node *node;

	mt = devm_kzalloc(&pdev->dev, sizeof(*mt), GFP_KERNEL);
	if (!mt)
		return -ENOMEM;

	of_id = of_match_device(sloa_fs_of_match, &pdev->dev);
	if (of_id)
		mt->conf = (const struct mtk_sloa_data *)of_id->data;

	mt->dev = &pdev->dev;

	node = of_parse_phandle(pdev->dev.of_node, "mediatek,scpsys", 0);

	mt->spm_base = of_iomap(node, 0);

	of_node_put(node);

	if (sysfs_create_group(&pdev->dev.kobj, &spm_attr_group))
		pr_info("sloa_fs_probe: fail to create sysfs node\n");

	platform_set_drvdata(pdev, mt);

	return 0;
}

static int sloa_fs_remove(struct platform_device *pdev)
{
	return 0;
}

static SIMPLE_DEV_PM_OPS(mtk_sloafs_pm_ops, NULL, mtk_sloafs_resume);

static struct platform_driver sloa_fs_driver = {
	.probe = sloa_fs_probe,
	.remove = sloa_fs_remove,
	.driver = {
		.name = "scpsys_debug",
		.pm = &mtk_sloafs_pm_ops,
		.of_match_table = sloa_fs_of_match,
	},
};

module_platform_driver(sloa_fs_driver);

MODULE_DESCRIPTION("SLOA-FS Driver v0.5");
