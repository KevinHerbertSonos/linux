/*
 * Copyright (c) 2014, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <linux/module.h>
#include "mdp.h"
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/of_fdt.h>
#include <linux/sonos_kernel.h>
#include "sect_upgrade_header.h"

/* This function provide a commen API for ADC voltage read */
int read_adc_voltage(int chan, int *mvolts)
{
#ifdef CONFIG_VF610_ADC
	(void)vf610_read_adc(chan, mvolts);
#elif defined(CONFIG_MEDIATEK_MT6577_AUXADC)
	(void)mt6577_read_adc(chan, mvolts);
#endif
	return 0;
}
EXPORT_SYMBOL(read_adc_voltage);

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

int sonos_product_id;
EXPORT_SYMBOL(sonos_product_id);

/* move Sonos special kernel code to here */
extern char uboot_version_str[120];
extern struct manufacturing_data_page3 sys_mdp3;

/*
 * copy the mdp from uboot
 */
static int __init early_mdp(char *p)
{
	unsigned long mdpAddr = 0;
	void *mdpAddrV = NULL;

	/* Build bomb - mdp sizes must be the same for all builds, platforms, controllers */
	BUILD_BUG_ON(sizeof(struct manufacturing_data_page)!=MDP1_BYTES);
	BUILD_BUG_ON(sizeof(struct manufacturing_data_page2)!=MDP2_BYTES);
	BUILD_BUG_ON(sizeof(struct manufacturing_data_page3)!=MDP3_BYTES);

	uboot_version_str[0] = 0;
	if (kstrtoul(p, 16, &mdpAddr)) {
		printk(KERN_ERR "early_mdp: strtoul returned an error\n");
		goto mdp_err;
	}
	mdpAddrV = phys_to_virt(mdpAddr);

	memcpy(&sys_mdp, mdpAddrV, sizeof(struct manufacturing_data_page));
	if (sys_mdp.mdp_magic == MDP_MAGIC) {
		struct smdp *s_mdp = (struct smdp *)mdpAddrV;
		printk("MDP: model %x, submodel %x, rev %x\n",
			sys_mdp.mdp_model, sys_mdp.mdp_submodel, sys_mdp.mdp_revision);
		memcpy(uboot_version_str, sys_mdp.mdp_reserved3, 120);
		if ( uboot_version_str[0] == 'U' ) {
			printk("U-boot revision %s\n", uboot_version_str);
		}
		memset(&sys_mdp3, 0, sizeof(struct manufacturing_data_page3));
		if ( s_mdp->mdp3.mdp3_magic == MDP_MAGIC3 ) {
			printk("got mdp 3\n");
			memcpy(&sys_mdp3, &s_mdp->mdp3,
				sizeof(struct manufacturing_data_page3));
		}
		return 0;
	} else {
		printk("MDP: invalid magic: is %08x should be %08x, using default\n",
			sys_mdp.mdp_magic, MDP_MAGIC);
		printk("MDP: flags = %#x\n", sys_mdp.mdp_flags);
	}

mdp_err:
	memset(&sys_mdp, 0, sizeof(struct manufacturing_data_page));
	return 0;
}
early_param("mdpaddr", early_mdp);

int bootgeneration = 0;
static int __init bootgeneration_setup(char *str)
{
	bootgeneration = simple_strtoul(str, NULL, 0);
	return 1;
}
__setup("bootgen=", bootgeneration_setup);

int bootsection = 0;
static int __init bootsection_setup(char *str)
{
	bootsection = simple_strtoul(str, NULL, 0);
	return 1;
}
__setup("bootsect=", bootsection_setup);

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
__setup("enable_console=", enable_console_setup);
#endif //CONFIG_SONOS_SECBOOT

char sonos_machine_name[128];
EXPORT_SYMBOL(sonos_machine_name);

static int __init sonos_productid_init(void)
{
	const u32 *product_id; /* One byte value in a 32-bit field */
	const char *uboot_version;

	sonos_product_id = 0;

	product_id = of_get_flat_dt_prop(0, "sonos-product-id", NULL);
	if ( product_id ) {
		sonos_product_id = ntohl(*product_id);
	}
	pr_info("Machine sonos_product_id %d\n", sonos_product_id);
	snprintf(sonos_machine_name, sizeof sonos_machine_name, "%s",
			of_flat_dt_get_machine_name());
	pr_info("Machine Name %s\n", sonos_machine_name);

	uboot_version_str[0] = 0;
	uboot_version = of_get_flat_dt_prop(0, "uboot-version", NULL);
	if ( uboot_version ) {
		snprintf(uboot_version_str, sizeof uboot_version_str, "%s",
				uboot_version);
		if ( uboot_version_str[0] == 'U' ) {
			pr_info("U-boot revision %s\n", uboot_version_str);
		}
	}
	return 0;
}
arch_initcall(sonos_productid_init);
