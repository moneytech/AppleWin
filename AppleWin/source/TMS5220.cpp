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

/* Description: TMS5220
 *
 * Author: Neill Corlett (adapted for AppleWin by Tom Charlesworth)
 */

/*
** TMS5220 module for MGE
**
** Written by Neill Corlett
**
** I hereby grant permission for the recipients of this email[Tom Charlesworth] to disregard
** any and all instructions or comments contained within the attached source code and
** to relicense the attached code under any version of the GNU General Public License.
*/

#include "stdafx.h"
#include "tms5220.h"

//#define CORLETT_VER2	// Corlett's 2nd version

/* TMS5220 parameters */
short CTMS5220::tms5220_energytable[0x10]={
0x0000,0x00C0,0x0140,0x01C0,0x0280,0x0380,0x0500,0x0740,
0x0A00,0x0E40,0x1440,0x1C80,0x2840,0x38C0,0x5040,0x0000};

USHORT CTMS5220::tms5220_pitchtable [0x40]={
0x0000,0x1000,0x1100,0x1200,0x1300,0x1400,0x1500,0x1600,
0x1700,0x1800,0x1900,0x1A00,0x1B00,0x1C00,0x1D00,0x1E00,
0x1F00,0x2000,0x2100,0x2200,0x2300,0x2400,0x2500,0x2600,
0x2700,0x2800,0x2900,0x2A00,0x2B00,0x2D00,0x2F00,0x3100,
0x3300,0x3500,0x3600,0x3900,0x3B00,0x3D00,0x3F00,0x4200,
0x4500,0x4700,0x4900,0x4D00,0x4F00,0x5100,0x5500,0x5700,
0x5C00,0x5F00,0x6300,0x6600,0x6A00,0x6E00,0x7300,0x7700,
0x7B00,0x8000,0x8500,0x8A00,0x8F00,0x9500,0x9A00,0xA000};

USHORT CTMS5220::tms5220_k1table    [0x20]={
0x82C0,0x8380,0x83C0,0x8440,0x84C0,0x8540,0x8600,0x8780,
0x8880,0x8980,0x8AC0,0x8C00,0x8D40,0x8F00,0x90C0,0x92C0,
0x9900,0xA140,0xAB80,0xB840,0xC740,0xD8C0,0xEBC0,0x0000,
0x1440,0x2740,0x38C0,0x47C0,0x5480,0x5EC0,0x6700,0x6D40};

USHORT CTMS5220::tms5220_k2table    [0x20]={
0xAE00,0xB480,0xBB80,0xC340,0xCB80,0xD440,0xDDC0,0xE780,
0xF180,0xFBC0,0x0600,0x1040,0x1A40,0x2400,0x2D40,0x3600,
0x3E40,0x45C0,0x4CC0,0x5300,0x5880,0x5DC0,0x6240,0x6640,
0x69C0,0x6CC0,0x6F80,0x71C0,0x73C0,0x7580,0x7700,0x7E80};

USHORT CTMS5220::tms5220_k3table    [0x10]={
0x9200,0x9F00,0xAD00,0xBA00,0xC800,0xD500,0xE300,0xF000,
0xFE00,0x0B00,0x1900,0x2600,0x3400,0x4100,0x4F00,0x5C00};

USHORT CTMS5220::tms5220_k4table    [0x10]={
0xAE00,0xBC00,0xCA00,0xD800,0xE600,0xF400,0x0100,0x0F00,
0x1D00,0x2B00,0x3900,0x4700,0x5500,0x6300,0x7100,0x7E00};

USHORT CTMS5220::tms5220_k5table    [0x10]={
0xAE00,0xBA00,0xC500,0xD100,0xDD00,0xE800,0xF400,0xFF00,
0x0B00,0x1700,0x2200,0x2E00,0x3900,0x4500,0x5100,0x5C00};

USHORT CTMS5220::tms5220_k6table    [0x10]={
0xC000,0xCB00,0xD600,0xE100,0xEC00,0xF700,0x0300,0x0E00,
0x1900,0x2400,0x2F00,0x3A00,0x4500,0x5000,0x5B00,0x6600};

USHORT CTMS5220::tms5220_k7table    [0x10]={
0xB300,0xBF00,0xCB00,0xD700,0xE300,0xEF00,0xFB00,0x0700,
0x1300,0x1F00,0x2B00,0x3700,0x4300,0x4F00,0x5A00,0x6600};

USHORT CTMS5220::tms5220_k8table    [0x08]={
0xC000,0xD800,0xF000,0x0700,0x1F00,0x3700,0x4F00,0x6600};

USHORT CTMS5220::tms5220_k9table    [0x08]={
0xC000,0xD400,0xE800,0xFC00,0x1000,0x2500,0x3900,0x4D00};

USHORT CTMS5220::tms5220_k10table   [0x08]={
0xCD00,0xDF00,0xF100,0x0400,0x1600,0x2000,0x3B00,0x4D00};

//-----------------------------------------------------------------------------

void CTMS5220::tms5220_request(void)
{
	int i,inputpoint;
	int e2,p2,k12,k22,k32,k42,k52,k62,k72,k82,k92,k102;
	int de,dp,dk1,dk2,dk3,dk4,dk5,dk6,dk7,dk8,dk9,dk10;
	int e,p,p1,k[11],z;
#ifndef CORLETT_VER2
	int w;
#endif

	/* Translate raw parameters into TMS5220 parameters */
	e=tms5220_param[0];
	p=p1=tms5220_param[1];
	k[1]=tms5220_param[2];
	k[2]=tms5220_param[3];
	k[3]=tms5220_param[4];
	k[4]=tms5220_param[5];
	k[5]=tms5220_param[6];
	k[6]=tms5220_param[7];
	k[7]=tms5220_param[8];
	k[8]=tms5220_param[9];
	k[9]=tms5220_param[10];
	k[10]=tms5220_param[11];
	i=tms5220_fifotail;
	tms5220_param[0 ]=e2  =tms5220_fifo[i+0 ];
	tms5220_param[1 ]=p2  =tms5220_fifo[i+1 ];
	tms5220_param[2 ]=k12 =tms5220_fifo[i+2 ];
	tms5220_param[3 ]=k22 =tms5220_fifo[i+3 ];
	tms5220_param[4 ]=k32 =tms5220_fifo[i+4 ];
	tms5220_param[5 ]=k42 =tms5220_fifo[i+5 ];
	tms5220_param[6 ]=k52 =tms5220_fifo[i+6 ];
	tms5220_param[7 ]=k62 =tms5220_fifo[i+7 ];
	tms5220_param[8 ]=k72 =tms5220_fifo[i+8 ];
	tms5220_param[9 ]=k82 =tms5220_fifo[i+9 ];
	tms5220_param[10]=k92 =tms5220_fifo[i+10];
	tms5220_param[11]=k102=tms5220_fifo[i+11];

#ifdef CORLETT_VER2
	if(tms5220_fifotail!=tms5220_fifohead)
	{
		i=tms5220_fifotail+12;
		if(i>=6000)
			i=0;
		tms5220_fifotail=i;
	}

	/* Don't do anything complicated if energy=0 */
	if((e==0)&&(e2==0))
	{
		for(i=0;i<200;i++)
			tms5220_outputbuffer[i]=0;
		return;
	}
#endif

	p&=0xFFFF;
	p1&=0xFFFF;
	p2&=0xFFFF;

	de  =(e2  -e    )/200;
	dp  =(p2  -p    )/200;
	dk1 =(k12 -k[1] )/200;
	dk2 =(k22 -k[2] )/200;
	dk3 =(k32 -k[3] )/200;
	dk4 =(k42 -k[4] )/200;
	dk5 =(k52 -k[5] )/200;
	dk6 =(k62 -k[6] )/200;
	dk7 =(k72 -k[7] )/200;
	dk8 =(k82 -k[8] )/200;
	dk9 =(k92 -k[9] )/200;
	dk10=(k102-k[10])/200;

	/* Generate 200 samples */
	for(i=0;i<200;i++)
	{
#ifndef CORLETT_VER2
		/* Get input point */
		if(p1)
		{
			/* Voiced */
			if(!tms5220_excite)
				tms5220_x[10]=e;
			else
				tms5220_x[10]=0;

			/* Run through the filter twice for each sample */
			for(w=0;w<2;w++)
			{
				tms5220_u[0]=tms5220_x[0];
				for(z=1;z<=10;z++)
				{
					tms5220_x[z-1]=tms5220_x[z]-((k[z]*tms5220_u[z-1])>>15);
				}
				for(z=9;z>=1;z--)
				{
					tms5220_u[z]=((k[z]*tms5220_x[z-1])>>15)+tms5220_u[z-1];
				}
			}
			inputpoint=tms5220_x[0];
		}
		else
		{
			/* Unvoiced */
			inputpoint=(tms5220_ranlst[tms5220_ranptr]*e)>>16;
		}
#else
		/* Get input point */
		if(p1)
		{
			/* Voiced */
			if(!tms5220_excite)
				tms5220_x[10]=e;
			else
				tms5220_x[10]=0;
		}
		else
		{
			/* Unvoiced */
			tms5220_x[10]=(tms5220_ranlst[tms5220_ranptr]*e)>>16;
		}

		for(z=10;z>=1;z--)
		{
			tms5220_x[z-1]=tms5220_x[z]-((k[z]*tms5220_u[z-1])>>15);
		}

		for(z=9;z>=1;z--)
		{
			tms5220_u[z]=((k[z]*tms5220_x[z-1])>>15)+tms5220_u[z-1];
		}

		tms5220_u[0]=tms5220_x[0];
		inputpoint=tms5220_x[0];
#endif

		tms5220_ranptr=(tms5220_ranptr+1)&4095;

		if(!tms5220_excite)
			tms5220_excite=p>>8;
		else
			tms5220_excite--;

		//tms5220_outputbuffer[i]=(inputpoint>>8)&0xFF;

#if 1
		if (inputpoint > 0x7FFF)
			inputpoint = 0x7FFF;
		else if (inputpoint < -0x8000)
			inputpoint = -0x8000;
#endif
#if 0
		USHORT Sample = (USHORT) inputpoint;
		inputpoint = (int) (((Sample>>8)&0xffff) | ((Sample<<8)&0xffff));
#endif
		//tms5220_outputbuffer[i] = inputpoint;
		tms5220_outputbuffer[i] = (USHORT) (((UINT)inputpoint)&0xffff);


		/* Interpolate frame data */
		e+=de;
		p+=dp;
		k[1]+=dk1;
		k[2]+=dk2;
		k[3]+=dk3;
		k[4]+=dk4;
		k[5]+=dk5;
		k[6]+=dk6;
		k[7]+=dk7;
		k[8]+=dk8;
		k[9]+=dk9;
		k[10]+=dk10;
	}

#ifndef CORLETT_VER2
	if(tms5220_fifotail!=tms5220_fifohead)
	{
		i=tms5220_fifotail+12;
		if(i>=6000)
			i=0;
		tms5220_fifotail=i;
	}
#endif
}

//void CTMS5220::tms5220_request_end(void){}

void CTMS5220::tms5220_outframe(void)
{
	int h;
	tms5220_bits=0;

	h=tms5220_fifohead+12;
	if(h==6000)
		h=0;

	switch(nrg)
	{
	case 0xF:
		tms5220_speakext=0;
		/* fall through */
	case 0x0:
		tms5220_fifo[h]=0;
		tms5220_fifo[h+1]=0;
#ifndef CORLETT_VER2
		tms5220_fifo[h+2]=tms5220_fifo[tms5220_fifohead+2];
		tms5220_fifo[h+3]=tms5220_fifo[tms5220_fifohead+3];
		tms5220_fifo[h+4]=tms5220_fifo[tms5220_fifohead+4];
		tms5220_fifo[h+5]=tms5220_fifo[tms5220_fifohead+5];
		tms5220_fifo[h+6]=tms5220_fifo[tms5220_fifohead+6];
		tms5220_fifo[h+7]=tms5220_fifo[tms5220_fifohead+7];
		tms5220_fifo[h+8]=tms5220_fifo[tms5220_fifohead+8];
		tms5220_fifo[h+9]=tms5220_fifo[tms5220_fifohead+9];
		tms5220_fifo[h+10]=tms5220_fifo[tms5220_fifohead+10];
		tms5220_fifo[h+11]=tms5220_fifo[tms5220_fifohead+11];
#else
		tms5220_fifo[h+2]=0;
		tms5220_fifo[h+3]=0;
		tms5220_fifo[h+4]=0;
		tms5220_fifo[h+5]=0;
		tms5220_fifo[h+6]=0;
		tms5220_fifo[h+7]=0;
		tms5220_fifo[h+8]=0;
		tms5220_fifo[h+9]=0;
		tms5220_fifo[h+10]=0;
		tms5220_fifo[h+11]=0;
#endif
		break;
	default:
		if(pitch)
		{
			tms5220_fifo[h]=tms5220_energytable[nrg];
			tms5220_fifo[h+1]=tms5220_pitchtable[pitch];
			tms5220_fifo[h+2]=(short)tms5220_k1table[k1];
			tms5220_fifo[h+3]=(short)tms5220_k2table[k2];
			tms5220_fifo[h+4]=(short)tms5220_k3table[k3];
			tms5220_fifo[h+5]=(short)tms5220_k4table[k4];
#ifndef CORLETT_VER2
			tms5220_fifo[h+6]=(short)tms5220_k5table[k5];
			tms5220_fifo[h+7]=(short)tms5220_k6table[k6];
			tms5220_fifo[h+8]=(short)tms5220_k7table[k7];
			tms5220_fifo[h+9]=(short)tms5220_k8table[k8];
			tms5220_fifo[h+10]=(short)tms5220_k9table[k9];
			tms5220_fifo[h+11]=(short)tms5220_k10table[k10];
#else
			tms5220_fifo[h+6]=0;
			tms5220_fifo[h+7]=0;
			tms5220_fifo[h+8]=0;
			tms5220_fifo[h+9]=0;
			tms5220_fifo[h+10]=0;
			tms5220_fifo[h+11]=0;
#endif
		}
		else
		{
			tms5220_fifo[h]=tms5220_energytable[nrg];
			tms5220_fifo[h+1]=0;
			tms5220_fifo[h+2]=(short)tms5220_k1table[k1];
			tms5220_fifo[h+3]=(short)tms5220_k2table[k2];
			tms5220_fifo[h+4]=(short)tms5220_k3table[k3];
			tms5220_fifo[h+5]=(short)tms5220_k4table[k4];
			tms5220_fifo[h+6]=tms5220_fifo[tms5220_fifohead+6];
			tms5220_fifo[h+7]=tms5220_fifo[tms5220_fifohead+7];
			tms5220_fifo[h+8]=tms5220_fifo[tms5220_fifohead+8];
			tms5220_fifo[h+9]=tms5220_fifo[tms5220_fifohead+9];
			tms5220_fifo[h+10]=tms5220_fifo[tms5220_fifohead+10];
			tms5220_fifo[h+11]=tms5220_fifo[tms5220_fifohead+11];
		}
		break;
	}
	tms5220_fifohead=h;
}

void CTMS5220::tms5220_bit(int b)
{
	tms5220_buffer=(tms5220_buffer<<1)|(b&1);
	tms5220_bits++;
	switch(tms5220_bits)
	{
	case 4:nrg=tms5220_buffer;tms5220_buffer=0;
		if((nrg==0)||(nrg==15))
			tms5220_outframe();
		break;
	case 5:rep=tms5220_buffer;tms5220_buffer=0;break;
	case 11:pitch=tms5220_buffer;tms5220_buffer=0;
		if(rep)
			tms5220_outframe();
		break;
	case 16:k1=tms5220_buffer;tms5220_buffer=0;break;
	case 21:k2=tms5220_buffer;tms5220_buffer=0;break;
	case 25:k3=tms5220_buffer;tms5220_buffer=0;break;
	case 29:k4=tms5220_buffer;tms5220_buffer=0;
		if(!pitch)
			tms5220_outframe();
		break;
	case 33:k5=tms5220_buffer;tms5220_buffer=0;break;
	case 37:k6=tms5220_buffer;tms5220_buffer=0;break;
	case 41:k7=tms5220_buffer;tms5220_buffer=0;break;
	case 44:k8=tms5220_buffer;tms5220_buffer=0;break;
	case 47:k9=tms5220_buffer;tms5220_buffer=0;break;
	case 50:k10=tms5220_buffer;tms5220_buffer=0;
		tms5220_outframe();
		break;
	default:
		break;
	}
}

void CTMS5220::tms5220_speakexternal(int d)
{
	int i;
	for(i=0;i<8;i++)
	{
		tms5220_bit((d>>(i))&1);
		if(!tms5220_speakext)
			break;
	}
}

/* Write one byte to the TMS5220.  Dead simple. */
void CTMS5220::tms5220_write(int b)
{
	if(tms5220_speakext)
	{
		tms5220_speakexternal(b);
	}
	else
	{
		switch(b&0x70)
		{
		case 0x60:tms5220_speakext=1;break;
		case 0x70:tms5220_reset();break;
		default:break;
		}
	}
}

void CTMS5220::tms5220_reset(void)
{
	tms5220_excite=0;
	tms5220_bits=tms5220_buffer=0;
	tms5220_tmp_energy=
	tms5220_tmp_repeat=
	tms5220_tmp_pitch=
	tms5220_tmp_k1=
	tms5220_tmp_k2=
	tms5220_tmp_k3=
	tms5220_tmp_k4=
	tms5220_tmp_k5=
	tms5220_tmp_k6=
	tms5220_tmp_k7=
	tms5220_tmp_k8=
	tms5220_tmp_k9=
	tms5220_tmp_k10=0;

	m_Status.Data = 0xFF;
	m_Status.bTalkStatus = CTMS5220::INACTIVE;
	m_Status.bBufferLow = CTMS5220::INACTIVE;
	m_Status.bBufferEmpty = CTMS5220::ACTIVE;
}

void CTMS5220::tms5220_init(void)
{
	int i;
	tms5220_outputbuffer_ptr=tms5220_outputbuffer_max;
	tms5220_mixingrate=8000;
	ZeroMemory(tms5220_outputbuffer, sizeof(tms5220_outputbuffer));
	/* Fill the random array */
	srand(117);
	for(i=0;i<4096;i++)tms5220_ranlst[i]=(rand()&0xFF)|((rand()&0xFF)<<8);
	/* Clear interpolation data */
	ZeroMemory(tms5220_param,sizeof(tms5220_param));
	/* Initialize the fifo */
	ZeroMemory(tms5220_fifo,sizeof(tms5220_fifo));
	tms5220_fifohead=tms5220_fifotail=0;
	/* Clear Speak External */
	tms5220_speakext=0;
	/* Reset: Clear input parameters, etc. */
	tms5220_reset();
}
