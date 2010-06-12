/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2010, Tom Charlesworth, Michael Pohoreski

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

/* Description: EchoII
 *
 * Author: Tom Charlesworth
 */

// Assume:
// . Interrupt isn't connected
//

#include "stdafx.h"
#include "Echo.h"

CEcho::CEcho(const int nSampleRate) :
	m_uSlot(0),
	m_nPlaybackRate(nSampleRate),
	m_nAudioCounter(nSampleRate)
{
	m_pBuffer = new short[nSampleRate];

	m_TMS5220.tms5220_init();
	Reset();
}

CEcho::~CEcho(void)
{
	delete [] m_pBuffer;
}

void CEcho::Reset(void)
{
	m_nAudioCounter = m_nPlaybackRate;
	m_TMS5220.tms5220_reset();
}

void CEcho::Initialize(LPBYTE pCxRomPeripheral, UINT uSlot)
{
	m_uSlot = uSlot;
	RegisterIoHandler(uSlot, &CEcho::IORead, &CEcho::IOWrite, NULL, NULL, this, NULL);
}

void CEcho::Uninitialize(void)
{
}

//===========================================================================

short* CEcho::AudioRequest(const UINT uNumSamples)
{
#if 0
	// Calc # of TMS5220 samples, before up-sampling
	UINT uNumTMSSamples = 0;
	for (UINT i=0; i<uNumSamples; i++)
	{
		m_nAudioCounter -= 8000;	//m_TMS5220.tms5220_mixingrate;

		if(m_nAudioCounter < 0)
		{
			m_nAudioCounter += m_nPlaybackRate;
			uNumTMSSamples++;
		}
	}
#endif

	//

	for (UINT i=0; i<uNumSamples; i++)
	{
		if(m_TMS5220.tms5220_outputbuffer_ptr >= m_TMS5220.tms5220_outputbuffer_max)
		{
			m_TMS5220.tms5220_outputbuffer_ptr -= m_TMS5220.tms5220_outputbuffer_max;
			m_TMS5220.tms5220_request();
		}

		m_pBuffer[i] = m_TMS5220.tms5220_outputbuffer[ m_TMS5220.tms5220_outputbuffer_ptr ];

		m_nAudioCounter -= m_TMS5220.tms5220_mixingrate;

		if(m_nAudioCounter < 0)
		{
			m_nAudioCounter += m_nPlaybackRate;
			m_TMS5220.tms5220_outputbuffer_ptr++;
		}
	}

	return m_pBuffer;
}

//===========================================================================

BYTE __stdcall CEcho::IORead(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nCyclesLeft)
{
	UINT uSlot = ((uAddr & 0xff) >> 4) - 8;
	CEcho* pEcho = (CEcho*) MemGetSlotParameters(uSlot);

//	char szDbg[100];
//	WORD uCallerAddr = (*(WORD*)&mem[regs.sp+1]) - 2;
//	sprintf(szDbg, "R : $%04X (PC=$%04X, Caller=$%04X)\n", uAddr, PC, uCallerAddr);
//	OutputDebugString(szDbg);

	return pEcho->m_TMS5220.GetStatus();
}

BYTE __stdcall CEcho::IOWrite(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nCyclesLeft)
{
	UINT uSlot = ((uAddr & 0xff) >> 4) - 8;
	CEcho* pEcho = (CEcho*) MemGetSlotParameters(uSlot);

//	char szDbg[100];
//	sprintf(szDbg, "W : $%04X (PC=$%04X) Data=$%02X\n", uAddr, PC, uValue);
//	OutputDebugString(szDbg);

	pEcho->m_TMS5220.tms5220_write(uValue);

	return 0;
}

