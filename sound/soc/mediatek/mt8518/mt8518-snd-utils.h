/*
 * mt8518-snd-utils.h  --  Mediatek 8518 sound utility
 *
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Hidalgo Huang <hidalgo.huang@mediatek.com>
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

#ifndef _MT8518_SND_UTILS_H_
#define _MT8518_SND_UTILS_H_

struct snd_card;

int mt8518_snd_ctl_notify(struct snd_card *card,
	unsigned char *ctl_name, unsigned int mask);

unsigned int mt8518_snd_get_dai_format(const char *fmt_str);

int mt8518_snd_get_etdm_format(unsigned int dai_fmt);

#endif
