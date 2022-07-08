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
 * @file  si_cpCEC.h
 *
 * @brief Implementation of the Foo API.
 *
 *****************************************************************************
 */

#ifndef _CECHANDLER_H_
#define _CECHANDLER_H_
#include <si_datatypes.h>

//------------------------------------------------------------------------------
// API Function Templates
//------------------------------------------------------------------------------
BOOL CpArcEnable(unsigned char mode);
void CpHecEnable(BOOL enable);
char *CpCecTranslateLA(unsigned char bLogAddr);
char *CpCecTranslateOpcodeName(struct SI_CpiData_t *pMsg);
BOOL CpCecPrintCommand(struct SI_CpiData_t *pMsg, BOOL isTX);
BOOL CpCecRxMsgHandler(struct SI_CpiData_t *pCpi);

#endif				// _CECHANDLER_H_

