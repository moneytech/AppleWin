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

/* Description: Slot7 enable/disable Code
 * slot7.cpp
 *
 * Author: Copyright (c) 2009, Glenn Jones
 * Note: Some code and formatting borrowed from other parts of Applewin
 *
 */


/* 05/14/2009 - RGJ - Initial File creation. 
					- Externalize Slot 7 support
*/

#include "StdAfx.h"
#pragma  hdrstop
#include "..\resource\resource.h"

static eSLOT7TYPE g_Slot7Type = SL7_HDD;	// Slot 7 enable for HDD(dialog var)
static bool g_bSLOT7_Enabled = false;

eSLOT7TYPE SLOT7_GetType()
{
	return g_Slot7Type;
}


bool SLOT7_IsEnabled()
{
	return g_bSLOT7_Enabled;
}

void SLOT7_SetEnabled(bool bEnabled)
{
	if(g_bSLOT7_Enabled == bEnabled)
		return;

	g_bSLOT7_Enabled = bEnabled;

}

void SLOT7_SetType(eSLOT7TYPE Slot7Type)
{
	if ((Slot7Type == SL7_UNINIT) || (g_Slot7Type == Slot7Type))
		return;

	g_Slot7Type = Slot7Type;

	if(g_Slot7Type == SC_NONE)
		// can this reaklly happen?
		assert(true);
}
