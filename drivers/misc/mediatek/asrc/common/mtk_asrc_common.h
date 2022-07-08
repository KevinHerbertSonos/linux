/*
 * mtk_asrc_common.h  -- common definitions in this header file.
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

#ifndef MTK_ASRC_COMMON_H_
#define MTK_ASRC_COMMON_H_

struct CALI_FS {
	int integer;
	int decimal;
};

enum afe_mem_asrc_id {
	MEM_ASRC_1 = 0,
	MEM_ASRC_2,
	MEM_ASRC_3,
	MEM_ASRC_4,
	MEM_ASRC_NUM
};

enum afe_mem_asrc_tracking_mode {
	MEM_ASRC_NO_TRACKING = 0,
	MEM_ASRC_TRACKING_TX,	/* internal test only */
	MEM_ASRC_TRACKING_RX,	/* internal test only */
	MEM_ASRC_TRACKING_NUM
};

/* calibrator inout source selection */
enum afe_mem_asrc_tracking_source {
	MEM_ASRC_MULTI_IN = 0,
	MEM_ASRC_SPDIF_IN,
	MEM_ASRC_PCMIF_SLAVE,    /* pcm_sync_in */
	MEM_ASRC_ETDM_IN2_SLAVE, /* i2sin_slv_ws (etdm_in2_slave_lrck_ext) */
	MEM_ASRC_ETDM_IN1_SLAVE, /* etdmin_slv_ws (etdm_in1_slave_lrck_ext) */
	MEM_ASRC_TRX_SRC_NUM
};

#endif
