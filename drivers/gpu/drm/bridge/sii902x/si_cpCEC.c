/*
 *****************************************************************************
 *
 * Copyright 2010, Silicon Image, Inc.  2020, Lattice Semiconductor Corp.
 * 2020, Lattice Semiconductor Corp.
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
 * @file  si_cpCEC.c
 *
 * @brief Implementation of the Foo API.
 *
 *****************************************************************************
 */

//#include <string.h>
//#include <stdio.h>
//#include <si_cp9387.h>
//#include <si_apiHeac.h>
#include <si_apiCEC.h>
//#include "TypeDefs.h"
//#include "i2c_master_sw.h"

static void CecViewOn(struct SI_CpiData_t *pCpi)
{
	pCpi = pCpi;
	SI_CecSetPowerState(CEC_POWERSTATUS_ON);
}

/*
 *----------------------------------------------------------------------------
 * Function:    CpCecRxMsgHandler
 * Description: Parse received messages and execute response as necessary
 *              Only handle the messages needed at the top level to interact
 *              with the Port Switch hardware.  The SI_API message handler
 *              handles all messages not handled here.
 * Warning:     This function is referenced by the Silicon Image CEC API library
 *              and must be present for it to link properly.  If not used,
 *              it should return 0 (false);
 *             Returns true if message was processed by this handler
 */
BOOL CpCecRxMsgHandler(struct SI_CpiData_t *pCpi)
{
	BOOL processedMsg, isDirectAddressed;

	isDirectAddressed =
		!((pCpi->srcDestAddr & 0x0F) ==
		CEC_LOGADDR_UNREGORBC);

	processedMsg = 1;
	if (isDirectAddressed) {
		    // Respond to messages addressed to us
		switch (pCpi->opcode) {
		case CECOP_IMAGE_VIEW_ON:
		case CECOP_TEXT_VIEW_ON:
			CecViewOn(pCpi);
			break;
		case CECOP_INACTIVE_SOURCE:
			break;
		default:
			processedMsg = 0;
			break;
		}
	} else {

		    // Respond to broadcast messages.
		switch (pCpi->opcode) {
		case CECOP_ACTIVE_SOURCE:
			break;
		default:
			processedMsg = 0;
			break;
		}
	}
	return processedMsg;
}

