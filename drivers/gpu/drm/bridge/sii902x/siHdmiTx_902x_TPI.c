//------------------------------------------------------------------------------
// File Name: siHdmiTx_902x_TPI.c
// File Description:
//              this file is used to call initalization of 902x,
//              including HDMI TX TPI,
//           processing EDID, HDCP and CEC.
//
// Create Date: 07/22/2010
//
// Modification history:
//                       - 07/22/2010  Add file desription and comment
//
//
// Copyright 2002-2010, Silicon Image, Inc.  2020, Lattice Semiconductor Corp.
// All rights reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
// GNU General Public License for more details.

//------------------------------------------------------------------------------
#include "siHdmiTx_902x_TPI.h"

struct SIHDMITX_CONFIG siHdmiTx;
struct GLOBAL_SYSTEM g_sys;
struct GLOBAL_HDCP g_hdcp;
struct GLOBAL_EDID g_edid;
// saved TPI Reg0x08/Reg0x09/Reg0x0A values.
byte tpivmode[3];

/*The following functions are related with target system!!!*/

byte siHdmiTx_HwResetPin;	// Connected to 9022A/4A pin C3 (CRST#)

//byte I2CReadByte ( byte SlaveAddr, byte RegAddr );
//void I2CWriteByte ( byte SlaveAddr, byte RegAddr, byte Data );
//byte I2CReadBlock( byte SlaveAddr, byte RegAddr, byte NBytes, byte * Data );
//byte I2CWriteBlock( byte SlaveAddr, byte RegAddr, byte NBytes, byte * Data );
byte siiReadSegmentBlockEDID(byte SlaveAddr,
byte Segment, byte Offset, byte *Buffer, byte Length)
{
	return 0;
}

//------------------------------------------------------------------------------
// Function Name: DelayMS()
// Function Description: Introduce a busy-wait delay equal,
//in milliseconds, to the input parameter.
//
// Accepts: Length of required delay in milliseconds (max. 65535 ms)
//------------------------------------------------------------------------------
void DelayMS(word MS)
{
	unsigned long i;

	i = MS * 100;

	while (i)
		i--;

}

//------------------------------------------------------------------------------
// Function Name: ReadSetWriteTPI()
// Function Description:
//Write "1" to all bits in TPI offset "Offset" that are set
//                  to "1" in "Pattern"; Leave all other bits in "Offset"
//                  unchanged.
//------------------------------------------------------------------------------

void ReadSetWriteTPI(byte Offset, byte Pattern)
{
	byte Tmp;

	Tmp = ReadByteTPI(Offset);

	Tmp |= Pattern;
	WriteByteTPI(Offset, Tmp);
}

//------------------------------------------------------------------------------
// Function Name: ReadSetWriteTPI()
// Function Description:
//Write "0" to all bits in TPI offset "Offset" that are set
//                  to "1" in "Pattern"; Leave all other bits in "Offset"
//                  unchanged.
//------------------------------------------------------------------------------
void ReadClearWriteTPI(byte Offset, byte Pattern)
{
	byte Tmp;

	Tmp = ReadByteTPI(Offset);

	Tmp &= ~Pattern;
	WriteByteTPI(Offset, Tmp);
}

//------------------------------------------------------------------------------
// Function Name: ReadSetWriteTPI()
// Function Description:
//Write "Value" to all bits in TPI offset "Offset" that are set
//                  to "1" in "Mask"; Leave all other bits in "Offset"
//                  unchanged.
//------------------------------------------------------------------------------
void ReadModifyWriteTPI(byte Offset, byte Mask, byte Value)
{
	byte Tmp;

	Tmp = ReadByteTPI(Offset);
	Tmp &= ~Mask;
	Tmp |= (Value & Mask);
	WriteByteTPI(Offset, Tmp);
}

//------------------------------------------------------------------------------
// Function Name: ReadBlockTPI()
// Function Description: Read NBytes from offset Addr of the TPI slave address
//                      into a byte Buffer pointed to by Data
//------------------------------------------------------------------------------
void ReadBlockTPI(byte TPI_Offset, word NBytes, byte *pData)
{
	I2CReadBlock(TX_SLAVE_ADDR, TPI_Offset, NBytes, pData);
}

//------------------------------------------------------------------------------
// Function Name: WriteBlockTPI()
// Function Description: Write NBytes from a byte Buffer pointed to by Data to
//                      the TPI I2C slave starting at offset Addr
//------------------------------------------------------------------------------
void WriteBlockTPI(byte TPI_Offset, word NBytes, byte *pData)
{
	I2CWriteBlock(TX_SLAVE_ADDR, TPI_Offset, NBytes, pData);
}

//------------------------------------------------------------------------------
// Function Name: ReadIndexedRegister()
// Function Description: Read an indexed register value
//
//                  Write:
//                      1. 0xBC => Internal page num
//                      2. 0xBD => Indexed register offset
//
//                  Read:
//                      3. 0xBE => Returns the indexed register value
//------------------------------------------------------------------------------
byte ReadIndexedRegister(byte PageNum, byte RegOffset)
{
	WriteByteTPI(TPI_INTERNAL_PAGE_REG, PageNum);
	WriteByteTPI(TPI_INDEXED_OFFSET_REG,
		RegOffset);
	return ReadByteTPI(TPI_INDEXED_VALUE_REG);
}

//------------------------------------------------------------------------------
// Function Name: WriteIndexedRegister()
// Function Description: Write a value to an indexed register
//
//                  Write:
//                      1. 0xBC => Internal page num
//                      2. 0xBD => Indexed register offset
//                      3. 0xBE => Set the indexed register value
//------------------------------------------------------------------------------
void WriteIndexedRegister(byte PageNum, byte RegOffset, byte RegValue)
{
	WriteByteTPI(TPI_INTERNAL_PAGE_REG, PageNum);	// Internal page
	WriteByteTPI(TPI_INDEXED_OFFSET_REG,
		RegOffset);
	WriteByteTPI(TPI_INDEXED_VALUE_REG,
		RegValue);
}

//------------------------------------------------------------------------------
// Function Name: ReadModifyWriteIndexedRegister()
// Function Description:
//Write "Value" to all bits in TPI offset "Offset" that are set
//                  to "1" in "Mask"; Leave all other bits in "Offset"
//                  unchanged.
//------------------------------------------------------------------------------
void ReadModifyWriteIndexedRegister(
byte PageNum, byte RegOffset,
byte Mask, byte Value)
{
	byte Tmp;

	WriteByteTPI(TPI_INTERNAL_PAGE_REG, PageNum);
	WriteByteTPI(TPI_INDEXED_OFFSET_REG, RegOffset);
	Tmp = ReadByteTPI(TPI_INDEXED_VALUE_REG);

	Tmp &= ~Mask;
	Tmp |= (Value & Mask);

	WriteByteTPI(TPI_INDEXED_VALUE_REG, Tmp);
}

//------------------------------------------------------------------------------
void TXHAL_InitPostReset(void)
{
	// Set terminations to default.
	WriteByteTPI(TMDS_CONT_REG, 0x25);
	// HW debounce to 64ms (0x14)
	WriteByteTPI(0x7C, 0x14);
}

//------------------------------------------------------------------------------
// Function Name: TxHW_Reset()
// Function Description: Hardware reset Tx
//------------------------------------------------------------------------------
void TxHW_Reset(void)
{
	TPI_TRACE_PRINT((">>TxHW_Reset()\n"));

	siHdmiTx_HwResetPin = LOW;
	DelayMS(TX_HW_RESET_PERIOD);
	siHdmiTx_HwResetPin = HIGH;

	TXHAL_InitPostReset();
}

//------------------------------------------------------------------------------
// Function Name: InitializeStateVariables()
// Function Description: Initialize system state variables
//------------------------------------------------------------------------------
void InitializeStateVariables(void)
{
	g_sys.tmdsPoweredUp = FALSE;
	g_sys.hdmiCableConnected = FALSE;
	g_sys.dsRxPoweredUp = FALSE;

#ifdef DEV_SUPPORT_EDID
	g_edid.edidDataValid = FALSE;
#endif
}

//------------------------------------------------------------------------------
// Function Name: EnableTMDS()
// Function Description: Enable TMDS
//------------------------------------------------------------------------------
void EnableTMDS(void)
{
	TPI_DEBUG_PRINT(("TMDS -> Enabled\n"));
	ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG,
			   TMDS_OUTPUT_CONTROL_MASK,
			   TMDS_OUTPUT_CONTROL_ACTIVE);
	WriteByteTPI(TPI_PIX_REPETITION, tpivmode[0]);	// Write register 0x08
	g_sys.tmdsPoweredUp = TRUE;
}

//------------------------------------------------------------------------------
// Function Name: DisableTMDS()
// Function Description: Disable TMDS
//------------------------------------------------------------------------------
void DisableTMDS(void)
{
	TPI_DEBUG_PRINT(("TMDS -> Disabled\n"));
	ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG,
			   TMDS_OUTPUT_CONTROL_MASK |
			   AV_MUTE_MASK,
			   TMDS_OUTPUT_CONTROL_POWER_DOWN |
			   AV_MUTE_MUTED);
	g_sys.tmdsPoweredUp = FALSE;
}

//------------------------------------------------------------------------------
// Function Name: EnableInterrupts()
// Function Description: Enable the interrupts specified in the input parameter
//
// Accepts: A bit pattern with "1" for each interrupt that needs to be
//                  set in the Interrupt Enable Register (TPI offset 0x3C)
// Returns: TRUE
// Globals: none
//------------------------------------------------------------------------------
byte EnableInterrupts(byte Interrupt_Pattern)
{
	TPI_TRACE_PRINT((">>EnableInterrupts()\n"));
	ReadSetWriteTPI(TPI_INTERRUPT_ENABLE_REG, Interrupt_Pattern);

	return TRUE;
}

//------------------------------------------------------------------------------
// Function Name: DisableInterrupts()
// Function Description: Disable the interrupts specified in the input parameter
//
// Accepts: A bit pattern with "1" for each interrupt that needs to be
//                  cleared in the Interrupt Enable Register (TPI offset 0x3C)
// Returns: TRUE
// Globals: none
//------------------------------------------------------------------------------
byte DisableInterrupts(byte Interrupt_Pattern)
{
	TPI_TRACE_PRINT((">>DisableInterrupts()\n"));
	ReadClearWriteTPI(TPI_INTERRUPT_ENABLE_REG, Interrupt_Pattern);

	return TRUE;
}

#ifdef DEV_SUPPORT_EDID

 /*EDID*/ byte g_CommData[EDID_BLOCK_SIZE];

#define ReadBlockEDID(a, b, c)	\
	I2CReadBlock(EDID_ROM_ADDR, a, b, c)
#define ReadSegmentBlockEDID(a, b, c, d)	\
	siiReadSegmentBlockEDID(EDID_ROM_ADDR, a, b, d, c)

//------------------------------------------------------------------------------
// Function Name: GetDDC_Access()
// Function Description: Request access to DDC bus from the receiver
//
// Accepts: none
// Returns: TRUE or FLASE
// Globals: none
//------------------------------------------------------------------------------
#define T_DDC_ACCESS    50

byte GetDDC_Access(byte *SysCtrlRegVal)
{
	byte sysCtrl;
	byte DDCReqTimeout = T_DDC_ACCESS;
	byte TPI_ControlImage;

	TPI_TRACE_PRINT((">>GetDDC_Access()\n"));
	// Read and store original value. Will be passed into ReleaseDDC()
	sysCtrl = ReadByteTPI(TPI_SYSTEM_CONTROL_DATA_REG);
	*SysCtrlRegVal = sysCtrl;

	sysCtrl |= DDC_BUS_REQUEST_REQUESTED;
	WriteByteTPI(TPI_SYSTEM_CONTROL_DATA_REG, sysCtrl);
	// Loop till 0x1A[1] reads "1"
	while (DDCReqTimeout--) {
		TPI_ControlImage = ReadByteTPI(TPI_SYSTEM_CONTROL_DATA_REG);
		// When 0x1A[1] reads "1"
		if (TPI_ControlImage & DDC_BUS_GRANT_MASK) {
			sysCtrl |= DDC_BUS_GRANT_GRANTED;
			// lock host DDC bus access (0x1A[2:1] = 11)
			WriteByteTPI(TPI_SYSTEM_CONTROL_DATA_REG, sysCtrl);
			return TRUE;
		}
		// 0x1A[2] = "1" - Request the DDC bus
		WriteByteTPI(TPI_SYSTEM_CONTROL_DATA_REG, sysCtrl);
		DelayMS(200);
	}
	// Failure... restore original value.
	WriteByteTPI(TPI_SYSTEM_CONTROL_DATA_REG, sysCtrl);
	return FALSE;
}

//------------------------------------------------------------------------------
// Function Name: ReleaseDDC()
// Function Description: Release DDC bus
//
// Accepts: none
// Returns: TRUE if bus released successfully. FALSE if failed.
// Globals: none
//------------------------------------------------------------------------------
byte ReleaseDDC(byte SysCtrlRegVal)
{
	byte DDCReqTimeout = T_DDC_ACCESS;
	byte TPI_ControlImage;

	TPI_TRACE_PRINT((">>ReleaseDDC()\n"));
	// Just to be sure bits [2:1] are 0 before it is written
	SysCtrlRegVal &= ~BITS_2_1;

	// Loop till 0x1A[1] reads "0"
	while (DDCReqTimeout--) {
		// Cannot use ReadClearWriteTPI() here.
		//A read of TPI_SYSTEM_CONTROL is invalid while DDC is granted.
		// Doing so will return 0xFF,
		//and cause an invalid value to be written back.
		//ReadClearWriteTPI(TPI_SYSTEM_CONTROL,BITS_2_1);
		// 0x1A[2:1] = "0" - release the DDC bus

		WriteByteTPI(TPI_SYSTEM_CONTROL_DATA_REG, SysCtrlRegVal);
		TPI_ControlImage = ReadByteTPI(TPI_SYSTEM_CONTROL_DATA_REG);
		// When 0x1A[2:1] read "0"
		if (!(TPI_ControlImage & BITS_2_1))
			return TRUE;
	}
	// Failed to release DDC bus control
	return FALSE;
}

//------------------------------------------------------------------------------
// Function Name: CheckEDID_Header()
// Function Description:
//Checks if EDID header is correct per
//VESA E-EDID standard
//
// Accepts: Pointer to 1st EDID block
// Returns: TRUE or FLASE
// Globals: EDID data
//------------------------------------------------------------------------------
byte CheckEDID_Header(byte *Block)
{
	byte i = 0;

	if (Block[i])		// byte 0 must be 0
		return FALSE;

	for (i = 1; i < 1 + EDID_HDR_NO_OF_FF; i++) {
		if (Block[i] != 0xFF)	// bytes [1..6] must be 0xFF
			return FALSE;
	}

	if (Block[i])		// byte 7 must be 0
		return FALSE;

	return TRUE;
}

//------------------------------------------------------------------------------
// Function Name: DoEDID_Checksum()
// Function Description:
//      Calculte checksum of the 128 byte block pointed to by the
//                  pointer passed as parameter
//
// Accepts: Pointer to a 128 byte block whose checksum needs to be calculated
// Returns: TRUE or FLASE
// Globals: EDID data
//------------------------------------------------------------------------------
byte DoEDID_Checksum(byte *Block)
{
	byte i;
	byte CheckSum = 0;

	for (i = 0; i < EDID_BLOCK_SIZE; i++)
		CheckSum += Block[i];

	if (CheckSum)
		return FALSE;

	return TRUE;
}

//------------------------------------------------------------------------------
// Function Name: ParseEstablishedTiming()
// Function Description:
//Parse the established timing section of EDID Block 0 and
//                  print their decoded meaning to the screen.
//
// Accepts: Pointer to the 128 byte array
//where the data read from EDID Block0 is stored.
// Returns: none
// Globals: EDID data
//------------------------------------------------------------------------------
#if (CONF__TPI_EDID_PRINT == ENABLE)
void ParseEstablishedTiming(byte *Data)
{
	TPI_EDID_PRINT(("Parsing Established Timing:\n"));
	TPI_EDID_PRINT(("===========================\n"));

	// Parse Established Timing Byte #0
	if (Data[ESTABLISHED_TIMING_INDEX] & BIT_7)
		TPI_EDID_PRINT(("720 x 400 @ 70Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX] & BIT_6)
		TPI_EDID_PRINT(("720 x 400 @ 88Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX] & BIT_5)
		TPI_EDID_PRINT(("640 x 480 @ 60Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX] & BIT_4)
		TPI_EDID_PRINT(("640 x 480 @ 67Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX] & BIT_3)
		TPI_EDID_PRINT(("640 x 480 @ 72Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX] & BIT_2)
		TPI_EDID_PRINT(("640 x 480 @ 75Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX] & BIT_1)
		TPI_EDID_PRINT(("800 x 600 @ 56Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX] & BIT_0)
		TPI_EDID_PRINT(("800 x 400 @ 60Hz\n"));

	// Parse Established Timing Byte #1:
	if (Data[ESTABLISHED_TIMING_INDEX + 1] & BIT_7)
		TPI_EDID_PRINT(("800 x 600 @ 72Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX + 1] & BIT_6)
		TPI_EDID_PRINT(("800 x 600 @ 75Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX + 1] & BIT_5)
		TPI_EDID_PRINT(("832 x 624 @ 75Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX + 1] & BIT_4)
		TPI_EDID_PRINT(("1024 x 768 @ 87Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX + 1] & BIT_3)
		TPI_EDID_PRINT(("1024 x 768 @ 60Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX + 1] & BIT_2)
		TPI_EDID_PRINT(("1024 x 768 @ 70Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX + 1] & BIT_1)
		TPI_EDID_PRINT(("1024 x 768 @ 75Hz\n"));
	if (Data[ESTABLISHED_TIMING_INDEX + 1] & BIT_0)
		TPI_EDID_PRINT(("1280 x 1024 @ 75Hz\n"));

	// Parse Established Timing Byte #2:
	if (Data[ESTABLISHED_TIMING_INDEX + 2] & 0x80)
		TPI_EDID_PRINT(("1152 x 870 @ 75Hz\n"));

	if ((!Data[0]) && (!Data[ESTABLISHED_TIMING_INDEX + 1]) && (!Data[2]))
		TPI_EDID_PRINT(("No established video modes\n"));
}

//------------------------------------------------------------------------------
// Function Name: ParseStandardTiming()
// Function Description: Parse the standard timing section of EDID Block 0 and
//                  print their decoded meaning to the screen.
//
// Accepts: Pointer to the 128 byte array
//where the data read from EDID Block0 is stored.
// Returns: none
// Globals: EDID data
//------------------------------------------------------------------------------
void ParseStandardTiming(byte *Data)
{
	byte i;
	byte AR_Code;

	TPI_EDID_PRINT(("Parsing Standard Timing:\n"));
	TPI_EDID_PRINT(("========================\n"));

	for (i = 0; i < NUM_OF_STANDARD_TIMINGS; i += 2) {
		if ((Data[STANDARD_TIMING_OFFSET + i] == 0x01) &&
		    ((Data[STANDARD_TIMING_OFFSET + i + 1]) == 1)) {
			TPI_EDID_PRINT(("Standard Timing Undefined\n"));
		} else {

			AR_Code = (
				Data[STANDARD_TIMING_OFFSET +
				i + 1] & TWO_MSBITS) >> 6;
			TPI_EDID_PRINT(("Aspect Ratio: "));

			switch (AR_Code) {
			case AR16_10:
				TPI_EDID_PRINT(("16:10\n"));
				break;

			case AR4_3:
				TPI_EDID_PRINT(("4:3\n"));
				break;

			case AR5_4:
				TPI_EDID_PRINT(("5:4\n"));
				break;

			case AR16_9:
				TPI_EDID_PRINT(("16:9\n"));
				break;
			}
		}
	}
}

//------------------------------------------------------------------------------
// Function Name: ParseDetailedTiming()
// Function Description: Parse the detailed timing section of EDID Block 0 and
//                  print their decoded meaning to the screen.
//
// Accepts: Pointer to the 128 byte array
//where the data read from EDID Block0 is stored.
//              Offset to the beginning of the Detailed Timing Descriptor data.
//
//              Block indicator to distinguish between
//block #0 and blocks #2, #3
// Returns: none
// Globals: EDID data
//------------------------------------------------------------------------------
byte ParseDetailedTiming(byte *Data, byte DetailedTimingOffset, byte Block)
{
	byte TmpByte;
	byte i;
	word TmpWord;

	TmpWord = Data[DetailedTimingOffset + PIX_CLK_OFFSET] +
	    256 * Data[DetailedTimingOffset + PIX_CLK_OFFSET + 1];

	if (TmpWord == 0x00) {
		if (Block == EDID_BLOCK_0) {
			if (Data[DetailedTimingOffset + 3] == 0xFC) {
				TPI_EDID_PRINT(("Monitor Name: "));
				TPI_EDID_PRINT(("\n"));
			} else if (Data[DetailedTimingOffset + 3] == 0xFD) {
				TPI_EDID_PRINT(("Monitor Range Limits:\n\n"));

				i = 0;
			}
		}

		else if (Block == EDID_BLOCK_2_3) {
			TPI_EDID_PRINT(("\n"));
			return FALSE;
		}
	}

	else {

		TPI_EDID_PRINT(("Pixel Clock (MHz * 100): %d\n", (int)TmpWord));

		TmpWord = Data[DetailedTimingOffset + H_ACTIVE_OFFSET] +
		    256 * ((Data[DetailedTimingOffset +
		    H_ACTIVE_OFFSET + 2] >> 4) &
		    FOUR_LSBITS);

		TmpWord = Data[DetailedTimingOffset + H_BLANKING_OFFSET] +
		    256 * (Data[DetailedTimingOffset +
		    H_BLANKING_OFFSET + 1] &
		    FOUR_LSBITS);

		TmpWord = (Data[DetailedTimingOffset +
				V_ACTIVE_OFFSET]) +
		    256 * ((Data[DetailedTimingOffset +
		    (V_ACTIVE_OFFSET) + 2] >> 4) &
		    FOUR_LSBITS);

		TmpWord = Data[DetailedTimingOffset +
			       V_BLANKING_OFFSET] +
		    256 * (Data[DetailedTimingOffset +
		    V_BLANKING_OFFSET + 1] &
		    LOW_NIBBLE);

		TmpWord = Data[DetailedTimingOffset +
			       H_SYNC_OFFSET] +
		    256 * ((Data[DetailedTimingOffset +
		    (H_SYNC_OFFSET + 3)] >> 6) &
		    TWO_LSBITS);

		TmpWord = Data[DetailedTimingOffset +
			       H_SYNC_PW_OFFSET] +
		    256 * ((Data[DetailedTimingOffset +
		    (H_SYNC_PW_OFFSET + 2)] >> 4) &
		    TWO_LSBITS);

		TmpWord = (Data[DetailedTimingOffset +
				V_SYNC_OFFSET] >> 4) & FOUR_LSBITS +
		    256 * ((Data[DetailedTimingOffset +
		    (V_SYNC_OFFSET + 1)] >> 2) &
		    TWO_LSBITS);

		TmpWord = (Data[DetailedTimingOffset +
				V_SYNC_PW_OFFSET]) & FOUR_LSBITS +
		    256 * (Data[DetailedTimingOffset +
		    (V_SYNC_PW_OFFSET + 1)] &
		    TWO_LSBITS);

		TmpWord = Data[DetailedTimingOffset +
			       H_IMAGE_SIZE_OFFSET] +
		    256 * (((Data[DetailedTimingOffset +
				  (H_IMAGE_SIZE_OFFSET + 2)]) >> 4) &
				  FOUR_LSBITS);

		TmpWord = Data[DetailedTimingOffset +
			       V_IMAGE_SIZE_OFFSET] +
		    256 * (Data[DetailedTimingOffset +
		    (V_IMAGE_SIZE_OFFSET + 1)] &
		    FOUR_LSBITS);
		TPI_EDID_PRINT(("Vertical Image Size(mm): %d\n", (int)TmpWord));

		TmpByte = Data[DetailedTimingOffset + H_BORDER_OFFSET];

		TmpByte = Data[DetailedTimingOffset + V_BORDER_OFFSET];
		TPI_EDID_PRINT(("Vertical Border(Lines): %d\n", (int)TmpByte));

		TmpByte = Data[DetailedTimingOffset + FLAGS_OFFSET];
		if (TmpByte & BIT_7)
			TPI_EDID_PRINT(("Interlaced\n"));
		else
			TPI_EDID_PRINT(("Non-Interlaced\n"));

		if (!(TmpByte & BIT_5) && !(TmpByte & BIT_6))
			TPI_EDID_PRINT(("Normal Display, No Stereo\n"));

		if (!(TmpByte & BIT_3) && !(TmpByte & BIT_4))
			TPI_EDID_PRINT(("Analog Composite\n"));
		if ((TmpByte & BIT_3) && !(TmpByte & BIT_4))
			TPI_EDID_PRINT(("Bipolar Analog Composite\n"));
		else if (!(TmpByte & BIT_3) && (TmpByte & BIT_4))
			TPI_EDID_PRINT(("Digital Composite\n"));

		else if ((TmpByte & BIT_3) && (TmpByte & BIT_4))
			TPI_EDID_PRINT(("Digital Separate\n"));

		TPI_EDID_PRINT(("\n"));
	}
	return TRUE;
}

//------------------------------------------------------------------------------
// Function Name: ParseBlock_0_TimingDescripors()
// Function Description: Parse EDID Block 0 timing descriptors per EEDID 1.3
//                  standard. printf() values to screen.
//
// Accepts: Pointer to the 128 byte array
//where the data read from EDID Block0 is stored.
// Returns: none
// Globals: EDID data
//------------------------------------------------------------------------------
void ParseBlock_0_TimingDescripors(byte *Data)
{
	byte i;
	byte Offset;

	ParseEstablishedTiming(Data);
	ParseStandardTiming(Data);

	for (i = 0; i < NUM_OF_DETAILED_DESCRIPTORS; i++) {
		Offset = DETAILED_TIMING_OFFSET + (LONG_DESCR_LEN * i);
		ParseDetailedTiming(Data, Offset, EDID_BLOCK_0);
	}
}
#endif

//------------------------------------------------------------------------------
// Function Name: ParseEDID()
// Function Description:
//Extract sink properties from its EDID file and save them in
//                  global structure g_edid.
//
// Accepts: none
// Returns: TRUE or FLASE
// Globals: EDID data
// NOTE: Fields that are not supported by the
//9022/4 (such as deep color) were not parsed.
//------------------------------------------------------------------------------
byte ParseEDID(byte *pEdid, byte *numExt)
{
	byte i, j, k;

	TPI_EDID_PRINT(("\n"));
	TPI_EDID_PRINT(("EDID DATA (Segment = 0 Block = 0 Offset = %d):\n",
		(int)EDID_BLOCK_0_OFFSET));

	for (j = 0, i = 0; j < 128; j++) {
		k = pEdid[j];
		TPI_EDID_PRINT(("%2.2X ", (int)k));
		i++;

		if (i == 0x10) {
			TPI_EDID_PRINT(("\n"));
			i = 0;
		}
	}
	TPI_EDID_PRINT(("\n"));

	if (!CheckEDID_Header(pEdid)) {
		// first 8 bytes of EDID must be {0, FF, FF, FF, FF, FF, FF, 0}
		TPI_DEBUG_PRINT(("EDID -> Incorrect Header\n"));
		return EDID_INCORRECT_HEADER;
	}

	if (!DoEDID_Checksum(pEdid)) {
		// non-zero EDID checksum
		TPI_DEBUG_PRINT(("EDID -> Checksum Error\n"));
		return EDID_CHECKSUM_ERROR;
	}
#if (CONF__TPI_EDID_PRINT == ENABLE)
	ParseBlock_0_TimingDescripors(pEdid);
#endif

	*numExt = pEdid[NUM_OF_EXTEN_ADDR];

	if (!(*numExt)) {
		// No extensions to worry about
		return EDID_NO_861_EXTENSIONS;
	}
	//return Parse861Extensions(NumOfExtensions);
	// Parse 861 Extensions (short and long descriptors);
	return EDID_OK;
}

//------------------------------------------------------------------------------
// Function Name: Parse861ShortDescriptors()
// Function Description:
//Parse CEA-861 extension short descriptors of the EDID block
//                  passed as a parameter and
//save them in global structure g_edid.
//
// Accepts: A pointer to the EDID 861 Extension block being parsed.
// Returns: EDID_PARSED_OK if EDID parsed correctly. Error code if failed.
// Globals: EDID data
// NOTE: Fields that are not supported by
//the 9022/4 (such as deep color) were not parsed.
//------------------------------------------------------------------------------
byte Parse861ShortDescriptors(byte *Data)
{
	byte LongDescriptorOffset;
	byte DataBlockLength;
	byte DataIndex;
	byte ExtendedTagCode;
	byte VSDB_BaseOffset = 0;

	byte V_DescriptorIndex = 0;
	byte A_DescriptorIndex = 0;
	byte TagCode;

	byte i;
	byte j;

	if (Data[EDID_TAG_ADDR] != EDID_EXTENSION_TAG) {
		TPI_EDID_PRINT(("EDID -> Extension Tag Error\n"));
		return EDID_EXT_TAG_ERROR;
	}

	if (Data[EDID_REV_ADDR] != EDID_REV_THREE) {
		TPI_EDID_PRINT(("EDID -> Revision Error\n"));
		return EDID_REV_ADDR_ERROR;
	}

	LongDescriptorOffset =
		Data[LONG_DESCR_PTR_IDX];

	g_edid.UnderScan =
		((Data[MISC_SUPPORT_IDX]) >> 7) & LSBIT;
	g_edid.BasicAudio = ((Data[MISC_SUPPORT_IDX]) >> 6) & LSBIT;
	g_edid.YCbCr_4_4_4 = ((Data[MISC_SUPPORT_IDX]) >> 5) & LSBIT;
	g_edid.YCbCr_4_2_2 = ((Data[MISC_SUPPORT_IDX]) >> 4) & LSBIT;

	DataIndex = EDID_DATA_START;	// 4

	while (DataIndex < LongDescriptorOffset) {
		TagCode = (Data[DataIndex] >> 5) & THREE_LSBITS;
		DataBlockLength = Data[DataIndex++] & FIVE_LSBITS;
		if ((DataIndex + DataBlockLength) > LongDescriptorOffset) {
			TPI_EDID_PRINT(("EDID -> V Descriptor Overflow\n"));
			return EDID_V_DESCR_OVERFLOW;
		}

		i = 0;

		switch (TagCode) {
		case VIDEO_D_BLOCK:
			while ((i < DataBlockLength) &&
				(i < MAX_V_DESCRIPTORS)) {
				g_edid.VideoDescriptor
					[V_DescriptorIndex++] =
					Data[DataIndex++];
				i++;
			}
			DataIndex += DataBlockLength - i;

			break;

		case AUDIO_D_BLOCK:
			while (i < DataBlockLength / 3) {
				j = 0;
				while (j < AUDIO_DESCR_SIZE) {
					g_edid.AudioDescriptor
						[A_DescriptorIndex][j++] =
					    Data[DataIndex++];
			}
			A_DescriptorIndex++;
			i++;
			}
			break;

		case SPKR_ALLOC_D_BLOCK:
			g_edid.SpkrAlloc[i++] = Data[DataIndex++];
			DataIndex += 2;
			break;

		case USE_EXTENDED_TAG:
			ExtendedTagCode = Data[DataIndex++];

			switch (ExtendedTagCode) {
			case VIDEO_CAPABILITY_D_BLOCK:
				DataIndex += 1;
				break;

			case COLORIMETRY_D_BLOCK:
				g_edid.ColorimetrySupportFlags =
					Data[DataIndex++] & BITS_1_0;
				g_edid.MetadataProfile =
					Data[DataIndex++] & BITS_2_1_0;

				break;
			}
			break;

		case VENDOR_SPEC_D_BLOCK:
			VSDB_BaseOffset = DataIndex - 1;

			if ((Data[DataIndex++] == 0x03) &&
				(Data[DataIndex++] == 0x0C) &&
				(Data[DataIndex++] == 0x00))

				g_edid.HDMI_Sink = TRUE;
			else
				g_edid.HDMI_Sink = FALSE;

			g_edid.CEC_A_B = Data[DataIndex++];
			g_edid.CEC_C_D = Data[DataIndex++];

#ifdef DEV_SUPPORT_CEC
		// to set the physical address for CEC.
		{
			word phyAddr;

			phyAddr = (word)g_edid.CEC_C_D;
			phyAddr |= ((word)g_edid.CEC_A_B << 8);
			if (phyAddr != SI_CecGetDevicePA()) {
				// Yes!  So change the PA
				SI_CecSetDevicePA(phyAddr);
			}
		}
#endif

			if ((DataIndex + 7) > VSDB_BaseOffset + DataBlockLength)
				g_edid._3D_Supported = FALSE;
			else if (Data[DataIndex + 7] >> 7)
				g_edid._3D_Supported = TRUE;
			else
				g_edid._3D_Supported = FALSE;

			DataIndex += DataBlockLength -
				HDMI_SIGNATURE_LEN - CEC_PHYS_ADDR_LEN;
			TPI_EDID_PRINT(("\n"));
			break;

		default:
			TPI_EDID_PRINT(("EDID -> Unknown Tag Code\n"));
			return EDID_UNKNOWN_TAG_CODE;

	}
}

return EDID_SHORT_DESCRIPTORS_OK;
}

//------------------------------------------------------------------------------
// Function Name: Parse861LongDescriptors()
// Function Description: Parse CEA-861
//extension long descriptors of the EDID block
//                  passed as a parameter and printf() them to the screen.
//
// Accepts: A pointer to the EDID block being parsed
// Returns: An error code if no long
//descriptors found; EDID_PARSED_OK if descriptors found.
// Globals: none
//------------------------------------------------------------------------------
byte Parse861LongDescriptors(byte *Data)
{
	byte LongDescriptorsOffset;
	byte DescriptorNum = 1;

	LongDescriptorsOffset = Data[LONG_DESCR_PTR_IDX];

	if (!LongDescriptorsOffset) {
		TPI_DEBUG_PRINT(("EDID -> No Detailed Descriptors\n"));
		return EDID_NO_DETAILED_DESCRIPTORS;
	}

	while (LongDescriptorsOffset + LONG_DESCR_LEN < EDID_BLOCK_SIZE) {
#if (CONF__TPI_EDID_PRINT == ENABLE)
		if (!ParseDetailedTiming(Data,
			LongDescriptorsOffset, EDID_BLOCK_2_3))
			break;
#endif
		LongDescriptorsOffset += LONG_DESCR_LEN;
		DescriptorNum++;
	}

	return EDID_LONG_DESCRIPTORS_OK;
}

//------------------------------------------------------------------------------
// Function Name: Parse861Extensions()
// Function Description: Parse CEA-861
//extensions from EDID ROM (EDID blocks beyond
//                  block #0). Save short descriptors in global structure
//                  g_edid. printf() long descriptors to the screen.
//
// Accepts: The number of extensions in the EDID being parsed
// Returns: EDID_PARSED_OK if EDID parsed correctly. Error code if failed.
// Globals: EDID data
// NOTE: Fields that are not supported by
//the 9022/4 (such as deep color) were not parsed.
//------------------------------------------------------------------------------
byte Parse861Extensions(byte NumOfExtensions)
{
	byte i, j, k;

	byte ErrCode;

	//byte V_DescriptorIndex = 0;
	//byte A_DescriptorIndex = 0;

	byte Segment = 0;
	byte Block = 0;
	byte Offset = 0;

	g_edid.HDMI_Sink = FALSE;

	do {
		Block++;

		Offset = 0;
		if ((Block % 2) > 0)
			Offset = EDID_BLOCK_SIZE;

		Segment = (byte)(Block / 2);

		if (Block == 1)
			ReadBlockEDID(EDID_BLOCK_1_OFFSET,
			EDID_BLOCK_SIZE, g_CommData);
		else
			ReadSegmentBlockEDID(Segment,
			Offset, EDID_BLOCK_SIZE,
			g_CommData);

		TPI_EDID_PRINT(("\n"));

		for (j = 0, i = 0; j < 128; j++) {
			k = g_CommData[j];
			TPI_EDID_PRINT(("%2.2X ", (int)k));
			i++;

			if (i == 0x10) {
				TPI_EDID_PRINT(("\n"));
				i = 0;
			}
		}
		TPI_EDID_PRINT(("\n"));

		if ((NumOfExtensions > 1) && (Block == 1))
			continue;

		ErrCode = Parse861ShortDescriptors(g_CommData);
		if (ErrCode != EDID_SHORT_DESCRIPTORS_OK)
			return ErrCode;

		ErrCode = Parse861LongDescriptors(g_CommData);
		if (ErrCode != EDID_LONG_DESCRIPTORS_OK)
			return ErrCode;

	} while (Block < NumOfExtensions);

	return EDID_OK;
}

//------------------------------------------------------------------------------
// Function Name: DoEdidRead()
// Function Description: EDID processing
//
// Accepts: none
// Returns: TRUE or FLASE
// Globals: none
//------------------------------------------------------------------------------
byte DoEdidRead(void)
{
	byte SysCtrlReg;
	byte Result;
	byte NumOfExtensions;

	// If we already have valid EDID data, ship this whole thing
	if (g_edid.edidDataValid == FALSE) {
		// Request access to DDC bus from the receiver
		if (GetDDC_Access(&SysCtrlReg)) {
			ReadBlockEDID(EDID_BLOCK_0_OFFSET,
				EDID_BLOCK_SIZE, g_CommData);
			Result = ParseEDID(g_CommData, &NumOfExtensions);
			if (Result != EDID_OK) {
				if (Result == EDID_NO_861_EXTENSIONS) {
					g_edid.HDMI_Sink = FALSE;
					g_edid.YCbCr_4_4_4 = FALSE;
					g_edid.YCbCr_4_2_2 = FALSE;
					g_edid.CEC_A_B = 0x00;
					g_edid.CEC_C_D = 0x00;
				} else {
					g_edid.HDMI_Sink = TRUE;
					g_edid.YCbCr_4_4_4 = FALSE;
					g_edid.YCbCr_4_2_2 = FALSE;
					g_edid.CEC_A_B = 0x00;
					g_edid.CEC_C_D = 0x00;
				}
			} else {
				TPI_DEBUG_PRINT(("EDID -> Parse OK\n"));
				Result = Parse861Extensions(
					NumOfExtensions);
				if (Result != EDID_OK) {
					g_edid.HDMI_Sink = TRUE;
					g_edid.YCbCr_4_4_4 = FALSE;
					g_edid.YCbCr_4_2_2 = FALSE;
					g_edid.CEC_A_B = 0x00;
					g_edid.CEC_C_D = 0x00;
				}
			}

			if (!ReleaseDDC(SysCtrlReg))
				return EDID_DDC_BUS_RELEASE_FAILURE;
		} else {
			TPI_DEBUG_PRINT(("EDID -> DDC bus request failed\n"));
			g_edid.HDMI_Sink = TRUE;
			g_edid.YCbCr_4_4_4 = FALSE;
			g_edid.YCbCr_4_2_2 = FALSE;
			g_edid.CEC_A_B = 0x00;
			g_edid.CEC_C_D = 0x00;
			return EDID_DDC_BUS_REQ_FAILURE;
		}

		g_edid.edidDataValid = TRUE;
	}
	return 0;
}

#endif

#ifdef DEV_SUPPORT_HDCP

/*HDCP */

//------------------------------------------------------------------------------
// Function Name: IsHDCP_Supported()
// Function Description:
//Check Tx revision number to find if this Tx supports HDCP
//by reading the HDCP revision number from TPI register 0x30.
//
// Accepts: none
// Returns: TRUE if Tx supports HDCP. FALSE if not.
// Globals: none
//------------------------------------------------------------------------------
byte IsHDCP_Supported(void)
{
	byte HDCP_Rev;
	byte HDCP_Supported;

	TPI_TRACE_PRINT((">>IsHDCP_Supported()\n"));

	HDCP_Supported = TRUE;

	// Check Device ID
	HDCP_Rev = ReadByteTPI(TPI_HDCP_REVISION_DATA_REG);

	if (HDCP_Rev != (HDCP_MAJOR_REVISION_VALUE |
		HDCP_MINOR_REVISION_VALUE))
		HDCP_Supported = FALSE;
	HDCP_Rev = ReadByteTPI(TPI_AKSV_1_REG);
	if (HDCP_Rev == 0x09) {
		HDCP_Rev = ReadByteTPI(TPI_AKSV_2_REG);
		if (HDCP_Rev == 0x00) {
			HDCP_Rev = ReadByteTPI(TPI_AKSV_3_REG);
			if (HDCP_Rev == 0x02) {
				HDCP_Rev = ReadByteTPI(TPI_AKSV_4_REG);
				if (HDCP_Rev == 0x02) {
					HDCP_Rev = ReadByteTPI(TPI_AKSV_5_REG);
					if (HDCP_Rev == 0x0a)
						HDCP_Supported = FALSE;
				}
			}
		}
	}
	return HDCP_Supported;
}

//------------------------------------------------------------------------------
// Function Name: AreAKSV_OK()
// Function Description: Check if AKSVs contain 20 '0' and 20 '1'
//
// Accepts: none
// Returns: TRUE if 20 zeros and 20 ones found in AKSV. FALSE OTHERWISE
// Globals: none
//------------------------------------------------------------------------------
byte AreAKSV_OK(void)
{
	byte B_Data[AKSV_SIZE];
	byte NumOfOnes = 0;
	byte i, j;

	TPI_TRACE_PRINT((">>AreAKSV_OK()\n"));

	ReadBlockTPI(TPI_AKSV_1_REG, AKSV_SIZE, B_Data);

	for (i = 0; i < AKSV_SIZE; i++) {
		for (j = 0; j < BYTE_SIZE; j++) {
			if (B_Data[i] & 0x01)
				NumOfOnes++;
			B_Data[i] >>= 1;
		}
	}
	if (NumOfOnes != NUM_OF_ONES_IN_KSV)
		return FALSE;

	return TRUE;
}

//------------------------------------------------------------------------------
// Function Name: HDCP_Off()
// Function Description: Switch hdcp off.
//------------------------------------------------------------------------------
void HDCP_Off(void)
{
	TPI_TRACE_PRINT((">>HDCP_Off()\n"));

	// AV MUTE
	ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG,
	AV_MUTE_MASK, AV_MUTE_MUTED);
	WriteByteTPI(TPI_HDCP_CONTROL_DATA_REG, PROTECTION_LEVEL_MIN);

	g_hdcp.HDCP_Started = FALSE;
	g_hdcp.HDCP_LinkProtectionLevel =
	    EXTENDED_LINK_PROTECTION_NONE | LOCAL_LINK_PROTECTION_NONE;
}

//------------------------------------------------------------------------------
// Function Name: HDCP_On()
// Function Description: Switch hdcp on.
//------------------------------------------------------------------------------
void HDCP_On(void)
{
	if (g_hdcp.HDCP_Override == FALSE) {
		TPI_DEBUG_PRINT(("HDCP Started\n"));

		WriteByteTPI(TPI_HDCP_CONTROL_DATA_REG, PROTECTION_LEVEL_MAX);

		g_hdcp.HDCP_Started = TRUE;
	} else {
		g_hdcp.HDCP_Started = FALSE;
	}
}

//------------------------------------------------------------------------------
// Function Name: RestartHDCP()
// Function Description: Restart HDCP.
//------------------------------------------------------------------------------
void RestartHDCP(void)
{
	TPI_DEBUG_PRINT(("HDCP -> Restart\n"));

	DisableTMDS();
	HDCP_Off();
	EnableTMDS();
}

//------------------------------------------------------------------------------
// Function Name: HDCP_Init()
// Function Description: Tests Tx and Rx support of HDCP. If found, checks if
//                  and attempts to set the security level accordingly.
//
// Accepts: none
// Returns: TRUE if HW TPI started successfully. FALSE if failed to.
// Globals: HDCP_TxSupports -
//initialized to FALSE, set to TRUE if supported by this device
//                 HDCP_AksvValid -
//initialized to FALSE, set to TRUE if valid AKSVs are read from this device
//                 HDCP_Started - initialized to FALSE
//                 HDCP_LinkProtectionLevel -
//initialized to (EXTENDED_LINK_PROTECTION_NONE |
//LOCAL_LINK_PROTECTION_NONE)
//------------------------------------------------------------------------------
void HDCP_Init(void)
{
	TPI_TRACE_PRINT((">>HDCP_Init()\n"));

	g_hdcp.HDCP_TxSupports = FALSE;
	g_hdcp.HDCP_AksvValid = FALSE;
	g_hdcp.HDCP_Started = FALSE;
	g_hdcp.HDCP_LinkProtectionLevel =
	    EXTENDED_LINK_PROTECTION_NONE | LOCAL_LINK_PROTECTION_NONE;

	// This is TX-related... need only be done once.
	if (!IsHDCP_Supported()) {
		// The TX does not support HDCP,
		//so authentication will never be attempted.
		// Video will be shown as soon as TMDS is enabled.
		TPI_DEBUG_PRINT(("HDCP -> TX does not support HDCP\n"));
		return;
	}
	g_hdcp.HDCP_TxSupports = TRUE;

	// This is TX-related... need only be done once.
	if (!AreAKSV_OK()) {
		// The TX supports HDCP, but does not have valid AKSVs.
		// Video will not be shown.
		TPI_DEBUG_PRINT(("HDCP -> Illegal AKSV\n"));
		return;
	}
	g_hdcp.HDCP_AksvValid = TRUE;

#ifdef KSVFORWARD
	// Enable the KSV Forwarding feature and the KSV FIFO Intererrupt
	ReadModifyWriteTPI(
	TPI_HDCP_CONTROL_DATA_REG,
	KSV_FORWARD_MASK, KSV_FORWARD_ENABLE);
	ReadModifyWriteTPI(TPI_KSV_FIFO_READY_INT_EN, KSV_FIFO_READY_EN_MASK,
			   KSV_FIFO_READY_ENABLE);
#endif

	TPI_DEBUG_PRINT(("HDCP -> Supported by TX, AKSVs valid\n"));
}

#ifdef READKSV
//------------------------------------------------------------------------------
// Function Name: IsRepeater()
// Function Description: Test if sink is a repeater.
//
// Accepts: none
// Returns: TRUE if sink is a repeater. FALSE if not.
// Globals: none
//------------------------------------------------------------------------------
byte IsRepeater(void)
{
	byte RegImage;

	TPI_TRACE_PRINT((">>IsRepeater()\n"));

	RegImage = ReadByteTPI(TPI_HDCP_QUERY_DATA_REG);

	if (RegImage & HDCP_REPEATER_MASK)
		return TRUE;

	return FALSE;		// not a repeater
}

//------------------------------------------------------------------------------
// Function Name: ReadBlockHDCP()
// Function Description: Read NBytes from offset
//Addr of the HDCP slave address
//                      into a byte Buffer pointed to by Data
//
// Accepts: HDCP port offset,
//number of bytes to read and a pointer to the data buffer where
//               the data read will be saved
// Returns: none
// Globals: none
//------------------------------------------------------------------------------
void ReadBlockHDCP(byte TPI_Offset, word NBytes, byte *pData)
{
	I2CReadBlock(HDCP_SLAVE_ADDR, TPI_Offset, NBytes, pData);
}

//------------------------------------------------------------------------------
// Function Name: GetKSV()
// Function Description: Collect all downstrean KSV for verification.
//
// Accepts: none
// Returns: TRUE if KSVs collected successfully. False if not.
// Globals: KSV_Array[],
//The buffer is limited to KSV_ARRAY_SIZE
//due to the 8051 implementation.
//------------------------------------------------------------------------------
byte GetKSV(void)
{
	byte i;
	word KeyCount;
	byte KSV_Array[KSV_ARRAY_SIZE];

	TPI_TRACE_PRINT((">>GetKSV()\n"));
	ReadBlockHDCP(DDC_BSTATUS_ADDR_L, 1, &i);
	KeyCount = (i & DEVICE_COUNT_MASK) * 5;
	if (KeyCount != 0)
		ReadBlockHDCP(DDC_KSV_FIFO_ADDR, KeyCount, KSV_Array);

	return TRUE;
}
#endif

//------------------------------------------------------------------------------
// Function Name: HDCP_CheckStatus()
// Function Description: Check HDCP status.
//
// Accepts: InterruptStatus
// Returns: none
// Globals: none
//------------------------------------------------------------------------------
void HDCP_CheckStatus(byte InterruptStatusImage)
{
	byte QueryData;
	byte LinkStatus;
	byte RegImage;
	byte NewLinkProtectionLevel;

#ifdef READKSV
	byte RiCnt;
#endif
#ifdef KSVFORWARD
	byte ksv;
#endif

	if ((g_hdcp.HDCP_TxSupports == TRUE) &&
		(g_hdcp.HDCP_AksvValid == TRUE)) {
		if ((g_hdcp.HDCP_LinkProtectionLevel ==
		     (EXTENDED_LINK_PROTECTION_NONE |
		     LOCAL_LINK_PROTECTION_NONE))
		    && (g_hdcp.HDCP_Started == FALSE)) {
			QueryData = ReadByteTPI(TPI_HDCP_QUERY_DATA_REG);

			if (QueryData &
				PROTECTION_TYPE_MASK) {
				HDCP_On();
			}
		}
		// Check if Link Status has changed:
		if (InterruptStatusImage & SECURITY_CHANGE_EVENT) {
			TPI_DEBUG_PRINT(("HDCP -> "));

			LinkStatus = ReadByteTPI(TPI_HDCP_QUERY_DATA_REG);
			LinkStatus &= LINK_STATUS_MASK;

			ClearInterrupt(SECURITY_CHANGE_EVENT);

			switch (LinkStatus) {
			case LINK_STATUS_NORMAL:
				TPI_DEBUG_PRINT(("Link = Normal\n"));
				break;

			case LINK_STATUS_LINK_LOST:
				TPI_DEBUG_PRINT(("Link = Lost\n"));
				RestartHDCP();
				break;

			case LINK_STATUS_RENEGOTIATION_REQ:
				HDCP_Off();
				HDCP_On();
				break;

			case LINK_STATUS_LINK_SUSPENDED:
				TPI_DEBUG_PRINT(("Link = Suspended\n"));
				HDCP_On();
				break;
			}
		}
		// Check if HDCP state has changed:
		if (InterruptStatusImage & HDCP_CHANGE_EVENT) {
			RegImage = ReadByteTPI(TPI_HDCP_QUERY_DATA_REG);

			NewLinkProtectionLevel =
			    RegImage & (
			    EXTENDED_LINK_PROTECTION_MASK |
			    LOCAL_LINK_PROTECTION_MASK);
			if (NewLinkProtectionLevel !=
				g_hdcp.HDCP_LinkProtectionLevel) {
				TPI_DEBUG_PRINT(("HDCP -> "));

				g_hdcp.HDCP_LinkProtectionLevel =
					NewLinkProtectionLevel;

				switch (g_hdcp.HDCP_LinkProtectionLevel) {
				case (EXTENDED_LINK_PROTECTION_NONE |
					LOCAL_LINK_PROTECTION_NONE):
					RestartHDCP();
					break;

				case LOCAL_LINK_PROTECTION_SECURE:

					if (IsHDMI_Sink()) {
						ReadModifyWriteTPI(
							TPI_AUDIO_INTERFACE_REG,
						AUDIO_MUTE_MASK,
						AUDIO_MUTE_NORMAL);
					}

					ReadModifyWriteTPI(
						TPI_SYSTEM_CONTROL_DATA_REG,
							   AV_MUTE_MASK,
							   AV_MUTE_NORMAL);
					break;

				case (EXTENDED_LINK_PROTECTION_SECURE |
					LOCAL_LINK_PROTECTION_SECURE):
#ifdef READKSV
				if (IsRepeater()) {
					RiCnt = ReadIndexedRegister(
						INDEXED_PAGE_0, 0x25);
					while (RiCnt > 0x70) {
						RiCnt =
						    ReadIndexedRegister(
						    INDEXED_PAGE_0,
							0x25);
					}
					ReadModifyWriteTPI(
						TPI_SYSTEM_CONTROL_DATA_REG,
							   0x06, 0x06);
					GetKSV();
					RiCnt = ReadByteTPI(
						TPI_SYSTEM_CONTROL_DATA_REG);
					ReadModifyWriteTPI(
						TPI_SYSTEM_CONTROL_DATA_REG,
							   0x08, 0x00);
				}
#endif
					break;

				default:
					RestartHDCP();
					break;
				}
			}
#ifdef KSVFORWARD
			// Check if KSV FIFO is ready and forward - Bug# 17892
			// If interrupt never goes off:
			//   a) KSV formwarding is not enabled
			//   b) not a repeater
			//   c) a repeater with device count == 0
			// and therefore no KSV list to forward
			if ((ReadByteTPI(TPI_KSV_FIFO_READY_INT) &
				KSV_FIFO_READY_MASK) ==
			    KSV_FIFO_READY_YES) {
				ReadModifyWriteTPI(
					TPI_KSV_FIFO_READY_INT,
					KSV_FIFO_READY_MASK,
						   KSV_FIFO_READY_YES);
				do {
					ksv = ReadByteTPI(
						TPI_KSV_FIFO_STATUS_REG);
					if (ksv & KSV_FIFO_COUNT_MASK)
						ksv = ReadByteTPI(
							TPI_KSV_FIFO_VALUE_REG);
				} while ((ksv &
				KSV_FIFO_LAST_MASK) ==
				KSV_FIFO_LAST_NO);
			}
#endif
			ClearInterrupt(HDCP_CHANGE_EVENT);
		}
	}
}
#endif

/*AV CONFIG*/

//-----------------------------
// Video mode table
//-----------------------------
struct ModeIdType {
	byte Mode_C1;
	byte Mode_C2;
	byte SubMode;
};

struct PxlLnTotalType {
	word Pixels;
	word Lines;
};

struct HVPositionType {
	word H;
	word V;
};

struct HVResolutionType {
	word H;
	word V;
};

struct TagType {
	byte RefrTypeVHPol;
	word VFreq;
	struct PxlLnTotalType Total;
};

struct _656Type {
	byte IntAdjMode;
	word HLength;
	byte VLength;
	word Top;
	word Dly;
	word HBit2HSync;
	byte VBit2VSync;
	word Field2Offset;
};

struct Vspace_Vblank {
	byte VactSpace1;
	byte VactSpace2;
	byte Vblank1;
	byte Vblank2;
	byte Vblank3;
};

//
// WARNING!  The entries in this enum
//must remian in the samre order as the PC Codes part
// of the VideoModeTable[].
//
enum PcModeCode_t {
	PC_640x350_85_08 = 0,
	PC_640x400_85_08,
	PC_720x400_70_08,
	PC_720x400_85_04,
	PC_640x480_59_94,
	PC_640x480_72_80,
	PC_640x480_75_00,
	PC_640x480_85_00,
	PC_800x600_56_25,
	PC_800x600_60_317,
	PC_800x600_72_19,
	PC_800x600_75,
	PC_800x600_85_06,
	PC_1024x768_60,
	PC_1024x768_70_07,
	PC_1024x768_75_03,
	PC_1024x768_85,
	PC_1152x864_75,
	PC_1600x1200_60,
	PC_1280x768_59_95,
	PC_1280x768_59_87,
	PC_280x768_74_89,
	PC_1280x768_85,
	PC_1280x960_60,
	PC_1280x960_85,
	PC_1280x1024_60,
	PC_1280x1024_75,
	PC_1280x1024_85,
	PC_1360x768_60,
	PC_1400x105_59_95,
	PC_1400x105_59_98,
	PC_1400x105_74_87,
	PC_1400x105_84_96,
	PC_1600x1200_65,
	PC_1600x1200_70,
	PC_1600x1200_75,
	PC_1600x1200_85,
	PC_1792x1344_60,
	PC_1792x1344_74_997,
	PC_1856x1392_60,
	PC_1856x1392_75,
	PC_1920x1200_59_95,
	PC_1920x1200_59_88,
	PC_1920x1200_74_93,
	PC_1920x1200_84_93,
	PC_1920x1440_60,
	PC_1920x1440_75,
	PC_12560x1440_60,
	PC_SIZE			// Must be last
};

struct VModeInfoType {
	struct ModeIdType ModeId;
	word PixClk;
	struct TagType Tag;
	struct HVPositionType Pos;
	struct HVResolutionType Res;
	byte AspectRatio;
	struct _656Type _656;
	byte PixRep;
	struct Vspace_Vblank VsVb;
	byte _3D_Struct;
};

#define NSM                     0	// No Sub-Mode

#define	DEFAULT_VIDEO_MODE		0	// 640  x 480p @ 60 VGA

#define ProgrVNegHNeg           0x00
#define ProgrVNegHPos		0x01
#define ProgrVPosHNeg		0x02
#define ProgrVPosHPos		0x03

#define InterlaceVNegHNeg	0x04
#define InterlaceVPosHNeg      0x05
#define InterlaceVNgeHPos	0x06
#define InterlaceVPosHPos	0x07

#define VIC_BASE			0
#define HDMI_VIC_BASE           43
#define VIC_3D_BASE		47
#define PC_BASE			64

// Aspect ratio
//=================================================
#define R_4				0	// 4:3
#define R_4or16			1	// 4:3 or 16:9
#define R_16				2	// 16:9

//
// These are the VIC codes that we support in a 3D mode
//
#define VIC_FOR_480P_60Hz_4X3		2
#define VIC_FOR_480P_60Hz_16X9		3
#define VIC_FOR_720P_60Hz			4
#define VIC_FOR_1080i_60Hz			5
#define VIC_FOR_1080p_60Hz			16
#define VIC_FOR_720P_50Hz			19
#define VIC_FOR_1080i_50Hz			20
#define VIC_FOR_1080p_50Hz			31
#define VIC_FOR_1080p_24Hz			32


const struct VModeInfoType VModesTable[100] = {};


//------------------------------------------------------------------------------
// Aspect Ratio table defines the aspect ratio
//as function of VIC. This table
// should be used in conjunction with the 861-D
//part of VModeInfoType VModesTable[]
// (formats 0 - 59) because some formats
//that differ only in their AR are grouped
// together (e.g., formats 2 and 3).
//------------------------------------------------------------------------------
const byte AspectRatioTable[] = {
	R_4, R_4, R_16, R_16, R_16, R_4, R_16, R_4, R_16, R_4,
	R_16, R_4, R_16, R_4, R_16, R_16, R_4, R_16, R_16, R_16,
	R_4, R_16, R_4, R_16, R_4, R_16, R_4, R_16, R_4, R_16,
	R_16, R_16, R_16, R_16, R_4, R_16, R_4, R_16, R_16, R_16,
	R_16, R_4, R_16, R_4, R_16, R_16, R_16, R_4, R_16, R_4,
	R_16, R_4, R_16, R_4, R_16, R_4, R_16, R_4, R_16
};

//------------------------------------------------------------------------------
// VIC to Indexc table defines which
//VideoModeTable entry is appropreate for this VIC code.
// Note: This table is valid ONLY for VIC
//codes in 861-D formats, NOT for HDMI_VIC codes
// or 3D codes!
//------------------------------------------------------------------------------
const byte VIC2Index[] = {
	0, 0, 1, 1, 2, 3, 4, 4, 5, 5,
	7, 7, 8, 8, 10, 10, 11, 12, 12, 13,
	14, 15, 15, 16, 16, 19, 19, 20, 20, 23,
	23, 24, 25, 26, 27, 28, 28, 29, 29, 30,
	31, 32, 33, 33, 34, 34, 35, 36, 37, 37,
	38, 38, 39, 39, 40, 40, 41, 41, 42, 42
};

//------------------------------------------------------------------------------
// Function Name: ConvertVIC_To_VM_Index()
// Function Description: Convert Video Identification
//Code to the corresponding
// index of VModesTable[]. Conversion also depends on the
//value of the 3D_Structure parameter in
//the case of 3D video format.
// Accepts: VIC to be converted; 3D_Structure value
// Returns: Index into VModesTable[] corrsponding to VIC
// Globals: VModesTable[] siHdmiTx
// Note: Conversion is for 861-D formats, HDMI_VIC or 3D
//------------------------------------------------------------------------------
byte ConvertVIC_To_VM_Index(void)
{
	byte index;

	//
	// The global VideoModeDescription contains
	//all the information we need about
	// the Video mode for use to find its entry in the Videio mode table.
	//
	// The first issue.  The "VIC" may be a
	//891-D VIC code, or it might be an
	// HDMI_VIC code, or it might be a 3D code.
	//Each require different handling
	// to get the proper video mode table index.
	//
	if (siHdmiTx.HDMIVideoFormat == VMD_HDMIFORMAT_CEA_VIC) {
		//
		// This is a regular 861-D format VIC,
		//so we use the VIC to Index
		// table to look up the index.
		//
		index = VIC2Index[siHdmiTx.VIC];
	} else if (siHdmiTx.HDMIVideoFormat == VMD_HDMIFORMAT_HDMI_VIC) {
		//
		// HDMI_VIC conversion is simple.
		//We need to subtract one because the codes start
		// with one instead of zero.
		//These values are from HDMI 1.4 Spec Table 8-13.
		//
		if ((siHdmiTx.VIC < 1) || (siHdmiTx.VIC > 4))
			index = DEFAULT_VIDEO_MODE;
		else
			index = (HDMI_VIC_BASE - 1) + siHdmiTx.VIC;
	} else if (siHdmiTx.HDMIVideoFormat == VMD_HDMIFORMAT_3D) {
		//
		// Currently there are only a few VIC modes
		//that we can do in 3D.  If the VIC code is not
		// one of these OR if the packing type is not
		//supported for that VIC code, then it is an
		// error and we go to the default video mode.
		//See HDMI Spec 1.4 Table H-6.
		//
		switch (siHdmiTx.VIC) {
		case VIC_FOR_480P_60Hz_4X3:
		case VIC_FOR_480P_60Hz_16X9:
			// We only support Side-by-Side (Half) for these modes
			if (siHdmiTx.ThreeDStructure == SIDE_BY_SIDE_HALF)
				index = VIC_3D_BASE + 0;
			else
				index = DEFAULT_VIDEO_MODE;
			break;

		case VIC_FOR_720P_60Hz:
			switch (siHdmiTx.ThreeDStructure) {
			case FRAME_PACKING:
				index = VIC_3D_BASE + 1;
				break;
			default:
				index = DEFAULT_VIDEO_MODE;
				break;
			}
			break;

		case VIC_FOR_1080i_60Hz:
			switch (siHdmiTx.ThreeDStructure) {
			case FRAME_PACKING:
				index = VIC_3D_BASE + 2;
				break;
			case VMD_3D_FIELDALTERNATIVE:
				index = VIC_3D_BASE + 3;
				break;
			default:
				index = DEFAULT_VIDEO_MODE;
				break;
			}
			break;

		case VIC_FOR_1080p_60Hz:
			switch (siHdmiTx.ThreeDStructure) {
			case FRAME_PACKING:
				index = VIC_3D_BASE + 4;
				break;
			case VMD_3D_LINEALTERNATIVE:
				index = VIC_3D_BASE + 5;
				break;
			case SIDE_BY_SIDE_FULL:
				index = VIC_3D_BASE + 6;
				break;
			case SIDE_BY_SIDE_HALF:
				index = VIC_3D_BASE + 7;
				break;
			default:
				index = DEFAULT_VIDEO_MODE;
				break;
			}
			break;

		case VIC_FOR_720P_50Hz:
			switch (siHdmiTx.ThreeDStructure) {
			case FRAME_PACKING:
				index = VIC_3D_BASE + 8;
				break;
			case VMD_3D_LDEPTH:
				index = VIC_3D_BASE + 9;
				break;
			case VMD_3D_LDEPTHGRAPHICS:
				index = VIC_3D_BASE + 10;
				break;
			default:
				index = DEFAULT_VIDEO_MODE;
				break;
			}
			break;

		case VIC_FOR_1080i_50Hz:
			switch (siHdmiTx.ThreeDStructure) {
			case FRAME_PACKING:
				index = VIC_3D_BASE + 11;
				break;
			case VMD_3D_FIELDALTERNATIVE:
				index = VIC_3D_BASE + 12;
				break;
			default:
				index = DEFAULT_VIDEO_MODE;
				break;
			}
			break;

		case VIC_FOR_1080p_50Hz:
			switch (siHdmiTx.ThreeDStructure) {
			case FRAME_PACKING:
				index = VIC_3D_BASE + 13;
				break;
			case VMD_3D_LINEALTERNATIVE:
				index = VIC_3D_BASE + 14;
				break;
			case SIDE_BY_SIDE_FULL:
				index = VIC_3D_BASE + 15;
				break;
			default:
				index = DEFAULT_VIDEO_MODE;
				break;
			}
			break;

		case VIC_FOR_1080p_24Hz:
			switch (siHdmiTx.ThreeDStructure) {
			case FRAME_PACKING:
				index = VIC_3D_BASE + 16;
				break;
			default:
				index = DEFAULT_VIDEO_MODE;
				break;
			}
			break;

		default:
			index = DEFAULT_VIDEO_MODE;
			break;
		}
	} else if (siHdmiTx.HDMIVideoFormat == VMD_HDMIFORMAT_PC) {
		if (siHdmiTx.VIC < PC_SIZE)
			index = siHdmiTx.VIC + PC_BASE;
		else
			index = DEFAULT_VIDEO_MODE;
	} else
		index = DEFAULT_VIDEO_MODE;

	return index;
}

// Patches
//========
byte TPI_REG0x63_SAVED;

//------------------------------------------------------------------------------
// Function Name: SetEmbeddedSync()
// Function Description: Set the 9022/4 registers to extract embedded sync.
//
// Accepts: Index of video mode to set
// Returns: TRUE
// Globals: VModesTable[]
//------------------------------------------------------------------------------
byte SetEmbeddedSync(void)
{
	byte ModeTblIndex;
	word H_Bit_2_H_Sync;
	word Field2Offset;
	word H_SyncWidth;

	byte V_Bit_2_V_Sync;
	byte V_SyncWidth;
	byte B_Data[8];

	TPI_TRACE_PRINT((">>SetEmbeddedSync()\n"));

	ReadModifyWriteIndexedRegister(INDEXED_PAGE_0, 0x0A, 0x01, 0x01);

	ReadClearWriteTPI(TPI_SYNC_GEN_CTRL,
		MSBIT);	// set 0x60[7] = 0 for DE mode
	WriteByteTPI(TPI_DE_CTRL, 0x30);
	ReadSetWriteTPI(TPI_SYNC_GEN_CTRL,
		MSBIT);	// set 0x60[7] = 1 for Embedded Sync

	ModeTblIndex = ConvertVIC_To_VM_Index();

	H_Bit_2_H_Sync = VModesTable[ModeTblIndex]._656.HBit2HSync;
	Field2Offset = VModesTable[ModeTblIndex]._656.Field2Offset;
	H_SyncWidth = VModesTable[ModeTblIndex]._656.HLength;
	V_Bit_2_V_Sync = VModesTable[ModeTblIndex]._656.VBit2VSync;
	V_SyncWidth = VModesTable[ModeTblIndex]._656.VLength;

	B_Data[0] = H_Bit_2_H_Sync &
		LOW_BYTE;	// Setup HBIT_TO_HSYNC 8 LSBits (0x62)

	B_Data[1] = (H_Bit_2_H_Sync >> 8) &
		TWO_LSBITS;	// HBIT_TO_HSYNC 2 MSBits
	//B_Data[1] |= BIT_EN_SYNC_EXTRACT;
	// and Enable Embedded Sync to 0x63
	TPI_REG0x63_SAVED = B_Data[1];

	B_Data[2] = Field2Offset & LOW_BYTE;
	B_Data[3] = (Field2Offset >> 8) & LOW_NIBBLE;

	B_Data[4] = H_SyncWidth & LOW_BYTE;
	B_Data[5] = (H_SyncWidth >> 8) & TWO_LSBITS;	// HWIDTH to 0x66, 0x67
	B_Data[6] = V_Bit_2_V_Sync;	// VBIT_TO_VSYNC to 0x68
	B_Data[7] = V_SyncWidth;	// VWIDTH to 0x69

	WriteBlockTPI(TPI_HBIT_TO_HSYNC_7_0, 8, &B_Data[0]);

	return TRUE;
}

//------------------------------------------------------------------------------
// Function Name: EnableEmbeddedSync()
// Function Description: EnableEmbeddedSync
//
// Accepts: none
// Returns: none
// Globals: none
//------------------------------------------------------------------------------
void EnableEmbeddedSync(void)
{
	TPI_TRACE_PRINT((">>EnableEmbeddedSync()\n"));

	ReadClearWriteTPI(TPI_SYNC_GEN_CTRL, MSBIT);
	WriteByteTPI(TPI_DE_CTRL, 0x30);
	ReadSetWriteTPI(TPI_SYNC_GEN_CTRL, MSBIT);
	ReadSetWriteTPI(TPI_DE_CTRL, BIT_6);
}

//------------------------------------------------------------------------------
// Function Name: SetDE()
// Function Description: Set the 9022/4 internal DE generator parameters
//
// Accepts: none
// Returns: DE_SET_OK
// Globals: none
//
// NOTE: 0x60[7] must be set to "0" for the following settings to take effect
//------------------------------------------------------------------------------
byte SetDE(void)
{
	byte RegValue;
	byte ModeTblIndex;

	word H_StartPos, V_StartPos;
	word Htotal, Vtotal;
	word H_Res, V_Res;

	byte Polarity;
	byte B_Data[12];

	TPI_TRACE_PRINT((">>SetDE()\n"));

	ModeTblIndex = ConvertVIC_To_VM_Index();

	if (VModesTable[ModeTblIndex]._3D_Struct != NO_3D_SUPPORT)
		return DE_CANNOT_BE_SET_WITH_3D_MODE;

	RegValue = ReadByteTPI(TPI_SYNC_GEN_CTRL);

	if (RegValue & BIT_7)
		return DE_CANNOT_BE_SET_WITH_EMBEDDED_SYNC;

	H_StartPos = VModesTable[ModeTblIndex].Pos.H;
	V_StartPos = VModesTable[ModeTblIndex].Pos.V;

	Htotal = VModesTable[ModeTblIndex].Tag.Total.Pixels;
	Vtotal = VModesTable[ModeTblIndex].Tag.Total.Lines;

	Polarity = (~VModesTable[ModeTblIndex].Tag.RefrTypeVHPol) & TWO_LSBITS;

	H_Res = VModesTable[ModeTblIndex].Res.H;

	if ((VModesTable[ModeTblIndex].Tag.RefrTypeVHPol & 0x04))
		V_Res = (VModesTable[ModeTblIndex].Res.V) >> 1;
	else
		V_Res = (VModesTable[ModeTblIndex].Res.V);

	B_Data[0] = H_StartPos & LOW_BYTE;	// 8 LSB of DE DLY in 0x62

	B_Data[1] = (H_StartPos >> 8) &
		TWO_LSBITS;	// 2 MSBits of DE DLY to 0x63
	B_Data[1] |= (Polarity << 4);	// V and H polarity
	B_Data[1] |= BIT_EN_DE_GEN;	// enable DE generator

	B_Data[2] = V_StartPos & SEVEN_LSBITS;	// DE_TOP in 0x64
	B_Data[3] = 0x00;	// 0x65 is reserved
	B_Data[4] = H_Res & LOW_BYTE;	// 8 LSBits of DE_CNT in 0x66
	B_Data[5] = (H_Res >> 8) & LOW_NIBBLE;	// 4 MSBits of DE_CNT in 0x67
	B_Data[6] = V_Res & LOW_BYTE;	// 8 LSBits of DE_LIN in 0x68
	B_Data[7] = (V_Res >> 8) &
		THREE_LSBITS;	// 3 MSBits of DE_LIN in 0x69
	B_Data[8] = Htotal & LOW_BYTE;	// 8 LSBits of H_RES in 0x6A
	B_Data[9] = (Htotal >> 8) & LOW_NIBBLE;	// 4 MSBITS of H_RES in 0x6B
	B_Data[10] = Vtotal & LOW_BYTE;	// 8 LSBits of V_RES in 0x6C
	B_Data[11] = (Vtotal >> 8) &
		BITS_2_1_0;	// 3 MSBITS of V_RES in 0x6D

	WriteBlockTPI(TPI_DE_DLY, 12, &B_Data[0]);
	TPI_REG0x63_SAVED = B_Data[1];

	return DE_SET_OK;	// Write completed successfully
}

//------------------------------------------------------------------------------
// Function Name: SetFormat()
// Function Description: Set the 9022/4 format
//
// Accepts: none
// Returns: DE_SET_OK
// Globals: none
//------------------------------------------------------------------------------
void SetFormat(byte *Data)
{
	ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG,
		OUTPUT_MODE_MASK, OUTPUT_MODE_HDMI);

	WriteBlockTPI(TPI_INPUT_FORMAT_REG, 2, Data);
	WriteByteTPI(TPI_END_RIGHT_BAR_MSB, 0x00);

	if (!IsHDMI_Sink())
		ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG,
			OUTPUT_MODE_MASK, OUTPUT_MODE_DVI);

	if (siHdmiTx.SyncMode == EMBEDDED_SYNC)
		EnableEmbeddedSync();
}

//------------------------------------------------------------------------------
// Function Name: printVideoMode()
// Function Description: print video mode
//
// Accepts: siHdmiTx.VIC
// Returns: none
// Globals: none
//------------------------------------------------------------------------------
void printVideoMode(void)
{
	TPI_TRACE_PRINT((">>Video mode = "));

	switch (siHdmiTx.VIC) {
	case 6:
		TPI_TRACE_PRINT(("HDMI_480I60_4X3\n"));
		break;
	case 21:
		TPI_TRACE_PRINT(("HDMI_576I50_4X3\n"));
		break;
	case 2:
		TPI_TRACE_PRINT(("HDMI_480P60_4X3\n"));
		break;
	case 17:
		TPI_TRACE_PRINT(("HDMI_576P50_4X3\n"));
		break;
	case 4:
		TPI_TRACE_PRINT(("HDMI_720P60\n"));
		break;
	case 19:
		TPI_TRACE_PRINT(("HDMI_720P50\n"));
		break;
	case 5:
		TPI_TRACE_PRINT(("HDMI_1080I60\n"));
		break;
	case 20:
		TPI_TRACE_PRINT(("HDMI_1080I50\n"));
		break;
	case 16:
		TPI_TRACE_PRINT(("HDMI_1080P60\n"));
		break;
	case 31:
		TPI_TRACE_PRINT(("HDMI_1080P50\n"));
		break;
	default:
		break;
	}
}

//------------------------------------------------------------------------------
// Function Name: InitVideo()
// Function Description:
//Set the 9022/4 to the video mode determined by GetVideoMode()
//
// Accepts: Index of video mode to set; Flag that distinguishes between
//                  calling this function after power up and after input
//                  resolution change
// Returns: TRUE
// Globals: VModesTable, VideoCommandImage
//------------------------------------------------------------------------------
byte InitVideo(byte TclkSel)
{
	byte ModeTblIndex;

#ifdef DEEP_COLOR
	byte temp;
#endif
	byte B_Data[8];

	byte EMB_Status;
	byte DE_Status;
	byte Pattern;

	TPI_TRACE_PRINT((">>InitVideo()\n"));
	printVideoMode();
	TPI_TRACE_PRINT((" HF:%d", (int)siHdmiTx.HDMIVideoFormat));
	TPI_TRACE_PRINT((" VIC:%d", (int)siHdmiTx.VIC));
	TPI_TRACE_PRINT((" A:%x", (int)siHdmiTx.AspectRatio));
	TPI_TRACE_PRINT((" CS:%x", (int)siHdmiTx.ColorSpace));
	TPI_TRACE_PRINT((" CD:%x", (int)siHdmiTx.ColorDepth));
	TPI_TRACE_PRINT((" CR:%x", (int)siHdmiTx.Colorimetry));
	TPI_TRACE_PRINT((" SM:%x", (int)siHdmiTx.SyncMode));
	TPI_TRACE_PRINT((" TCLK:%x", (int)siHdmiTx.TclkSel));
	TPI_TRACE_PRINT((" 3D:%d", (int)siHdmiTx.ThreeDStructure));
	TPI_TRACE_PRINT((" 3Dx:%d\n", (int)siHdmiTx.ThreeDExtData));

	ModeTblIndex = (byte) ConvertVIC_To_VM_Index();

	Pattern = (TclkSel << 6) & TWO_MSBITS;
	ReadSetWriteTPI(TPI_PIX_REPETITION, Pattern);

	// Take values from VModesTable[]:
	if ((siHdmiTx.VIC == 6) || (siHdmiTx.VIC == 7) ||	//480i
	    (siHdmiTx.VIC == 21) || (siHdmiTx.VIC == 22)) {
		if (siHdmiTx.ColorSpace == YCBCR422_8BITS) {
			B_Data[0] = VModesTable[ModeTblIndex].PixClk & 0x00FF;
			B_Data[1] =
				(VModesTable[ModeTblIndex].PixClk >>
				8) & 0xFF;
		} else {
			B_Data[0] = (VModesTable[ModeTblIndex].PixClk /
				2) & 0x00FF;
			B_Data[1] = ((VModesTable[ModeTblIndex].PixClk /
				2) >> 8) & 0xFF;
		}
	} else {
		B_Data[0] = VModesTable[ModeTblIndex].PixClk & 0x00FF;
		B_Data[1] = (VModesTable[ModeTblIndex].PixClk >> 8) & 0xFF;
	}

	B_Data[2] = VModesTable[ModeTblIndex].Tag.VFreq & 0x00FF;
	B_Data[3] = (VModesTable[ModeTblIndex].Tag.VFreq >> 8) & 0xFF;

	if ((siHdmiTx.VIC == 6) || (siHdmiTx.VIC == 7) ||	//480i
	    (siHdmiTx.VIC == 21) || (siHdmiTx.VIC == 22)) {
		B_Data[4] = (VModesTable[ModeTblIndex].Tag.Total.Pixels /
			2) & 0x00FF;
		B_Data[5] = ((VModesTable[ModeTblIndex].Tag.Total.Pixels /
			2) >> 8) & 0xFF;
	} else {
		B_Data[4] = VModesTable[ModeTblIndex].Tag.Total.Pixels
			& 0x00FF;
		B_Data[5] = (VModesTable[ModeTblIndex].Tag.Total.Pixels
			>> 8) & 0xFF;
	}

	B_Data[6] = VModesTable[ModeTblIndex].Tag.Total.Lines & 0x00FF;
	B_Data[7] = (VModesTable[ModeTblIndex].Tag.Total.Lines >> 8) & 0xFF;

	WriteBlockTPI(TPI_PIX_CLK_LSB, 8, B_Data);	// Write TPI Mode data.

	// TPI Input Bus and Pixel Repetition Data
	// B_Data[0] = Reg0x08;
	B_Data[0] = 0;		// Set to default 0 for use again
	B_Data[0] |= BIT_BUS_24;	// Set 24 bit bus
	B_Data[0] |= (TclkSel << 6) & TWO_MSBITS;

#ifdef CLOCK_EDGE_FALLING
	B_Data[0] &= ~BIT_EDGE_RISE;	// Set to falling edge
#endif
#ifdef CLOCK_EDGE_RISING
	B_Data[0] |= BIT_EDGE_RISE;	// Set to rising edge
#endif
	tpivmode[0] = B_Data[0];	// saved TPI Reg0x08 value.
	WriteByteTPI(TPI_PIX_REPETITION, B_Data[0]);	// 0x08

	// TPI AVI Input and Output Format Data
	// B_Data[0] = Reg0x09;
	// B_Data[1] = Reg0x0A;
	B_Data[0] = 0;		// Set to default 0 for use again
	B_Data[1] = 0;		// Set to default 0 for use again

	if (siHdmiTx.SyncMode == EMBEDDED_SYNC) {
		EMB_Status = SetEmbeddedSync();
		EnableEmbeddedSync();
	}

	if (siHdmiTx.SyncMode == INTERNAL_DE) {
		ReadClearWriteTPI(TPI_SYNC_GEN_CTRL, MSBIT);
		DE_Status = SetDE();
	}

	if (siHdmiTx.ColorSpace == RGB)
		B_Data[0] = (((BITS_IN_RGB | BITS_IN_AUTO_RANGE) &
		~BIT_EN_DITHER_10_8) & ~BIT_EXTENDED_MODE);	// 0x09

	else if (siHdmiTx.ColorSpace == YCBCR444)
		B_Data[0] = (((BITS_IN_YCBCR444 |
		BITS_IN_AUTO_RANGE) & ~BIT_EN_DITHER_10_8) &
		~BIT_EXTENDED_MODE);	// 0x09

	else if ((siHdmiTx.ColorSpace == YCBCR422_16BITS)
		 || (siHdmiTx.ColorSpace == YCBCR422_8BITS))
		B_Data[0] = (((BITS_IN_YCBCR422 | BITS_IN_AUTO_RANGE) &
		~BIT_EN_DITHER_10_8) & ~BIT_EXTENDED_MODE);	// 0x09

#ifdef DEEP_COLOR
	switch (siHdmiTx.ColorDepth) {
	case 0:
		temp = 0x00;
		ReadModifyWriteTPI(TPI_DEEP_COLOR_GCP, BIT_2, 0x00);
		break;
	case 1:
		temp = 0x80;
		ReadModifyWriteTPI(TPI_DEEP_COLOR_GCP, BIT_2, BIT_2);
		break;
	case 2:
		temp = 0xC0;
		ReadModifyWriteTPI(TPI_DEEP_COLOR_GCP, BIT_2, BIT_2);
		break;
	case 3:
		temp = 0x40;
		ReadModifyWriteTPI(TPI_DEEP_COLOR_GCP, BIT_2, BIT_2);
		break;
	default:
		temp = 0x00;
		ReadModifyWriteTPI(TPI_DEEP_COLOR_GCP, BIT_2, 0x00);
		break;
	}
	B_Data[0] = ((B_Data[0] & 0x3F) | temp);
#endif

	B_Data[1] = (BITS_OUT_RGB | BITS_OUT_AUTO_RANGE);	//Reg0x0A

	if ((siHdmiTx.VIC == 6) || (siHdmiTx.VIC == 7) ||	//480i
	    (siHdmiTx.VIC == 21) || (siHdmiTx.VIC == 22) ||	//576i
	    (siHdmiTx.VIC == 2) || (siHdmiTx.VIC == 3) ||	//480p
	    (siHdmiTx.VIC == 17) || (siHdmiTx.VIC == 18))	//576p
		B_Data[1] &= ~BIT_BT_709;
	else
		B_Data[1] |= BIT_BT_709;

#ifdef DEEP_COLOR
	B_Data[1] = ((B_Data[1] & 0x3F) | temp);
#endif

#ifdef DEV_SUPPORT_EDID
	if (!IsHDMI_Sink()) {
		B_Data[1] = ((B_Data[1] & 0xFC) | BITS_OUT_RGB);
	} else {
		// Set YCbCr color space depending on EDID
		if (g_edid.YCbCr_4_4_4) {
			B_Data[1] = ((B_Data[1] & 0xFC) | BITS_OUT_YCBCR444);
		} else {
			if (g_edid.YCbCr_4_2_2)
				B_Data[1] = ((B_Data[1] &
					0xFC) | BITS_OUT_YCBCR422);
			else
				B_Data[1] = ((B_Data[1] & 0xFC) | BITS_OUT_RGB);
		}
	}
#endif

	tpivmode[1] = B_Data[0];	// saved TPI Reg0x09 value.
	tpivmode[2] = B_Data[1];	// saved TPI Reg0x0A value.
	SetFormat(B_Data);

	ReadClearWriteTPI(TPI_SYNC_GEN_CTRL, BIT_2);

	return TRUE;
}

//------------------------------------------------------------------------------
// Function Name: SetAVI_InfoFrames()
// Function Description: Load AVI InfoFrame data into registers and send to sink
//
// Accepts: An API_Cmd parameter
//that holds the data to be sent in the InfoFrames
// Returns: TRUE
// Globals: none
//
// Note:          : Infoframe contents are from spec CEA-861-D
//
//------------------------------------------------------------------------------
byte SetAVI_InfoFrames(void)
{
	byte B_Data[SIZE_AVI_INFOFRAME];
	byte i;
	byte TmpVal;
	byte VModeTblIndex;

	TPI_TRACE_PRINT((">>SetAVI_InfoFrames()\n"));

	for (i = 0; i < SIZE_AVI_INFOFRAME; i++)
		B_Data[i] = 0;

#ifdef DEV_SUPPORT_EDID
	if (g_edid.YCbCr_4_4_4)
		TmpVal = 2;
	else if (g_edid.YCbCr_4_2_2)
		TmpVal = 1;
	else
		TmpVal = 0;
#else
	TmpVal = 0;
#endif

	B_Data[1] = (TmpVal << 5) & BITS_OUT_FORMAT;
	B_Data[1] |= 0x11;

	if (siHdmiTx.ColorSpace == XVYCC444) {
		B_Data[2] = 0xC0;

		if (siHdmiTx.Colorimetry == COLORIMETRY_601)	// xvYCC601
			B_Data[3] &= ~BITS_6_5_4;

		else if (siHdmiTx.Colorimetry == COLORIMETRY_709)
			B_Data[3] = (B_Data[3] & ~BITS_6_5_4) | BIT_4;
	}

	else if (siHdmiTx.Colorimetry == COLORIMETRY_709)	// BT.709
		B_Data[2] = 0x80;	// AVI Byte2: C1C0

	else if (siHdmiTx.Colorimetry == COLORIMETRY_601)	// BT.601
		B_Data[2] = 0x40;	// AVI Byte2: C1C0

	else {			// AVI Byte2: C1C0
		B_Data[2] &= ~BITS_7_6;	// colorimetry = 0
		B_Data[3] &= ~BITS_6_5_4;	// Extended colorimetry = 0
	}

	VModeTblIndex = ConvertVIC_To_VM_Index();

	B_Data[4] = siHdmiTx.VIC;

	//  Set the Aspect Ration info into the Infoframe Byte 2
	if (siHdmiTx.AspectRatio == VMD_ASPECT_RATIO_16x9) {
		B_Data[2] |= _16_To_9;	// AVI Byte2: M1M0
		if ((VModesTable[VModeTblIndex].AspectRatio == R_4or16)
		    && (AspectRatioTable[siHdmiTx.VIC - 1] == R_4)) {
			siHdmiTx.VIC++;
			B_Data[4]++;
		}
	} else {
		B_Data[2] |= _4_To_3;	// AVI Byte4: VIC
	}

	B_Data[2] |= SAME_AS_AR;
	B_Data[5] = VModesTable[VModeTblIndex].PixRep;

	// Calculate AVI InfoFrame ChecKsum
	B_Data[0] = 0x82 + 0x02 + 0x0D;
	for (i = 1; i < SIZE_AVI_INFOFRAME; i++)
		B_Data[0] += B_Data[i];
	B_Data[0] = 0x100 - B_Data[0];

	// Write the Inforframe data to the TPI Infoframe registers
	WriteBlockTPI(TPI_AVI_BYTE_0, SIZE_AVI_INFOFRAME, B_Data);

	if (siHdmiTx.SyncMode == EMBEDDED_SYNC)
		EnableEmbeddedSync();

	return TRUE;
}

//------------------------------------------------------------------------------
// Function Name: siHdmiTx_Init()
// Function Description: Set the 9022/4 video and video.
//
// Accepts: none
// Returns: none
// Globals: siHdmiTx
//------------------------------------------------------------------------------
void siHdmiTx_Init(void)
{
	TPI_TRACE_PRINT((">>siHdmiTx_Init()\n"));

	// workaround for Bug#18128
	if (siHdmiTx.ColorDepth == VMD_COLOR_DEPTH_8BIT) {
		// Yes it is, so force 16bpps first!
		siHdmiTx.ColorDepth = VMD_COLOR_DEPTH_16BIT;
		InitVideo(siHdmiTx.TclkSel);
		siHdmiTx.ColorDepth = VMD_COLOR_DEPTH_8BIT;
	}
	// end workaround

	InitVideo(siHdmiTx.TclkSel);
	siHdmiTx_PowerStateD0();

	if (IsHDMI_Sink()) {
		SetAVI_InfoFrames();
		siHdmiTx_AudioSet();
	} else {
		SetAudioMute(AUDIO_MUTE_MUTED);
	}

	if (siHdmiTx.ColorSpace == YCBCR422_8BITS)
		ReadSetWriteTPI(
		TPI_SYNC_GEN_CTRL, BIT_5);

	WriteByteTPI(TPI_DE_CTRL, TPI_REG0x63_SAVED);



	//=====================
	WriteByteTPI(TPI_YC_Input_Mode, 0x00);

#ifdef DEV_SUPPORT_HDCP
	if ((g_hdcp.HDCP_TxSupports == TRUE)
	    && (g_hdcp.HDCPAuthenticated == VMD_HDCP_AUTHENTICATED)) {
		if (g_hdcp.HDCP_AksvValid == TRUE) {
			// AV MUTE
			TPI_DEBUG_PRINT(("TMDS -> Enabled (Video Muted)\n"));
			ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG,
					   LINK_INTEGRITY_MODE_MASK |
					   TMDS_OUTPUT_CONTROL_MASK |
					   AV_MUTE_MASK,
					   LINK_INTEGRITY_DYNAMIC |
					   TMDS_OUTPUT_CONTROL_ACTIVE |
					   AV_MUTE_MUTED);

			WriteByteTPI(TPI_PIX_REPETITION,
				tpivmode[0]);	// Write register 0x08
			g_sys.tmdsPoweredUp = TRUE;
			EnableInterrupts(HOT_PLUG_EVENT |
				RX_SENSE_EVENT | AUDIO_ERROR_EVENT |
					 SECURITY_CHANGE_EVENT |
					 HDCP_CHANGE_EVENT);
		}
	} else
#endif
	{
		TPI_DEBUG_PRINT(("TMDS -> Enabled\n"));
		ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG,
				   LINK_INTEGRITY_MODE_MASK |
				   TMDS_OUTPUT_CONTROL_MASK |
				   AV_MUTE_MASK,
				   LINK_INTEGRITY_DYNAMIC |
				   TMDS_OUTPUT_CONTROL_ACTIVE |
				   AV_MUTE_NORMAL);

		WriteByteTPI(TPI_PIX_REPETITION,
			tpivmode[0]);	// Write register 0x08
		g_sys.tmdsPoweredUp = TRUE;
		EnableInterrupts(HOT_PLUG_EVENT |
			RX_SENSE_EVENT | AUDIO_ERROR_EVENT);
	}
}

//------------------------------------------------------------------------------
// Function Name: siHdmiTx_VideoSet()
// Function Description: Set the 9022/4 video resolution
//
// Accepts: none
// Returns: Success message if video resolution changed successfully.
//                  Error Code if resolution change failed
// Globals: siHdmiTx
//------------------------------------------------------------------------------
//============================================================
#define T_RES_CHANGE_DELAY      128

byte siHdmiTx_VideoSet(void)
{
	TPI_TRACE_PRINT((">>siHdmiTx_VideoSet()\n"));

	siHdmiTx_TPI_Init();
	g_sys.hdmiCableConnected = TRUE;
	g_sys.dsRxPoweredUp = TRUE;

	siHdmiTx_Init();

#ifdef HW_INT_ENABLE

#ifdef DEV_SUPPORT_HDCP
	HDCP_CheckStatus(ReadByteTPI(TPI_INTERRUPT_STATUS_REG));
#endif

#endif

	return VIDEO_MODE_SET_OK;
}

//------------------------------------------------------------------------------
// Function Name: SetAudioInfoFrames()
// Function Description:
//Load Audio InfoFrame data into registers and send to sink
//
// Accepts: (1) Channel count
//              (2) speaker configuration per CEA-861D Tables 19, 20
//              (3) Coding type: 0x09 for
//DSD Audio. 0 (refer to stream header) for all the rest
//              (4) Sample Frequency. Non zero for HBR only
//              (5) Audio Sample Length. Non zero for HBR only.
// Returns: TRUE
// Globals: none
//------------------------------------------------------------------------------
byte SetAudioInfoFrames(byte ChannelCount,
byte CodingType, byte SS, byte Fs, byte SpeakerConfig)
{
	byte B_Data[SIZE_AUDIO_INFOFRAME];	// 14
	byte i;
	//byte TmpVal = 0;

	TPI_TRACE_PRINT((">>SetAudioInfoFrames()\n"));

	for (i = 0; i < SIZE_AUDIO_INFOFRAME; i++)
		B_Data[i] = 0;

	WriteByteTPI(MISC_INFO_FRAMES_CTRL, DISABLE_AUDIO);

	B_Data[0] = TYPE_AUDIO_INFOFRAMES;	// 0x84
	B_Data[1] = AUDIO_INFOFRAMES_VERSION;	// 0x01
	B_Data[2] = AUDIO_INFOFRAMES_LENGTH;	// 0x0A
	B_Data[3] = TYPE_AUDIO_INFOFRAMES +
	    AUDIO_INFOFRAMES_VERSION + AUDIO_INFOFRAMES_LENGTH;

	B_Data[4] = ChannelCount;
	B_Data[4] |= (CodingType << 4);	// 0xC7[7:4] == 0b1001 for DSD Audio

	B_Data[5] = ((Fs & THREE_LSBITS) << 2) | (SS & TWO_LSBITS);

	B_Data[7] = SpeakerConfig;

	for (i = 4; i < SIZE_AUDIO_INFOFRAME; i++)
		B_Data[3] += B_Data[i];

	B_Data[3] = 0x100 - B_Data[3];

	WriteByteTPI(MISC_INFO_FRAMES_CTRL,
		EN_AND_RPT_AUDIO);

	WriteBlockTPI(MISC_INFO_FRAMES_TYPE, SIZE_AUDIO_INFOFRAME, B_Data);

	if (siHdmiTx.SyncMode == EMBEDDED_SYNC)
		EnableEmbeddedSync();

	return TRUE;
}

//------------------------------------------------------------------------------
// Function Name: SetAudioMute()
// Function Description: Mute audio
//
// Accepts: Mute or unmute.
// Returns: none
// Globals: none
//------------------------------------------------------------------------------
void SetAudioMute(byte audioMute)
{
	ReadModifyWriteTPI(TPI_AUDIO_INTERFACE_REG, AUDIO_MUTE_MASK, audioMute);
}

#ifndef F_9022A_9334
//------------------------------------------------------------------------------
// Function Name: SetChannelLayout()
// Function Description:
//Set up the Channel layout field of internal register 0x2F (0x2F[1])
//
// Accepts: Number of audio channels: "0 for 2-Channels ."1" for 8.
// Returns: none
// Globals: none
//------------------------------------------------------------------------------
void SetChannelLayout(byte Count)
{
	// Indexed register 0x7A:0x2F[1]:
	WriteByteTPI(TPI_INTERNAL_PAGE_REG, 0x02);	// Internal page 2
	WriteByteTPI(TPI_INDEXED_OFFSET_REG, 0x2F);

	Count &= THREE_LSBITS;

	if (Count == TWO_CHANNEL_LAYOUT) {
		// Clear 0x2F[1]:
		ReadClearWriteTPI(TPI_INDEXED_VALUE_REG, BIT_1);
	}

	else if (Count == EIGHT_CHANNEL_LAYOUT) {
		// Set 0x2F[1]:
		ReadSetWriteTPI(TPI_INDEXED_VALUE_REG, BIT_1);
	}
}
#endif

//------------------------------------------------------------------------------
// Function Name: siHdmiTx_AudioSet()
// Function Description: Set the 9022/4 audio interface to basic audio.
//
// Accepts: none
// Returns: Success message if audio changed successfully.
//                  Error Code if resolution change failed
// Globals: siHdmiTx
//------------------------------------------------------------------------------
byte siHdmiTx_AudioSet(void)
{
	TPI_TRACE_PRINT((">>siHdmiTx_AudioSet()\n"));

	SetAudioMute(AUDIO_MUTE_MUTED);	// mute output

	if (siHdmiTx.AudioMode == AMODE_I2S) {
		ReadModifyWriteTPI(TPI_AUDIO_INTERFACE_REG,
			AUDIO_SEL_MASK, AUD_IF_I2S);	// 0x26 = 0x80
		WriteByteTPI(TPI_AUDIO_HANDLING, 0x08 |
			AUD_DO_NOT_CHECK);	// 0x25
	} else {
		ReadModifyWriteTPI(TPI_AUDIO_INTERFACE_REG,
			AUDIO_SEL_MASK, AUD_IF_SPDIF);	// 0x26 = 0x40
		WriteByteTPI(TPI_AUDIO_HANDLING,
			AUD_PASS_BASIC);	// 0x25 = 0x00
	}

#ifndef F_9022A_9334
	if (siHdmiTx.AudioChannels == ACHANNEL_2CH)
		SetChannelLayout(TWO_CHANNELS);	// Always 2 channesl in S/PDIF
	else
		SetChannelLayout(EIGHT_CHANNELS);
#else
	if (siHdmiTx.AudioChannels == ACHANNEL_2CH)
		ReadClearWriteTPI(TPI_AUDIO_INTERFACE_REG, BIT_5);
	else
		ReadSetWriteTPI(TPI_AUDIO_INTERFACE_REG, BIT_5);
#endif

	if (siHdmiTx.AudioMode == AMODE_I2S) {
		// I2S - Map channels - replace with call to API MAPI2S
		WriteByteTPI(TPI_I2S_EN, 0x80);	// 0x1F

		if (siHdmiTx.AudioChannels > ACHANNEL_2CH)
			WriteByteTPI(TPI_I2S_EN, 0x91);

		if (siHdmiTx.AudioChannels > ACHANNEL_4CH)
			WriteByteTPI(TPI_I2S_EN, 0xA2);

		if (siHdmiTx.AudioChannels > ACHANNEL_6CH)
			WriteByteTPI(TPI_I2S_EN, 0xB3);

		WriteByteTPI(TPI_I2S_CHST_0, 0x00);	// 0x21
		WriteByteTPI(TPI_I2S_CHST_1, 0x00);
		WriteByteTPI(TPI_I2S_CHST_2, 0x00);
		WriteByteTPI(TPI_I2S_CHST_3, siHdmiTx.AudioFs);
		WriteByteTPI(TPI_I2S_CHST_4,
			(siHdmiTx.AudioFs << 4) |
			siHdmiTx.AudioWordLength);

		// Oscar 20100929 added for 16bit auido noise issue
		WriteIndexedRegister(INDEXED_PAGE_1,
		AUDIO_INPUT_LENGTH, siHdmiTx.AudioWordLength);

		// I2S - Input Configuration
		WriteByteTPI(TPI_I2S_IN_CFG,
		siHdmiTx.AudioI2SFormat);	//TPI_Reg0x20
	}

	WriteByteTPI(TPI_AUDIO_SAMPLE_CTRL, REFER_TO_STREAM_HDR);
	SetAudioInfoFrames(siHdmiTx.AudioChannels &
		THREE_LSBITS, REFER_TO_STREAM_HDR,
			   REFER_TO_STREAM_HDR, REFER_TO_STREAM_HDR, 0x00);

	SetAudioMute(AUDIO_MUTE_NORMAL);	// unmute output

	return AUDIO_MODE_SET_OK;
}

#ifdef F_9022A_9334
//------------------------------------------------------------------------------
// Function Name: SetGBD_InfoFrame()
// Function Description: Sets and sends the the 9022A/4A GBD InfoFrames.
//
// Accepts: none
// Returns: Success message if GBD packet set successfully. Error
//                  Code if failed
// Globals: none
// NOTE: Currently this function is a place holder.
//It always returns a Success message
//------------------------------------------------------------------------------
byte SetGBD_InfoFrame(void)
{
	byte CheckSum;

	TPI_TRACE_PRINT((">>SetGBD_InfoFrame()\n"));

	// Set MPEG InfoFrame Header to GBD InfoFrame Header values:
	WriteByteTPI(MISC_INFO_FRAMES_CTRL,
	DISABLE_MPEG);	// 0xBF = Use MPEG      InfoFrame for GBD - 0x03
	WriteByteTPI(MISC_INFO_FRAMES_TYPE,
		TYPE_GBD_INFOFRAME);	// 0xC0 = 0x0A
	WriteByteTPI(MISC_INFO_FRAMES_VER, NEXT_FIELD |
		GBD_PROFILE | AFFECTED_GAMUT_SEQ_NUM);
	WriteByteTPI(MISC_INFO_FRAMES_LEN, ONLY_PACKET |
		CURRENT_GAMUT_SEQ_NUM);	// 0x0C2 = 0x31

	CheckSum = TYPE_GBD_INFOFRAME +
	    NEXT_FIELD + GBD_PROFILE +
	    AFFECTED_GAMUT_SEQ_NUM +
	    ONLY_PACKET + CURRENT_GAMUT_SEQ_NUM;

	CheckSum = 0x100 - CheckSum;

	WriteByteTPI(MISC_INFO_FRAMES_CTRL, EN_AND_RPT_MPEG);
	WriteByteTPI(MISC_INFO_FRAMES_CHKSUM, CheckSum);

	return GBD_SET_SUCCESSFULLY;
}
#endif

#ifdef DEV_SUPPORT_3D
//------------------------------------------------------------------------------
// Function Name: Set_VSIF()
// Function Description:
//Construct Vendor Specific InfoFrame for 3D support. use MPEG InfoFrame
//
// Accepts: none
// Returns: none
// Globals: siHdmiTx
//------------------------------------------------------------------------------
// VSIF Constants
//============================================================
#define VSIF_TYPE			0x81
#define VSIF_VERSION		0x01
#define VSIF_LEN				0x06

void Set_VSIF(void)
{
	byte i;
	byte Data[SIZE_MPEG_INFOFRAME];	//10

	for (i = 0; i < SIZE_MPEG_INFOFRAME; i++)
		Data[i] = 0;

	// Disable transmission of VSIF during re-configuration
	WriteByteTPI(MISC_INFO_FRAMES_CTRL, DISABLE_MPEG);

	// Header Bytes
	Data[0] = VSIF_TYPE;	// HB0 Packet Type 0x81
	Data[1] = VSIF_VERSION;	// HB1 Version = 0x01

	// PB1 - PB3 contain the 24bit IEEE Registration Identifier
	Data[4] = 0x03;		// HDMI Signature LS Byte
	Data[5] = 0x0C;		// HDMI Signature middle byte
	Data[6] = 0x00;		// HDMI Signature MS Byte

	// PB4 - HDMI_Video_Format into bits 7:5
	Data[7] = siHdmiTx.HDMIVideoFormat << 5;

	// code in buts 7:0, OR the 3D_Structure in bits 7:4.
	switch (siHdmiTx.HDMIVideoFormat) {
	case VMD_HDMIFORMAT_HDMI_VIC:
		// This is a 2x4K mode, set the HDMI_VIC in buts 7:0.  Values
		// are from HDMI 1.4 Spec, 8.2.3.1 (Table 8-13).
		Data[8] = siHdmiTx.VIC;
		Data[9] = 0;
		break;

	case VMD_HDMIFORMAT_3D:
		// This is a 3D mode, set the 3D_Structure in buts 7:4
		// Bits 3:0 are reseved so set to 0.  Values are from HDMI 1.4
		// Spec, Appendix H (Table H-2).
		Data[8] = siHdmiTx.ThreeDStructure << 4;
		// See Spec Table H-3 for details.
		if ((Data[8] >> 4) == VMD_3D_SIDEBYSIDEHALF) {
			Data[2] = VSIF_LEN;
			Data[9] = siHdmiTx.ThreeDExtData << 4;
		} else {
			Data[2] = VSIF_LEN - 1;
		}
		break;

	case VMD_HDMIFORMAT_CEA_VIC:
	default:
		Data[8] = 0;
		Data[9] = 0;
		break;
	}

	// Packet Bytes
	Data[3] = VSIF_TYPE +	// PB0 partial checksum
	    VSIF_VERSION + Data[2];

	// Complete the checksum with PB1 through PB7
	for (i = 4; i < SIZE_MPEG_INFOFRAME; i++)
		Data[3] += Data[i];
	// Data[3] %= 0x100;
	Data[3] = 0x100 - Data[3];	// Final checksum

	WriteByteTPI(MISC_INFO_FRAMES_CTRL, EN_AND_RPT_MPEG);

	WriteBlockTPI(MISC_INFO_FRAMES_TYPE, SIZE_MPEG_INFOFRAME, Data);
	WriteByteTPI(0xDE, 0x00);
}
#endif

//------------------------------------------------------------------------------
// Function Name: StartTPI()
// Function Description: Start HW TPI mode by writing 0x00 to TPI address 0xC7.
//
// Accepts: none
// Returns: TRUE if HW TPI started successfully. FALSE if failed to.
// Globals: none
//------------------------------------------------------------------------------
byte StartTPI(void)
{
	byte devID = 0x00;
	word wID = 0x0000;

	TPI_TRACE_PRINT((">>StartTPI()\n"));

	WriteByteTPI(TPI_ENABLE, 0x00);
	DelayMS(100);

	devID = ReadIndexedRegister(INDEXED_PAGE_0, 0x03);
	wID = devID;
	wID <<= 8;
	devID = ReadIndexedRegister(INDEXED_PAGE_0, 0x02);
	wID |= devID;

	devID = ReadByteTPI(TPI_DEVICE_ID);

	TPI_TRACE_PRINT(("0x%04X\n", (int)wID));

	if (devID == SII902XA_DEVICE_ID)
		return TRUE;

	TPI_TRACE_PRINT(("Unsupported TX, devID = 0x%X\n", (int)devID));
	return FALSE;
}

//------------------------------------------------------------------------------
// Function Name: siHdmiTx_TPI_Init()
// Function Description: TPI initialization: HW Reset, Interrupt enable.
//
// Accepts: none
// Returns: TRUE or FLASE
// Globals: none
//------------------------------------------------------------------------------
byte siHdmiTx_TPI_Init(void)
{
	TPI_TRACE_PRINT(("\n>>siHdmiTx_TPI_Init()\n"));
	TPI_TRACE_PRINT(("\n%s\n", TPI_FW_VERSION));

	// Chip powers up in D2 mode.
	g_sys.txPowerState = TX_POWER_STATE_D0;

	InitializeStateVariables();

	// Toggle TX reset pin
	TxHW_Reset();

	// Enable HW TPI mode, check device ID
	if (StartTPI()) {
#ifdef DEV_SUPPORT_HDCP
		g_hdcp.HDCP_Override = FALSE;
		g_hdcp.HDCPAuthenticated = VMD_HDCP_AUTHENTICATED;
		HDCP_Init();
#endif

#ifdef DEV_SUPPORT_CEC
		//SI_CecInit();
#endif

		EnableInterrupts(HOT_PLUG_EVENT);

		return TRUE;
	}

	return FALSE;
}

//------------------------------------------------------------------------------
// Function Name: OnDownstreamRxPoweredDown()
// Function Description: HDMI cable unplug handle.
//
// Accepts: none
// Returns: none
// Globals: none
//------------------------------------------------------------------------------
void OnDownstreamRxPoweredDown(void)
{
	TPI_DEBUG_PRINT(("DSRX -> Powered Down\n"));
	g_sys.dsRxPoweredUp = FALSE;

#ifdef DEV_SUPPORT_HDCP
	if (g_hdcp.HDCP_Started == TRUE)
		HDCP_Off();
#endif

	DisableTMDS();
	ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG,
		OUTPUT_MODE_MASK, OUTPUT_MODE_DVI);
}

//------------------------------------------------------------------------------
// Function Name: OnDownstreamRxPoweredUp()
// Function Description: DSRX power up handle.
//
// Accepts: none
// Returns: none
// Globals: none
//------------------------------------------------------------------------------
void OnDownstreamRxPoweredUp(void)
{
	TPI_DEBUG_PRINT(("DSRX -> Powered Up\n"));
	g_sys.dsRxPoweredUp = TRUE;

	HotPlugService();
}

//------------------------------------------------------------------------------
// Function Name: OnHdmiCableDisconnected()
// Function Description: HDMI cable unplug handle.
//
// Accepts: none
// Returns: none
// Globals: none
//------------------------------------------------------------------------------
void OnHdmiCableDisconnected(void)
{
	TPI_DEBUG_PRINT(("HDMI Disconnected\n"));

	g_sys.hdmiCableConnected = FALSE;

#ifdef DEV_SUPPORT_EDID
	g_edid.edidDataValid = FALSE;
#endif

	OnDownstreamRxPoweredDown();
}

//------------------------------------------------------------------------------
// Function Name: OnHdmiCableConnected()
// Function Description: HDMI cable plug in handle.
//
// Accepts: none
// Returns: none
// Globals: none
//------------------------------------------------------------------------------
void OnHdmiCableConnected(void)
{
	TPI_DEBUG_PRINT(("Cable Connected\n"));
	//TPI_Init();

	g_sys.hdmiCableConnected = TRUE;

#ifdef DEV_SUPPORT_HDCP
	if ((g_hdcp.HDCP_TxSupports == TRUE)
	    && (g_hdcp.HDCP_AksvValid == TRUE)
	    && (g_hdcp.HDCPAuthenticated == VMD_HDCP_AUTHENTICATED)) {
		WriteIndexedRegister(INDEXED_PAGE_0,
			0xCE, 0x00);	// Clear BStatus
		WriteIndexedRegister(INDEXED_PAGE_0, 0xCF, 0x00);
	}
#endif

	// Added for EDID read for Michael Wang recommaned by oscar 20100908
	//siHdmiTx_PowerStateD0();

#ifdef DEV_SUPPORT_EDID
	DoEdidRead();
#endif

#ifdef READKSV
	ReadModifyWriteTPI(0xBB, 0x08, 0x08);
#endif

	if (IsHDMI_Sink()) {
		TPI_DEBUG_PRINT(("HDMI Sink Detected\n"));
		ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG,
			OUTPUT_MODE_MASK, OUTPUT_MODE_HDMI);
	} else {
		TPI_DEBUG_PRINT(("DVI Sink Detected\n"));
		ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG,
			OUTPUT_MODE_MASK, OUTPUT_MODE_DVI);
	}
}

//------------------------------------------------------------------------------
// Function Name: siHdmiTx_PowerStateD0()
// Function Description: Set TX to D0 mode.
//------------------------------------------------------------------------------
void siHdmiTx_PowerStateD0(void)
{
	ReadModifyWriteTPI(TPI_DEVICE_POWER_STATE_CTRL_REG,
		TX_POWER_STATE_MASK, TX_POWER_STATE_D0);
	TPI_DEBUG_PRINT(("TX Power State D0\n"));
	g_sys.txPowerState = TX_POWER_STATE_D0;
}

//------------------------------------------------------------------------------
// Function Name: siHdmiTx_PowerStateD2()
// Function Description: Set TX to D2 mode.
//------------------------------------------------------------------------------
void siHdmiTx_PowerStateD2(void)
{
	ReadModifyWriteTPI(TPI_DEVICE_POWER_STATE_CTRL_REG,
		TX_POWER_STATE_MASK, TX_POWER_STATE_D2);
	TPI_DEBUG_PRINT(("TX Power State D2\n"));
	g_sys.txPowerState = TX_POWER_STATE_D2;
}

//------------------------------------------------------------------------------
// Function Name: siHdmiTx_PowerStateD0fromD2()
// Function Description: Set TX to D0 mode from D2 mode.
//------------------------------------------------------------------------------
void siHdmiTx_PowerStateD0fromD2(void)
{
	ReadModifyWriteTPI(TPI_DEVICE_POWER_STATE_CTRL_REG,
		TX_POWER_STATE_MASK, TX_POWER_STATE_D0);

#ifdef DEV_SUPPORT_HDCP
	RestartHDCP();
#else
	EnableTMDS();
#endif

	TPI_DEBUG_PRINT(("TX Power State D0 from D2\n"));
	g_sys.txPowerState = TX_POWER_STATE_D0;
}

//------------------------------------------------------------------------------
// Function Name: HotPlugService()
// Function Description: Implement Hot Plug Service Loop activities
//
// Accepts: none
// Returns: An error code that indicates success or cause of failure
// Globals: LinkProtectionLevel
//------------------------------------------------------------------------------
void HotPlugService(void)
{
	TPI_TRACE_PRINT((">>HotPlugService()\n"));

	DisableInterrupts(0xFF);

	siHdmiTx_Init();
}

//------------------------------------------------------------------------------
// Function Name: siHdmiTx_TPI_Poll()
// Function Description: Poll Interrupt Status register for new interrupts
//
// Accepts: none
// Returns: none
// Globals: none
//------------------------------------------------------------------------------
void siHdmiTx_TPI_Poll(void)
{
	byte InterruptStatus;

#ifdef HW_INT_ENABLE
	if ((g_sys.txPowerState == TX_POWER_STATE_D0) && (!INT_PIN))
#else
	if (g_sys.txPowerState == TX_POWER_STATE_D0)
#endif
	{
		InterruptStatus = ReadByteTPI(TPI_INTERRUPT_STATUS_REG);

		if (InterruptStatus & HOT_PLUG_EVENT) {
			TPI_DEBUG_PRINT(("HPD  -> "));
			ReadSetWriteTPI(TPI_INTERRUPT_ENABLE_REG,
				HOT_PLUG_EVENT);
			// Repeat this loop while cable is bouncing:
			do {
				WriteByteTPI(TPI_INTERRUPT_STATUS_REG,
					HOT_PLUG_EVENT);
				DelayMS(T_HPD_DELAY);
				InterruptStatus =
					ReadByteTPI(TPI_INTERRUPT_STATUS_REG);
			} while (InterruptStatus & HOT_PLUG_EVENT);

			if (((InterruptStatus &
				HOT_PLUG_STATE) >> 2) !=
				g_sys.hdmiCableConnected) {
				if (g_sys.hdmiCableConnected == TRUE)
					OnHdmiCableDisconnected();
				else {
					OnHdmiCableConnected();
					ReadModifyWriteIndexedRegister
						(INDEXED_PAGE_0,
						0x0A, 0x08, 0x08);
				}

				if (g_sys.hdmiCableConnected == FALSE)
					return;
			}
		}
		// Check rx power
		if (((InterruptStatus & RX_SENSE_STATE) >> 3)
			!= g_sys.dsRxPoweredUp) {
			if (g_sys.hdmiCableConnected == TRUE) {
				if (g_sys.dsRxPoweredUp == TRUE)
					OnDownstreamRxPoweredDown();
				else
					OnDownstreamRxPoweredUp();
			}

			ClearInterrupt(RX_SENSE_EVENT);
		}
		// Check if Audio Error event has occurred:
		if (InterruptStatus & AUDIO_ERROR_EVENT)
			ClearInterrupt(AUDIO_ERROR_EVENT);
#ifdef DEV_SUPPORT_HDCP
		if ((g_sys.hdmiCableConnected == TRUE)
		    && (g_sys.dsRxPoweredUp == TRUE)
		    && (g_hdcp.HDCPAuthenticated == VMD_HDCP_AUTHENTICATED)) {
			HDCP_CheckStatus(InterruptStatus);
		}
#endif

#ifdef DEV_SUPPORT_CEC
		//SI_CecHandler(0 , 0);
#endif
	}
}
void sii9022_HdmiTx_TPI_Poll(void)
{
	byte InterruptStatus;

	InterruptStatus =
		ReadByteTPI(TPI_INTERRUPT_STATUS_REG);

	if (InterruptStatus & HOT_PLUG_EVENT) {
		TPI_DEBUG_PRINT(("HPD  -> "));
		ReadSetWriteTPI(TPI_INTERRUPT_ENABLE_REG,
			HOT_PLUG_EVENT);

		// Repeat this loop while cable is bouncing:
		do {
			WriteByteTPI(TPI_INTERRUPT_STATUS_REG,
				HOT_PLUG_EVENT);
			DelayMS(T_HPD_DELAY);
			InterruptStatus = ReadByteTPI(TPI_INTERRUPT_STATUS_REG);
		} while (InterruptStatus & HOT_PLUG_EVENT);

		if (((InterruptStatus & HOT_PLUG_STATE) >> 2)
			!= g_sys.hdmiCableConnected) {
			if (g_sys.hdmiCableConnected == TRUE) {
				g_sys.hdmiCableConnected = FALSE;
				TPI_DEBUG_PRINT(("hdmitx plug out\n"));
			} else {
				g_sys.hdmiCableConnected = TRUE;
				TPI_DEBUG_PRINT(("hdmitx plug in\n"));
			}
		}
	}
}

//------------------------------------------------------------------------------
// Function Name: siHdmiTx_VideoSel()
// Function Description: Select output video mode
//
// Accepts: Video mode
// Returns: none
// Globals: none
//------------------------------------------------------------------------------
void siHdmiTx_VideoSel(byte vmode)
{
	siHdmiTx.HDMIVideoFormat = VMD_HDMIFORMAT_CEA_VIC;
	siHdmiTx.ColorSpace = RGB;
	siHdmiTx.ColorDepth = VMD_COLOR_DEPTH_8BIT;
	siHdmiTx.SyncMode = EXTERNAL_HSVSDE;

	switch (vmode) {
	case HDMI_480I60_4X3:
		siHdmiTx.VIC = 6;
		siHdmiTx.AspectRatio = VMD_ASPECT_RATIO_4x3;
		siHdmiTx.Colorimetry = COLORIMETRY_601;
		siHdmiTx.TclkSel = X2;
		break;

	case HDMI_576I50_4X3:
		siHdmiTx.VIC = 21;
		siHdmiTx.AspectRatio = VMD_ASPECT_RATIO_4x3;
		siHdmiTx.Colorimetry = COLORIMETRY_601;
		siHdmiTx.TclkSel = X2;
		break;

	case HDMI_480P60_4X3:
		siHdmiTx.VIC = 2;
		siHdmiTx.AspectRatio = VMD_ASPECT_RATIO_4x3;
		siHdmiTx.Colorimetry = COLORIMETRY_601;
		siHdmiTx.TclkSel = X1;
		break;

	case HDMI_576P50_4X3:
		siHdmiTx.VIC = 17;
		siHdmiTx.AspectRatio = VMD_ASPECT_RATIO_4x3;
		siHdmiTx.Colorimetry = COLORIMETRY_601;
		siHdmiTx.TclkSel = X1;
		break;

	case HDMI_720P60:
		siHdmiTx.VIC = 4;
		siHdmiTx.AspectRatio = VMD_ASPECT_RATIO_16x9;
		siHdmiTx.Colorimetry = COLORIMETRY_709;
		siHdmiTx.TclkSel = X1;
		break;

	case HDMI_720P50:
		siHdmiTx.VIC = 19;
		siHdmiTx.AspectRatio = VMD_ASPECT_RATIO_16x9;
		siHdmiTx.Colorimetry = COLORIMETRY_709;
		siHdmiTx.TclkSel = X1;
		break;

	case HDMI_1080I60:
		siHdmiTx.VIC = 5;
		siHdmiTx.AspectRatio = VMD_ASPECT_RATIO_16x9;
		siHdmiTx.Colorimetry = COLORIMETRY_709;
		siHdmiTx.TclkSel = X1;
		break;

	case HDMI_1080I50:
		siHdmiTx.VIC = 20;
		siHdmiTx.AspectRatio = VMD_ASPECT_RATIO_16x9;
		siHdmiTx.Colorimetry = COLORIMETRY_709;
		siHdmiTx.TclkSel = X1;
		break;

	case HDMI_1080P60:
		siHdmiTx.VIC = 16;
		siHdmiTx.AspectRatio = VMD_ASPECT_RATIO_16x9;
		siHdmiTx.Colorimetry = COLORIMETRY_709;
		siHdmiTx.TclkSel = X1;
		break;

	case HDMI_1080P50:
		siHdmiTx.VIC = 31;
		siHdmiTx.AspectRatio = VMD_ASPECT_RATIO_16x9;
		siHdmiTx.Colorimetry = COLORIMETRY_709;
		siHdmiTx.TclkSel = X1;
		break;
	default:
		break;
	}
}

//------------------------------------------------------------------------------
// Function Name: siHdmiTx_AudioSel()
// Function Description: Select output audio mode
//
// Accepts: Audio Fs
// Returns: none
// Globals: none
//------------------------------------------------------------------------------
void siHdmiTx_AudioSel(byte Afs)
{
	siHdmiTx.AudioMode = AMODE_SPDIF;
	siHdmiTx.AudioChannels = ACHANNEL_2CH;
	siHdmiTx.AudioFs = Afs;
	siHdmiTx.AudioWordLength = ALENGTH_24BITS;
	siHdmiTx.AudioI2SFormat =
		(MCLK256FS << 4) | SCK_SAMPLE_RISING_EDGE | 0x00;
}
