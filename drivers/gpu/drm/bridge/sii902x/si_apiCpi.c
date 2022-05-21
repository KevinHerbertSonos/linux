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
 * @file  si_apiCpi.c
 *
 * @brief Implementation of the Foo API.
 *
 *****************************************************************************
 */

//#include <stdio.h>
//#include <string.h>
//#include "i2c_slave_addrs.h"
#include "si_apiCpi.h"
#include "si_cpi_regs.h"
#include "si_cec_enums.h"
//#include "TypeDefs.h"
//#include "Defs.h"
//#include "i2c_master_sw.h"
#if 0
//------------------------------------------------------------------------------
// Function:    SiIRegioReadBlock
// Description: Read a block of registers starting with the specified register.
//              The register address parameter is translated into an I2C
//              slave address and offset.
//              The block of data bytes is read from the I2C slave address
//              and offset.
//------------------------------------------------------------------------------
void SiIRegioReadBlock(unsigned short regAddr,
unsigned char *buffer, unsigned short length)
{
//I2C_ReadBlock(SA_TX_CPI_Primary,
//(unsigned char)regAddr, buffer, length);
}

//------------------------------------------------------------------------------
// Function:    SiIRegioWriteBlock
// Description: Write a block of registers starting with the specified register.
//              The register address parameter is translated into an I2C slave
//              address and offset.
//              The block of data bytes is written to the I2C slave address
//              and offset.
//------------------------------------------------------------------------------
void SiIRegioWriteBlock(unsigned short regAddr,
unsigned char *buffer, unsigned short length)
{

//I2C_WriteBlock(SA_TX_CPI_Primary,
//(unsigned char)regAddr, buffer, length);
}

//------------------------------------------------------------------------------
// Function:    SiIRegioRead
// Description: Read a one byte register.
//              The register address parameter is translated into an I2C slave
//              address and offset. The I2C slave address and offset are used
//              to perform an I2C read operation.
//------------------------------------------------------------------------------
unsigned char SiIRegioRead(unsigned short regAddr)
{
	return 0;

	    //return (I2C_ReadByte(SA_TX_CPI_Primary, (unsigned char)regAddr));
}


//------------------------------------------------------------------------------
// Function:    SiIRegioWrite
// Description: Write a one byte register.
//              The register address parameter is translated into an I2C
//              slave address and offset. The I2C slave address and offset
//              are used to perform an I2C write operation.
//------------------------------------------------------------------------------
void SiIRegioWrite(unsigned short regAddr, unsigned char value)
{

	    //I2C_WriteByte(SA_TX_CPI_Primary, (unsigned char)regAddr, value);
}

//------------------------------------------------------------------------------
// Function:    SiIRegioModify
// Description: Modify a one byte register under mask.
//              The register address parameter is translated into an I2C
//              slave address and offset. The I2C slave address and offset are
//              used to perform I2C read and write operations.
//
//              All bits specified in the mask are set in the register
//              according to the value specified.
//              A mask of 0x00 does not change any bits.
//              A mask of 0xFF is the same a writing a byte - all bits
//              are set to the value given.
//              When only some bits in the mask are set, only those bits are
//              changed to the values given.
//------------------------------------------------------------------------------
#endif	/*  */
void SiIRegioModify(unsigned short regAddr,
unsigned char mask, unsigned char value)
{
	unsigned char abyte;

	abyte = SiIRegioRead((unsigned char)regAddr);
	abyte &= (~mask);	//first clear all bits in mask
	abyte |= (mask & value);	//then set bits from value
	SiIRegioWrite((unsigned char)regAddr, abyte);
}

//------------------------------------------------------------------------------
// Function:    SI_CpiSetLogicalAddr
// Description: Configure the CPI subsystem to respond to a specific CEC
//              logical address.
//------------------------------------------------------------------------------
BOOL SI_CpiSetLogicalAddr(unsigned char logicalAddress)
{
	unsigned char capture_address[2];
	unsigned char capture_addr_sel = 0x01;

	capture_address[0] = 0;
	capture_address[1] = 0;
	if (logicalAddress < 8) {
		capture_addr_sel =
			capture_addr_sel <<
			logicalAddress;
		capture_address[0] = capture_addr_sel;
	} else {
		capture_addr_sel = capture_addr_sel << (logicalAddress - 8);
		capture_address[1] = capture_addr_sel;
	}
	    // Set Capture Address
	SiIRegioWriteBlock(REG_CEC_CAPTURE_ID0, capture_address, 2);
	SiIRegioWrite(REG_CEC_TX_INIT, logicalAddress);
	DEBUG_PRINT(MSG_STAT,
		("CEC: logicalAddress: 0x%x\n",
		(int)logicalAddress));
	return 1;
}


//------------------------------------------------------------------------------
// Function:    SI_CpiSendPing
// Description: Initiate sending a ping, and used for checking available
//                       CEC devices
//------------------------------------------------------------------------------
void SI_CpiSendPing(unsigned char bCECLogAddr)
{
	SiIRegioWrite(REG_CEC_TX_DEST, BIT_SEND_POLL |
		bCECLogAddr);
}

//------------------------------------------------------------------------------
// Function:    SI_CpiWrite
// Description: Send CEC command via CPI register set
//------------------------------------------------------------------------------
BOOL SI_CpiWrite(struct SI_CpiData_t *pCpi)
{
	unsigned char cec_int_status_reg[2];

#if (INCLUDE_CEC_NAMES > CEC_NO_TEXT)
	    CpCecPrintCommand(pCpi, 1);
#endif	/*  */
	SiIRegioModify(REG_CEC_DEBUG_3,
	BIT_FLUSH_TX_FIFO, BIT_FLUSH_TX_FIFO);

	/* Clear Tx-related buffers; write 1 to bits to be cleared. */
	cec_int_status_reg[0] = 0x64;
	cec_int_status_reg[1] = 0x02;
	SiIRegioWriteBlock(REG_CEC_INT_STATUS_0,
		cec_int_status_reg, 2);

	/* Send the command */
	SiIRegioWrite(REG_CEC_TX_DEST,
	pCpi->srcDestAddr & 0x0F);
	SiIRegioWrite(REG_CEC_TX_COMMAND, pCpi->opcode);
	SiIRegioWriteBlock(REG_CEC_TX_OPERAND_0, pCpi->args, pCpi->argCount);
	SiIRegioWrite(REG_CEC_TRANSMIT_DATA,
		BIT_TRANSMIT_CMD |
		pCpi->argCount);
	return 1;
}

//------------------------------------------------------------------------------
// Function:    SI_CpiRead
// Description: Reads a CEC message from the CPI read FIFO, if present.
//------------------------------------------------------------------------------
BOOL SI_CpiRead(struct SI_CpiData_t *pCpi)
{
	BOOL error = 0;
	unsigned char argCount;

	argCount = SiIRegioRead(REG_CEC_RX_COUNT);
	if (argCount & BIT_MSG_ERROR)
		error = 1;
	else {
		pCpi->argCount = argCount & 0x0F;
		pCpi->srcDestAddr =
			SiIRegioRead(REG_CEC_RX_CMD_HEADER);
		pCpi->opcode = SiIRegioRead(REG_CEC_RX_OPCODE);
		if (pCpi->argCount)
			SiIRegioReadBlock(REG_CEC_RX_OPERAND_0,
			pCpi->args, pCpi->argCount);
	}

	// Clear CLR_RX_FIFO_CUR;
	// Clear current frame from Rx FIFO
	SiIRegioModify(REG_CEC_RX_CONTROL,
	BIT_CLR_RX_FIFO_CUR, BIT_CLR_RX_FIFO_CUR);
#if (INCLUDE_CEC_NAMES > CEC_NO_TEXT)
	if (!error)
		CpCecPrintCommand(pCpi, 0);

#endif	/*  */
	return error;
}

//------------------------------------------------------------------------------
// Function:    SI_CpiStatus
// Description: Check CPI registers for a CEC event
//------------------------------------------------------------------------------
BOOL SI_CpiStatus(struct SI_CpiStatus_t *pStatus)
{
	unsigned char cecStatus[2] = { 0 };

	pStatus->txState = 0;
	pStatus->cecError = 0;
	pStatus->rxState = 0;
	SiIRegioReadBlock(REG_CEC_INT_STATUS_0, cecStatus, 2);
	if ((cecStatus[0] & 0x7F) || cecStatus[1]) {
		DEBUG_PRINT(MSG_STAT,
			     ("\nCEC Status: %02X %02X\n",
			     (int)cecStatus[0],
			     (int)cecStatus[1]));

		// Clear interrupts
		if (cecStatus[1] & BIT_FRAME_RETRANSM_OV) {
			DEBUG_PRINT(MSG_DBG,
				("\n!TX retry count exceeded! [%02X][%02X]\n",
				(int)cecStatus[0], (int)cecStatus[1]));

			SiIRegioModify(REG_CEC_DEBUG_3,
			BIT_FLUSH_TX_FIFO, BIT_FLUSH_TX_FIFO);
		}

		// Clear set bits that are set
		SiIRegioWriteBlock(REG_CEC_INT_STATUS_0, cecStatus, 2);
		// RX Processing
		if (cecStatus[0] & BIT_RX_MSG_RECEIVED)
			pStatus->rxState = 1;

		// RX Errors processing
		if (cecStatus[1] & BIT_SHORT_PULSE_DET)
			pStatus->cecError |= SI_CEC_SHORTPULSE;

		if (cecStatus[1] & BIT_START_IRREGULAR)
			pStatus->cecError |= SI_CEC_BADSTART;

		if (cecStatus[1] & BIT_RX_FIFO_OVERRUN)
			pStatus->cecError |= SI_CEC_RXOVERFLOW;


		// TX Processing
		if (cecStatus[0] & BIT_TX_FIFO_EMPTY)
			pStatus->txState = SI_TX_WAITCMD;

		if (cecStatus[0] & BIT_TX_MESSAGE_SENT)
			pStatus->txState = SI_TX_SENDACKED;

		if (cecStatus[1] & BIT_FRAME_RETRANSM_OV)
			pStatus->txState = SI_TX_SENDFAILED;

	}
	return 1;
}


//------------------------------------------------------------------------------
// Function:    SI_CpiGetLogicalAddr
// Description: Get Logical Address
//------------------------------------------------------------------------------
unsigned char SI_CpiGetLogicalAddr(void)
{
	return SiIRegioRead(REG_CEC_TX_INIT);
}


//------------------------------------------------------------------------------
// Function:    SI_CpiInit
// Description: Initialize the CPI subsystem for communicating via CEC
//------------------------------------------------------------------------------
BOOL SI_CpiInit(void)
{

#ifdef DEV_SUPPORT_CEC_FEATURE_ABORT
	// Turn on CEC auto response to <Abort> command.
	SiIRegioWrite(CEC_OP_ABORT_31, BIT7);

#else	/*  */
	// Turn off CEC auto response to <Abort> command.
	SiIRegioWrite(CEC_OP_ABORT_31, CLEAR_BITS);

#endif	/*  */

#ifdef DEV_SUPPORT_CEC_CONFIG_CPI_0
	// Bit 4 of the CC Config reister must be cleared to enable CEC
	SiIRegioModify(REG_CEC_CONFIG_CPI, 0x10, 0x00);

#endif	/*  */

	// initialized he CEC CEC_LOGADDR_TV logical address
	if (!SI_CpiSetLogicalAddr(CEC_LOGADDR_AUDSYS)) {
		DEBUG_PRINT(MSG_ALWAYS, ("\n Cannot init CPI/CEC"));
		return 0;
	}
	return 1;
}

