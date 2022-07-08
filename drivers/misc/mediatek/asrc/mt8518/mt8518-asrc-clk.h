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


#ifndef MT8518_ASRC_CLK_H
#define MT8518_ASRC_CLK_H

enum audio_system_clock_type_8518 {
	MT8518_CLK_FASM_L,
	MT8518_CLK_APLL1,
	MT8518_CLK_APLL2,
	MT8518_CLK_NUM
};

int asrc_init_clock(struct device *dev);
int asrc_turn_on_asrc_clock(void);
void asrc_turn_off_asrc_clock(void);
unsigned long asrc_get_clock_rate(enum audio_system_clock_type_8518 clk_type);
#endif
