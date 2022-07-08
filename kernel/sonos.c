/*
 * Copyright (c) 2014-2017, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <linux/module.h>
#include "mdp.h"
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/sonos_kernel.h>
#include "sect_upgrade_header.h"

/* In memory copy of the MDP */
struct manufacturing_data_page sys_mdp;
EXPORT_SYMBOL(sys_mdp);
struct manufacturing_data_page3 sys_mdp3;
EXPORT_SYMBOL(sys_mdp3);

/* Provides the atheros HAL a way of getting the calibration data from NAND */
extern int ath_nand_local_read(u_char *cal_part,loff_t from, size_t len,
		size_t *retlen, u_char *buf);
EXPORT_SYMBOL(ath_nand_local_read);

/* In memory copy of the MDP */
char uboot_version_str[120];
EXPORT_SYMBOL(uboot_version_str);

/* This function provide a commen API for ADC voltage read */
int read_adc_voltage(int chan, int *mvolts)
{
	(void)vf610_read_adc(chan, mvolts);
	return 0;
}
EXPORT_SYMBOL(read_adc_voltage);

/* These functions are defined to provide a callback interface
 * for getting orientation events from audiodev to the wifi driver.
 *
 * See all/cc/atheros/driver/diversity.c
 */
#if defined(CONFIG_SONOS_ENCORE)

// Encore only has one wifi device
#define MAX_ORIENTATION_CALLBACKS	1

struct orientation_cback {
	void	(*function)(int orient, void *param);
	void	*param;
};

static struct orientation_cback orientation_cb_tbl[MAX_ORIENTATION_CALLBACKS];
static int orientation_cb_refs = 0;

void *sonos_orientation_register_callback(void (*function)(int orient, void *param), void *param)
{
	int idx;
	struct orientation_cback *new_entry = NULL;

	// First register initializes the array
	if (!orientation_cb_refs) {
		memset(orientation_cb_tbl, 0, sizeof(orientation_cb_tbl));
	}
	orientation_cb_refs++;

	// Search table for a free entry
	for (idx = 0; idx < ARRAY_SIZE(orientation_cb_tbl); idx++) {
		if (orientation_cb_tbl[idx].function == NULL) {
			new_entry = &(orientation_cb_tbl[idx]);
			break;
		}
	}

	if (new_entry == NULL) {
		printk(KERN_WARNING "%s: max callbacks registered!\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	// Set callback
	new_entry->function = function;
	new_entry->param = param;

	return new_entry;
}
EXPORT_SYMBOL(sonos_orientation_register_callback);

int sonos_orientation_unregister_callback(void *entry)
{
	struct orientation_cback *del_entry = entry;

	if (del_entry == NULL) {
		printk(KERN_WARNING "%s: callback entry is null!\n", __func__);
		return -EFAULT;
	}

	if (del_entry->function == NULL) {
		printk(KERN_WARNING "%s: callback already unregistered!\n", __func__);
		return -EINVAL;
	}

	del_entry->function = NULL;
	del_entry->param = NULL;
	orientation_cb_refs--;
	return 0;
}
EXPORT_SYMBOL(sonos_orientation_unregister_callback);

void sonos_orientation_change_event(int orient)
{
	int idx;

	// Call all registered callbacks (blank entries in table are silently ignored)
	for (idx = 0; idx < ARRAY_SIZE(orientation_cb_tbl); idx++) {
		if (orientation_cb_tbl[idx].function != NULL) {
			orientation_cb_tbl[idx].function(orient, orientation_cb_tbl[idx].param);
		}
	}
}
EXPORT_SYMBOL(sonos_orientation_change_event);

#endif // CONFIG_SONOS_ENCORE

#ifdef CONFIG_SONOS_SECBOOT
#if defined CONFIG_SONOS_DIAGS && !defined SONOS_STRICT_DIAG_BUILD
int enable_console = 1;
#else
int enable_console;
#endif

static int __init enable_console_setup(char *str)
{
	if(!str) return 1;

	// when uboot passes enable_console it always passes 1
	enable_console = simple_strtoul(str, NULL, 10);

	return 1;
}
/* FIXME - Uboot needs to change this to "enable_console=" so it's consistent with other platforms */
__setup("enable_imx=", enable_console_setup);
#endif //CONFIG_SONOS_SECBOOT
