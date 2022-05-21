/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/regmap.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <dt-bindings/pinctrl/mt65xx.h>
#include "pinctrl-mtk-common.h"
#include "pinctrl-mtk-mt8518.h"

static const struct mtk_pin_spec_pupd_set_samereg mt8518_spec_pupd[] = {
	MTK_PIN_PUPD_SPEC_SR(0, 0xE60, 6, 5, 4),
	MTK_PIN_PUPD_SPEC_SR(1, 0xE60, 10, 9, 8),
	MTK_PIN_PUPD_SPEC_SR(2, 0xE60, 14, 13, 12),
	MTK_PIN_PUPD_SPEC_SR(3, 0xE10, 14, 13, 12),
	MTK_PIN_PUPD_SPEC_SR(4, 0xE10, 10, 9, 8),
	MTK_PIN_PUPD_SPEC_SR(5, 0xE10, 6, 5, 4),
	MTK_PIN_PUPD_SPEC_SR(6, 0xE10, 2, 1, 0),
	MTK_PIN_PUPD_SPEC_SR(7, 0xE20, 10, 9, 8),
	MTK_PIN_PUPD_SPEC_SR(8, 0xE20, 2, 1, 0),
	MTK_PIN_PUPD_SPEC_SR(9, 0xE20, 6, 5, 4),
	MTK_PIN_PUPD_SPEC_SR(10, 0xE00, 14, 13, 12),
	MTK_PIN_PUPD_SPEC_SR(11, 0xE00, 10, 9, 8),
	MTK_PIN_PUPD_SPEC_SR(12, 0xE00, 6, 5, 4),
	MTK_PIN_PUPD_SPEC_SR(13, 0xE00, 2, 1, 0),

	MTK_PIN_PUPD_SPEC_SR(29, 0xE50, 10, 9, 8),
	MTK_PIN_PUPD_SPEC_SR(30, 0xE50, 6, 5, 4),
	MTK_PIN_PUPD_SPEC_SR(31, 0xE40, 6, 5, 4),
	MTK_PIN_PUPD_SPEC_SR(32, 0xE40, 10, 9, 8),
	MTK_PIN_PUPD_SPEC_SR(33, 0xE40, 14, 13, 12),
	MTK_PIN_PUPD_SPEC_SR(34, 0xE50, 2, 1, 0),
	MTK_PIN_PUPD_SPEC_SR(35, 0xE50, 14, 13, 12),

	MTK_PIN_PUPD_SPEC_SR(114, 0xE40, 2, 1, 0),
	MTK_PIN_PUPD_SPEC_SR(115, 0xE30, 14, 13, 12),
	MTK_PIN_PUPD_SPEC_SR(116, 0xE20, 14, 13, 12),
	MTK_PIN_PUPD_SPEC_SR(117, 0xE30, 2, 1, 0),
	MTK_PIN_PUPD_SPEC_SR(118, 0xE30, 6, 5, 4),
	MTK_PIN_PUPD_SPEC_SR(119, 0xE30, 10, 9, 8)
};

static int mt8518_spec_pull_set(struct regmap *regmap, unsigned int pin,
		unsigned char align, bool isup, unsigned int r1r0)
{
	return mtk_pctrl_spec_pull_set_samereg(regmap, mt8518_spec_pupd,
		ARRAY_SIZE(mt8518_spec_pupd), pin, align, isup, r1r0);
}

#if defined(CONFIG_PINCTRL_MTK_NO_UPSTREAM)
static int mt8518_spec_pull_get(struct regmap *regmap, unsigned int pin)
{
	return mtk_spec_pull_get_samereg(regmap, mt8518_spec_pupd,
		ARRAY_SIZE(mt8518_spec_pupd), pin);
}
#endif

static const struct mtk_pin_ies_smt_set mt8518_smt_set[] = {
	MTK_PIN_IES_SMT_SPEC(0, 2, 0xA00, 0),
	MTK_PIN_IES_SMT_SPEC(3, 3, 0xA20, 9),
	MTK_PIN_IES_SMT_SPEC(4, 4, 0xA20, 8),
	MTK_PIN_IES_SMT_SPEC(5, 5, 0xA20, 7),
	MTK_PIN_IES_SMT_SPEC(6, 6, 0xA20, 6),
	MTK_PIN_IES_SMT_SPEC(7, 7, 0xA20, 10),
	MTK_PIN_IES_SMT_SPEC(8, 8, 0xA20, 1),
	MTK_PIN_IES_SMT_SPEC(9, 9, 0xA20, 0),
	MTK_PIN_IES_SMT_SPEC(10, 10, 0xA20, 5),
	MTK_PIN_IES_SMT_SPEC(11, 11, 0xA20, 4),
	MTK_PIN_IES_SMT_SPEC(12, 12, 0xA20, 3),
	MTK_PIN_IES_SMT_SPEC(13, 13, 0xA20, 2),
	MTK_PIN_IES_SMT_SPEC(14, 14, 0xA00, 1),
	MTK_PIN_IES_SMT_SPEC(15, 15, 0xA00, 2),
	MTK_PIN_IES_SMT_SPEC(16, 16, 0xA00, 3),
	MTK_PIN_IES_SMT_SPEC(17, 20, 0xA00, 4),
	MTK_PIN_IES_SMT_SPEC(21, 22, 0xA00, 5),
	MTK_PIN_IES_SMT_SPEC(23, 27, 0xA10, 15),
	MTK_PIN_IES_SMT_SPEC(28, 28, 0xA00, 6),
	MTK_PIN_IES_SMT_SPEC(29, 29, 0xA30, 2),
	MTK_PIN_IES_SMT_SPEC(30, 30, 0xA30, 1),
	MTK_PIN_IES_SMT_SPEC(31, 31, 0xA30, 6),
	MTK_PIN_IES_SMT_SPEC(32, 32, 0xA30, 5),
	MTK_PIN_IES_SMT_SPEC(33, 33, 0xA30, 4),
	MTK_PIN_IES_SMT_SPEC(34, 35, 0xA30, 3),
	MTK_PIN_IES_SMT_SPEC(36, 39, 0xA00, 7),
	MTK_PIN_IES_SMT_SPEC(40, 41, 0xA00, 8),
	MTK_PIN_IES_SMT_SPEC(42, 44, 0xA00, 9),
	MTK_PIN_IES_SMT_SPEC(45, 47, 0xA00, 10),
	MTK_PIN_IES_SMT_SPEC(48, 51, 0xA00, 11),
	MTK_PIN_IES_SMT_SPEC(52, 55, 0xA00, 12),
	MTK_PIN_IES_SMT_SPEC(56, 56, 0xA00, 13),
	MTK_PIN_IES_SMT_SPEC(57, 57, 0xA00, 14),
	MTK_PIN_IES_SMT_SPEC(58, 58, 0xA00, 15),
	MTK_PIN_IES_SMT_SPEC(59, 60, 0xA10, 0),
	MTK_PIN_IES_SMT_SPEC(61, 61, 0xA10, 1),
	MTK_PIN_IES_SMT_SPEC(62, 62, 0xA10, 2),
	MTK_PIN_IES_SMT_SPEC(63, 69, 0xA10, 3),
	MTK_PIN_IES_SMT_SPEC(70, 70, 0xA10, 4),
	MTK_PIN_IES_SMT_SPEC(71, 76, 0xA10, 5),
	MTK_PIN_IES_SMT_SPEC(77, 80, 0xA10, 6),
	MTK_PIN_IES_SMT_SPEC(81, 87, 0xA10, 7),
	MTK_PIN_IES_SMT_SPEC(88, 97, 0xA10, 8),
	MTK_PIN_IES_SMT_SPEC(98, 103, 0xA10, 9),
	MTK_PIN_IES_SMT_SPEC(104, 107, 0xA10, 10),
	MTK_PIN_IES_SMT_SPEC(108, 109, 0xA10, 11),
	MTK_PIN_IES_SMT_SPEC(110, 111, 0xA10, 12),
	MTK_PIN_IES_SMT_SPEC(112, 113, 0xA10, 13),
	MTK_PIN_IES_SMT_SPEC(114, 114, 0xA20, 12),
	MTK_PIN_IES_SMT_SPEC(115, 115, 0xA20, 11),
	MTK_PIN_IES_SMT_SPEC(116, 116, 0xA30, 0),
	MTK_PIN_IES_SMT_SPEC(117, 117, 0xA20, 15),
	MTK_PIN_IES_SMT_SPEC(118, 118, 0xA20, 14),
	MTK_PIN_IES_SMT_SPEC(119, 119, 0xA20, 13)
};

static const struct mtk_pin_ies_smt_set mt8518_ies_set[] = {
	MTK_PIN_IES_SMT_SPEC(0, 2, 0x900, 0),
	MTK_PIN_IES_SMT_SPEC(3, 3, 0x920, 9),
	MTK_PIN_IES_SMT_SPEC(4, 4, 0x920, 8),
	MTK_PIN_IES_SMT_SPEC(5, 5, 0x920, 7),
	MTK_PIN_IES_SMT_SPEC(6, 6, 0x920, 6),
	MTK_PIN_IES_SMT_SPEC(7, 7, 0x920, 10),
	MTK_PIN_IES_SMT_SPEC(8, 8, 0x920, 1),
	MTK_PIN_IES_SMT_SPEC(9, 9, 0x920, 0),
	MTK_PIN_IES_SMT_SPEC(10, 10, 0x920, 5),
	MTK_PIN_IES_SMT_SPEC(11, 11, 0x920, 4),
	MTK_PIN_IES_SMT_SPEC(12, 12, 0x920, 3),
	MTK_PIN_IES_SMT_SPEC(13, 13, 0x920, 2),
	MTK_PIN_IES_SMT_SPEC(14, 14, 0x900, 1),
	MTK_PIN_IES_SMT_SPEC(15, 15, 0x900, 2),
	MTK_PIN_IES_SMT_SPEC(16, 16, 0x900, 3),
	MTK_PIN_IES_SMT_SPEC(17, 20, 0x900, 4),
	MTK_PIN_IES_SMT_SPEC(21, 22, 0x900, 5),
	MTK_PIN_IES_SMT_SPEC(23, 27, 0x910, 15),
	MTK_PIN_IES_SMT_SPEC(28, 28, 0x900, 6),
	MTK_PIN_IES_SMT_SPEC(29, 29, 0x930, 2),
	MTK_PIN_IES_SMT_SPEC(30, 30, 0x930, 1),
	MTK_PIN_IES_SMT_SPEC(31, 31, 0x930, 6),
	MTK_PIN_IES_SMT_SPEC(32, 32, 0x930, 5),
	MTK_PIN_IES_SMT_SPEC(33, 33, 0x930, 4),
	MTK_PIN_IES_SMT_SPEC(34, 35, 0x930, 3),
	MTK_PIN_IES_SMT_SPEC(36, 39, 0x900, 7),
	MTK_PIN_IES_SMT_SPEC(40, 41, 0x900, 8),
	MTK_PIN_IES_SMT_SPEC(42, 44, 0x900, 9),
	MTK_PIN_IES_SMT_SPEC(45, 47, 0x900, 10),
	MTK_PIN_IES_SMT_SPEC(48, 51, 0x900, 11),
	MTK_PIN_IES_SMT_SPEC(52, 55, 0x900, 12),
	MTK_PIN_IES_SMT_SPEC(56, 56, 0x900, 13),
	MTK_PIN_IES_SMT_SPEC(57, 57, 0x900, 14),
	MTK_PIN_IES_SMT_SPEC(58, 58, 0x900, 15),
	MTK_PIN_IES_SMT_SPEC(59, 60, 0x910, 0),
	MTK_PIN_IES_SMT_SPEC(61, 61, 0x910, 1),
	MTK_PIN_IES_SMT_SPEC(62, 62, 0x910, 2),
	MTK_PIN_IES_SMT_SPEC(63, 69, 0x910, 3),
	MTK_PIN_IES_SMT_SPEC(70, 70, 0x910, 4),
	MTK_PIN_IES_SMT_SPEC(71, 76, 0x910, 5),
	MTK_PIN_IES_SMT_SPEC(77, 80, 0x910, 6),
	MTK_PIN_IES_SMT_SPEC(81, 87, 0x910, 7),
	MTK_PIN_IES_SMT_SPEC(88, 97, 0x910, 8),
	MTK_PIN_IES_SMT_SPEC(98, 103, 0x910, 9),
	MTK_PIN_IES_SMT_SPEC(104, 107, 0x910, 10),
	MTK_PIN_IES_SMT_SPEC(108, 109, 0x910, 11),
	MTK_PIN_IES_SMT_SPEC(110, 111, 0x910, 12),
	MTK_PIN_IES_SMT_SPEC(112, 113, 0x910, 13),
	MTK_PIN_IES_SMT_SPEC(114, 114, 0x920, 12),
	MTK_PIN_IES_SMT_SPEC(115, 115, 0x920, 11),
	MTK_PIN_IES_SMT_SPEC(116, 116, 0x930, 0),
	MTK_PIN_IES_SMT_SPEC(117, 117, 0x920, 15),
	MTK_PIN_IES_SMT_SPEC(118, 118, 0x920, 14),
	MTK_PIN_IES_SMT_SPEC(119, 119, 0x920, 13)
};

static int mt8518_ies_smt_set(struct regmap *regmap, unsigned int pin,
		unsigned char align, int value, enum pin_config_param arg)
{
	if (arg == PIN_CONFIG_INPUT_ENABLE)
		return mtk_pconf_spec_set_ies_smt_range(regmap, mt8518_ies_set,
			ARRAY_SIZE(mt8518_ies_set), pin, align, value);
	if (arg == PIN_CONFIG_INPUT_SCHMITT_ENABLE)
		return mtk_pconf_spec_set_ies_smt_range(regmap, mt8518_smt_set,
			ARRAY_SIZE(mt8518_smt_set), pin, align, value);
	return -EINVAL;
}

#if defined(CONFIG_PINCTRL_MTK_NO_UPSTREAM)
static int mt8518_spec_ies_get(struct regmap *regmap, unsigned int pin)
{
	return mtk_spec_get_ies_smt_range(regmap, mt8518_ies_set,
		ARRAY_SIZE(mt8518_ies_set), pin);
}

static int mt8518_spec_smt_get(struct regmap *regmap, unsigned int pin)
{
	return mtk_spec_get_ies_smt_range(regmap, mt8518_smt_set,
		ARRAY_SIZE(mt8518_smt_set), pin);
}
#endif

static const struct mtk_drv_group_desc mt8518_drv_grp[] =  {
	/* 0E4E8SR 4/8/12/16 */
	MTK_DRV_GRP(4, 16, 1, 2, 4),
	/* 0E2E4SR  2/4/6/8 */
	MTK_DRV_GRP(2, 8, 1, 2, 2),
	/* E8E4E2  2/4/6/8/10/12/14/16 */
	MTK_DRV_GRP(2, 16, 0, 2, 2)
};

static const struct mtk_pin_drv_grp mt8518_pin_drv[] = {
	MTK_PIN_DRV_GRP(0, 0xd70, 8, 2),
	MTK_PIN_DRV_GRP(1, 0xd70, 8, 2),
	MTK_PIN_DRV_GRP(2, 0xd70, 8, 2),
	MTK_PIN_DRV_GRP(3, 0xd70, 0, 2),
	MTK_PIN_DRV_GRP(4, 0xd70, 0, 2),
	MTK_PIN_DRV_GRP(5, 0xd70, 0, 2),
	MTK_PIN_DRV_GRP(6, 0xd70, 0, 2),
	MTK_PIN_DRV_GRP(7, 0xd70, 4, 2),
	MTK_PIN_DRV_GRP(8, 0xd60, 8, 2),
	MTK_PIN_DRV_GRP(9, 0xd60, 12, 2),
	MTK_PIN_DRV_GRP(10, 0xd70, 0, 2),
	MTK_PIN_DRV_GRP(11, 0xd70, 0, 2),
	MTK_PIN_DRV_GRP(12, 0xd70, 0, 2),
	MTK_PIN_DRV_GRP(13, 0xd70, 0, 2),
	MTK_PIN_DRV_GRP(14, 0xd50, 8, 1),
	MTK_PIN_DRV_GRP(15, 0xd20, 4, 1),
	MTK_PIN_DRV_GRP(16, 0xd50, 8, 1),
	MTK_PIN_DRV_GRP(17, 0xd20, 12, 1),
	MTK_PIN_DRV_GRP(18, 0xd20, 12, 1),
	MTK_PIN_DRV_GRP(19, 0xd20, 12, 1),
	MTK_PIN_DRV_GRP(20, 0xd20, 12, 1),
	MTK_PIN_DRV_GRP(23, 0xd30, 8, 1),
	MTK_PIN_DRV_GRP(24, 0xd30, 8, 1),
	MTK_PIN_DRV_GRP(25, 0xd30, 8, 1),
	MTK_PIN_DRV_GRP(26, 0xd30, 8, 1),
	MTK_PIN_DRV_GRP(27, 0xd30, 8, 1),
	MTK_PIN_DRV_GRP(28, 0xd10, 0, 1),
	MTK_PIN_DRV_GRP(29, 0xd40, 12, 2),
	MTK_PIN_DRV_GRP(30, 0xd50, 0, 2),
	MTK_PIN_DRV_GRP(31, 0xd50, 4, 2),
	MTK_PIN_DRV_GRP(32, 0xd50, 4, 2),
	MTK_PIN_DRV_GRP(33, 0xd50, 4, 2),
	MTK_PIN_DRV_GRP(34, 0xd50, 4, 2),
	MTK_PIN_DRV_GRP(35, 0xd50, 4, 2),
	MTK_PIN_DRV_GRP(36, 0xd00, 0, 0),
	MTK_PIN_DRV_GRP(37, 0xd00, 0, 0),
	MTK_PIN_DRV_GRP(38, 0xd00, 0, 0),
	MTK_PIN_DRV_GRP(39, 0xd00, 0, 0),
	MTK_PIN_DRV_GRP(40, 0xd00, 0, 0),
	MTK_PIN_DRV_GRP(41, 0xd00, 0, 0),
	MTK_PIN_DRV_GRP(42, 0xd00, 4, 0),
	MTK_PIN_DRV_GRP(43, 0xd00, 4, 0),
	MTK_PIN_DRV_GRP(44, 0xd00, 4, 0),
	MTK_PIN_DRV_GRP(45, 0xd00, 4, 0),
	MTK_PIN_DRV_GRP(46, 0xd00, 4, 0),
	MTK_PIN_DRV_GRP(47, 0xd00, 4, 0),
	MTK_PIN_DRV_GRP(48, 0xd00, 8, 0),
	MTK_PIN_DRV_GRP(49, 0xd00, 8, 0),
	MTK_PIN_DRV_GRP(50, 0xd00, 8, 0),
	MTK_PIN_DRV_GRP(51, 0xd00, 8, 0),
	MTK_PIN_DRV_GRP(52, 0xd10, 12, 0),
	MTK_PIN_DRV_GRP(53, 0xd10, 12, 0),
	MTK_PIN_DRV_GRP(54, 0xd10, 12, 0),
	MTK_PIN_DRV_GRP(55, 0xd10, 12, 0),
	MTK_PIN_DRV_GRP(56, 0xdb0, 4, 0),
	MTK_PIN_DRV_GRP(57, 0xd00, 8, 0),
	MTK_PIN_DRV_GRP(58, 0xd00, 8, 0),
	MTK_PIN_DRV_GRP(59, 0xd00, 12, 0),
	MTK_PIN_DRV_GRP(60, 0xd00, 12, 0),
	MTK_PIN_DRV_GRP(61, 0xd00, 12, 0),
	MTK_PIN_DRV_GRP(62, 0xd00, 12, 0),
	MTK_PIN_DRV_GRP(63, 0xd90, 12, 0),
	MTK_PIN_DRV_GRP(64, 0xd90, 12, 0),
	MTK_PIN_DRV_GRP(65, 0xd90, 12, 0),
	MTK_PIN_DRV_GRP(66, 0xd90, 12, 0),
	MTK_PIN_DRV_GRP(67, 0xd90, 12, 0),
	MTK_PIN_DRV_GRP(68, 0xd90, 12, 0),
	MTK_PIN_DRV_GRP(69, 0xda0, 0, 0),
	MTK_PIN_DRV_GRP(70, 0xda0, 12, 0),
	MTK_PIN_DRV_GRP(71, 0xd80, 12, 0),
	MTK_PIN_DRV_GRP(72, 0xd80, 12, 0),
	MTK_PIN_DRV_GRP(73, 0xd80, 12, 0),
	MTK_PIN_DRV_GRP(74, 0xd90, 0, 0),
	MTK_PIN_DRV_GRP(75, 0xd90, 0, 0),
	MTK_PIN_DRV_GRP(76, 0xd90, 0, 0),
	MTK_PIN_DRV_GRP(77, 0xd20, 0, 0),
	MTK_PIN_DRV_GRP(78, 0xd20, 0, 0),
	MTK_PIN_DRV_GRP(79, 0xd20, 0, 0),
	MTK_PIN_DRV_GRP(80, 0xd20, 0, 0),
	MTK_PIN_DRV_GRP(81, 0xd80, 8, 0),
	MTK_PIN_DRV_GRP(82, 0xd80, 8, 0),
	MTK_PIN_DRV_GRP(83, 0xd80, 8, 0),
	MTK_PIN_DRV_GRP(84, 0xd80, 8, 0),
	MTK_PIN_DRV_GRP(85, 0xd80, 8, 0),
	MTK_PIN_DRV_GRP(86, 0xd80, 8, 0),
	MTK_PIN_DRV_GRP(87, 0xd80, 8, 0),
	MTK_PIN_DRV_GRP(88, 0xd30, 0, 0),
	MTK_PIN_DRV_GRP(89, 0xd30, 0, 0),
	MTK_PIN_DRV_GRP(90, 0xd30, 0, 0),
	MTK_PIN_DRV_GRP(91, 0xd30, 0, 0),
	MTK_PIN_DRV_GRP(92, 0xd30, 0, 0),
	MTK_PIN_DRV_GRP(93, 0xd30, 0, 0),
	MTK_PIN_DRV_GRP(94, 0xd30, 0, 0),
	MTK_PIN_DRV_GRP(95, 0xd30, 0, 0),
	MTK_PIN_DRV_GRP(96, 0xd30, 0, 0),
	MTK_PIN_DRV_GRP(97, 0xd30, 0, 0),
	MTK_PIN_DRV_GRP(98, 0xd10, 4, 0),
	MTK_PIN_DRV_GRP(99, 0xd10, 4, 0),
	MTK_PIN_DRV_GRP(100, 0xd10, 4, 0),
	MTK_PIN_DRV_GRP(101, 0xd10, 4, 0),
	MTK_PIN_DRV_GRP(102, 0xd10, 4, 0),
	MTK_PIN_DRV_GRP(103, 0xd10, 4, 0),
	MTK_PIN_DRV_GRP(104, 0xd40, 8, 1),
	MTK_PIN_DRV_GRP(105, 0xd40, 8, 1),
	MTK_PIN_DRV_GRP(106, 0xd10, 8, 1),
	MTK_PIN_DRV_GRP(107, 0xd10, 8, 1),
	MTK_PIN_DRV_GRP(114, 0xd50, 12, 2),
	MTK_PIN_DRV_GRP(115, 0xd60, 0, 2),
	MTK_PIN_DRV_GRP(116, 0xd60, 4, 2),
	MTK_PIN_DRV_GRP(117, 0xd60, 4, 2),
	MTK_PIN_DRV_GRP(118, 0xd60, 4, 2),
	MTK_PIN_DRV_GRP(119, 0xd60, 4, 2),
};

int mtk_pinctrl_get_gpio_mode_for_eint(int pin)
{
	return mtk_pinctrl_get_gpio_value(pctl, pin,
		pctl->devdata->n_pin_mode, pctl->devdata->pin_mode_grps);
}

static const struct mtk_pinctrl_devdata mt8518_pinctrl_data = {
	.pins = mtk_pins_mt8518,
	.npins = ARRAY_SIZE(mtk_pins_mt8518),
	.grp_desc = mt8518_drv_grp,
	.n_grp_cls = ARRAY_SIZE(mt8518_drv_grp),
	.pin_drv_grp = mt8518_pin_drv,
	.n_pin_drv_grps = ARRAY_SIZE(mt8518_pin_drv),
	.spec_pull_set = mt8518_spec_pull_set,
#if defined(CONFIG_PINCTRL_MTK_NO_UPSTREAM)
	.spec_pull_get = mt8518_spec_pull_get,
#endif
	.spec_ies_smt_set = mt8518_ies_smt_set,
#if defined(CONFIG_PINCTRL_MTK_NO_UPSTREAM)
	.spec_ies_get = mt8518_spec_ies_get,
	.spec_smt_get = mt8518_spec_smt_get,
#endif
	.dir_offset = 0x0000,
	.dout_offset = 0x0100,
	.din_offset = 0x0200,
	.pinmux_offset = 0x0300,
	.pullen_offset = 0x0500,
	.pullsel_offset = 0x0600,
	.type1_start = 120,
	.type1_end = 120,
	.port_shf = 4,
	.port_mask = 0xf,
	.port_align = 4,
	.eint_offsets = {
		.name = "mt8518_eint",
		.stat      = 0x000,
		.ack       = 0x040,
		.mask      = 0x080,
		.mask_set  = 0x0c0,
		.mask_clr  = 0x100,
		.sens      = 0x140,
		.sens_set  = 0x180,
		.sens_clr  = 0x1c0,
		.soft      = 0x200,
		.soft_set  = 0x240,
		.soft_clr  = 0x280,
		.pol       = 0x300,
		.pol_set   = 0x340,
		.pol_clr   = 0x380,
		.dom_en    = 0x400,
		.dbnc_ctrl = 0x500,
		.dbnc_set  = 0x600,
		.dbnc_clr  = 0x700,
		.port_mask = 7,
		.ports     = 6,
	},
	.ap_num = 120,
	.db_cnt = 13,
};

static int mtk_pinctrl_probe(struct platform_device *pdev)
{
	int ret = 0;

	pr_info("[%04d][%s]: probe start..\n", __LINE__, __func__);
	ret = mtk_pctrl_init(pdev, &mt8518_pinctrl_data, NULL);
	pr_info("[%04d][%s]: probe end, ret:%d\n", __LINE__, __func__, ret);
	return ret;
}

static const struct of_device_id mtk_pctrl_match[] = {
	{
		.compatible = "mediatek,mt8518-pinctrl",
	}, {
	}
};
MODULE_DEVICE_TABLE(of, mt8518_pctrl_match);

static struct platform_driver mtk_pinctrl_driver = {
	.probe = mtk_pinctrl_probe,
	.driver = {
		.name = "mediatek-mt8518-pinctrl",
		.owner = THIS_MODULE,
		.of_match_table = mtk_pctrl_match,
		.pm = &mtk_eint_pm_ops,
	},
};

static int __init mtk_pinctrl_init(void)
{
	return platform_driver_register(&mtk_pinctrl_driver);
}

/* module_init(mtk_pinctrl_init); */

postcore_initcall(mtk_pinctrl_init);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek Pinctrl Driver");
MODULE_AUTHOR("ZH Chen <zh.chen@mediatek.com>");
