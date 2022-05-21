/*
 * ak4438.h  --  audio driver for AK4438
 *
 * Copyright (C) 2019 Asahi Kasei Microdevices Corporation
 *  Author				Date		Revision
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Tsuyoshi Mutsuro    16/04/11		1.0
 * Norishige Nakashima	19/10/15		2.0
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _AK4438_H
#define _AK4438_H

/***************************************************
 *				  Settings
 ****************************************************/
//#define AK4438_DEBUG
//#define AK4438_ACKS_USE_MANUAL_MODE

#define AK4438_00_CONTROL1			0x00
#define AK4438_01_CONTROL2			0x01
#define AK4438_02_CONTROL3			0x02
#define AK4438_03_LCHATT			0x03
#define AK4438_04_RCHATT			0x04
#define AK4438_05_CONTROL4			0x05
#define AK4438_06_RESERVED			0x06
#define AK4438_07_CONTROL6			0x07
#define AK4438_08_CONTROL7			0x08
#define AK4438_09_RESERVED			0x09
#define AK4438_0A_CONTROL8			0x0A
#define AK4438_0B_CONTROL9			0x0B
#define AK4438_0C_CONTROL10			0x0C
#define AK4438_0D_CONTROL11			0x0D
#define AK4438_0E_CONTROL12			0x0E
#define AK4438_0F_L2CHATT			0x0F
#define AK4438_10_R2CHATT			0x10
#define AK4438_11_L3CHATT			0x11
#define AK4438_12_R3CHATT			0x12
#define AK4438_13_L4CHATT			0x13
#define AK4438_14_R4CHATT			0x14
#define AK4438_MAX_REGISTERS	(AK4438_14_R4CHATT + 1)

/* Bitfield Definitions */

/* AK4438_00_CONTROL1 (0x00) Fields */
//Addr Register Name  D7     D6    D5    D4    D3    D2    D1    D0
//00H  Control 1      ACKS   0     0     0     DIF2  DIF1  DIF0  RSTN

//MONO1 & SELLR1 bits
#define AK4438_DAC1_LR_MASK		0x0A
#define AK4438_DAC1_INV_MASK	0xC0

//MONO2 & SELLR2 bits
#define AK4438_DAC2_MASK1	0x20
#define AK4438_DAC2_MASK2	0x38

//MONO3 & SELLR3 bits
#define AK4438_DAC3_LR_MASK		0x44
#define AK4438_DAC3_INV_MASK	0x30

//MONO4 & SELLR4 bits
#define AK4438_DAC4_LR_MASK		0x88
#define AK4438_DAC4_INV_MASK	0xC0


//SDS2-0 bits
#define AK4438_SDS0__MASK		0x10
#define AK4438_SDS12_MASK		0x30

//Digital Filter (SD, SLOW, SSLOW)
#define AK4438_SD_MASK			0x20
#define AK4438_SLOW_MASK		0x01
#define AK4438_SSLOW_MASK		0x01

//DIF2 1 0
//  x  1 0 MSB justified  Figure 3 (default)
//  x  1 1 I2S Compliment  Figure 4
#define AK4438_DIF_MASK			0x06
#define AK4438_DIF_MSB_LOW_FS_MODE	    (2 << 1)
#define AK4438_DIF_I2S_LOW_FS_MODE		(3 << 1)

#define AK4438_DIF2_1_0_MASK		0x0e

// ACKS is Auto mode so disable the Manual feature
//#define AK4438_ACKS_USE_MANUAL_MODE

/* AK4438_00_CONTROL1 (0x00) D0 bit */
#define AK4438_RSTN_MASK		0x01
#define AK4438_RSTN				(0x1 << 0)

#ifdef AK4438_ACKS_USE_MANUAL_MODE
/* AK4438_01_CONTROL2 (0x01) and AK4438_05_CONTROL4 (0x05) Fields */
#define AK4438_DFS01_MASK		0x18
#define AK4438_DFS2__MASK		0x02
#define AK4438_DFS01_48KHZ		(0x0 << 3)  //  30kHz to 54kHz
#define AK4438_DFS2__48KHZ		(0x0 << 1)  //  30kHz to 54kHz

#define AK4438_DFS01_96KHZ		(0x1 << 3)  //  54kHz to 108kHz
#define AK4438_DFS2__96KHZ		(0x0 << 1)  //  54kHz to 108kHz

#define AK4438_DFS01_192KHZ		(0x2 << 3)  //  120kHz  to 216kHz
#define AK4438_DFS2__192KHZ		(0x0 << 1)  //  120kHz  to 216kHz

#define AK4438_DFS01_384KHZ		(0x0 << 3)	//	384kHz
#define AK4438_DFS2__384KHZ		(0x1 << 1)	//	384kHz

#define AK4438_DFS01_768KHZ		(0x1 << 3)	//	768kHz
#define AK4438_DFS2__768KHZ		(0x1 << 1)	//	768kHz
#endif

#define GPO_PDN_LOW	0
#define GPO_PDN_HIGH	1

#endif

