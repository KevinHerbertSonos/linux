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
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#define IDLE_TAG	"[Power/idle] "
#define idle_err(fmt, args...)	pr_info(IDLE_TAG fmt, ##args)

#define MTK_SIP_IDLE_DEBUG_READ		0x82000404
#define MTK_SIP_IDLE_DEBUG_WRITE	0x82000405

static unsigned long invoke_psci_fn(unsigned long function_id,
			unsigned long arg0, unsigned long arg1,
			unsigned long arg2)
{
	struct arm_smccc_res res;

	arm_smccc_smc(function_id, arg0, arg1, arg2, 0, 0, 0, 0, &res);
	return res.a0;
}

enum {
	IDLE_TYPE_DP = 0,
	IDLE_TYPE_SO = 1,
	IDLE_TYPE_ALL = 2,
};

enum {
	CMD_TYPE_SW_SWITCH = 0,
	CMD_TYPE_ENABLE_MASK_BIT = 1,
	CMD_TYPE_DISABLE_MASK_BIT = 2,
	CMD_TYPE_BYPASS = 3,
	CMD_TYPE_DEBUG = 4,
};

/*
 * debugfs
 */
static char cmd_buf[512] = {0};

/*
 * idle_state
 */
static int _idle_state_open(struct seq_file *s, void *data)
{
	return 0;
}

static int idle_state_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _idle_state_open, inode->i_private);
}

static ssize_t idle_state_read(struct file *filp, char __user *userbuf,
				size_t count, loff_t *f_pos)
{
	invoke_psci_fn(MTK_SIP_IDLE_DEBUG_READ, IDLE_TYPE_ALL, 0, 0);
	return 0;
}

static const struct file_operations idle_state_fops = {
	.owner = THIS_MODULE,
	.open = idle_state_open,
	.read = idle_state_read,
	.llseek = seq_lseek,
	.release = single_release,
};


/*
 * dpidle_state
 */
static int _dpidle_state_open(struct seq_file *s, void *data)
{
	return 0;
}

static int dpidle_state_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _dpidle_state_open, inode->i_private);
}

static ssize_t dpidle_state_read(struct file *filp, char __user *userbuf,
				size_t count, loff_t *f_pos)
{
	invoke_psci_fn(MTK_SIP_IDLE_DEBUG_READ, IDLE_TYPE_DP, 0, 0);
	return 0;
}

static ssize_t dpidle_state_write(struct file *filp, const char __user *userbuf,
				size_t count, loff_t *f_pos)
{
	char cmd[32];
	int param;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	if (sscanf(cmd_buf, "%s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "dpidle"))
			invoke_psci_fn(MTK_SIP_IDLE_DEBUG_WRITE, IDLE_TYPE_DP,
				CMD_TYPE_SW_SWITCH, param);
		else if (!strcmp(cmd, "enable"))
			invoke_psci_fn(MTK_SIP_IDLE_DEBUG_WRITE, IDLE_TYPE_DP,
				CMD_TYPE_ENABLE_MASK_BIT, param);
		else if (!strcmp(cmd, "disable"))
			invoke_psci_fn(MTK_SIP_IDLE_DEBUG_WRITE, IDLE_TYPE_DP,
				CMD_TYPE_DISABLE_MASK_BIT, param);
		else if (!strcmp(cmd, "bypass"))
			invoke_psci_fn(MTK_SIP_IDLE_DEBUG_WRITE, IDLE_TYPE_DP,
				CMD_TYPE_BYPASS, param);
		else if (!strcmp(cmd, "debug"))
			invoke_psci_fn(MTK_SIP_IDLE_DEBUG_WRITE, IDLE_TYPE_DP,
				CMD_TYPE_DEBUG, param);
		return count;
	} else if (!kstrtoint(cmd_buf, 0, &param)) {
		invoke_psci_fn(MTK_SIP_IDLE_DEBUG_WRITE, IDLE_TYPE_DP,
				CMD_TYPE_SW_SWITCH, param);
		return count;
	}

	return -EINVAL;
}

static const struct file_operations dpidle_state_fops = {
	.owner = THIS_MODULE,
	.open = dpidle_state_open,
	.read = dpidle_state_read,
	.write = dpidle_state_write,
	.llseek = seq_lseek,
	.release = single_release,
};

void enable_dpidle_by_bit(int id)
{
	invoke_psci_fn(MTK_SIP_IDLE_DEBUG_WRITE, IDLE_TYPE_DP,
			CMD_TYPE_ENABLE_MASK_BIT, id);
}
EXPORT_SYMBOL(enable_dpidle_by_bit);

void disable_dpidle_by_bit(int id)
{
	invoke_psci_fn(MTK_SIP_IDLE_DEBUG_WRITE, IDLE_TYPE_DP,
			CMD_TYPE_DISABLE_MASK_BIT, id);
}
EXPORT_SYMBOL(disable_dpidle_by_bit);

/*
 * soidle_state
 */
static int _soidle_state_open(struct seq_file *s, void *data)
{
	return 0;
}

static int soidle_state_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _soidle_state_open, inode->i_private);
}

static ssize_t soidle_state_read(struct file *filp, char __user *userbuf,
				size_t count, loff_t *f_pos)
{
	invoke_psci_fn(MTK_SIP_IDLE_DEBUG_READ, IDLE_TYPE_SO, 0, 0);
	return 0;
}

static ssize_t soidle_state_write(struct file *filp, const char __user *userbuf,
				size_t count, loff_t *f_pos)
{
	char cmd[32];
	int param;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	if (sscanf(cmd_buf, "%s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "soidle"))
			invoke_psci_fn(MTK_SIP_IDLE_DEBUG_WRITE, IDLE_TYPE_SO,
					CMD_TYPE_SW_SWITCH, param);
		else if (!strcmp(cmd, "enable"))
			invoke_psci_fn(MTK_SIP_IDLE_DEBUG_WRITE, IDLE_TYPE_SO,
					CMD_TYPE_ENABLE_MASK_BIT, param);
		else if (!strcmp(cmd, "disable"))
			invoke_psci_fn(MTK_SIP_IDLE_DEBUG_WRITE, IDLE_TYPE_SO,
					CMD_TYPE_DISABLE_MASK_BIT, param);
		else if (!strcmp(cmd, "bypass"))
			invoke_psci_fn(MTK_SIP_IDLE_DEBUG_WRITE, IDLE_TYPE_SO,
					CMD_TYPE_BYPASS, param);
		else if (!strcmp(cmd, "debug"))
			invoke_psci_fn(MTK_SIP_IDLE_DEBUG_WRITE, IDLE_TYPE_SO,
				CMD_TYPE_DEBUG, param);
		return count;
	} else if (!kstrtoint(cmd_buf, 0, &param)) {
		invoke_psci_fn(MTK_SIP_IDLE_DEBUG_WRITE, IDLE_TYPE_SO,
					CMD_TYPE_SW_SWITCH, param);
		return count;
	}

	return -EINVAL;
}

static const struct file_operations soidle_state_fops = {
	.owner = THIS_MODULE,
	.open = soidle_state_open,
	.read = soidle_state_read,
	.write = soidle_state_write,
	.llseek = seq_lseek,
	.release = single_release,
};

void enable_soidle_by_bit(int id)
{
	invoke_psci_fn(MTK_SIP_IDLE_DEBUG_WRITE, IDLE_TYPE_SO,
					CMD_TYPE_ENABLE_MASK_BIT, id);
}
EXPORT_SYMBOL(enable_soidle_by_bit);

void disable_soidle_by_bit(int id)
{
	invoke_psci_fn(MTK_SIP_IDLE_DEBUG_WRITE, IDLE_TYPE_SO,
					CMD_TYPE_DISABLE_MASK_BIT, id);
}
EXPORT_SYMBOL(disable_soidle_by_bit);

void spm_enable_sodi(bool en)
{
	invoke_psci_fn(MTK_SIP_IDLE_DEBUG_WRITE, IDLE_TYPE_SO,
					CMD_TYPE_SW_SWITCH, en);
}
EXPORT_SYMBOL(spm_enable_sodi);

static struct dentry *root_entry;

static int __init mt_cpuidle_debugfs_init(void)
{
	root_entry = debugfs_create_dir("cpuidle", NULL);
	if (!root_entry) {
		idle_err("Can not create debugfs 'cpuidle'\n");
		return 1;
	}

	debugfs_create_file("idle_state", 0444, root_entry, NULL,
				&idle_state_fops);
	debugfs_create_file("dpidle_state", 0644, root_entry, NULL,
				&dpidle_state_fops);
	debugfs_create_file("soidle_state", 0644, root_entry, NULL,
				&soidle_state_fops);

	return 0;
}
module_init(mt_cpuidle_debugfs_init);
