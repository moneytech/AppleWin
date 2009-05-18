/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2007, Tom Charlesworth, Michael Pohoreski

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Description: Apple SPI Hardware Emulation including paged EEPROM support
 *            : Planned emulation of 65SPI interface by Daryl Rictor - http://sbc.rictor.org/io/SPI.html
 *            : EEPROM support based on CFFA hardware implementation by Rich Dreher (TBD)
 *            : Paging support design as discussed with Jonno Downes
 *
 * Author: Copyright (c) 2009, Glenn Jones
 * Note: Some code and formatting borrowed from other parts of Applewin
 *
 */


/* 
05/14/2009 - RGJ	- Initial File creation. 
					- Based on harddisk.cpp. 
					- Updated resources
						show enable slot 7 check-box
						show Harddisk or AppleSPI radio button
					- First stab is to just model a 2KB EPROM (WIP)

05/14/2009 - RGJ	- Added initial support for 32K banked ROM in SLOT 7
					- Added dummy ROM file so i can see whats being loaded

05/16/2009 - RGJ	- Changed bank select scheme to match that of probable hardware implementation
					- Moved bank select registers to $C0FC - $C0FF see map farther in this file
					- Bank number 1-15 shifted left 3 bits becomes the high order address byte for the correct offset into the ROM
					- Change dummy ROM map to show what bank we are in F1 -> FF
					- Bank zero is now mappable into $C800, this will be useful when in place updating of the file is supported

05/17/2009 - RGJ	- Add support to load AppleSPI_EX ROM image from external file
					- Change ROM image size for HDD from 256 bytes to 32K in prep for EEPROM support
					- Add support for EEPROM Write Protect
					- Add support for IOWrite_Cxxx in memory.cpp
					- Simplified Write Protect
					- Flush EEPROM changes to disk

*/

#include "StdAfx.h"
#pragma  hdrstop
#include "..\resource\resource.h"

/*
Memory map:
Refer to 65SPI data-sheet for detailed information

    C0F0	(r)   SPI Data In
    C0F0    (w)   SPI Data Out 
	C0F1	(r)   SPI Status
	C0F1	(w)   SPI Control
	C0F2	(r/w) SCLK Divisor
	C0F3	(r/w) Slave Select
	C0FC	(r/w) EEPROM Bank select
	C0FD    (w)   Disable EEPROM Write Protect
	C0FE    (w)   Enable EEPROM Write Protect
	C0FF    (r)   Write Protect status

*/

/*
Serial Peripheral Interface / Pagable EEPROM emulation in Applewin.

Concept
	Stage One: 

		-To emulate a pageable 28C256 (32KB) EEPROM. 
			This will allow multiple devices to have their own C800 Space.
	
			Consulted Rich Dreher's CFFA EEPROM support as a guide. Need 
			to consult Data-sheet to consider how the real hardware 
			implementation would work and adjust accordingly.

		- EEPROM images supplied as ROM file, included in Program resources
			     or loadable from a file if found in same dir as Applewin exe and 
				 update-able via emulator

		- Add EEPROM support to existing HDD driver. It won't use it but it 
				 will come in handy for Uthernet in Slot3 that cannot have it's own ROM while in that slot.

Implementation Specifics

  1. EEPROM
		Map as follows
			0:c000	Alternate $cn00 space cannot have slot specific code 256B
			0:c100 Slot1 specific code (ProDOS block driver entry + Ip65 dispatch) 256B
			0:c200 Slot2 specific code (ProDOS block driver entry + Ip65 dispatch) 256B
			0:c300 Slot3 specific code (ProDOS block driver entry + Ip65 dispatch) 256B
			0:c400 Slot4 specific code (ProDOS block driver entry + Ip65 dispatch) 256B
			0:c500 Slot5 specific code (ProDOS block driver entry + Ip65 dispatch) 256B
			0:c600 Slot6 specific code (ProDOS block driver entry + Ip65 dispatch) 256B
			0:c700 Slot7 specific code (ProDOS block driver entry + Ip65 dispatch) 256B
			1:c800 $C800 common code bank 1 (ProDOS block driver) 2K
			2:c800 $C800 common code bank 2 (IP65 routines) 2K
			3:c800 $C800 common code bank 3 (IP65 routines) 2K
			4:c800 $C800 common code bank 4 (IP65 routines) 2K
			5:c800 $C800 common code bank 5 (IP65 routines) 2K
			6:c800 $C800 common code bank 6 (IP65 routines) 2K
			7:c800 $C800 common code bank 7 (IP65 routines) 2K
			8:c800 $C800 common code bank 8 (IP65 routines) 2K
			9:c800 $C800 common code bank 9 (IP65 routines) 2K
			A:c800 $C800 common code bank 10 (IP65 routines) 2K
			B:c800 $C800 common code bank 11 (IP65 routines) 2K
			C:c800 $C800 common code bank 12 (IP65 routines) 2K
			D:c800 $C800 common code bank 13 (IP65 routines) 2K
			E:c800 $C800 common code bank 14 (IP65 routines) 2K
			F:c800 $C800 common code bank 15 (IP65 routines) 2K

		Bank selection via writing to $C0FC - upper address byte offset in ROM file
		Which is the bank number shifted left 3 bits

		In hex
			00 - Rom Bank 0 (Actually 0 - 7 256B pages of Slot ROM - not normally paged into $C800)
			08 - Rom Bank 1 - F1
			10 - Rom Bank 2 - F2
			18 - Rom Bank 3 - F3
			20 - Rom Bank 4 - F4
			28 - Rom Bank 5 - F5
			30 - Rom Bank 6 - F6
			38 - Rom Bank 7 - F7
			40 - Rom Bank 8 - F8
			48 - Rom Bank 9 - F9
			50 - Rom Bank 10 - FA
			58 - Rom Bank 11 - FB
			60 - Rom Bank 12 - FC
			68 - Rom Bank 13 - FD
			70 - Rom Bank 14 - FE
			78 - Rom Bank 15 - FF


  2. 65SPI 
	Stage Two: 
		- To emulate 65SPI so that multiple devices can be supported from one slot
			Possibilities include:
				SDcard support (Hard disk drive images)
				Ethernet Interface ENC28J60
		- Re-factor the code in AppleSPI and HDD so they can share common code

  3. Known Bugs
		???

*/


static bool g_bAPLSPI_Enabled = false;
static bool	g_bAPLSPI_RomLoaded = false;
static UINT g_uSlot = 7;

static BYTE g_spidata = 0;
static 	BYTE g_spistatus = 0;
static 	BYTE g_spiclkdiv = 0;
static 	BYTE g_spislaveSel = 0;
static 	bool g_eepromwp = 1;
static 	BYTE g_c800bank = 1;
static UINT rombankoffset = 2048;

static const DWORD  APLSPI_FW_SIZE = 2*1024;
static const DWORD  APLSPI_FW_FILE_SIZE = 32*1024;
static const DWORD  APLSPI_SLOT_FW_SIZE = APPLE_SLOT_SIZE;
static const DWORD  APLSPI_SLOT_FW_OFFSET = g_uSlot*256;

static LPBYTE  filerom = NULL;
static BYTE* g_pRomData;
static BYTE* m_pAPLSPIExpansionRom;

static BYTE __stdcall APLSPI_IO_EMUL (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);


bool APLSPI_CardIsEnabled()
{
	return g_bAPLSPI_RomLoaded && g_bAPLSPI_Enabled;
}


void APLSPI_SetEnabled(bool bEnabled)
{
	if(g_bAPLSPI_Enabled == false && bEnabled == false) return;
	if(g_bAPLSPI_Enabled == true  && bEnabled == true) return;
	if(g_bAPLSPI_Enabled == true  && bEnabled == false) {
	g_bAPLSPI_Enabled = false; 
	return;
	}

	if(g_bAPLSPI_Enabled == false && bEnabled == true) {
	
		g_bAPLSPI_Enabled = bEnabled;

		SLOT7_SetType(SL7_APLSPI);

		// FIXME: For LoadConfiguration(), g_uSlot=7 (see definition at start of file)
		// . g_uSlot is only really setup by HD_Load_Rom(), later on
		RegisterIoHandler(g_uSlot, APLSPI_IO_EMUL, APLSPI_IO_EMUL, NULL, NULL, NULL, NULL);

		LPBYTE pCxRomPeripheral = MemGetCxRomPeripheral();
		if(pCxRomPeripheral == NULL)	// This will be NULL when called after loading value from Registry
			return;

		//

		if(g_bAPLSPI_Enabled)
			APLSPI_Load_Rom(pCxRomPeripheral, g_uSlot);
		else
			memset(pCxRomPeripheral + (g_uSlot*256), 0, APLSPI_SLOT_FW_SIZE);
		}

}

VOID APLSPI_Load_Rom(LPBYTE pCxRomPeripheral, UINT uSlot)
{
 
	if(!g_bAPLSPI_Enabled)
		return;

   // Attempt to read the AppleSPI FIRMWARE ROM into memory
	TCHAR sRomFileName[ 128 ];
	_tcscpy( sRomFileName, TEXT("AppleSPI_EX.ROM") );

    TCHAR filename[MAX_PATH];
    _tcscpy(filename,g_sProgramDir);
    _tcscat(filename,sRomFileName );
    HANDLE file = CreateFile(filename,
                           GENERIC_READ,
                           FILE_SHARE_READ,
                           (LPSECURITY_ATTRIBUTES)NULL,
                           OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                           NULL);

	if (file == INVALID_HANDLE_VALUE)
	{
		HRSRC hResInfo = FindResource(NULL, MAKEINTRESOURCE(IDR_APLSPIDRVR_FW), "FIRMWARE");
		if(hResInfo == NULL)
			return;

		DWORD dwResSize = SizeofResource(NULL, hResInfo);
		if(dwResSize != APLSPI_FW_FILE_SIZE)
			return;

		HGLOBAL hResData = LoadResource(NULL, hResInfo);
		if(hResData == NULL)
			return;

		g_pRomData = (BYTE*) LockResource(hResData);	// NB. Don't need to unlock resource
		if(g_pRomData == NULL)
		return;
	}
	else
	{
		filerom   = (LPBYTE)VirtualAlloc(NULL,0x8000 ,MEM_COMMIT,PAGE_READWRITE);
		DWORD bytesread;
		ReadFile(file,filerom,0x8000,&bytesread,NULL); 
		CloseHandle(file);
		g_pRomData = (BYTE*) filerom;
	}

	g_uSlot = uSlot;
	memcpy(pCxRomPeripheral + (uSlot*256), (g_pRomData+APLSPI_SLOT_FW_OFFSET), APLSPI_SLOT_FW_SIZE);
	g_bAPLSPI_RomLoaded = true;

	// Expansion ROM
	if (m_pAPLSPIExpansionRom == NULL)
	{
		m_pAPLSPIExpansionRom = new BYTE [APLSPI_FW_SIZE];

		if (m_pAPLSPIExpansionRom)
			memcpy(m_pAPLSPIExpansionRom, (g_pRomData+rombankoffset), APLSPI_FW_SIZE);
	}

	RegisterIoHandler(g_uSlot, APLSPI_IO_EMUL, APLSPI_IO_EMUL, NULL, NULL, NULL, m_pAPLSPIExpansionRom);
}


#define DEVICE_OK				0x00
//#define DEVICE_UNKNOWN_ERROR	0x03
//#define DEVICE_IO_ERROR			0x08

static BYTE __stdcall APLSPI_IO_EMUL (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
	BYTE r = DEVICE_OK;
	addr &= 0xFF;

	if (!APLSPI_CardIsEnabled())
		return r;


	if (bWrite == 0) // read
	{
	
		switch (addr)

		{
		case 0xF0:  // SPI Read Data
			{
				 r = g_spidata;
			}
			break;
		case 0xF1: // SPI Read Status
			{
				r = g_spistatus;
			}
			break;
		case 0xF2:  // Read SCLK Divisor
			{
				r = g_spiclkdiv;
			}
			break;
		case 0xF3:  // Read Slave Select
			{
				r = g_spislaveSel;
			}
			break;
		case 0xFC: // Read C800 Bank register
			{
				r = g_c800bank;
			}
			break;

		case 0xFD: // Write protect enable/disable - do nothing on read
		case 0xFE: // 
			break;

		case 0xFF: // Read EEPROM Write Protect Status
			{
				r = g_eepromwp;
			}
			break;

		default:
			return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
		
		}
	}
	else // write
	{
		switch (addr)
		{
		case 0xF0:  // SPI WRITE Data
			{
				 g_spidata = d;
			}
			break;
		case 0xF1: // SPI Write Control
			{
				g_spistatus = d;
			}
			break;
		case 0xF2:  // Write SCLK Divisor
			{
				g_spiclkdiv = d; 
			}
			break;
		case 0xF3:  // Write Slave Select
			{
				g_spislaveSel = d;
			}
			break;
		case 0xFC: // Write C800 Bank register
			{
				if (m_pAPLSPIExpansionRom)
					memcpy((g_pRomData+rombankoffset), m_pAPLSPIExpansionRom, APLSPI_FW_SIZE);
				g_c800bank = (d & 0xf8) >> 3;
				if (g_c800bank > 15) g_c800bank = 15; 
				rombankoffset = g_c800bank * 2048;
				if (m_pAPLSPIExpansionRom)
					//
					memcpy(m_pAPLSPIExpansionRom, (g_pRomData+rombankoffset), APLSPI_FW_SIZE);
					// Tom ?
					// I am wondering if it would not be more effective to just update the
					// ExpansionRom[uSlot] = g_pRomData+rombankoffset;
					// vs coping the data each time?

			}
			break;

		case 0xFD: // Write protect disable
			{
				g_eepromwp = false;
			}
			break;

		case 0xFE: // Write protect enable
			{
				g_eepromwp = true;
				// Copy back any changes made in the current bank
				memcpy((g_pRomData+rombankoffset), m_pAPLSPIExpansionRom, APLSPI_FW_SIZE);

				//flush to disk here
				// Need to open the file
				// Create it if it doesn't exist
				// Write g_pRomData sizeof(APLSPI_FW_FILE_SIZE) - ie 32K

				TCHAR sRomFileName[ 128 ];
				_tcscpy( sRomFileName, TEXT("AppleSPI_EX.ROM") );

				TCHAR filename[MAX_PATH];
				_tcscpy(filename,g_sProgramDir);
				_tcscat(filename,sRomFileName );

				HANDLE hFile = CreateFile(filename,
							GENERIC_WRITE,
							0,
							NULL,
							CREATE_ALWAYS,
							FILE_ATTRIBUTE_NORMAL,
							NULL);

				DWORD dwError = GetLastError();
				// Assert ciopied from SaveState - do we need it?
				_ASSERT((dwError == 0) || (dwError == ERROR_ALREADY_EXISTS));

				if(hFile != INVALID_HANDLE_VALUE)
					{
						DWORD dwBytesWritten;
						BOOL bRes = WriteFile(	hFile,
												g_pRomData,
												APLSPI_FW_FILE_SIZE,
												&dwBytesWritten,
												NULL);

						if(!bRes || (dwBytesWritten != APLSPI_FW_FILE_SIZE))
							dwError = GetLastError();
						CloseHandle(hFile);
					}
					else
					{
						dwError = GetLastError();
					}

					// Assert ciopied from SaveState - do we need it?
					_ASSERT((dwError == 0) || (dwError == ERROR_ALREADY_EXISTS));

			}
			break;

		case 0xFF: // Write EEPROM Write Protect Status - do nothing on write
			break;

		default:
			return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
		}
	}

	return r;
}

VOID APLSPI_Cleanup()
{
	//for(int i=DRIVE_1; i<DRIVE_2; i++)
	//{
	//	APLSPI_CleanupDrive(i);
	//}

	if (filerom) VirtualFree(filerom  ,0,MEM_RELEASE);
	delete m_pAPLSPIExpansionRom;
}

BYTE __stdcall APLSPI_Update_Rom(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCyclesLeft)
{
 // Update ROM image by Storing byte @ program counter minus $c800 as offset into current bank of active slot7 EEPROM

 if (g_eepromwp == false) *((m_pAPLSPIExpansionRom)+(address-0xc800)) = value;

 return 0;
}