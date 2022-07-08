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


#ifndef MT8518_ASRC_HW_H_
#define MT8518_ASRC_HW_H_

#include <linux/interrupt.h>
#include <linux/device.h>
#include "mtk_asrc_common.h"

//ioctl command
#define ASRC_CMD_TRIGGER         _IOW(0U, 0U, int)
#define ASRC_CMD_CHANNELS        _IOW(0U, 1U, int)
#define ASRC_CMD_INPUT_FREQ      _IOW(0U, 2U, int)
#define ASRC_CMD_INPUT_BITWIDTH  _IOW(0U, 3U, int)
#define ASRC_CMD_OUTPUT_FREQ     _IOW(0U, 4U, int)
#define ASRC_CMD_OUTPUT_BITWIDTH _IOW(0U, 5U, int)
#define ASRC_CMD_IS_DRAIN        _IOR(0U, 6U, int)
#define ASRC_CMD_SIGNAL_REASON   _IOR(0U, 7U, int)
#define ASRC_CMD_INPUT_BUFSIZE   _IOW(0U, 8U, int)
#define ASRC_CMD_OUTPUT_BUFSIZE  _IOW(0U, 9U, int)
#define ASRC_CMD_SIGNAL_CLEAR    _IO(0, 10)
#define ASRC_CMD_TRACK_MODE _IOW(0U, 11U, int)
#define ASRC_CMD_TRACK_SRC _IOW(0U, 12U, int)
#define ASRC_CMD_CALI_CYC _IOW(0U, 13U, int)
#define ASRC_CMD_CALI_MODE_SET _IOW(0U, 14U, int)
#define ASRC_CMD_CALI_MODE_GET _IOR(0U, 15U, struct CALI_FS)
#define ASRC_CMD_CALI_MODE_TRIGGER _IOW(0U, 16U, int)

#define ASRC_MAX_NUM (MEM_ASRC_4 + 1)
#define ASRC_ENABLE (1)
#define ASRC_DISABLE (!ASRC_ENABLE)
#define ASRC_CALI_ENABLE (1)
#define ASRC_CALI_DISABLE (!ASRC_CALI_ENABLE)

enum afe_sampling_rate {
	FS_8000HZ = 0x0,
	FS_12000HZ = 0x1,
	FS_16000HZ = 0x2,
	FS_24000HZ = 0x3,
	FS_32000HZ = 0x4,
	FS_48000HZ = 0x5,
	FS_96000HZ = 0x6,
	FS_192000HZ = 0x7,
	FS_384000HZ = 0x8,
	FS_I2S1 = 0x9,
	FS_I2S2 = 0xA,
	FS_I2S3 = 0xB,
	FS_I2S4 = 0xC,
	FS_I2S5 = 0xD,
	FS_I2S6 = 0xE,
	FS_7350HZ = 0x10,
	FS_11025HZ = 0x11,
	FS_14700HZ = 0x12,
	FS_22050HZ = 0x13,
	FS_29400HZ = 0x14,
	FS_44100HZ = 0x15,
	FS_88200HZ = 0x16,
	FS_176400HZ = 0x17,
	FS_352800HZ = 0x18,
	FS_NUM
};

static inline unsigned int fs_integer(enum afe_sampling_rate fs)
{
	u32 ret;

	switch (fs) {
	case FS_8000HZ:
		ret = 8000;
		break;
	case FS_12000HZ:
		ret = 12000;
		break;
	case FS_16000HZ:
		ret = 16000;
		break;
	case FS_24000HZ:
		ret = 24000;
		break;
	case FS_32000HZ:
		ret = 32000;
		break;
	case FS_48000HZ:
		ret = 48000;
		break;
	case FS_96000HZ:
		ret = 96000;
		break;
	case FS_192000HZ:
		ret = 192000;
		break;
	case FS_384000HZ:
		ret = 384000;
		break;
	case FS_7350HZ:
		ret = 7350;
		break;
	case FS_11025HZ:
		ret = 11025;
		break;
	case FS_14700HZ:
		ret = 14700;
		break;
	case FS_22050HZ:
		ret = 22050;
		break;
	case FS_29400HZ:
		ret = 29400;
		break;
	case FS_44100HZ:
		ret = 44100;
		break;
	case FS_88200HZ:
		ret = 88200;
		break;
	case FS_176400HZ:
		ret = 176400;
		break;
	case FS_352800HZ:
		ret = 352800;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static inline enum afe_sampling_rate fs_enum(unsigned int fs)
{
	enum afe_sampling_rate ret;

	switch (fs) {
	case 8000:
		ret = FS_8000HZ;
		break;
	case 12000:
		ret = FS_12000HZ;
		break;
	case 16000:
		ret = FS_16000HZ;
		break;
	case 24000:
		ret = FS_24000HZ;
		break;
	case 32000:
		ret = FS_32000HZ;
		break;
	case 48000:
		ret = FS_48000HZ;
		break;
	case 96000:
		ret = FS_96000HZ;
		break;
	case 192000:
		ret = FS_192000HZ;
		break;
	case 384000:
		ret = FS_384000HZ;
		break;
	case 7350:
		ret = FS_7350HZ;
		break;
	case 11025:
		ret = FS_11025HZ;
		break;
	case 14700:
		ret = FS_14700HZ;
		break;
	case 22050:
		ret = FS_22050HZ;
		break;
	case 29400:
		ret = FS_29400HZ;
		break;
	case 44100:
		ret = FS_44100HZ;
		break;
	case 88200:
		ret = FS_88200HZ;
		break;
	case 176400:
		ret = FS_176400HZ;
		break;
	case 352800:
		ret = FS_352800HZ;
		break;
	default:
		ret = FS_48000HZ;
		break;
	}
	return ret;
}

/********************* memory asrc *********************/
struct afe_mem_asrc_buffer {
	u32 base;		/* physical */
	u32 size;
	u32 freq;
	u32 bitwidth;
};

struct afe_mem_asrc_config {
	struct afe_mem_asrc_buffer input_buffer;
	struct afe_mem_asrc_buffer output_buffer;
	int stereo;
	enum afe_mem_asrc_tracking_mode tracking_mode;
	enum afe_mem_asrc_tracking_source tracking_src;
	u32 cali_cycle;
};

int afe_power_on_mem_asrc_brg(int on);
int afe_power_on_mem_asrc(enum afe_mem_asrc_id id, int on);
int afe_mem_asrc_configurate(enum afe_mem_asrc_id id,
			     const struct afe_mem_asrc_config *config);
int afe_mem_asrc_enable(enum afe_mem_asrc_id id, int en);
u32 afe_mem_asrc_get_ibuf_rp(enum afe_mem_asrc_id id);
u32 afe_mem_asrc_get_ibuf_wp(enum afe_mem_asrc_id id);
int afe_mem_asrc_set_ibuf_wp(enum afe_mem_asrc_id id, u32 p);
u32 afe_mem_asrc_get_obuf_rp(enum afe_mem_asrc_id id);
u32 afe_mem_asrc_get_obuf_wp(enum afe_mem_asrc_id id);
int afe_mem_asrc_set_obuf_rp(enum afe_mem_asrc_id id, u32 p);
int afe_mem_asrc_set_ibuf_freq(enum afe_mem_asrc_id id, u32 freq);
int afe_mem_asrc_set_obuf_freq(enum afe_mem_asrc_id id, u32 freq);

#define IBUF_EMPTY_INT	 ((u32)0x1U << 20U)
#define IBUF_AMOUNT_INT  ((u32)0x1U << 16U)
#define OBUF_OV_INT      ((u32)0x1U << 12U)
#define OBUF_AMOUNT_INT  ((u32)0x1U <<  8U)
int afe_mem_asrc_irq_enable(enum afe_mem_asrc_id id, u32 interrupts, int en);
int afe_mem_asrc_irq_is_enabled(enum afe_mem_asrc_id id, u32 interrupt);
u32 afe_mem_asrc_irq_status(enum afe_mem_asrc_id id);
void afe_mem_asrc_irq_clear(enum afe_mem_asrc_id id, u32 status);
int afe_mem_asrc_cali_enable(enum afe_mem_asrc_id id, int en);
int afe_mem_asrc_cali_setup(enum afe_mem_asrc_id id,
	enum afe_mem_asrc_tracking_mode tracking_mode,
	enum afe_mem_asrc_tracking_source tracking_src,
	u32 cali_cycle,
	u32 input_freq,
	u32 output_freq);
u64 afe_mem_asrc_calibration_result(enum afe_mem_asrc_id id, u32 freq);
#endif
