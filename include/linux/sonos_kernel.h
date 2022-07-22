/*
 * Copyright (c) 2014-2020, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 *
 * The following file is used for prototypes and data structures for
 * sonos APIs embedded in the Linux kernel.
 *
 * See kernel/sonos.c.
 */

#ifndef SONOS_KERNEL_H
#define SONOS_KERNEL_H

#include <linux/string.h>

#if defined(CONFIG_SONOS)

/* Board disambiguation - value from the dtb */
extern int sonos_product_id;

#if defined(SONOS_ARCH_ATTR_SOC_IS_MT8521P)
/* Match gpio values defined in patches/2016.11/to_add/board/sonos/mt8521p/board_parameter.h */
#define PRODUCT_ID_ELREY   3
#define PRODUCT_ID_HIDEOUT 1

/* NOTE:  usage of these defines should be wrapped in SOC_IS checks, as the product id is only
 * unique on a per-SoC basis.
 */
#define PRODUCT_ID_IS_ELREY ( (sonos_product_id == PRODUCT_ID_ELREY) )
#define PRODUCT_ID_IS_HIDEOUT ( (sonos_product_id == PRODUCT_ID_HIDEOUT) )
#elif defined(SONOS_ARCH_ATTR_SOC_IS_A113)
/* Match gpio values defined in patches/2016.11/to_add/board/sonos/a113/board_parameter.h */
#define PRODUCT_ID_TUPELO 0x1
#define PRODUCT_ID_APOLLO 0xf
#define PRODUCT_ID_APOLLOX 0xb
#define PRODUCT_ID_DHUEZ 0xc
#define PRODUCT_ID_MONACO 0xd
#define PRODUCT_ID_MONACOSL 0xe
#define PRODUCT_ID_FURY 0x2
#define PRODUCT_ID_GOLDENEYE 0x8

/* NOTE:  usage of these defines should be wrapped in SOC_IS checks, as the product id is only
 * unique on a per-SoC basis.
 */
#define PRODUCT_ID_IS_TUPELO ( (sonos_product_id == PRODUCT_ID_TUPELO) )
#define PRODUCT_ID_IS_DHUEZ ( (sonos_product_id == PRODUCT_ID_DHUEZ) )
#define PRODUCT_ID_IS_APOLLO ( (sonos_product_id == PRODUCT_ID_APOLLO) )
#define PRODUCT_ID_IS_APOLLOX ( (sonos_product_id == PRODUCT_ID_APOLLOX) )
#define PRODUCT_ID_IS_MONACO ( (sonos_product_id == PRODUCT_ID_MONACO) )
#define PRODUCT_ID_IS_MONACOSL ( (sonos_product_id == PRODUCT_ID_MONACOSL) )
#define PRODUCT_ID_IS_FURY ( (sonos_product_id == PRODUCT_ID_FURY) )
#define PRODUCT_ID_IS_GOLDENEYE ( (sonos_product_id == PRODUCT_ID_GOLDENEYE) )

#elif defined(SONOS_ARCH_ATTR_SOC_IS_S767)
/* Match gpio values defined in u-boot/patches/2016.11/to_add/board/sonos/s767/board_parameter.h */
#define PRODUCT_ID_OPTIMO1 0x1
#define PRODUCT_ID_OPTIMO1SL 0x2
#define PRODUCT_ID_OPTIMO2 0x3
#define PRODUCT_ID_PRIMA 0x4
#define PRODUCT_ID_LASSO 0x5

/* NOTE:  usage of these defines should be wrapped in SOC_IS checks, as the product id is only
 * unique on a per-SoC basis.
 */
#define PRODUCT_ID_IS_OPTIMO1 ( (sonos_product_id == PRODUCT_ID_OPTIMO1) )
#define PRODUCT_ID_IS_OPTIMO1SL ( (sonos_product_id == PRODUCT_ID_OPTIMO1SL) )
#define PRODUCT_ID_IS_OPTIMO2 ( (sonos_product_id == PRODUCT_ID_OPTIMO2) )

#elif defined(SONOS_ARCH_ATTR_SOC_IS_IMX6)
/* Match gpio values defined in patches/2016.11/to_add/board/sonos/imx6/board_parameter.h */
#define PRODUCT_ID_ENCORE    0x1
#define PRODUCT_ID_SOLBASE   0x2
#define PRODUCT_ID_ROYALE    0x3
#define PRODUCT_ID_BOOTLEG   0x4
#define PRODUCT_ID_PARAMOUNT 0x5
#define PRODUCT_ID_CHAPLIN   0x6
#define PRODUCT_ID_NEPTUNE   0x7
#define PRODUCT_ID_VERTIGO   0x8
#define PRODUCT_ID_TITAN     0x9
#define PRODUCT_ID_GRAVITY   0xa
/* NOTE:  usage of these defines should be wrapped in SOC_IS checks, as the product id is only
 * unique on a per-SoC basis.
 */
#define PRODUCT_ID_IS_ENCORE ( (sonos_product_id == PRODUCT_ID_ENCORE) )
#define PRODUCT_ID_IS_SOLBASE ( (sonos_product_id == PRODUCT_ID_SOLBASE) )
#define PRODUCT_ID_IS_ROYALE ( (sonos_product_id == PRODUCT_ID_ROYALE) )
#define PRODUCT_ID_IS_BOOTLEG ( (sonos_product_id == PRODUCT_ID_BOOTLEG) )
#define PRODUCT_ID_IS_PARAMOUNT ( (sonos_product_id == PRODUCT_ID_PARAMOUNT) )
#define PRODUCT_ID_IS_CHAPLIN ( (sonos_product_id == PRODUCT_ID_CHAPLIN) )
#define PRODUCT_ID_IS_NEPTUNE ( (sonos_product_id == PRODUCT_ID_NEPTUNE) )
#define PRODUCT_ID_IS_VERTIGO ( (sonos_product_id == PRODUCT_ID_VERTIGO) )
#define PRODUCT_ID_IS_TITAN ( (sonos_product_id == PRODUCT_ID_TITAN) )
#define PRODUCT_ID_IS_GRAVITY ( (sonos_product_id == PRODUCT_ID_GRAVITY) )

#elif defined(SONOS_ARCH_ATTR_SOC_IS_MT8518S)
#define PRODUCT_ID_BRAVO     0x1
#define PRODUCT_ID_IS_BRAVO  ( (sonos_product_id == PRODUCT_ID_BRAVO) )

#endif

extern char sonos_machine_name[];

static inline bool product_is_dhuez(void)
{
	return strstr(sonos_machine_name, "Dhuez") != 0;
}

static inline bool product_is_tupelo(void)
{
	return strstr(sonos_machine_name, "Tupelo") != 0;
}

static inline bool product_is_apollo(void)
{
	return strstr(sonos_machine_name, "Apollo") != 0;
}

static inline bool product_is_apollox(void)
{
	return strstr(sonos_machine_name, "ApolloX") != 0;
}

static inline bool product_is_elrey(void)
{
	return strstr(sonos_machine_name, "El Rey HT") != 0;
}

static inline bool product_is_hideout(void)
{
	return strstr(sonos_machine_name, "Hideout") != 0;
}

static inline bool product_is_encore(void)
{
	return strstr(sonos_machine_name, "Encore") != 0;
}

static inline bool product_is_solbase(void)
{
	return strstr(sonos_machine_name, "Solbase") != 0;
}

static inline bool product_is_royale(void)
{
	return strstr(sonos_machine_name, "Royale") != 0;
}

static inline bool product_is_bootleg(void)
{
	return strstr(sonos_machine_name, "Bootleg") != 0;
}

static inline bool product_is_paramount(void)
{
	return strstr(sonos_machine_name, "Paramount") != 0;
}

static inline bool product_is_chaplin(void)
{
	return strstr(sonos_machine_name, "Chaplin") != 0;
}

static inline bool product_is_neptune(void)
{
	return strstr(sonos_machine_name, "Neptune") != 0;
}

static inline bool product_is_vertigo(void)
{
	return strstr(sonos_machine_name, "Vertigo") != 0;
}

static inline bool product_is_monaco(void)
{
	return strstr(sonos_machine_name, "Monaco") != 0;
}

static inline bool product_is_monacosl(void)
{
	return strstr(sonos_machine_name, "Monaco SL") != 0;
}

static inline bool product_is_fury(void)
{
	return strstr(sonos_machine_name, "Fury") != 0;
}

static inline bool product_is_bravo(void)
{
	return strstr(sonos_machine_name, "Bravo") != 0;
}

static inline bool product_is_gravity(void)
{
	return strstr(sonos_machine_name, "Gravity") != 0;
}

static inline bool product_is_optimo1(void)
{
	return strstr(sonos_machine_name, "Optimo1") != 0;
}

static inline bool product_is_optimo1sl(void)
{
	return strstr(sonos_machine_name, "Optimo1SL") != 0;
}

static inline bool product_is_optimo2(void)
{
	return strstr(sonos_machine_name, "Optimo2") != 0;
}

extern void *sonos_orientation_register_callback(void (*function)(int orient, void *param), void *param);
extern int sonos_orientation_unregister_callback(void *entry);
extern void sonos_orientation_change_event(int orient);

#ifdef CONFIG_VF610_ADC
// see drivers/iio/adc/vf610_adc.c
extern int vf610_read_adc(int chan, int *mvolts);
#elif defined(CONFIG_MEDIATEK_MT6577_AUXADC)
// see drivers/iio/adc/mt6577_auxadc.c
extern int mt6577_read_adc(int chan, int *mvolts);
#elif defined(CONFIG_AMLOGIC_SARADC)
extern int sonos_sar_adc_iio_info_read_raw(int chan, int *mvolts);
#endif

// returns completion status, passes back milli-volts
extern int read_adc_voltage(int chan, int *mvolts);

// disable/enable NAND access during shutdown
// see drivers/mtd/nand/nand_base.c
extern int nand_shutdown_access(int);

#include "mdp.h"
extern int bootgeneration;
extern int bootsection;
extern struct manufacturing_data_page sys_mdp;

#if defined(CONFIG_IMX_SDMA)
/*
 * The following are special Sonos functions retrofitted into the
 * imx-sdma driver.
 */

#include <linux/dmaengine.h>
#include <linux/types.h>

extern dma_addr_t sdma_sonos_swap_data_pointer(struct dma_chan *chan,
		u32 index, dma_addr_t data_phys);

#endif // CONFIG_IMX_SDMA

#endif // CONFIG_SONOS
#endif // SONOS_KERNEL_H
