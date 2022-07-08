/*
 * mtk_asrc_export_api.h  -- export asrc apis for other drivers.
 *
 * Copyright (c) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#ifndef MTK_ASRC_EXPORT_H_
#define MTK_ASRC_EXPORT_H_

#include "mtk_asrc_common.h"

int mtk_asrc_cali_open(enum afe_mem_asrc_id id);
int mtk_asrc_cali_release(enum afe_mem_asrc_id id);
int mtk_asrc_cali_start(enum afe_mem_asrc_id id,
	enum afe_mem_asrc_tracking_mode tracking_mode,
	enum afe_mem_asrc_tracking_source tracking_src,
	u32 cali_cycle,
	u32 input_freq,
	u32 output_freq);
u64 mtk_asrc_cali_get_result(enum afe_mem_asrc_id id);
int mtk_asrc_cali_stop(enum afe_mem_asrc_id id);
#endif
