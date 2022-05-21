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
 * @file  si_apiCpi.h
 *
 * @brief Implementation of the Foo API.
 *
 *****************************************************************************
 */

#ifndef __SI_APICPI_H__
#define __SI_APICPI_H__

#include "si_datatypes.h"

//---------------------------------------------------------
// CPI Enums and manifest constants
//---------------------------------------------------------

#define SII_MAX_CMD_SIZE 16
enum SI_txState_t {
	SI_TX_WAITCMD,
	SI_TX_SENDING,
	SI_TX_SENDACKED,
	SI_TX_SENDFAILED
};
enum SI_cecError_t {
	SI_CEC_SHORTPULSE = 0x80,
	SI_CEC_BADSTART = 0x40,
	SI_CEC_RXOVERFLOW = 0x20,
	SI_CEC_ERRORS =
	(SI_CEC_SHORTPULSE |
	SI_CEC_BADSTART |
	SI_CEC_RXOVERFLOW)
};

//---------------------------------------------------------
// CPI data structures
//---------------------------------------------------------
struct SI_CpiData_t {
	unsigned char srcDestAddr;
	unsigned char opcode;
	unsigned char args[SII_MAX_CMD_SIZE];
	unsigned char argCount;
	unsigned char nextFrameArgCount;
	unsigned char chn;
};
struct SI_CpiStatus_t {
	unsigned char rxState;
	 unsigned char txState;
	 unsigned char cecError;
};

//--------------------------------------------------------
// CPI Function Prototypes
//--------------------------------------------------------
BOOL SI_CpiRead(struct SI_CpiData_t *pCpi);
BOOL SI_CpiWrite(struct SI_CpiData_t *pCpi);
BOOL SI_CpiStatus(struct SI_CpiStatus_t *pCpiStat);
BOOL SI_CpiInit(void);
BOOL SI_CpiSetLogicalAddr(unsigned char logicalAddress);
unsigned char SI_CpiGetLogicalAddr(void);
void SI_CpiSendPing(unsigned char bCECLogAddr);
void SiIRegioWrite(unsigned short regAddr, unsigned char value);
unsigned char SiIRegioRead(unsigned short regAddr);
void SiIRegioWriteBlock(unsigned short regAddr,
	unsigned char *buffer, unsigned short length);
void SiIRegioReadBlock(unsigned short regAddr,
	unsigned char *buffer, unsigned short length);

#endif				// _SI_APICPI_H_
