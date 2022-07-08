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


#ifndef MT8518_ASRC_REG_H
#define MT8518_ASRC_REG_H
#include <linux/io.h>
#include <linux/regmap.h>

extern struct regmap *asrc_regmap;

static inline unsigned int afe_read(unsigned int addr)
{
	unsigned int val;
	int ret;

	ret = regmap_read(asrc_regmap, addr, &val);
	return val;
}

#define afe_write(addr, val) \
	regmap_write(asrc_regmap, addr, val)

#define afe_msk_write(addr, val, msk) \
	regmap_update_bits(asrc_regmap, addr, msk, val)


/*****************************************************************************
 *                  R E G I S T E R       D E F I N I T I O N
 *****************************************************************************/
#define MASRC_OFFSET (0x100U)

#define MASRC_ASM_CON2           (0x1074U)
#define MEM_ASRC_1_CALI_CLK_SEL_POS  2
#define MEM_ASRC_2_CALI_CLK_SEL_POS  5
#define MEM_ASRC_3_CALI_CLK_SEL_POS  8
#define MEM_ASRC_4_CALI_CLK_SEL_POS  11
#define MEM_ASRC_1_RESET_POS  20
#define MEM_ASRC_2_RESET_POS  21
#define MEM_ASRC_3_RESET_POS  22
#define MEM_ASRC_4_RESET_POS  23

#define MASRC_TOP_CON    (0x01090U)
#define PDN_ASRC_BRG_POS           (16)
#define LAT_DATA_EN_I2SO3_POS  (17)
#define LAT_DATA_EN_I2SO4_POS  (18)
#define LAT_DATA_EN_I2SO5_POS  (19)
#define LAT_DATA_EN_I2SO6_POS  (20)
#define PDN_DSD_ENC_POS     15
#define PDN_DSD_ENC_MASK    (1<<PDN_DSD_ENC_POS)
#define PDN_MEM_ASRC5_POS   14
#define PDN_MEM_ASRC5_MASK  (1<<PDN_MEM_ASRC5_POS)
#define PDN_MEM_ASRC4_POS   13
#define PDN_MEM_ASRC4_MASK  (1<<PDN_MEM_ASRC4_POS)
#define PDN_MEM_ASRC3_POS   12
#define PDN_MEM_ASRC3_MASK  (1<<PDN_MEM_ASRC3_POS)
#define PDN_MEM_ASRC2_POS   11
#define PDN_MEM_ASRC2_MASK  (1<<PDN_MEM_ASRC2_POS)
#define PDN_MEM_ASRC1_POS   10
#define PDN_MEM_ASRC1_MASK  (1<<PDN_MEM_ASRC1_POS)

//ASM_GEN_CONF
#define REG_ASRC_GEN_CONF                       (0x0B00U)
#define POS_CH_CNTX_SWEN        20
#define MASK_CH_CNTX_SWEN       (1<<POS_CH_CNTX_SWEN)
#define POS_CH_CLEAR            16
#define MASK_CH_CLEAR           (1<<POS_CH_CLEAR)
#define POS_CH_EN               12
#define MASK_CH_EN              (1<<POS_CH_EN)
#define POS_DSP_CTRL_COEFF_SRAM 11
#define MASK_DSP_CTRL_COEFF_SRAM (1<<POS_DSP_CTRL_COEFF_SRAM)
#define POS_ASRC_BUSY           9
#define MASK_ASRC_BUSY          (1<<POS_ASRC_BUSY)
#define POS_ASRC_EN             8
#define MASK_ASRC_EN            (1<<POS_ASRC_EN)

//ASM_IER, ASM_IFR
#define REG_ASRC_IER                            (0x0B04U)
#define REG_ASRC_IFR                            (0x0B08U)

//ASM_CH01_CNFG
#define REG_ASRC_CH01_CNFG                      (0x0B10U)
#define POS_CLR_IIR_BUF         23
#define MASK_CLR_IIR_BUF        (1<<POS_CLR_IIR_BUF)
#define POS_OBIT_WIDTH          22
#define MASK_OBIT_WIDTH         (1<<POS_OBIT_WIDTH)
#define POS_IBIT_WIDTH          21
#define MASK_IBIT_WIDTH         (1<<POS_IBIT_WIDTH)
#define POS_MONO                20
#define MASK_MONO               (1<<POS_MONO)
#define POS_OFS                 18
#define MASK_ASRC_OFS                (3<<POS_OFS)
#define POS_IFS                 16
#define MASK_ASRC_IFS                (3<<POS_IFS)
#define POS_CLAC_AMOUNT         8
#define MASK_CLAC_AMOUNT        (0xFF<<POS_CLAC_AMOUNT)
#define POS_IIR_EN              7
#define MASK_IIR_EN             (1<<POS_IIR_EN)
#define POS_IIR_STAGE           4
#define MASK_IIR_STAGE          (7<<POS_IIR_STAGE)

//ASM_FREQUENCY_0
#define REG_ASRC_FREQUENCY_0                    (0x0B20U)
/* USE [23:0] */
#define REG_ASRC_FREQUENCY_1                    (0x0B24U)
/* USE [23:0] */
#define REG_ASRC_FREQUENCY_2                    (0x0B28U)
/* USE [23:0] */
#define REG_ASRC_FREQUENCY_3                    (0x0B2CU)
/* USE [23:0] */

//ASM_IBUF_SADR
#define REG_ASRC_IBUF_SADR                      (0x0B30U)
#define POS_IBUF_SADR           0
#define MASK_IBUF_SADR          (0xFFFFFFFFU<<POS_IBUF_SADR)
#define REG_ASRC_IBUF_SIZE                      (0x0B34U)
#define POS_CH_IBUF_SIZE        0
#define MASK_CH_IBUF_SIZE       (0xFFFFF<<POS_CH_IBUF_SIZE)
#define REG_ASRC_OBUF_SADR                      (0x0B38U)
#define POS_OBUF_SADR           0
#define MASK_OBUF_SADR          (0xFFFFFFFFU<<POS_OBUF_SADR)
#define REG_ASRC_OBUF_SIZE                      (0x0B3CU)
#define POS_CH_OBUF_SIZE        0
#define MASK_CH_OBUF_SIZE       (0xFFFFF<<POS_CH_OBUF_SIZE)

//ASM_CH01_IBUF_RDPNT
#define REG_ASRC_CH01_IBUF_RDPNT                (0x0B40U)
#define POS_ASRC_CH01_IBUF_RDPNT    0
#define MASK_ASRC_CH01_IBUF_RDPNT   (0xFFFFFFFFU<<POS_ASRC_CH01_IBUF_RDPNT)
#define REG_ASRC_CH01_IBUF_WRPNT                (0x0B50U)
#define POS_ASRC_CH01_IBUF_WRPNT    0
#define MASK_ASRC_CH01_IBUF_WRPNT   (0xFFFFFFFFU<<POS_ASRC_CH01_IBUF_WRPNT)
#define REG_ASRC_CH01_OBUF_WRPNT                (0x0B60U)
#define POS_ASRC_CH01_OBUF_WRPNT    0
#define MASK_ASRC_CH01_OBUF_WRPNT   (0xFFFFFFFFU<<POS_ASRC_CH01_OBUF_WRPNT)
#define REG_ASRC_CH01_OBUF_RDPNT                (0x0B70U)
#define POS_ASRC_CH01_OBUF_RDPNT    0
#define MASK_ASRC_CH01_OBUF_RDPNT   (0xFFFFFFFFU<<POS_ASRC_CH01_OBUF_RDPNT)

//ASM_IBUF_INTR_CNT0
#define REG_ASRC_IBUF_INTR_CNT0                 (0x0B80U)
#define POS_CH01_IBUF_INTR_CNT      8
#define MASK_CH01_IBUF_INTR_CNT     (0xFF<<POS_CH01_IBUF_INTR_CNT)
#define REG_ASRC_OBUF_INTR_CNT0                 (0x0B88U)
#define POS_CH01_OBUF_INTR_CNT      8
#define MASK_CH01_OBUF_INTR_CNT     (0xFF<<POS_CH01_OBUF_INTR_CNT)

//ASM_BAK_REG
#define REG_ASRC_BAK_REG                        (0x0B90U)
#define POS_RESULT_SEL              0
#define MASK_RESULT_SEL             (7<<POS_CH01_IBUF_INTR_CNT)

//ASM_FREQ_CALI_CTRL
#define REG_ASRC_FREQ_CALI_CTRL                      (0x0B94U)
#define POS_FREQ_CALC_BUSY      20
#define MASK_FREQ_CALC_BUSY     (1<<POS_FREQ_CALC_BUSY)
#define POS_COMP_FREQRES_EN     19
#define MASK_COMP_FREQRES_EN    (1<<POS_COMP_FREQRES_EN)
#define POS_SRC_SEL             16
#define MASK_SRC_SEL            (3<<POS_SRC_SEL)
#define POS_BYPASS_DEGLITCH     15
#define MASK_BYPASS_DEGLITCH    (1<<POS_BYPASS_DEGLITCH)
#define POS_MAX_GWIDTH          12
#define MASK_MAX_GWIDTH         (7<<POS_MAX_GWIDTH)
#define POS_AUTO_FS2_UPDATE     11
#define MASK_AUTO_FS2_UPDATE    (1<<POS_AUTO_FS2_UPDATE)
#define POS_AUTO_RESTART        10
#define MASK_AUTO_RESTART       (1<<POS_AUTO_RESTART)
#define POS_FREQ_UPDATE_FS2     9
#define MASK_FREQ_UPDATE_FS2    (1<<POS_FREQ_UPDATE_FS2)
#define POS_CALI_EN             8
#define MASK_CALI_EN            (1<<POS_CALI_EN)

//ASM_FREQ_CALI_CYC
#define REG_ASRC_FREQ_CALI_CYC                       (0x0B98U)
#define POS_ASRC_FREQ_CALI_CYC  8
#define MASK_ASRC_FREQ_CALI_CYC (0xFFFF<<POS_ASRC_FREQ_CALI_CYC)

//ASM_PRD_CALI_RESULT
#define REG_ASRC_PRD_CALI_RESULT                     (0x0B9CU)
#define POS_ASRC_PRD_CALI_RESULT    0
#define MASK_ASRC_PRD_CALI_RESULT   (0xFFFFFF<<POS_ASRC_PRD_CALI_RESULT)

//ASM_FREQ_CALI_RESULT
#define REG_ASRC_FREQ_CALI_RESULT                    (0x0BA0U)
#define POS_ASRC_FREQ_CALI_RESULT   0
#define MASK_ASRC_FREQ_CALI_RESULT  (0xFFFFFF<<POS_ASRC_FREQ_CALI_RESULT)

//ASM_CALI_DENOMINATOR
#define REG_ASRC_CALI_DENOMINATOR                    (0x0BD8U)
#define POS_ASRC_CALI_DENOMINATOR   0
#define MASK_ASRC_CALI_DENOMINATOR  (0xFFFFFF<<POS_ASRC_CALI_DENOMINATOR)

//ASM_MAX_OUT_PER_IN0
#define REG_ASRC_MAX_OUT_PER_IN0                     (0x0BE0U)
#define POS_CH01_MAX_OUT_PER_IN0    8
#define MASK_CH01_MAX_OUT_PER_IN0   (0xF<<POS_CH01_MAX_OUT_PER_IN0)

//ASM_IIR_CRAM_ADDR
#define REG_ASRC_IIR_CRAM_ADDR                       (0x0BF0U)
#define POS_ASRC_IIR_CRAM_ADDR      0
#define MASK_ASRC_IIR_CRAM_ADDR     (0xFF<<POS_ASRC_IIR_CRAM_ADDR)

//ASM_IIR_CRAM_DATA
#define REG_ASRC_IIR_CRAM_DATA                       (0x0BF4U)
#define POS_ASRC_IIR_CRAM_DATA      0
#define MASK_ASRC_IIR_CRAM_DATA     (0xFFFFFFFFU<<POS_ASRC_IIR_CRAM_DATA)

//ASM_OUT_BUF_MON0
#define REG_ASRC_OUT_BUF_MON0                        (0x0BF8U)
#define POS_WDLE_CNT                8
#define MASK_WDLE_CNT               (0xFF<<POS_WDLE_CNT)
#define POS_ASRC_WRITE_DONE         0
#define MASK_ASRC_WRITE_DONE        (1<<POS_ASRC_WRITE_DONE)

//ASM_OUT_BUF_MON1
#define REG_ASRC_OUT_BUF_MON1                        (0x0BFCU)
#define POS_ASRC_WR_ADR             4
#define MASK_ASRC_WR_ADR            (0x0FFFFFFF<<POS_ASRC_WR_ADR)

#define MASRC_ASM_CON3               (0x1078U)
#endif
