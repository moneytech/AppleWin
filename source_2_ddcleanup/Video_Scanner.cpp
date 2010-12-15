/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2009, Tom Charlesworth, Michael Pohoreski, Nick Westgate

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

/* Description: Emulation of video modes
 *
 * Author: Nick Westgate, Michael Pohoreski
 */

#include "StdAfx.h"
#pragma  hdrstop
#include "..\resource\resource.h"

// Types ____________________________________________________________

	// video scanner constants
	int const kHBurstClock      =    53; // clock when Color Burst starts
	int const kHBurstClocks     =     4; // clocks per Color Burst duration
	int const kHClock0State     =  0x18; // H[543210] = 011000
	int const kHClocks          =    65; // clocks per horizontal scan (including HBL)
	int const kHPEClock         =    40; // clock when HPE (horizontal preset enable) goes low
	int const kHPresetClock     =    41; // clock when H state presets
	int const kHSyncClock       =    49; // clock when HSync starts
	int const kHSyncClocks      =     4; // clocks per HSync duration
	int const kNTSCScanLines    =   262; // total scan lines including VBL (NTSC)
	int const kNTSCVSyncLine    =   224; // line when VSync starts (NTSC)
	int const kPALScanLines     =   312; // total scan lines including VBL (PAL)
	int const kPALVSyncLine     =   264; // line when VSync starts (PAL)
	int const kVLine0State      = 0x100; // V[543210CBA] = 100000000
	int const kVPresetLine      =   256; // line when V state presets
	int const kVSyncLines       =     4; // lines per VSync duration

// Globals (Public)__________________________________________________

	// TODO: FIXME: Add to video dialog and Serialize!!!
	bool bVideoScannerNTSC = true;  // NTSC video scanning (or PAL)

	
// Globals (Private)_________________________________________________

	static DWORD     dwVBlCounter     = 0;

// Implementation
// ________________________________________________________________________________________________

//===========================================================================
void VideoUpdateVbl (DWORD dwCyclesThisFrame)
{
	// TODO: Do we need both?? dwClksPerFrame, kHClocks
	dwVBlCounter = (DWORD) ((double)dwCyclesThisFrame / (double)uCyclesPerLine);

#if VIDEO_SCANNER_PIXEL_ACCURATE
#endif

#if VIDEO_SCANNER_LINE_ACCURATE
	int nOldY = 0;
	int nNewY= 0;

	int iOldVideoMode = 0;
	int iNewVideoMode = 0;
#endif

}

//===========================================================================
//
// References to Jim Sather's books are given as eg:
// UTAIIe:5-7,P3 (Understanding the Apple IIe, chapter 5, page 7, Paragraph 3)
//
WORD VideoGetScannerAddress(bool* pbVblBar_OUT, const DWORD uExecutedCycles)
{
    // get video scanner position
    //
    int nCycles = CpuGetCyclesThisFrame(uExecutedCycles);

    // machine state switches
    //
    int nHires   = (SW_HIRES && !SW_TEXT) ? 1 : 0;
    int nPage2   = (SW_PAGE2) ? 1 : 0;
    int n80Store = (MemGet80Store()) ? 1 : 0;

    // calculate video parameters according to display standard
    //
    int nScanLines  = bVideoScannerNTSC ? kNTSCScanLines : kPALScanLines;
    int nVSyncLine  = bVideoScannerNTSC ? kNTSCVSyncLine : kPALVSyncLine;
    int nScanCycles = nScanLines * kHClocks;

    // calculate horizontal scanning state
    //
    int nHClock = (nCycles + kHPEClock) % kHClocks; // which horizontal scanning clock
    int nHState = kHClock0State + nHClock; // H state bits
    if (nHClock >= kHPresetClock) // check for horizontal preset
    {
        nHState -= 1; // correct for state preset (two 0 states)
    }
    int h_0 = (nHState >> 0) & 1; // get horizontal state bits
    int h_1 = (nHState >> 1) & 1;
    int h_2 = (nHState >> 2) & 1;
    int h_3 = (nHState >> 3) & 1;
    int h_4 = (nHState >> 4) & 1;
    int h_5 = (nHState >> 5) & 1;

    // calculate vertical scanning state
    //
    int nVLine  = nCycles / kHClocks; // which vertical scanning line
    int nVState = kVLine0State + nVLine; // V state bits
    if ((nVLine >= kVPresetLine)) // check for previous vertical state preset
    {
        nVState -= nScanLines; // compensate for preset
    }
    int v_A = (nVState >> 0) & 1; // get vertical state bits
    int v_B = (nVState >> 1) & 1;
    int v_C = (nVState >> 2) & 1;
    int v_0 = (nVState >> 3) & 1;
    int v_1 = (nVState >> 4) & 1;
    int v_2 = (nVState >> 5) & 1;
    int v_3 = (nVState >> 6) & 1;
    int v_4 = (nVState >> 7) & 1;
    int v_5 = (nVState >> 8) & 1;

    // calculate scanning memory address
    //
    if (nHires && SW_MIXED && v_4 && v_2) // HIRES TIME signal (UTAIIe:5-7,P3)
    {
        nHires = 0; // address is in text memory for mixed hires
    }

    int nAddend0 = 0x0D; // 1            1            0            1
    int nAddend1 =              (h_5 << 2) | (h_4 << 1) | (h_3 << 0);
    int nAddend2 = (v_4 << 3) | (v_3 << 2) | (v_4 << 1) | (v_3 << 0);
    int nSum     = (nAddend0 + nAddend1 + nAddend2) & 0x0F; // SUM (UTAIIe:5-9)

    int nAddress = 0; // build address from video scanner equations (UTAIIe:5-8,T5.1)
    nAddress |= h_0  << 0; // a0
    nAddress |= h_1  << 1; // a1
    nAddress |= h_2  << 2; // a2
    nAddress |= nSum << 3; // a3 - a6
    nAddress |= v_0  << 7; // a7
    nAddress |= v_1  << 8; // a8
    nAddress |= v_2  << 9; // a9

    int p2a = !(nPage2 && !n80Store);
    int p2b = nPage2 && !n80Store;

    if (nHires) // hires?
    {
        // Y: insert hires-only address bits
        //
        nAddress |= v_A << 10; // a10
        nAddress |= v_B << 11; // a11
        nAddress |= v_C << 12; // a12
        nAddress |= p2a << 13; // a13
        nAddress |= p2b << 14; // a14
    }
    else
    {
        // N: insert text-only address bits
        //
        nAddress |= p2a << 10; // a10
        nAddress |= p2b << 11; // a11

        // Apple ][ (not //e) and HBL?
		//
		if (IS_APPLE2 && // Apple II only (UTAIIe:I-4,#5)
			!h_5 && (!h_4 || !h_3)) // HBL (UTAIIe:8-10,F8.5)
        {
            nAddress |= 1 << 12; // Y: a12 (add $1000 to address!)
        }
    }

    // update VBL' state
    //
	if (pbVblBar_OUT != NULL)
	{
		*pbVblBar_OUT = !v_4 || !v_3; // VBL' = (v_4 & v_3)' (UTAIIe:5-10,#3)
	}
    return static_cast<WORD>(nAddress);
}

//===========================================================================

// Derived from VideoGetScannerAddress()
bool VideoGetVbl(const DWORD uExecutedCycles)
{
    // get video scanner position
    //
    int nCycles = CpuGetCyclesThisFrame(uExecutedCycles);

    // calculate video parameters according to display standard
    //
    int nScanLines  = bVideoScannerNTSC ? kNTSCScanLines : kPALScanLines;

    // calculate vertical scanning state
    //
    int nVLine  = nCycles / kHClocks; // which vertical scanning line
    int nVState = kVLine0State + nVLine; // V state bits
    if ((nVLine >= kVPresetLine)) // check for previous vertical state preset
    {
        nVState -= nScanLines; // compensate for preset
    }
    int v_3 = (nVState >> 6) & 1;
    int v_4 = (nVState >> 7) & 1;

    // update VBL' state
    //
	if (v_4 & v_3) // VBL?
	{
		return false; // Y: VBL' is false
	}
	else
	{
		return true; // N: VBL' is true
	}
}
