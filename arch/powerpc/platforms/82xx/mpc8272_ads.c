/*
 * MPC8272 ADS board support
 *
 * Copyright 2007 Freescale Semiconductor, Inc.
 * Author: Scott Wood <scottwood@freescale.com>
 *
 * Based on code by Vitaly Bordug <vbordug@ru.mvista.com>
 * Copyright (c) 2006 MontaVista Software, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/fsl_devices.h>
#include <linux/of_platform.h>
#include <linux/io.h>

#include <asm/cpm2.h>
#include <asm/udbg.h>
#include <asm/machdep.h>
#include <asm/time.h>

#include <platforms/82xx/pq2.h>

#include <sysdev/fsl_soc.h>
#include <sysdev/cpm2_pic.h>
#ifdef CONFIG_SONOS
#include <linux/of_gpio.h>
#include <linux/mtd/nand-gpio.h>
#endif

#include "pq2.h"

static void __init mpc8272_ads_pic_init(void)
{
	struct device_node *np = of_find_compatible_node(NULL, NULL,
	                                                 "fsl,cpm2-pic");
	if (!np) {
		printk(KERN_ERR "PIC init: can not find fsl,cpm2-pic node\n");
		return;
	}

	cpm2_pic_init(np);
	of_node_put(np);

	/* Initialize stuff for the 82xx CPLD IC and install demux  */
#ifndef CONFIG_SONOS
	pq2ads_pci_init_irq();
#endif
}

struct cpm_pin {
	int port, pin, flags;
};

static struct cpm_pin mpc8272_ads_pins[] = {
#ifdef CONFIG_SONOS
	/* LEDs */
	{0, 10, CPM_PIN_OUTPUT | CPM_PIN_GPIO | CPM_PIN_PRIMARY}, /* Red or Green */
	{0, 12, CPM_PIN_OUTPUT | CPM_PIN_GPIO | CPM_PIN_PRIMARY}, /* Green or Red */
	{0, 13, CPM_PIN_OUTPUT | CPM_PIN_GPIO | CPM_PIN_PRIMARY}, /* Yellow */
	{3, 30, CPM_PIN_OUTPUT | CPM_PIN_GPIO | CPM_PIN_PRIMARY}, /* White */

	/* NAND */
	{2, 29, CPM_PIN_INPUT | CPM_PIN_GPIO | CPM_PIN_PRIMARY},  /* rdy */
	{2, 28, CPM_PIN_OUTPUT | CPM_PIN_GPIO | CPM_PIN_PRIMARY}, /* nce */
	{0, 23, CPM_PIN_OUTPUT | CPM_PIN_GPIO | CPM_PIN_PRIMARY}, /* ale */
	{0, 22, CPM_PIN_OUTPUT | CPM_PIN_GPIO | CPM_PIN_PRIMARY}, /* cle */

	/* FCC1 */
	{0, 14, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{0, 15, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{0, 16, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{0, 17, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{0, 18, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{0, 19, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{0, 20, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{0, 21, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{0, 26, CPM_PIN_INPUT | CPM_PIN_SECONDARY},
	{0, 27, CPM_PIN_INPUT | CPM_PIN_SECONDARY},
	{0, 28, CPM_PIN_OUTPUT | CPM_PIN_SECONDARY},
	{0, 29, CPM_PIN_OUTPUT | CPM_PIN_SECONDARY},
	{0, 30, CPM_PIN_INPUT | CPM_PIN_SECONDARY},
	{0, 31, CPM_PIN_INPUT | CPM_PIN_SECONDARY},
	{2, 21, CPM_PIN_INPUT | CPM_PIN_GPIO | CPM_PIN_PRIMARY},
	{2, 22, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{2, 28, CPM_PIN_INPUT | CPM_PIN_GPIO | CPM_PIN_PRIMARY},

	/* FCC2 */
	{1, 18, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{1, 19, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{1, 20, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{1, 21, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{1, 22, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{1, 23, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{1, 24, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{1, 25, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{1, 26, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{1, 27, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{1, 28, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{1, 29, CPM_PIN_OUTPUT | CPM_PIN_SECONDARY},
	{1, 30, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{1, 31, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{2, 16, CPM_PIN_INPUT | CPM_PIN_GPIO | CPM_PIN_PRIMARY},
	{2, 17, CPM_PIN_INPUT | CPM_PIN_GPIO | CPM_PIN_PRIMARY},

	/* MDIO */
	{3, 23, CPM_PIN_OUTPUT | CPM_PIN_GPIO | CPM_PIN_PRIMARY},
	{3, 24, CPM_PIN_OUTPUT | CPM_PIN_GPIO | CPM_PIN_PRIMARY},
	{3, 25, CPM_PIN_OUTPUT | CPM_PIN_GPIO | CPM_PIN_PRIMARY},
#else
	/* SCC1 */
	{3, 30, CPM_PIN_OUTPUT | CPM_PIN_SECONDARY},
	{3, 31, CPM_PIN_INPUT | CPM_PIN_PRIMARY},

	/* SCC4 */
	{3, 21, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{3, 22, CPM_PIN_INPUT | CPM_PIN_PRIMARY},

	/* FCC1 */
	{0, 14, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{0, 15, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{0, 16, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{0, 17, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{0, 18, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{0, 19, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{0, 20, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{0, 21, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{0, 26, CPM_PIN_INPUT | CPM_PIN_SECONDARY},
	{0, 27, CPM_PIN_INPUT | CPM_PIN_SECONDARY},
	{0, 28, CPM_PIN_OUTPUT | CPM_PIN_SECONDARY},
	{0, 29, CPM_PIN_OUTPUT | CPM_PIN_SECONDARY},
	{0, 30, CPM_PIN_INPUT | CPM_PIN_SECONDARY},
	{0, 31, CPM_PIN_INPUT | CPM_PIN_SECONDARY},
	{2, 21, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{2, 22, CPM_PIN_INPUT | CPM_PIN_PRIMARY},

	/* FCC2 */
	{1, 18, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{1, 19, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{1, 20, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{1, 21, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{1, 22, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{1, 23, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{1, 24, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{1, 25, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{1, 26, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{1, 27, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{1, 28, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{1, 29, CPM_PIN_OUTPUT | CPM_PIN_SECONDARY},
	{1, 30, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{1, 31, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{2, 16, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{2, 17, CPM_PIN_INPUT | CPM_PIN_PRIMARY},

	/* I2C */
	{3, 14, CPM_PIN_INPUT | CPM_PIN_SECONDARY | CPM_PIN_OPENDRAIN},
	{3, 15, CPM_PIN_INPUT | CPM_PIN_SECONDARY | CPM_PIN_OPENDRAIN},

	/* USB */
	{2, 10, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{2, 11, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{2, 20, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{2, 24, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{3, 23, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{3, 24, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{3, 25, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
#endif
};

static void __init init_ioports(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mpc8272_ads_pins); i++) {
		struct cpm_pin *pin = &mpc8272_ads_pins[i];
		cpm2_set_pin(pin->port, pin->pin, pin->flags);
	}

	cpm2_clk_setup(CPM_CLK_SCC1, CPM_BRG1, CPM_CLK_RX);
	cpm2_clk_setup(CPM_CLK_SCC1, CPM_BRG1, CPM_CLK_TX);
	cpm2_clk_setup(CPM_CLK_SCC3, CPM_CLK8, CPM_CLK_RX);
	cpm2_clk_setup(CPM_CLK_SCC3, CPM_CLK8, CPM_CLK_TX);
	cpm2_clk_setup(CPM_CLK_SCC4, CPM_BRG4, CPM_CLK_RX);
	cpm2_clk_setup(CPM_CLK_SCC4, CPM_BRG4, CPM_CLK_TX);
#ifdef CONFIG_SONOS
	cpm2_clk_setup(CPM_CLK_FCC1, CPM_CLK9, CPM_CLK_RX);
	cpm2_clk_setup(CPM_CLK_FCC1, CPM_CLK10, CPM_CLK_TX);
	cpm2_clk_setup(CPM_CLK_FCC2, CPM_CLK13, CPM_CLK_RX);
	cpm2_clk_setup(CPM_CLK_FCC2, CPM_CLK14, CPM_CLK_TX);
#else
	cpm2_clk_setup(CPM_CLK_FCC1, CPM_CLK11, CPM_CLK_RX);
	cpm2_clk_setup(CPM_CLK_FCC1, CPM_CLK10, CPM_CLK_TX);
	cpm2_clk_setup(CPM_CLK_FCC2, CPM_CLK15, CPM_CLK_RX);
	cpm2_clk_setup(CPM_CLK_FCC2, CPM_CLK16, CPM_CLK_TX);
#endif
}

#ifdef CONFIG_SONOS
#if defined(CONFIG_MTD_NAND_GPIO) || defined(CONFIG_MTD_NAND_GPIO_MODULE)
static struct resource mpc8272_nand_resource = {
	.start = 0,
};

struct gpio_nand_platdata mpc8272_nand_platdata = {
};

static struct platform_device mpc8272_nand_device = {
	.name		= "gpio-nand",
	.id		= -1,
	.resource	= &mpc8272_nand_resource,
	.num_resources	= 1,
	.dev		= {
			.platform_data = &mpc8272_nand_platdata,
			},
};

static int __init mpc8272_init_nand(void)
{
	struct device_node *np;
	const u32 *data;
	int len;
	struct gpio_nand_platdata *plat = &mpc8272_nand_platdata;

	np = of_find_compatible_node(NULL, NULL, "gpio-control-nand");
	if ( !np ) {
                return -ENODEV;
	}

	if ( of_address_to_resource(np, 0, &mpc8272_nand_resource) ) {
		pr_err("mpc8272-nand: resource failed\n");
                return -ENODEV;
	}

	plat->gpio_rdy = of_get_gpio(np, 0);
	plat->gpio_nce = of_get_gpio(np, 1);
	plat->gpio_ale = of_get_gpio(np, 2);
	plat->gpio_cle = of_get_gpio(np, 3);

        data = of_get_property(np, "chip-delay", &len);
        if (data && len == 4) {
                plat->chip_delay = *data;
	}

        data = of_get_property(np, "bank-width", &len);
        if (data && len == 4) {
		u32 val = *data;
		if (val == 2) {
			plat->options |= NAND_BUSWIDTH_16;
		} else if (val != 1) {
			printk("invalid bank-width %u\n", val);
		}
	}
	platform_device_register(&mpc8272_nand_device);
	return 0;
}
#else
static inline int mpc8272_init_nand(void)
{
	return 0;
}
#endif
module_init(mpc8272_init_nand);
#endif

static void __init mpc8272_ads_setup_arch(void)
{
#ifndef CONFIG_SONOS
	struct device_node *np;
	__be32 __iomem *bcsr;
#endif

	if (ppc_md.progress)
		ppc_md.progress("mpc8272_ads_setup_arch()", 0);

	cpm2_reset();

#ifndef CONFIG_SONOS
	np = of_find_compatible_node(NULL, NULL, "fsl,mpc8272ads-bcsr");
	if (!np) {
		printk(KERN_ERR "No bcsr in device tree\n");
		return;
	}

	bcsr = of_iomap(np, 0);
	of_node_put(np);
	if (!bcsr) {
		printk(KERN_ERR "Cannot map BCSR registers\n");
		return;
	}

#define BCSR1_FETHIEN		0x08000000
#define BCSR1_FETH_RST		0x04000000
#define BCSR1_RS232_EN1		0x02000000
#define BCSR1_RS232_EN2		0x01000000
#define BCSR3_USB_nEN		0x80000000
#define BCSR3_FETHIEN2		0x10000000
#define BCSR3_FETH2_RST		0x08000000

	clrbits32(&bcsr[1], BCSR1_RS232_EN1 | BCSR1_RS232_EN2 | BCSR1_FETHIEN);
	setbits32(&bcsr[1], BCSR1_FETH_RST);

	clrbits32(&bcsr[3], BCSR3_FETHIEN2);
	setbits32(&bcsr[3], BCSR3_FETH2_RST);

	clrbits32(&bcsr[3], BCSR3_USB_nEN);

	iounmap(bcsr);
#endif

	init_ioports();
	pq2_init_pci();

	if (ppc_md.progress)
		ppc_md.progress("mpc8272_ads_setup_arch(), finish", 0);
}

static struct of_device_id __initdata of_bus_ids[] = {
	{ .name = "soc", },
	{ .name = "cpm", },
	{ .name = "localbus", },
	{},
};

static int __init declare_of_platform_devices(void)
{
	/* Publish the QE devices */
	of_platform_bus_probe(NULL, of_bus_ids, NULL);
	return 0;
}
machine_device_initcall(mpc8272_ads, declare_of_platform_devices);

/*
 * Called very early, device-tree isn't unflattened
 */
static int __init mpc8272_ads_probe(void)
{
	unsigned long root = of_get_flat_dt_root();
	return of_flat_dt_is_compatible(root, "fsl,mpc8272ads");
}

define_machine(mpc8272_ads)
{
	.name = "Freescale MPC8272 ADS",
	.probe = mpc8272_ads_probe,
	.setup_arch = mpc8272_ads_setup_arch,
	.init_IRQ = mpc8272_ads_pic_init,
	.get_irq = cpm2_get_irq,
#ifdef CONFIG_SONOS
	.pci_irq_fixup = mpc8272_pci_irq_fixup,
#endif
	.calibrate_decr = generic_calibrate_decr,
	.restart = pq2_restart,
	.progress = udbg_progress,
};
