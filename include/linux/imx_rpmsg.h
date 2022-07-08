/*
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 * Copyright (C) 2017 NXP.
 */

/*
 * The code contained herein is licensed under the GNU Lesser General
 * Public License.  You may obtain a copy of the GNU Lesser General
 * Public License Version 2.1 or later at the following locations:
 *
 * http://www.opensource.org/licenses/lgpl-license.html
 * http://www.gnu.org/copyleft/lgpl.html
 */

/*
 * @file linux/imx_rpmsg.h
 *
 * @brief Global header file for imx RPMSG
 *
 * @ingroup RPMSG
 */
#ifndef __LINUX_IMX_RPMSG_H__
#define __LINUX_IMX_RPMSG_H__

/* Category define */
#define IMX_RMPSG_LIFECYCLE	1
#define IMX_RPMSG_PMIC		2
#define IMX_RPMSG_AUDIO		3
#define IMX_RPMSG_KEY		4
#define IMX_RPMSG_GPIO		5
#define IMX_RPMSG_RTC		6
#define IMX_RPMSG_SENSOR	7
#define IMX_RPMSG_I2C		8
#define IMX_RPMSG_PSOC		9
#define IMX_RPMSG_PSY		10
#define IMX_RPMSG_BLUETOOTH	11
#define IMX_RPMSG_SNDCRD	12
#define IMX_RPMSG_DSP		13
#define IMX_RPMSG_PM		14
#define IMX_RPMSG_SRFS		15
#define IMX_RPMSG_LEDCTL	16
#define IMX_RPMSG_NOTUSED4	17
#define IMX_RPMSG_NOTUSED5	18
/* rpmsg version */
#define IMX_RMPSG_MAJOR		1
#define IMX_RMPSG_MINOR		0

struct imx_rpmsg_head {
	u8 cate;
	u8 major;
	u8 minor;
	u8 type;
	u8 cmd;
	u8 reserved[5];
} __attribute__ ((packed));

#endif /* __LINUX_IMX_RPMSG_H__ */
