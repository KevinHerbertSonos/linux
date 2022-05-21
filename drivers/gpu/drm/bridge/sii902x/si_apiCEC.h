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
 * @file  si_apiCEC.h
 *
 * @brief Implementation of the Foo API.
 *
 *****************************************************************************
 */

#ifndef __SI_APICEC_H__
#define __SI_APICEC_H__
#include <si_datatypes.h>
#include <si_apiCpi.h>
#include <si_cpi_regs.h>
#include <si_cec_enums.h>

//static struct SI_CpiData_t  l_cecFrame;

//--------------------------------------
// CPI Enums, typedefs, and manifest constants
//--------------------------------------

#define MAKE_SRCDEST(src, dest)    ((src << 4) | dest)

#define SII_NUMBER_OF_PORTS         5
#define SII_EHDMI_PORT              (1)
enum { SI_CECTASK_IDLE,
	SI_CECTASK_ENUMERATE,
	SI_CECTASK_NEWSOURCE,
	SI_CECTASK_ONETOUCH,
	SI_CECTASK_SENDMSG
};
struct cec_device {
	unsigned char deviceType;	// 0 - Device is a TV.
	// 1 - Device is a Recording device
	// 2 - Device is a reserved device
	// 3 - Device is a Tuner
	// 4 - Device is a Playback device
	// 5 - Device is an Audio System
	// 6 - Pure CEC Switch
	// 7 - Video Processer
	unsigned char cecLA;	// CEC Logical address of the device.
	unsigned short cecPA;	// CEC Physical address of the device.
};

extern unsigned char g_cecAddress;	// Initiator
extern unsigned short g_cecPhysical;

//------------------------------------------------------------------------------
// Data
//------------------------------------------------------------------------------
extern struct cec_device g_childPortList[SII_NUMBER_OF_PORTS];

//------------------------------------------------------------------------------
// API Function Templates
//------------------------------------------------------------------------------
void si_CecSendMessage(unsigned char opCode, unsigned char dest);
void SI_CecSendUserControlPressed(unsigned char keyCode);
void SI_CecSendUserControlReleased(void);
BOOL SI_CecSwitchSources(unsigned char portIndex);
BOOL SI_CecEnumerate(void);
unsigned char SI_CecHandler(unsigned char currentPort, BOOL returnTask);
unsigned char SI_CecGetPowerState(void);
void SI_CecSetPowerState(unsigned char newPowerState);
void SI_CecSourceRemoved(unsigned char portIndex);
unsigned short SI_CecGetDevicePA(void);
void SI_CecSetDevicePA(unsigned short devPa);
BOOL SI_CecInit(void);
unsigned char SI_CecPortToLA(unsigned char portIndex);
unsigned char SI_CecLaToPort(unsigned char logicalAddr);
void sii9022_cec_handler(void);
BOOL si_CecRxMsgHandlerARC(struct SI_CpiData_t *pCpi);
BOOL CpCecRxMsgHandler(struct SI_CpiData_t *pCpi);


#endif				// __SI_APICEC_H__

