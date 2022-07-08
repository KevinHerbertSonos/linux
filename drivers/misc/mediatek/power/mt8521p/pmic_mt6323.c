/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Chen Zhong <chen.zhong@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/regmap.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/io.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/mfd/mt6323/registers.h>

/* #include <mt-plat/upmu_common.h> */

#define PMIC6323_E1_CID_CODE    0x1023
#define PMIC6323_E2_CID_CODE    0x2023

struct regmap *pwrap_regmap;

unsigned int pmic_read_interface(unsigned int RegNum, unsigned int *val, unsigned int MASK, unsigned int SHIFT)
{
	unsigned int return_value = 0;
	unsigned int pmic_reg = 0;

	return_value = regmap_read(pwrap_regmap, RegNum, &pmic_reg);
	if (return_value != 0) {
		pr_err("[Power/PMIC][pmic_read_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
		return return_value;
	}

	pmic_reg &= (MASK << SHIFT);
	*val = (pmic_reg >> SHIFT);

	return return_value;
}

unsigned int pmic_config_interface(unsigned int RegNum, unsigned int val, unsigned int MASK, unsigned int SHIFT)
{
	unsigned int return_value = 0;
	unsigned int pmic_reg = 0;

	return_value = regmap_read(pwrap_regmap, RegNum, &pmic_reg);
	if (return_value != 0) {
		pr_err("[Power/PMIC][pmic_config_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
		return return_value;
	}

	pmic_reg &= ~(MASK << SHIFT);
	pmic_reg |= (val << SHIFT);

	return_value = regmap_write(pwrap_regmap, RegNum, pmic_reg);
	if (return_value != 0) {
		pr_err("[Power/PMIC][pmic_config_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
		return return_value;
	}

	return return_value;
}

u32 upmu_get_reg_value(u32 reg)
{
	u32 reg_val = 0;

	pmic_read_interface(reg, &reg_val, 0xFFFF, 0x0);

	return reg_val;
}
EXPORT_SYMBOL(upmu_get_reg_value);

void upmu_set_reg_value(u32 reg, u32 reg_val)
{
	pmic_config_interface(reg, reg_val, 0xFFFF, 0x0);
}

u32 g_reg_value;
static ssize_t show_pmic_access(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_notice("[show_pmic_access] 0x%x\n", g_reg_value);
	return sprintf(buf, "%04X\n", g_reg_value);
}

static ssize_t store_pmic_access(struct device *dev, struct device_attribute *attr, const char *buf,
				 size_t size)
{
	int ret = 0;
	char temp_buf[32];
	char *pvalue;
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;

	strncpy(temp_buf, buf, sizeof(temp_buf));
	temp_buf[sizeof(temp_buf) - 1] = 0;
	pvalue = temp_buf;

	if (size != 0) {
		if (size > 5) {
			ret = kstrtouint(strsep(&pvalue, " "), 16, &reg_address);
			if (ret)
				return ret;
			ret = kstrtouint(pvalue, 16, &reg_value);
			if (ret)
				return ret;
			pr_notice("[store_pmic_access] write PMU reg 0x%x with value 0x%x !\n",
				  reg_address, reg_value);
			ret = pmic_config_interface(reg_address, reg_value, 0xFFFF, 0x0);
		} else {
			ret = kstrtouint(pvalue, 16, &reg_address);
			if (ret)
				return ret;
			ret = pmic_read_interface(reg_address, &g_reg_value, 0xFFFF, 0x0);
			pr_notice("[store_pmic_access] read PMU reg 0x%x with value 0x%x !\n",
				  reg_address, g_reg_value);
			pr_notice
			    ("[store_pmic_access] Please use \"cat pmic_access\" to get value\r\n");
		}
	}
	return size;
}

static DEVICE_ATTR(pmic_access, 0664, show_pmic_access, store_pmic_access);	/* 664 */

void PMIC_INIT_SETTING_V1(void)
{
	unsigned int chip_version = 0;
	unsigned int ret = 0;

	chip_version = upmu_get_reg_value(0x100);

	/* put init setting from DE/SA */
	if (chip_version >= PMIC6323_E2_CID_CODE) {
		pr_notice("[Kernel_PMIC_INIT_SETTING_V1] PMIC Chip = %x\n", chip_version);

		ret = pmic_config_interface(0x2, 0xB, 0xF, 4); /* [7:4]: RG_VCDT_HV_VTH; 7V OVP */
		ret = pmic_config_interface(0xC, 0x1, 0x7, 1); /* [3:1]: RG_VBAT_OV_VTH; VBAT_OV=4.3V */
		ret = pmic_config_interface(0x1A, 0x3, 0xF, 0); /* [3:0]: RG_CHRWDT_TD; align to 6250's */
		ret = pmic_config_interface(0x24, 0x1, 0x1, 1); /* [1:1]: RG_BC11_RST; Reset BC11 detection */
		ret = pmic_config_interface(0x2A, 0x0, 0x7, 4); /* [6:4]: RG_CSDAC_STP; align to 6250's setting */
		ret = pmic_config_interface(0x2E, 0x1, 0x1, 2); /* [2:2]: RG_CSDAC_MODE; */
		ret = pmic_config_interface(0x2E, 0x1, 0x1, 6); /* [6:6]: RG_HWCV_EN; */
		ret = pmic_config_interface(0x2E, 0x1, 0x1, 7); /* [7:7]: RG_ULC_DET_EN; */
		ret = pmic_config_interface(0x3C, 0x1, 0x1, 5); /* [5:5]: THR_HWPDN_EN; */
		ret = pmic_config_interface(0x40, 0x1, 0x1, 4); /* [4:4]: RG_EN_DRVSEL; */
		ret = pmic_config_interface(0x40, 0x1, 0x1, 5); /* [5:5]: RG_RST_DRVSEL; */
		ret = pmic_config_interface(0x46, 0x1, 0x1, 1); /* [1:1]: PWRBB_DEB_EN; */
		ret = pmic_config_interface(0x48, 0x1, 0x1, 8); /* [8:8]: VPROC_PG_H2L_EN; */
		ret = pmic_config_interface(0x48, 0x1, 0x1, 9); /* [9:9]: VSYS_PG_H2L_EN; */
		ret = pmic_config_interface(0x4E, 0x1, 0x1, 5); /* [5:5]: STRUP_AUXADC_RSTB_SW; */
		ret = pmic_config_interface(0x4E, 0x1, 0x1, 7); /* [7:7]: STRUP_AUXADC_RSTB_SEL; */
		ret = pmic_config_interface(0x50, 0x1, 0x1, 0); /* [0:0]: STRUP_PWROFF_SEQ_EN; */
		ret = pmic_config_interface(0x50, 0x1, 0x1, 1); /* [1:1]: STRUP_PWROFF_PREOFF_EN; */
		ret = pmic_config_interface(0x52, 0x1, 0x1, 9); /* [9:9]: SPK_THER_SHDN_L_EN; */
		ret = pmic_config_interface(0x56, 0x1, 0x1, 0); /* [0:0]: RG_SPK_INTG_RST_L; */
		ret = pmic_config_interface(0x64, 0x1, 0xF, 8); /* [11:8]: RG_SPKPGA_GAIN; */
		ret = pmic_config_interface(0x102, 0x1, 0x1, 6); /* [6:6]: RG_RTC_75K_CK_PDN; */
		ret = pmic_config_interface(0x102, 0x0, 0x1, 11); /* [11:11]: RG_DRV_32K_CK_PDN; */
		ret = pmic_config_interface(0x102, 0x1, 0x1, 15); /* [15:15]: RG_BUCK32K_PDN; */
		ret = pmic_config_interface(0x108, 0x1, 0x1, 12); /* [12:12]: RG_EFUSE_CK_PDN; */
		ret = pmic_config_interface(0x10E, 0x1, 0x1, 5); /* [5:5]: RG_AUXADC_CTL_CK_PDN; */
		ret = pmic_config_interface(0x120, 0x1, 0x1, 4); /* [4:4]: RG_SRCLKEN_HW_MODE; */
		ret = pmic_config_interface(0x120, 0x1, 0x1, 5); /* [5:5]: RG_OSC_HW_MODE; */
		ret = pmic_config_interface(0x148, 0x1, 0x1, 1); /* [1:1]: RG_SMT_INT; */
		ret = pmic_config_interface(0x148, 0x1, 0x1, 3); /* [3:3]: RG_SMT_RTC_32K1V8; */
		ret = pmic_config_interface(0x160, 0x1, 0x1, 2); /* [2:2]: RG_INT_EN_BAT_L; */
		ret = pmic_config_interface(0x160, 0x1, 0x1, 6); /* [6:6]: RG_INT_EN_THR_L; */
		ret = pmic_config_interface(0x160, 0x1, 0x1, 7); /* [7:7]: RG_INT_EN_THR_H; */
		ret = pmic_config_interface(0x166, 0x1, 0x1, 1); /* [1:1]: RG_INT_EN_FCHRKEY; */
		ret = pmic_config_interface(0x212, 0x2, 0x3, 4); /* [5:4]: QI_VPROC_VSLEEP; */
		ret = pmic_config_interface(0x216, 0x1, 0x1, 1); /* [1:1]: VPROC_VOSEL_CTRL; */
		ret = pmic_config_interface(0x21C, 0x17, 0x7F, 0); /* [6:0]: VPROC_SFCHG_FRATE; */
		ret = pmic_config_interface(0x21C, 0x1, 0x1, 7); /* [7:7]: VPROC_SFCHG_FEN; */
		ret = pmic_config_interface(0x21C, 0x1, 0x1, 15); /* [15:15]: VPROC_SFCHG_REN; */
		ret = pmic_config_interface(0x222, 0x18, 0x7F, 0); /* [6:0]: VPROC_VOSEL_SLEEP; */
		ret = pmic_config_interface(0x230, 0x3, 0x3, 0); /* [1:0]: VPROC_TRANSTD; */
		ret = pmic_config_interface(0x230, 0x1, 0x1, 8); /* [8:8]: VPROC_VSLEEP_EN; */
		ret = pmic_config_interface(0x238, 0x3, 0x3, 0); /* [1:0]: RG_VSYS_SLP; */
		ret = pmic_config_interface(0x238, 0x2, 0x3, 4); /* [5:4]: QI_VSYS_VSLEEP; */
		ret = pmic_config_interface(0x23C, 0x1, 0x1, 1); /* [1:1]: VSYS_VOSEL_CTRL; after 0x0256 */
		ret = pmic_config_interface(0x242, 0x23, 0x7F, 0); /* [6:0]: VSYS_SFCHG_FRATE; */
		ret = pmic_config_interface(0x242, 0x11, 0x7F, 8); /* [14:8]: VSYS_SFCHG_RRATE; */
		ret = pmic_config_interface(0x242, 0x1, 0x1, 15); /* [15:15]: VSYS_SFCHG_REN; */
		ret = pmic_config_interface(0x256, 0x3, 0x3, 0); /* [1:0]: VSYS_TRANSTD; */
		ret = pmic_config_interface(0x256, 0x1, 0x3, 4); /* [5:4]: VSYS_VOSEL_TRANS_EN; */
		ret = pmic_config_interface(0x256, 0x1, 0x1, 8); /* [8:8]: VSYS_VSLEEP_EN; */
		ret = pmic_config_interface(0x302, 0x2, 0x3, 8); /* [9:8]: RG_VPA_CSL; OC limit */
		ret = pmic_config_interface(0x302, 0x1, 0x3, 14); /* [15:14]: RG_VPA_ZX_OS; ZX limit */
		ret = pmic_config_interface(0x310, 0x1, 0x1, 7); /* [7:7]: VPA_SFCHG_FEN; */
		ret = pmic_config_interface(0x310, 0x1, 0x1, 15); /* [15:15]: VPA_SFCHG_REN; */
		ret = pmic_config_interface(0x326, 0x1, 0x1, 0); /* [0:0]: VPA_DLC_MAP_EN; */
		ret = pmic_config_interface(0x402, 0x1, 0x1, 0); /* [0:0]: VTCXO_LP_SEL; */
		ret = pmic_config_interface(0x402, 0x0, 0x1, 11); /* [11:11]: VTCXO_ON_CTRL; */
		ret = pmic_config_interface(0x404, 0x1, 0x1, 0); /* [0:0]: VA_LP_SEL; */
		ret = pmic_config_interface(0x404, 0x2, 0x3, 8); /* [9:8]: RG_VA_SENSE_SEL; */
		ret = pmic_config_interface(0x500, 0x1, 0x1, 0); /* [0:0]: VIO28_LP_SEL; */
		ret = pmic_config_interface(0x502, 0x1, 0x1, 0); /* [0:0]: VUSB_LP_SEL; */
		ret = pmic_config_interface(0x504, 0x1, 0x1, 0); /* [0:0]: VMC_LP_SEL; */
		ret = pmic_config_interface(0x506, 0x1, 0x1, 0); /* [0:0]: VMCH_LP_SEL; */
		ret = pmic_config_interface(0x508, 0x1, 0x1, 0); /* [0:0]: VEMC_3V3_LP_SEL; */
		ret = pmic_config_interface(0x514, 0x1, 0x1, 0); /* [0:0]: RG_STB_SIM1_SIO; */
		ret = pmic_config_interface(0x516, 0x1, 0x1, 0); /* [0:0]: VSIM1_LP_SEL; */
		ret = pmic_config_interface(0x518, 0x1, 0x1, 0); /* [0:0]: VSIM2_LP_SEL; */
		ret = pmic_config_interface(0x524, 0x1, 0x1, 0); /* [0:0]: RG_STB_SIM2_SIO; */
		ret = pmic_config_interface(0x542, 0x1, 0x1, 2); /* [2:2]: VIBR_THER_SHEN_EN; */
		ret = pmic_config_interface(0x54E, 0x0, 0x1, 15); /* [15:15]: RG_VRF18_EN; */
		ret = pmic_config_interface(0x550, 0x0, 0x1, 1); /* [1:1]: VRF18_ON_CTRL; */
		ret = pmic_config_interface(0x552, 0x1, 0x1, 0); /* [0:0]: VM_LP_SEL; */
		ret = pmic_config_interface(0x552, 0x1, 0x1, 14); /* [14:14]: RG_VM_EN; */
		ret = pmic_config_interface(0x556, 0x1, 0x1, 0); /* [0:0]: VIO18_LP_SEL; */
		ret = pmic_config_interface(0x756, 0x1, 0x3, 2); /* [3:2]: RG_ADC_TRIM_CH6_SEL; */
		ret = pmic_config_interface(0x756, 0x1, 0x3, 4); /* [5:4]: RG_ADC_TRIM_CH5_SEL; */
		ret = pmic_config_interface(0x756, 0x1, 0x3, 8); /* [9:8]: RG_ADC_TRIM_CH3_SEL; */
		ret = pmic_config_interface(0x756, 0x1, 0x3, 10); /* [11:10]: RG_ADC_TRIM_CH2_SEL; */
		ret = pmic_config_interface(0x778, 0x1, 0x1, 15); /* [15:15]: RG_VREF18_ENB_MD; */
	} else if (chip_version >= PMIC6323_E1_CID_CODE) {
		pr_notice("[Kernel_PMIC_INIT_SETTING_V1] PMIC Chip = %x\n", chip_version);

		ret = pmic_config_interface(0x2, 0xB, 0xF, 4); /* [7:4]: RG_VCDT_HV_VTH; 7V OVP */
		ret = pmic_config_interface(0xC, 0x1, 0x7, 1); /* [3:1]: RG_VBAT_OV_VTH; VBAT_OV=4.3V */
		ret = pmic_config_interface(0x1A, 0x3, 0xF, 0); /* [3:0]: RG_CHRWDT_TD; align to 6250's */
		ret = pmic_config_interface(0x24, 0x1, 0x1, 1); /* [1:1]: RG_BC11_RST; Reset BC11 detection */
		ret = pmic_config_interface(0x2A, 0x0, 0x7, 4); /* [6:4]: RG_CSDAC_STP; align to 6250's setting */
		ret = pmic_config_interface(0x2E, 0x1, 0x1, 2); /* [2:2]: RG_CSDAC_MODE; */
		ret = pmic_config_interface(0x2E, 0x1, 0x1, 6); /* [6:6]: RG_HWCV_EN; */
		ret = pmic_config_interface(0x2E, 0x1, 0x1, 7); /* [7:7]: RG_ULC_DET_EN; */
		ret = pmic_config_interface(0x3C, 0x1, 0x1, 5); /* [5:5]: THR_HWPDN_EN; */
		ret = pmic_config_interface(0x40, 0x1, 0x1, 4); /* [4:4]: RG_EN_DRVSEL; */
		ret = pmic_config_interface(0x40, 0x1, 0x1, 5); /* [5:5]: RG_RST_DRVSEL; */
		ret = pmic_config_interface(0x46, 0x1, 0x1, 1); /* [1:1]: PWRBB_DEB_EN; */
		ret = pmic_config_interface(0x48, 0x1, 0x1, 8); /* [8:8]: VPROC_PG_H2L_EN; */
		ret = pmic_config_interface(0x48, 0x1, 0x1, 9); /* [9:9]: VSYS_PG_H2L_EN; */
		ret = pmic_config_interface(0x4E, 0x1, 0x1, 5); /* [5:5]: STRUP_AUXADC_RSTB_SW; */
		ret = pmic_config_interface(0x4E, 0x1, 0x1, 7); /* [7:7]: STRUP_AUXADC_RSTB_SEL; */
		ret = pmic_config_interface(0x50, 0x1, 0x1, 0); /* [0:0]: STRUP_PWROFF_SEQ_EN; */
		ret = pmic_config_interface(0x50, 0x1, 0x1, 1); /* [1:1]: STRUP_PWROFF_PREOFF_EN; */
		ret = pmic_config_interface(0x52, 0x1, 0x1, 9); /* [9:9]: SPK_THER_SHDN_L_EN; */
		ret = pmic_config_interface(0x56, 0x1, 0x1, 0); /* [0:0]: RG_SPK_INTG_RST_L; */
		ret = pmic_config_interface(0x64, 0x1, 0xF, 8); /* [11:8]: RG_SPKPGA_GAIN; */
		ret = pmic_config_interface(0x102, 0x1, 0x1, 6); /* [6:6]: RG_RTC_75K_CK_PDN; */
		ret = pmic_config_interface(0x102, 0x0, 0x1, 11); /* [11:11]: RG_DRV_32K_CK_PDN; */
		ret = pmic_config_interface(0x102, 0x1, 0x1, 15); /* [15:15]: RG_BUCK32K_PDN; */
		ret = pmic_config_interface(0x108, 0x1, 0x1, 12); /* [12:12]: RG_EFUSE_CK_PDN; */
		ret = pmic_config_interface(0x10E, 0x1, 0x1, 5); /* [5:5]: RG_AUXADC_CTL_CK_PDN; */
		ret = pmic_config_interface(0x120, 0x1, 0x1, 4); /* [4:4]: RG_SRCLKEN_HW_MODE; */
		ret = pmic_config_interface(0x120, 0x1, 0x1, 5); /* [5:5]: RG_OSC_HW_MODE; */
		ret = pmic_config_interface(0x148, 0x1, 0x1, 1); /* [1:1]: RG_SMT_INT; */
		ret = pmic_config_interface(0x148, 0x1, 0x1, 3); /* [3:3]: RG_SMT_RTC_32K1V8; */
		ret = pmic_config_interface(0x160, 0x1, 0x1, 2); /* [2:2]: RG_INT_EN_BAT_L; */
		ret = pmic_config_interface(0x160, 0x1, 0x1, 6); /* [6:6]: RG_INT_EN_THR_L; */
		ret = pmic_config_interface(0x160, 0x1, 0x1, 7); /* [7:7]: RG_INT_EN_THR_H; */
		ret = pmic_config_interface(0x166, 0x1, 0x1, 1); /* [1:1]: RG_INT_EN_FCHRKEY; */
		ret = pmic_config_interface(0x212, 0x2, 0x3, 4); /* [5:4]: QI_VPROC_VSLEEP; */
		ret = pmic_config_interface(0x216, 0x1, 0x1, 1); /* [1:1]: VPROC_VOSEL_CTRL; */
		ret = pmic_config_interface(0x21C, 0x17, 0x7F, 0); /* [6:0]: VPROC_SFCHG_FRATE; */
		ret = pmic_config_interface(0x21C, 0x1, 0x1, 7); /* [7:7]: VPROC_SFCHG_FEN; */
		ret = pmic_config_interface(0x21C, 0x1, 0x1, 15); /* [15:15]: VPROC_SFCHG_REN; */
		ret = pmic_config_interface(0x222, 0x18, 0x7F, 0); /* [6:0]: VPROC_VOSEL_SLEEP; */
		ret = pmic_config_interface(0x230, 0x3, 0x3, 0); /* [1:0]: VPROC_TRANSTD; */
		ret = pmic_config_interface(0x230, 0x1, 0x1, 8); /* [8:8]: VPROC_VSLEEP_EN; */
		ret = pmic_config_interface(0x238, 0x3, 0x3, 0); /* [1:0]: RG_VSYS_SLP; */
		ret = pmic_config_interface(0x238, 0x2, 0x3, 4); /* [5:4]: QI_VSYS_VSLEEP; */
		ret = pmic_config_interface(0x23C, 0x1, 0x1, 1); /* [1:1]: VSYS_VOSEL_CTRL; after 0x0256 */
		ret = pmic_config_interface(0x242, 0x23, 0x7F, 0); /* [6:0]: VSYS_SFCHG_FRATE; */
		ret = pmic_config_interface(0x242, 0x11, 0x7F, 8); /* [14:8]: VSYS_SFCHG_RRATE; */
		ret = pmic_config_interface(0x242, 0x1, 0x1, 15); /* [15:15]: VSYS_SFCHG_REN; */
		ret = pmic_config_interface(0x256, 0x3, 0x3, 0); /* [1:0]: VSYS_TRANSTD; */
		ret = pmic_config_interface(0x256, 0x1, 0x3, 4); /* [5:4]: VSYS_VOSEL_TRANS_EN; */
		ret = pmic_config_interface(0x256, 0x1, 0x1, 8); /* [8:8]: VSYS_VSLEEP_EN; */
		ret = pmic_config_interface(0x302, 0x2, 0x3, 8); /* [9:8]: RG_VPA_CSL; OC limit */
		ret = pmic_config_interface(0x302, 0x1, 0x3, 14); /* [15:14]: RG_VPA_ZX_OS; ZX limit */
		ret = pmic_config_interface(0x310, 0x1, 0x1, 7); /* [7:7]: VPA_SFCHG_FEN; */
		ret = pmic_config_interface(0x310, 0x1, 0x1, 15); /* [15:15]: VPA_SFCHG_REN; */
		ret = pmic_config_interface(0x326, 0x1, 0x1, 0); /* [0:0]: VPA_DLC_MAP_EN; */
		ret = pmic_config_interface(0x402, 0x1, 0x1, 0); /* [0:0]: VTCXO_LP_SEL; */
		ret = pmic_config_interface(0x402, 0x0, 0x1, 11); /* [11:11]: VTCXO_ON_CTRL; */
		ret = pmic_config_interface(0x404, 0x1, 0x1, 0); /* [0:0]: VA_LP_SEL; */
		ret = pmic_config_interface(0x404, 0x2, 0x3, 8); /* [9:8]: RG_VA_SENSE_SEL; */
		ret = pmic_config_interface(0x500, 0x1, 0x1, 0); /* [0:0]: VIO28_LP_SEL; */
		ret = pmic_config_interface(0x502, 0x1, 0x1, 0); /* [0:0]: VUSB_LP_SEL; */
		ret = pmic_config_interface(0x504, 0x1, 0x1, 0); /* [0:0]: VMC_LP_SEL; */
		ret = pmic_config_interface(0x506, 0x1, 0x1, 0); /* [0:0]: VMCH_LP_SEL; */
		ret = pmic_config_interface(0x508, 0x1, 0x1, 0); /* [0:0]: VEMC_3V3_LP_SEL; */
		ret = pmic_config_interface(0x514, 0x1, 0x1, 0); /* [0:0]: RG_STB_SIM1_SIO; */
		ret = pmic_config_interface(0x516, 0x1, 0x1, 0); /* [0:0]: VSIM1_LP_SEL; */
		ret = pmic_config_interface(0x518, 0x1, 0x1, 0); /* [0:0]: VSIM2_LP_SEL; */
		ret = pmic_config_interface(0x524, 0x1, 0x1, 0); /* [0:0]: RG_STB_SIM2_SIO; */
		ret = pmic_config_interface(0x542, 0x1, 0x1, 2); /* [2:2]: VIBR_THER_SHEN_EN; */
		ret = pmic_config_interface(0x54E, 0x0, 0x1, 15); /* [15:15]: RG_VRF18_EN; */
		ret = pmic_config_interface(0x550, 0x0, 0x1, 1); /* [1:1]: VRF18_ON_CTRL; */
		ret = pmic_config_interface(0x552, 0x1, 0x1, 0); /* [0:0]: VM_LP_SEL; */
		ret = pmic_config_interface(0x552, 0x1, 0x1, 14); /* [14:14]: RG_VM_EN; */
		ret = pmic_config_interface(0x556, 0x1, 0x1, 0); /* [0:0]: VIO18_LP_SEL; */
		ret = pmic_config_interface(0x756, 0x1, 0x3, 2); /* [3:2]: RG_ADC_TRIM_CH6_SEL; */
		ret = pmic_config_interface(0x756, 0x1, 0x3, 4); /* [5:4]: RG_ADC_TRIM_CH5_SEL; */
		ret = pmic_config_interface(0x756, 0x1, 0x3, 8); /* [9:8]: RG_ADC_TRIM_CH3_SEL; */
		ret = pmic_config_interface(0x756, 0x1, 0x3, 10); /* [11:10]: RG_ADC_TRIM_CH2_SEL; */
		ret = pmic_config_interface(0x778, 0x1, 0x1, 15); /* [15:15]: RG_VREF18_ENB_MD; */
	} else
		pr_notice("[Kernel_PMIC_INIT_SETTING_V1] Unknown PMIC Chip (%x)\n", chip_version);
}

static int mt6323_pmic_probe(struct platform_device *dev)
{
	int ret_val = 0;
	struct mt6397_chip *mt6323_chip = dev_get_drvdata(dev->dev.parent);

	pr_debug("[Power/PMIC] ******** mt6323 pmic driver probe!! ********\n");

	pwrap_regmap = mt6323_chip->regmap;

	/* get PMIC CID */
	ret_val = upmu_get_reg_value(0x100);
	pr_notice("[Power/PMIC] mt6323 PMIC CID=0x%x\n", ret_val);

	/* pmic initial setting */
	PMIC_INIT_SETTING_V1();

	device_create_file(&(dev->dev), &dev_attr_pmic_access);
	return 0;
}

static const struct platform_device_id mt6323_pmic_ids[] = {
	{"mt6323-pmic", 0},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(platform, mt6323_pmic_ids);

static const struct of_device_id mt6323_pmic_of_match[] = {
	{ .compatible = "mediatek,mt6323-pmic", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, mt6323_pmic_of_match);

static struct platform_driver mt6323_pmic_driver = {
	.driver = {
		.name = "mt6323-pmic",
		.of_match_table = of_match_ptr(mt6323_pmic_of_match),
	},
	.probe = mt6323_pmic_probe,
	.id_table = mt6323_pmic_ids,
};

module_platform_driver(mt6323_pmic_driver);

MODULE_AUTHOR("Chen Zhong <chen.zhong@mediatek.com>");
MODULE_DESCRIPTION("PMIC Misc Setting Driver for MediaTek MT6323 PMIC");
MODULE_LICENSE("GPL v2");
