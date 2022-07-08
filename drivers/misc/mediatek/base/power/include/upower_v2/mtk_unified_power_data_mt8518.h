/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef MTK_UNIFIED_POWER_DATA_MT8518_H
#define MTK_UNIFIED_POWER_DATA_MT8518_H

struct upower_tbl upower_tbl_l_FY = {
	.row = {
		{.cap = 451, .volt = 90000, .dyn_pwr = 52701,
			.lkg_pwr = {42737, 42737, 42737, 42737, 42737, 42737} },
		{.cap = 603, .volt = 95000, .dyn_pwr = 79143,
			.lkg_pwr = {46022, 46022, 46022, 46022, 46022, 46022} },
		{.cap = 737, .volt = 100000, .dyn_pwr = 113152,
			.lkg_pwr = {52000, 52000, 52000, 52000, 52000, 52000} },
		{.cap = 845, .volt = 105000, .dyn_pwr = 145022,
			.lkg_pwr = {60276, 60276, 60276, 60276, 60276, 60276} },
		{.cap = 1024, .volt = 112500, .dyn_pwr = 207652,
			.lkg_pwr = {78094, 78094, 78094, 78094, 78094, 78094} },
	},
	.lkg_idx = DEFAULT_LKG_IDX,
	.row_num = UPOWER_OPP_NUM,
	.nr_idle_states = NR_UPOWER_CSTATES,
	.idle_states = {
		{{0}, {42737} },
		{{0}, {42737} },
		{{0}, {42737} },
		{{0}, {42737} },
		{{0}, {42737} },
		{{0}, {42737} },
	},
};

struct upower_tbl upower_tbl_l_FY2 = {
	.row = {
		{.cap = 478, .volt = 90000, .dyn_pwr = 52701,
			.lkg_pwr = {42737, 42737, 42737, 42737, 42737, 42737} },
		{.cap = 640, .volt = 95000, .dyn_pwr = 79143,
			.lkg_pwr = {46022, 46022, 46022, 46022, 46022, 46022} },
		{.cap = 782, .volt = 100000, .dyn_pwr = 113152,
			.lkg_pwr = {52000, 52000, 52000, 52000, 52000, 52000} },
		{.cap = 897, .volt = 105000, .dyn_pwr = 145022,
			.lkg_pwr = {60276, 60276, 60276, 60276, 60276, 60276} },
		{.cap = 1024, .volt = 112500, .dyn_pwr = 193331,
			.lkg_pwr = {78094, 78094, 78094, 78094, 78094, 78094} },
	},
	.lkg_idx = DEFAULT_LKG_IDX,
	.row_num = UPOWER_OPP_NUM,
	.nr_idle_states = NR_UPOWER_CSTATES,
	.idle_states = {
		{{0}, {42737} },
		{{0}, {42737} },
		{{0}, {42737} },
		{{0}, {42737} },
		{{0}, {42737} },
		{{0}, {42737} },
	},
};


#endif /* UNIFIED_POWER_DATA_MT8518H */
