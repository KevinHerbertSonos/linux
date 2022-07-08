/*
 *****************************************************************************
 *
 * Copyright 2010, Silicon Image, Inc.  2020, Lattice Semiconductor Corp.
 * All rights reserved.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *****************************************************************************
 */
/*
 *****************************************************************************
 * @file  si_datatypes.h
 *
 * @brief Implementation of the Foo API.
 *
 *****************************************************************************
 */

#ifndef __SI_DATATYPES_H__
#define __SI_DATATYPES_H__
#include <linux/kernel.h>
    /* C99 defined data types.  */
#if 0
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed long int32_t;

    /* Emulate C99/C++ bool type    */

#ifdef __cplusplus
//typedef bool bool_t;

#else	/*  */
//typedef enum  { false = 0, true = !(false)
//} bool_t;
//typedef char BOOL;

#endif				// __cplusplus
#endif	/*  */
#define BOOL char

//#define ROM     code        // 8051 type of ROM memory
//#define XDATA   xdata       // 8051 type of external memory

//------------------------------------------------------------------------------
// Configuration defines used by hal_config.h
//------------------------------------------------------------------------------

//#define ENABLE      (0xFF)
//#define DISABLE     (0x00)

#define BIT0                    0x01
#define BIT1                    0x02
#define BIT2                    0x04
#define BIT3                    0x08
#define BIT4                    0x10
#define BIT5                    0x20
#define BIT6                    0x40
#define BIT7                    0x80

#define CEC_NO_TEXT         0
#define CEC_NO_NAMES        1
#define CEC_ALL_TEXT        2
#define INCLUDE_CEC_NAMES   CEC_NO_TEXT

#define MSG_ALWAYS              0x00
#define MSG_STAT                0x01
#define MSG_DBG                 0x02
#define DEBUG_PRINT(l, x)      \
	do {				\
		if (l <= 5)			\
			pr_info x;			\
	} while (0)
//#define DEBUG_PRINT(l,x)

// see include/i2c_slave_addrs.h

#define SET_BITS    0xFF
#define CLEAR_BITS  0x00
unsigned char SiIRegioRead(unsigned short regAddr);
void SiIRegioWrite(unsigned short regAddr, unsigned char value);
void SiIRegioModify(unsigned short regAddr,
	unsigned char mask, unsigned char value);
void SiIRegioWriteBlock(unsigned short regAddr,
	unsigned char *buffer, unsigned short length);

#endif				// __SI_DATATYPES_H__

