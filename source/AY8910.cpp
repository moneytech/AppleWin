/***************************************************************************

  ay8910.c


  Emulation of the AY-3-8910 / YM2149 sound chip.

  Based on various code snippets by Ville Hallik, Michael Cuddy,
  Tatsuyuki Satoh, Fabrice Frances, Nicola Salmoria.

***************************************************************************/

// 
// From mame.txt (http://www.mame.net/readme.html)
// 
// VI. Reuse of Source Code
// --------------------------
//    This chapter might not apply to specific portions of MAME (e.g. CPU
//    emulators) which bear different copyright notices.
//    The source code cannot be used in a commercial product without the written
//    authorization of the authors. Use in non-commercial products is allowed, and
//    indeed encouraged.  If you use portions of the MAME source code in your
//    program, however, you must make the full source code freely available as
//    well.
//    Usage of the _information_ contained in the source code is free for any use.
//    However, given the amount of time and energy it took to collect this
//    information, if you find new information we would appreciate if you made it
//    freely available as well.
// 


#include <windows.h>
#include <stdio.h>
#include <crtdbg.h>
#include "AY8910.h"

#include "Common.h"
#include "Structs.h"
#include "Applewin.h"		// For g_fh
#include "Mockingboard.h"	// For g_uTimer1IrqCount

#if 0

///////////////////////////////////////////////////////////
// typedefs & dummy funcs to allow MAME code to compile:

typedef UINT8 (*mem_read_handler)(UINT32);
typedef void (*mem_write_handler)(UINT32, UINT8);

static void logerror(char* psz, ...)
{
}

static unsigned short activecpu_get_pc()
{
	return 0;
}

//
///////////////////////////////////////////////////////////

#define MAX_OUTPUT 0x7fff

// See AY8910_set_clock() for definition of STEP
#define STEP 0x8000


static int num = 0, ym_num = 0;

struct AY8910
{
	int Channel;
	int SampleRate;
	mem_read_handler PortAread;
	mem_read_handler PortBread;
	mem_write_handler PortAwrite;
	mem_write_handler PortBwrite;
	int register_latch;
	unsigned char Regs[16];
	int lastEnable;
	unsigned int UpdateStep;
	int PeriodA,PeriodB,PeriodC,PeriodN,PeriodE;
	int CountA,CountB,CountC,CountN,CountE;
	unsigned int VolA,VolB,VolC,VolE;
	unsigned char EnvelopeA,EnvelopeB,EnvelopeC;
	unsigned char OutputA,OutputB,OutputC,OutputN;
	signed char CountEnv;
	unsigned char Hold,Alternate,Attack,Holding;
	int RNG;
	unsigned int VolTable[32];
};

/* register id's */
#define AY_AFINE	(0)
#define AY_ACOARSE	(1)
#define AY_BFINE	(2)
#define AY_BCOARSE	(3)
#define AY_CFINE	(4)
#define AY_CCOARSE	(5)
#define AY_NOISEPER	(6)
#define AY_ENABLE	(7)
#define AY_AVOL		(8)
#define AY_BVOL		(9)
#define AY_CVOL		(10)
#define AY_EFINE	(11)
#define AY_ECOARSE	(12)
#define AY_ESHAPE	(13)

#define AY_PORTA	(14)
#define AY_PORTB	(15)


static struct AY8910 AYPSG[MAX_8910];		/* array of PSG's */

static bool g_bAYReset = false;		// Doing AY8910_reset()

//-----------------------------------------------------------------------------

//#define LOG_AY8910
#ifdef LOG_AY8910
static void LogAY8910(int n, int r, UINT uFreq)
{
	// TO DO: Determine freq from 6522 timer

	if ((g_fh == NULL) || g_bAYReset)
		return;

	static UINT nCnt = 0;
	const UINT nNumAYs = 4;				// 1..4
	if((r == 0))
	{
		if(nCnt == 0)
		{
			fprintf(g_fh, "Time : ");
			for(UINT i=0; i<nNumAYs; i++)
				fprintf(g_fh, "APer BPer CPer NP EN AV BV CV  ");
			fprintf(g_fh, "\n");
		}

		fprintf(g_fh, "%02d.%02d: ", g_uTimer1IrqCount/uFreq, g_uTimer1IrqCount%uFreq);

		for(int j=0; j<n*(3*5+5*3+1); j++)
			fprintf(g_fh, " ");

		UINT i=n;
		{
			UCHAR* pAYRegs = &AYPSG[i].Regs[0];
			fprintf(g_fh, "%04X ", *(USHORT*)&pAYRegs[AY_AFINE]);
			fprintf(g_fh, "%04X ", *(USHORT*)&pAYRegs[AY_BFINE]);
			fprintf(g_fh, "%04X ", *(USHORT*)&pAYRegs[AY_CFINE]);
			fprintf(g_fh, "%02X ", pAYRegs[AY_NOISEPER]);
			fprintf(g_fh, "%02X ", pAYRegs[AY_ENABLE]);
			fprintf(g_fh, "%02X ", pAYRegs[AY_AVOL]);
			fprintf(g_fh, "%02X ", pAYRegs[AY_BVOL]);
			fprintf(g_fh, "%02X  ", pAYRegs[AY_CVOL]);
		}
		fprintf(g_fh, "\n");

		nCnt++;
	}
}
#endif

//-----------------------------------------------------------------------------

void _AYWriteReg(int n, int r, int v)
{
	struct AY8910 *PSG = &AYPSG[n];
	int old;


	PSG->Regs[r] = v;

#ifdef LOG_AY8910
	LogAY8910(n, r, 60);
#endif

	/* A note about the period of tones, noise and envelope: for speed reasons,*/
	/* we count down from the period to 0, but careful studies of the chip     */
	/* output prove that it instead counts up from 0 until the counter becomes */
	/* greater or equal to the period. This is an important difference when the*/
	/* program is rapidly changing the period to modulate the sound.           */
	/* To compensate for the difference, when the period is changed we adjust  */
	/* our internal counter.                                                   */
	/* Also, note that period = 0 is the same as period = 1. This is mentioned */
	/* in the YM2203 data sheets. However, this does NOT apply to the Envelope */
	/* period. In that case, period = 0 is half as period = 1. */
	switch( r )
	{
	case AY_AFINE:
	case AY_ACOARSE:
		PSG->Regs[AY_ACOARSE] &= 0x0f;
		old = PSG->PeriodA;
		PSG->PeriodA = (PSG->Regs[AY_AFINE] + 256 * PSG->Regs[AY_ACOARSE]) * PSG->UpdateStep;
		if (PSG->PeriodA == 0) PSG->PeriodA = PSG->UpdateStep;
		PSG->CountA += PSG->PeriodA - old;
		if (PSG->CountA <= 0) PSG->CountA = 1;
		break;
	case AY_BFINE:
	case AY_BCOARSE:
		PSG->Regs[AY_BCOARSE] &= 0x0f;
		old = PSG->PeriodB;
		PSG->PeriodB = (PSG->Regs[AY_BFINE] + 256 * PSG->Regs[AY_BCOARSE]) * PSG->UpdateStep;
		if (PSG->PeriodB == 0) PSG->PeriodB = PSG->UpdateStep;
		PSG->CountB += PSG->PeriodB - old;
		if (PSG->CountB <= 0) PSG->CountB = 1;
		break;
	case AY_CFINE:
	case AY_CCOARSE:
		PSG->Regs[AY_CCOARSE] &= 0x0f;
		old = PSG->PeriodC;
		PSG->PeriodC = (PSG->Regs[AY_CFINE] + 256 * PSG->Regs[AY_CCOARSE]) * PSG->UpdateStep;
		if (PSG->PeriodC == 0) PSG->PeriodC = PSG->UpdateStep;
		PSG->CountC += PSG->PeriodC - old;
		if (PSG->CountC <= 0) PSG->CountC = 1;
		break;
	case AY_NOISEPER:
		PSG->Regs[AY_NOISEPER] &= 0x1f;
		old = PSG->PeriodN;
		PSG->PeriodN = PSG->Regs[AY_NOISEPER] * PSG->UpdateStep;
		if (PSG->PeriodN == 0) PSG->PeriodN = PSG->UpdateStep;
		PSG->CountN += PSG->PeriodN - old;
		if (PSG->CountN <= 0) PSG->CountN = 1;
		break;
	case AY_ENABLE:
		if ((PSG->lastEnable == -1) ||
		    ((PSG->lastEnable & 0x40) != (PSG->Regs[AY_ENABLE] & 0x40)))
		{
			/* write out 0xff if port set to input */
			if (PSG->PortAwrite)
				(*PSG->PortAwrite)(0, (UINT8) ((PSG->Regs[AY_ENABLE] & 0x40) ? PSG->Regs[AY_PORTA] : 0xff));	// [TC: UINT8 cast]
		}

		if ((PSG->lastEnable == -1) ||
		    ((PSG->lastEnable & 0x80) != (PSG->Regs[AY_ENABLE] & 0x80)))
		{
			/* write out 0xff if port set to input */
			if (PSG->PortBwrite)
				(*PSG->PortBwrite)(0, (UINT8) ((PSG->Regs[AY_ENABLE] & 0x80) ? PSG->Regs[AY_PORTB] : 0xff));	// [TC: UINT8 cast]
		}

		PSG->lastEnable = PSG->Regs[AY_ENABLE];
		break;
	case AY_AVOL:
		PSG->Regs[AY_AVOL] &= 0x1f;
		PSG->EnvelopeA = PSG->Regs[AY_AVOL] & 0x10;
		PSG->VolA = PSG->EnvelopeA ? PSG->VolE : PSG->VolTable[PSG->Regs[AY_AVOL] ? PSG->Regs[AY_AVOL]*2+1 : 0];
		break;
	case AY_BVOL:
		PSG->Regs[AY_BVOL] &= 0x1f;
		PSG->EnvelopeB = PSG->Regs[AY_BVOL] & 0x10;
		PSG->VolB = PSG->EnvelopeB ? PSG->VolE : PSG->VolTable[PSG->Regs[AY_BVOL] ? PSG->Regs[AY_BVOL]*2+1 : 0];
		break;
	case AY_CVOL:
		PSG->Regs[AY_CVOL] &= 0x1f;
		PSG->EnvelopeC = PSG->Regs[AY_CVOL] & 0x10;
		PSG->VolC = PSG->EnvelopeC ? PSG->VolE : PSG->VolTable[PSG->Regs[AY_CVOL] ? PSG->Regs[AY_CVOL]*2+1 : 0];
		break;
	case AY_EFINE:
	case AY_ECOARSE:
//		_ASSERT((PSG->Regs[AY_EFINE] == 0) && (PSG->Regs[AY_ECOARSE] == 0));
		old = PSG->PeriodE;
		PSG->PeriodE = ((PSG->Regs[AY_EFINE] + 256 * PSG->Regs[AY_ECOARSE])) * PSG->UpdateStep;
		if (PSG->PeriodE == 0) PSG->PeriodE = PSG->UpdateStep / 2;
		PSG->CountE += PSG->PeriodE - old;
		if (PSG->CountE <= 0) PSG->CountE = 1;
		break;
	case AY_ESHAPE:
//		_ASSERT(PSG->Regs[AY_ESHAPE] == 0);
		/* envelope shapes:
		C AtAlH
		0 0 x x  \___

		0 1 x x  /___

		1 0 0 0  \\\\

		1 0 0 1  \___

		1 0 1 0  \/\/
		          ___
		1 0 1 1  \

		1 1 0 0  ////
		          ___
		1 1 0 1  /

		1 1 1 0  /\/\

		1 1 1 1  /___

		The envelope counter on the AY-3-8910 has 16 steps. On the YM2149 it
		has twice the steps, happening twice as fast. Since the end result is
		just a smoother curve, we always use the YM2149 behaviour.
		*/
		PSG->Regs[AY_ESHAPE] &= 0x0f;
		PSG->Attack = (PSG->Regs[AY_ESHAPE] & 0x04) ? 0x1f : 0x00;
		if ((PSG->Regs[AY_ESHAPE] & 0x08) == 0)
		{
			/* if Continue = 0, map the shape to the equivalent one which has Continue = 1 */
			PSG->Hold = 1;
			PSG->Alternate = PSG->Attack;
		}
		else
		{
			PSG->Hold = PSG->Regs[AY_ESHAPE] & 0x01;
			PSG->Alternate = PSG->Regs[AY_ESHAPE] & 0x02;
		}
		PSG->CountE = PSG->PeriodE;
		PSG->CountEnv = 0x1f;
		PSG->Holding = 0;
		PSG->VolE = PSG->VolTable[PSG->CountEnv ^ PSG->Attack];
		if (PSG->EnvelopeA) PSG->VolA = PSG->VolE;
		if (PSG->EnvelopeB) PSG->VolB = PSG->VolE;
		if (PSG->EnvelopeC) PSG->VolC = PSG->VolE;
		break;
	case AY_PORTA:
		if (PSG->Regs[AY_ENABLE] & 0x40)
		{
			if (PSG->PortAwrite)
				(*PSG->PortAwrite)(0, PSG->Regs[AY_PORTA]);
			else
				logerror("PC %04x: warning - write %02x to 8910 #%d Port A\n",activecpu_get_pc(),PSG->Regs[AY_PORTA],n);
		}
		else
		{
			logerror("warning: write to 8910 #%d Port A set as input - ignored\n",n);
		}
		break;
	case AY_PORTB:
		if (PSG->Regs[AY_ENABLE] & 0x80)
		{
			if (PSG->PortBwrite)
				(*PSG->PortBwrite)(0, PSG->Regs[AY_PORTB]);
			else
				logerror("PC %04x: warning - write %02x to 8910 #%d Port B\n",activecpu_get_pc(),PSG->Regs[AY_PORTB],n);
		}
		else
		{
			logerror("warning: write to 8910 #%d Port B set as input - ignored\n",n);
		}
		break;
	}
}


// /length/ is the number of samples we require
// NB. This should be called at twice the 6522 IRQ rate or (eg) 60Hz if no IRQ.
void AY8910Update(int chip,INT16 **buffer,int length)	// [TC: Removed static]
{
	struct AY8910 *PSG = &AYPSG[chip];
	INT16 *buf1,*buf2,*buf3;
	int outn;

	buf1 = buffer[0];
	buf2 = buffer[1];
	buf3 = buffer[2];


	/* The 8910 has three outputs, each output is the mix of one of the three */
	/* tone generators and of the (single) noise generator. The two are mixed */
	/* BEFORE going into the DAC. The formula to mix each channel is: */
	/* (ToneOn | ToneDisable) & (NoiseOn | NoiseDisable). */
	/* Note that this means that if both tone and noise are disabled, the output */
	/* is 1, not 0, and can be modulated changing the volume. */


	/* If the channels are disabled, set their output to 1, and increase the */
	/* counter, if necessary, so they will not be inverted during this update. */
	/* Setting the output to 1 is necessary because a disabled channel is locked */
	/* into the ON state (see above); and it has no effect if the volume is 0. */
	/* If the volume is 0, increase the counter, but don't touch the output. */
	if (PSG->Regs[AY_ENABLE] & 0x01)
	{
		if (PSG->CountA <= length*STEP) PSG->CountA += length*STEP;
		PSG->OutputA = 1;
	}
	else if (PSG->Regs[AY_AVOL] == 0)
	{
		/* note that I do count += length, NOT count = length + 1. You might think */
		/* it's the same since the volume is 0, but doing the latter could cause */
		/* interferencies when the program is rapidly modulating the volume. */
		if (PSG->CountA <= length*STEP) PSG->CountA += length*STEP;
	}
	if (PSG->Regs[AY_ENABLE] & 0x02)
	{
		if (PSG->CountB <= length*STEP) PSG->CountB += length*STEP;
		PSG->OutputB = 1;
	}
	else if (PSG->Regs[AY_BVOL] == 0)
	{
		if (PSG->CountB <= length*STEP) PSG->CountB += length*STEP;
	}
	if (PSG->Regs[AY_ENABLE] & 0x04)
	{
		if (PSG->CountC <= length*STEP) PSG->CountC += length*STEP;
		PSG->OutputC = 1;
	}
	else if (PSG->Regs[AY_CVOL] == 0)
	{
		if (PSG->CountC <= length*STEP) PSG->CountC += length*STEP;
	}

	/* for the noise channel we must not touch OutputN - it's also not necessary */
	/* since we use outn. */
	if ((PSG->Regs[AY_ENABLE] & 0x38) == 0x38)	/* all off */
		if (PSG->CountN <= length*STEP) PSG->CountN += length*STEP;

	outn = (PSG->OutputN | PSG->Regs[AY_ENABLE]);


	/* buffering loop */
	while (length)
	{
		int vola,volb,volc;
		int left;


		/* vola, volb and volc keep track of how long each square wave stays */
		/* in the 1 position during the sample period. */
		vola = volb = volc = 0;

		left = STEP;
		do
		{
			int nextevent;


			if (PSG->CountN < left) nextevent = PSG->CountN;
			else nextevent = left;

			if (outn & 0x08)
			{
				if (PSG->OutputA) vola += PSG->CountA;
				PSG->CountA -= nextevent;
				/* PeriodA is the half period of the square wave. Here, in each */
				/* loop I add PeriodA twice, so that at the end of the loop the */
				/* square wave is in the same status (0 or 1) it was at the start. */
				/* vola is also incremented by PeriodA, since the wave has been 1 */
				/* exactly half of the time, regardless of the initial position. */
				/* If we exit the loop in the middle, OutputA has to be inverted */
				/* and vola incremented only if the exit status of the square */
				/* wave is 1. */
				while (PSG->CountA <= 0)
				{
					PSG->CountA += PSG->PeriodA;
					if (PSG->CountA > 0)
					{
						PSG->OutputA ^= 1;
						if (PSG->OutputA) vola += PSG->PeriodA;
						break;
					}
					PSG->CountA += PSG->PeriodA;
					vola += PSG->PeriodA;
				}
				if (PSG->OutputA) vola -= PSG->CountA;
			}
			else
			{
				PSG->CountA -= nextevent;
				while (PSG->CountA <= 0)
				{
					PSG->CountA += PSG->PeriodA;
					if (PSG->CountA > 0)
					{
						PSG->OutputA ^= 1;
						break;
					}
					PSG->CountA += PSG->PeriodA;
				}
			}

			if (outn & 0x10)
			{
				if (PSG->OutputB) volb += PSG->CountB;
				PSG->CountB -= nextevent;
				while (PSG->CountB <= 0)
				{
					PSG->CountB += PSG->PeriodB;
					if (PSG->CountB > 0)
					{
						PSG->OutputB ^= 1;
						if (PSG->OutputB) volb += PSG->PeriodB;
						break;
					}
					PSG->CountB += PSG->PeriodB;
					volb += PSG->PeriodB;
				}
				if (PSG->OutputB) volb -= PSG->CountB;
			}
			else
			{
				PSG->CountB -= nextevent;
				while (PSG->CountB <= 0)
				{
					PSG->CountB += PSG->PeriodB;
					if (PSG->CountB > 0)
					{
						PSG->OutputB ^= 1;
						break;
					}
					PSG->CountB += PSG->PeriodB;
				}
			}

			if (outn & 0x20)
			{
				if (PSG->OutputC) volc += PSG->CountC;
				PSG->CountC -= nextevent;
				while (PSG->CountC <= 0)
				{
					PSG->CountC += PSG->PeriodC;
					if (PSG->CountC > 0)
					{
						PSG->OutputC ^= 1;
						if (PSG->OutputC) volc += PSG->PeriodC;
						break;
					}
					PSG->CountC += PSG->PeriodC;
					volc += PSG->PeriodC;
				}
				if (PSG->OutputC) volc -= PSG->CountC;
			}
			else
			{
				PSG->CountC -= nextevent;
				while (PSG->CountC <= 0)
				{
					PSG->CountC += PSG->PeriodC;
					if (PSG->CountC > 0)
					{
						PSG->OutputC ^= 1;
						break;
					}
					PSG->CountC += PSG->PeriodC;
				}
			}

			PSG->CountN -= nextevent;
			if (PSG->CountN <= 0)
			{
				/* Is noise output going to change? */
				if ((PSG->RNG + 1) & 2)	/* (bit0^bit1)? */
				{
					PSG->OutputN = ~PSG->OutputN;
					outn = (PSG->OutputN | PSG->Regs[AY_ENABLE]);
				}

				/* The Random Number Generator of the 8910 is a 17-bit shift */
				/* register. The input to the shift register is bit0 XOR bit3 */
				/* (bit0 is the output). This was verified on AY-3-8910 and YM2149 chips. */

				/* The following is a fast way to compute bit17 = bit0^bit3. */
				/* Instead of doing all the logic operations, we only check */
				/* bit0, relying on the fact that after three shifts of the */
				/* register, what now is bit3 will become bit0, and will */
				/* invert, if necessary, bit14, which previously was bit17. */
				if (PSG->RNG & 1) PSG->RNG ^= 0x24000; /* This version is called the "Galois configuration". */
				PSG->RNG >>= 1;
				PSG->CountN += PSG->PeriodN;
			}

			left -= nextevent;
		} while (left > 0);

		/* update envelope */
		if (PSG->Holding == 0)
		{
			PSG->CountE -= STEP;
			if (PSG->CountE <= 0)
			{
				do
				{
					PSG->CountEnv--;
					PSG->CountE += PSG->PeriodE;
				} while (PSG->CountE <= 0);

				/* check envelope current position */
				if (PSG->CountEnv < 0)
				{
					if (PSG->Hold)
					{
						if (PSG->Alternate)
							PSG->Attack ^= 0x1f;
						PSG->Holding = 1;
						PSG->CountEnv = 0;
					}
					else
					{
						/* if CountEnv has looped an odd number of times (usually 1), */
						/* invert the output. */
						if (PSG->Alternate && (PSG->CountEnv & 0x20))
 							PSG->Attack ^= 0x1f;

						PSG->CountEnv &= 0x1f;
					}
				}

				PSG->VolE = PSG->VolTable[PSG->CountEnv ^ PSG->Attack];
				/* reload volume */
				if (PSG->EnvelopeA) PSG->VolA = PSG->VolE;
				if (PSG->EnvelopeB) PSG->VolB = PSG->VolE;
				if (PSG->EnvelopeC) PSG->VolC = PSG->VolE;
			}
		}

#if 0
		*(buf1++) = (vola * PSG->VolA) / STEP;
		*(buf2++) = (volb * PSG->VolB) / STEP;
		*(buf3++) = (volc * PSG->VolC) / STEP;
#else
		// Output PCM wave [-32768...32767] instead of MAME's voltage level [0...32767]
		// - This allows for better s/w mixing

		if(PSG->VolA)
		{
			if(vola)
				*(buf1++) = (vola * PSG->VolA) / STEP;
			else
				*(buf1++) = - (int) PSG->VolA;
		}
		else
		{
			*(buf1++) = 0;
		}

		//

		if(PSG->VolB)
		{
			if(volb)
				*(buf2++) = (volb * PSG->VolB) / STEP;
			else
				*(buf2++) = - (int) PSG->VolB;
		}
		else
		{
			*(buf2++) = 0;
		}

		//

		if(PSG->VolC)
		{
			if(volc)
				*(buf3++) = (volc * PSG->VolC) / STEP;
			else
				*(buf3++) = - (int) PSG->VolC;
		}
		else
		{
			*(buf3++) = 0;
		}
#endif

		length--;
	}
}


static void AY8910_set_clock(int chip,int clock)
{
	struct AY8910 *PSG = &AYPSG[chip];

	/* the step clock for the tone and noise generators is the chip clock    */
	/* divided by 8; for the envelope generator of the AY-3-8910, it is half */
	/* that much (clock/16), but the envelope of the YM2149 goes twice as    */
	/* fast, therefore again clock/8.                                        */
	/* Here we calculate the number of steps which happen during one sample  */
	/* at the given sample rate. No. of events = sample rate / (clock/8).    */
	/* STEP is a multiplier used to turn the fraction into a fixed point     */
	/* number.                                                               */
	PSG->UpdateStep = (unsigned int) (((double)STEP * PSG->SampleRate * 8 + clock/2) / clock);	// [TC: unsigned int cast]
}


static void build_mixer_table(int chip)
{
	struct AY8910 *PSG = &AYPSG[chip];
	int i;
	double out;


	/* calculate the volume->voltage conversion table */
	/* The AY-3-8910 has 16 levels, in a logarithmic scale (3dB per step) */
	/* The YM2149 still has 16 levels for the tone generators, but 32 for */
	/* the envelope generator (1.5dB per step). */
	out = MAX_OUTPUT;
	for (i = 31;i > 0;i--)
	{
		PSG->VolTable[i] = (unsigned int) (out + 0.5);	/* round to nearest */	// [TC: unsigned int cast]

		out /= 1.188502227;	/* = 10 ^ (1.5/20) = 1.5dB */
	}
	PSG->VolTable[0] = 0;
}


#if 0
void ay8910_write_ym(int chip, int addr, int data)
{
	struct AY8910 *PSG = &AYPSG[chip];

//	if (addr & 1)
//	{	/* Data port */
//		int r = PSG->register_latch;
		int r = addr;

		if (r > 15) return;
		if (r < 14)
		{
			if (r == AY_ESHAPE || PSG->Regs[r] != data)
			{
				/* update the output buffer before changing the register */
//				stream_update(PSG->Channel,0);
				AY8910Update(chip, INT16 **buffer, int length)
			}
		}

		_AYWriteReg(PSG,r,data);
	}
//	else
//	{	/* Register port */
//		PSG->register_latch = data & 0x0f;
//	}
}
#endif


void AY8910_reset(int chip)
{
	g_bAYReset = true;

	int i;
	struct AY8910 *PSG = &AYPSG[chip];

	PSG->register_latch = 0;
	PSG->RNG = 1;
	PSG->OutputA = 0;
	PSG->OutputB = 0;
	PSG->OutputC = 0;
	PSG->OutputN = 0xff;
	PSG->lastEnable = -1;	/* force a write */
	for (i = 0;i < AY_PORTA;i++)
		_AYWriteReg(chip,i,0);	/* AYWriteReg() uses the timer system; we cannot */
								/* call it at this time because the timer system */
								/* has not been initialized. */

	g_bAYReset = false;
}

//-------------------------------------

void AY8910_InitAll(int nClock, int nSampleRate)
{
	for(int nChip=0; nChip<MAX_8910; nChip++)
	{
		struct AY8910 *PSG = &AYPSG[nChip];

		memset(PSG,0,sizeof(struct AY8910));
		PSG->SampleRate = nSampleRate;

		PSG->PortAread = NULL;
		PSG->PortBread = NULL;
		PSG->PortAwrite = NULL;
		PSG->PortBwrite = NULL;

		AY8910_set_clock(nChip, nClock);

		build_mixer_table(nChip);
	}
}

//-------------------------------------

void AY8910_InitClock(int nClock)
{
	for(int nChip=0; nChip<MAX_8910; nChip++)
	{
		AY8910_set_clock(nChip, nClock);
	}
}

//-------------------------------------

BYTE* AY8910_GetRegsPtr(UINT uChip)
{
	if(uChip >= MAX_8910)
		return NULL;

	return &AYPSG[uChip].Regs[0];
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#else

libspectrum_signed_word** g_ppSoundBuffers;	// Used to pass param to sound_ay_overlay()

/* configuration */
//int sound_enabled = 0;		/* Are we currently using the sound card */
//int sound_enabled_ever = 0;	/* if it's *ever* been in use; see
//				   sound_ay_write() and sound_ay_reset() */
//int sound_stereo = 0;		/* true for stereo *output sample* (only) */
//int sound_stereo_ay_abc = 0;	/* (AY stereo) true for ABC stereo, else ACB */
//int sound_stereo_ay_narrow = 0;	/* (AY stereo) true for narrow AY st. sep. */

//int sound_stereo_ay = 0;	/* local copy of settings_current.stereo_ay */
//int sound_stereo_beeper = 0;	/* and settings_current.stereo_beeper */


/* assume all three tone channels together match the beeper volume (ish).
 * Must be <=127 for all channels; 50+2+(24*3) = 124.
 * (Now scaled up for 16-bit.)
 */
//#define AMPL_BEEPER		( 50 * 256)
//#define AMPL_TAPE		( 2 * 256 )
//#define AMPL_AY_TONE		( 24 * 256 )	/* three of these */
#define AMPL_AY_TONE		( 42 * 256 )	// 42*3 = 126

/* max. number of sub-frame AY port writes allowed;
 * given the number of port writes theoretically possible in a
 * 50th I think this should be plenty.
 */
//#define AY_CHANGE_MAX		8000	// [TC] Moved into AY8910.h

///* frequency to generate sound at for hifi sound */
//#define HIFI_FREQ              88200

#ifdef HAVE_SAMPLERATE
static SRC_STATE *src_state;
#endif /* #ifdef HAVE_SAMPLERATE */

int sound_generator_framesiz;
int sound_framesiz;

static int sound_generator_freq;

static int sound_channels;

static unsigned int ay_tone_levels[16];

//static libspectrum_signed_word *sound_buf, *tape_buf;
//static float *convert_input_buffer, *convert_output_buffer;

#if 0
/* beeper stuff */
static int sound_oldpos[2], sound_fillpos[2];
static int sound_oldval[2], sound_oldval_orig[2];
#endif

#if 0
#define STEREO_BUF_SIZE 4096

static int pstereobuf[ STEREO_BUF_SIZE ];
static int pstereobufsiz, pstereopos;
static int psgap = 250;
static int rstereobuf_l[ STEREO_BUF_SIZE ], rstereobuf_r[ STEREO_BUF_SIZE ];
static int rstereopos, rchan1pos, rchan2pos, rchan3pos;
#endif


// Statics:
double CAY8910::m_fCurrentCLK_AY8910 = 0.0;


CAY8910::CAY8910() :
	// Init the statics that were in sound_ay_overlay()
	rng(1),
	noise_toggle(0),
	env_first(1), env_rev(0), env_counter(15)
{
	m_fCurrentCLK_AY8910 = g_fCurrentCLK6502;
};


void CAY8910::sound_ay_init( void )
{
	/* AY output doesn't match the claimed levels; these levels are based
	* on the measurements posted to comp.sys.sinclair in Dec 2001 by
	* Matthew Westcott, adjusted as I described in a followup to his post,
	* then scaled to 0..0xffff.
	*/
	static const int levels[16] = {
		0x0000, 0x0385, 0x053D, 0x0770,
		0x0AD7, 0x0FD5, 0x15B0, 0x230C,
		0x2B4C, 0x43C1, 0x5A4B, 0x732F,
		0x9204, 0xAFF1, 0xD921, 0xFFFF
	};
	int f;

	/* scale the values down to fit */
	for( f = 0; f < 16; f++ )
		ay_tone_levels[f] = ( levels[f] * AMPL_AY_TONE + 0x8000 ) / 0xffff;

	ay_noise_tick = ay_noise_period = 0;
	ay_env_internal_tick = ay_env_tick = ay_env_period = 0;
	ay_tone_subcycles = ay_env_subcycles = 0;
	for( f = 0; f < 3; f++ )
		ay_tone_tick[f] = ay_tone_high[f] = 0, ay_tone_period[f] = 1;

	ay_change_count = 0;
}


void CAY8910::sound_init( const char *device )
{
//  static int first_init = 1;
//  int f, ret;
  float hz;
#ifdef HAVE_SAMPLERATE
  int error;
#endif /* #ifdef HAVE_SAMPLERATE */

/* if we don't have any sound I/O code compiled in, don't do sound */
#ifdef NO_SOUND
  return;
#endif

#if 0
  if( !( !sound_enabled && settings_current.sound &&
	 settings_current.emulation_speed == 100 ) )
    return;

  sound_stereo_ay = settings_current.stereo_ay;
  sound_stereo_beeper = settings_current.stereo_beeper;

/* only try for stereo if we need it */
  if( sound_stereo_ay || sound_stereo_beeper )
    sound_stereo = 1;
  ret =
    sound_lowlevel_init( device, &settings_current.sound_freq,
			 &sound_stereo );
  if( ret )
    return;
#endif

#if 0
/* important to override these settings if not using stereo
 * (it would probably be confusing to mess with the stereo
 * settings in settings_current though, which is why we make copies
 * rather than using the real ones).
 */
  if( !sound_stereo ) {
    sound_stereo_ay = 0;
    sound_stereo_beeper = 0;
  }

  sound_enabled = sound_enabled_ever = 1;

  sound_channels = ( sound_stereo ? 2 : 1 );
#endif
  sound_channels = 3;	// 3 mono channels: ABC

//  hz = ( float ) machine_current->timings.processor_speed /
//    machine_current->timings.tstates_per_frame;
  hz = 50;

//  sound_generator_freq =
//    settings_current.sound_hifi ? HIFI_FREQ : settings_current.sound_freq;
  sound_generator_freq = SPKR_SAMPLE_RATE;
  sound_generator_framesiz = sound_generator_freq / (int)hz;

#if 0
  if( ( sound_buf = (libspectrum_signed_word*) malloc( sizeof( libspectrum_signed_word ) *
			    sound_generator_framesiz * sound_channels ) ) ==
      NULL
      || ( tape_buf =
	   malloc( sizeof( libspectrum_signed_word ) *
		   sound_generator_framesiz ) ) == NULL ) {
    if( sound_buf ) {
      free( sound_buf );
      sound_buf = NULL;
    }
    sound_end();
    return;
  }
#endif

//  sound_framesiz = ( float ) settings_current.sound_freq / hz;
  sound_framesiz = sound_generator_freq / (int)hz;

#ifdef HAVE_SAMPLERATE
  if( settings_current.sound_hifi ) {
    if( ( convert_input_buffer = malloc( sizeof( float ) *
					 sound_generator_framesiz *
					 sound_channels ) ) == NULL
	|| ( convert_output_buffer =
	     malloc( sizeof( float ) * sound_framesiz * sound_channels ) ) ==
	NULL ) {
      if( convert_input_buffer ) {
	free( convert_input_buffer );
	convert_input_buffer = NULL;
      }
      sound_end();
      return;
    }
  }

  src_state = src_new( SRC_SINC_MEDIUM_QUALITY, sound_channels, &error );
  if( error ) {
    ui_error( UI_ERROR_ERROR,
	      "error initialising sample rate converter %s",
	      src_strerror( error ) );
    sound_end();
    return;
  }
#endif /* #ifdef HAVE_SAMPLERATE */

/* if we're resuming, we need to be careful about what
 * gets reset. The minimum we can do is the beeper
 * buffer positions, so that's here.
 */
#if 0
  sound_oldpos[0] = sound_oldpos[1] = -1;
  sound_fillpos[0] = sound_fillpos[1] = 0;
#endif

/* this stuff should only happen on the initial call.
 * (We currently assume the new sample rate will be the
 * same as the previous one, hence no need to recalculate
 * things dependent on that.)
 */
#if 0
  if( first_init ) {
    first_init = 0;

    for( f = 0; f < 2; f++ )
      sound_oldval[f] = sound_oldval_orig[f] = 0;
  }
#endif

#if 0
  if( sound_stereo_beeper ) {
    for( f = 0; f < STEREO_BUF_SIZE; f++ )
      pstereobuf[f] = 0;
    pstereopos = 0;
    pstereobufsiz = ( sound_generator_freq * psgap ) / 22000;
  }

  if( sound_stereo_ay ) {
    int pos =
      ( sound_stereo_ay_narrow ? 3 : 6 ) * sound_generator_freq / 8000;

    for( f = 0; f < STEREO_BUF_SIZE; f++ )
      rstereobuf_l[f] = rstereobuf_r[f] = 0;
    rstereopos = 0;

    /* the actual ACB/ABC bit :-) */
    rchan1pos = -pos;
    if( sound_stereo_ay_abc )
      rchan2pos = 0, rchan3pos = pos;
    else
      rchan2pos = pos, rchan3pos = 0;
  }
#endif

#if 0
  ay_tick_incr = ( int ) ( 65536. *
			   libspectrum_timings_ay_speed( machine_current->
							 machine ) /
			   sound_generator_freq );
#endif
  ay_tick_incr = ( int ) ( 65536. * m_fCurrentCLK_AY8910 / sound_generator_freq );	// [TC]
}


#if 0
void
sound_pause( void )
{
  if( sound_enabled )
    sound_end();
}


void
sound_unpause( void )
{
/* No sound if fastloading in progress */
  if( settings_current.fastload && tape_is_playing() )
    return;

  sound_init( settings_current.sound_device );
}
#endif


void CAY8910::sound_end( void )
{
#if 0
  if( sound_enabled ) {
    if( sound_buf ) {
      free( sound_buf );
      sound_buf = NULL;
      free( tape_buf );
      tape_buf = NULL;
    }
    if( convert_input_buffer ) {
      free( convert_input_buffer );
      convert_input_buffer = NULL;
    }
    if( convert_output_buffer ) {
      free( convert_output_buffer );
      convert_output_buffer = NULL;
    }
#ifdef HAVE_SAMPLERATE
    if( src_state )
      src_state = src_delete( src_state );
#endif /* #ifdef HAVE_SAMPLERATE */
    sound_lowlevel_end();
    sound_enabled = 0;
  }
#endif

#if 0
    if( sound_buf ) {
      free( sound_buf );
      sound_buf = NULL;
    }
#endif
}


#if 0
/* write sample to buffer as pseudo-stereo */
static void
sound_write_buf_pstereo( libspectrum_signed_word * out, int c )
{
  int bl = ( c - pstereobuf[ pstereopos ] ) / 2;
  int br = ( c + pstereobuf[ pstereopos ] ) / 2;

  if( bl < -AMPL_BEEPER )
    bl = -AMPL_BEEPER;
  if( br < -AMPL_BEEPER )
    br = -AMPL_BEEPER;
  if( bl > AMPL_BEEPER )
    bl = AMPL_BEEPER;
  if( br > AMPL_BEEPER )
    br = AMPL_BEEPER;

  *out = bl;
  out[1] = br;

  pstereobuf[ pstereopos ] = c;
  pstereopos++;
  if( pstereopos >= pstereobufsiz )
    pstereopos = 0;
}
#endif



/* not great having this as a macro to inline it, but it's only
 * a fairly short routine, and it saves messing about.
 * (XXX ummm, possibly not so true any more :-))
 */
#define AY_GET_SUBVAL( chan ) \
  ( level * 2 * ay_tone_tick[ chan ] / tone_count )

#define AY_DO_TONE( var, chan ) \
  ( var ) = 0;								\
  is_low = 0;								\
  if( level ) {								\
    if( ay_tone_high[ chan ] )						\
      ( var ) = ( level );						\
    else {								\
      ( var ) = -( level );						\
      is_low = 1;							\
    }									\
  }									\
  									\
  ay_tone_tick[ chan ] += tone_count;					\
  count = 0;								\
  while( ay_tone_tick[ chan ] >= ay_tone_period[ chan ] ) {		\
    count++;								\
    ay_tone_tick[ chan ] -= ay_tone_period[ chan ];			\
    ay_tone_high[ chan ] = !ay_tone_high[ chan ];			\
    									\
    /* has to be here, unfortunately... */				\
    if( count == 1 && level && ay_tone_tick[ chan ] < tone_count ) {	\
      if( is_low )							\
        ( var ) += AY_GET_SUBVAL( chan );				\
      else								\
        ( var ) -= AY_GET_SUBVAL( chan );				\
      }									\
    }									\
  									\
  /* if it's changed more than once during the sample, we can't */	\
  /* represent it faithfully. So, just hope it's a sample.      */	\
  /* (That said, this should also help avoid aliasing noise.)   */	\
  if( count > 1 )							\
    ( var ) = -( level )


#if 0
/* add val, correctly delayed on either left or right buffer,
 * to add the AY stereo positioning. This doesn't actually put
 * anything directly in sound_buf, though.
 */
#define GEN_STEREO( pos, val ) \
  if( ( pos ) < 0 ) {							\
    rstereobuf_l[ rstereopos ] += ( val );				\
    rstereobuf_r[ ( rstereopos - pos ) % STEREO_BUF_SIZE ] += ( val );	\
  } else {								\
    rstereobuf_l[ ( rstereopos + pos ) % STEREO_BUF_SIZE ] += ( val );	\
    rstereobuf_r[ rstereopos ] += ( val );				\
  }
#endif


/* bitmasks for envelope */
#define AY_ENV_CONT	8
#define AY_ENV_ATTACK	4
#define AY_ENV_ALT	2
#define AY_ENV_HOLD	1

#define HZ_COMMON_DENOMINATOR 50

void CAY8910::sound_ay_overlay( void )
{
  int tone_level[3];
  int mixer, envshape;
  int f, g, level, count;
//  libspectrum_signed_word *ptr;
  struct ay_change_tag *change_ptr = ay_change;
  int changes_left = ay_change_count;
  int reg, r;
  int is_low;
  int chan1, chan2, chan3;
  unsigned int tone_count, noise_count;
  libspectrum_dword sfreq, cpufreq;

///* If no AY chip, don't produce any AY sound (!) */
//  if( !machine_current->capabilities & LIBSPECTRUM_MACHINE_CAPABILITY_AY )
//    return;

/* convert change times to sample offsets, use common denominator of 50 to
   avoid overflowing a dword */
  sfreq = sound_generator_freq / HZ_COMMON_DENOMINATOR;
//  cpufreq = machine_current->timings.processor_speed / HZ_COMMON_DENOMINATOR;
  cpufreq = (libspectrum_dword) (m_fCurrentCLK_AY8910 / HZ_COMMON_DENOMINATOR);	// [TC]
  for( f = 0; f < ay_change_count; f++ )
    ay_change[f].ofs = (USHORT) (( ay_change[f].tstates * sfreq ) / cpufreq);	// [TC] Added cast

  libspectrum_signed_word* pBuf1 = g_ppSoundBuffers[0];
  libspectrum_signed_word* pBuf2 = g_ppSoundBuffers[1];
  libspectrum_signed_word* pBuf3 = g_ppSoundBuffers[2];

//  for( f = 0, ptr = sound_buf; f < sound_generator_framesiz; f++ ) {
  for( f = 0; f < sound_generator_framesiz; f++ ) {
    /* update ay registers. All this sub-frame change stuff
     * is pretty hairy, but how else would you handle the
     * samples in Robocop? :-) It also clears up some other
     * glitches.
     */
    while( changes_left && f >= change_ptr->ofs ) {
      sound_ay_registers[ reg = change_ptr->reg ] = change_ptr->val;
      change_ptr++;
      changes_left--;

      /* fix things as needed for some register changes */
      switch ( reg ) {
      case 0:
      case 1:
      case 2:
      case 3:
      case 4:
      case 5:
	r = reg >> 1;
	/* a zero-len period is the same as 1 */
	ay_tone_period[r] = ( sound_ay_registers[ reg & ~1 ] |
			      ( sound_ay_registers[ reg | 1 ] & 15 ) << 8 );
	if( !ay_tone_period[r] )
	  ay_tone_period[r]++;

	/* important to get this right, otherwise e.g. Ghouls 'n' Ghosts
	 * has really scratchy, horrible-sounding vibrato.
	 */
	if( ay_tone_tick[r] >= ay_tone_period[r] * 2 )
	  ay_tone_tick[r] %= ay_tone_period[r] * 2;
	break;
      case 6:
	ay_noise_tick = 0;
	ay_noise_period = ( sound_ay_registers[ reg ] & 31 );
	break;
      case 11:
      case 12:
	/* this one *isn't* fixed-point */
	ay_env_period =
	  sound_ay_registers[11] | ( sound_ay_registers[12] << 8 );
	break;
      case 13:
	ay_env_internal_tick = ay_env_tick = ay_env_subcycles = 0;
	env_first = 1;
	env_rev = 0;
	env_counter = ( sound_ay_registers[13] & AY_ENV_ATTACK ) ? 0 : 15;
	break;
      }
    }

    /* the tone level if no enveloping is being used */
    for( g = 0; g < 3; g++ )
      tone_level[g] = ay_tone_levels[ sound_ay_registers[ 8 + g ] & 15 ];

    /* envelope */
    envshape = sound_ay_registers[13];
    level = ay_tone_levels[ env_counter ];

    for( g = 0; g < 3; g++ )
      if( sound_ay_registers[ 8 + g ] & 16 )
	tone_level[g] = level;

    /* envelope output counter gets incr'd every 16 AY cycles.
     * Has to be a while, as this is sub-output-sample res.
     */
    ay_env_subcycles += ay_tick_incr;
    noise_count = 0;
    while( ay_env_subcycles >= ( 16 << 16 ) ) {
      ay_env_subcycles -= ( 16 << 16 );
      noise_count++;
      ay_env_tick++;
      while( ay_env_tick >= ay_env_period ) {
	ay_env_tick -= ay_env_period;

	/* do a 1/16th-of-period incr/decr if needed */
	if( env_first ||
	    ( ( envshape & AY_ENV_CONT ) && !( envshape & AY_ENV_HOLD ) ) ) {
	  if( env_rev )
	    env_counter -= ( envshape & AY_ENV_ATTACK ) ? 1 : -1;
	  else
	    env_counter += ( envshape & AY_ENV_ATTACK ) ? 1 : -1;
	  if( env_counter < 0 )
	    env_counter = 0;
	  if( env_counter > 15 )
	    env_counter = 15;
	}

	ay_env_internal_tick++;
	while( ay_env_internal_tick >= 16 ) {
	  ay_env_internal_tick -= 16;

	  /* end of cycle */
	  if( !( envshape & AY_ENV_CONT ) )
	    env_counter = 0;
	  else {
	    if( envshape & AY_ENV_HOLD ) {
	      if( env_first && ( envshape & AY_ENV_ALT ) )
		env_counter = ( env_counter ? 0 : 15 );
	    } else {
	      /* non-hold */
	      if( envshape & AY_ENV_ALT )
		env_rev = !env_rev;
	      else
		env_counter = ( envshape & AY_ENV_ATTACK ) ? 0 : 15;
	    }
	  }

	  env_first = 0;
	}

	/* don't keep trying if period is zero */
	if( !ay_env_period )
	  break;
      }
    }

    /* generate tone+noise... or neither.
     * (if no tone/noise is selected, the chip just shoves the
     * level out unmodified. This is used by some sample-playing
     * stuff.)
     */
    chan1 = tone_level[0];
    chan2 = tone_level[1];
    chan3 = tone_level[2];
    mixer = sound_ay_registers[7];

    ay_tone_subcycles += ay_tick_incr;
    tone_count = ay_tone_subcycles >> ( 3 + 16 );
    ay_tone_subcycles &= ( 8 << 16 ) - 1;

    if( ( mixer & 1 ) == 0 ) {
      level = chan1;
      AY_DO_TONE( chan1, 0 );
    }
    if( ( mixer & 0x08 ) == 0 && noise_toggle )
      chan1 = 0;

    if( ( mixer & 2 ) == 0 ) {
      level = chan2;
      AY_DO_TONE( chan2, 1 );
    }
    if( ( mixer & 0x10 ) == 0 && noise_toggle )
      chan2 = 0;

    if( ( mixer & 4 ) == 0 ) {
      level = chan3;
      AY_DO_TONE( chan3, 2 );
    }
    if( ( mixer & 0x20 ) == 0 && noise_toggle )
      chan3 = 0;

    /* write the sample(s) */
	*pBuf1++ = chan1;	// [TC]
	*pBuf2++ = chan2;	// [TC]
	*pBuf3++ = chan3;	// [TC]
#if 0
    if( !sound_stereo ) {
      /* mono */
      ( *ptr++ ) += chan1 + chan2 + chan3;
    } else {
      if( !sound_stereo_ay ) {
	/* stereo output, but mono AY sound; still,
	 * incr separately in case of beeper pseudostereo.
	 */
	( *ptr++ ) += chan1 + chan2 + chan3;
	( *ptr++ ) += chan1 + chan2 + chan3;
      } else {
	/* stereo with ACB/ABC AY positioning.
	 * Here we use real stereo positions for the channels.
	 * Just because, y'know, it's cool and stuff. No, really. :-)
	 * This is a little tricky, as it works by delaying sounds
	 * on the left or right channels to model the delay you get
	 * in the real world when sounds originate at different places.
	 */
	GEN_STEREO( rchan1pos, chan1 );
	GEN_STEREO( rchan2pos, chan2 );
	GEN_STEREO( rchan3pos, chan3 );
	( *ptr++ ) += rstereobuf_l[ rstereopos ];
	( *ptr++ ) += rstereobuf_r[ rstereopos ];
	rstereobuf_l[ rstereopos ] = rstereobuf_r[ rstereopos ] = 0;
	rstereopos++;
	if( rstereopos >= STEREO_BUF_SIZE )
	  rstereopos = 0;
      }
    }
#endif

    /* update noise RNG/filter */
    ay_noise_tick += noise_count;
    while( ay_noise_tick >= ay_noise_period ) {
      ay_noise_tick -= ay_noise_period;

      if( ( rng & 1 ) ^ ( ( rng & 2 ) ? 1 : 0 ) )
	noise_toggle = !noise_toggle;

      /* rng is 17-bit shift reg, bit 0 is output.
       * input is bit 0 xor bit 2.
       */
      rng |= ( ( rng & 1 ) ^ ( ( rng & 4 ) ? 1 : 0 ) ) ? 0x20000 : 0;
      rng >>= 1;

      /* don't keep trying if period is zero */
      if( !ay_noise_period )
	break;
    }
  }
}


/* don't make the change immediately; record it for later,
 * to be made by sound_frame() (via sound_ay_overlay()).
 */
void CAY8910::sound_ay_write( int reg, int val, libspectrum_dword now )
{
  if( ay_change_count < AY_CHANGE_MAX ) {
    ay_change[ ay_change_count ].tstates = now;
    ay_change[ ay_change_count ].reg = ( reg & 15 );
    ay_change[ ay_change_count ].val = val;
    ay_change_count++;
  }
}


/* no need to call this initially, but should be called
 * on reset otherwise.
 */
void CAY8910::sound_ay_reset( void )
{
  int f;

/* recalculate timings based on new machines ay clock */
  sound_ay_init();

  ay_change_count = 0;
  for( f = 0; f < 16; f++ )
    sound_ay_write( f, 0, 0 );
  for( f = 0; f < 3; f++ )
    ay_tone_high[f] = 0;
  ay_tone_subcycles = ay_env_subcycles = 0;
}


#if 0
/* write stereo or mono beeper sample, and incr ptr */
#define SOUND_WRITE_BUF_BEEPER( ptr, val ) \
  do {							\
    if( sound_stereo_beeper ) {				\
      sound_write_buf_pstereo( ( ptr ), ( val ) );	\
      ( ptr ) += 2;					\
    } else {						\
      *( ptr )++ = ( val );				\
      if( sound_stereo )				\
        *( ptr )++ = ( val );				\
    }							\
  } while(0)

/* the tape version works by writing to a separate mono buffer,
 * which gets added after being generated.
 */
#define SOUND_WRITE_BUF( is_tape, ptr, val ) \
  if( is_tape )					\
    *( ptr )++ = ( val );			\
  else						\
    SOUND_WRITE_BUF_BEEPER( ptr, val )
#endif

#ifdef HAVE_SAMPLERATE
static void
sound_resample( void )
{
  int error;
  SRC_DATA data;

  data.data_in = convert_input_buffer;
  data.input_frames = sound_generator_framesiz;
  data.data_out = convert_output_buffer;
  data.output_frames = sound_framesiz;
  data.src_ratio =
    ( double ) settings_current.sound_freq / sound_generator_freq;
  data.end_of_input = 0;

  src_short_to_float_array( ( const short * ) sound_buf, convert_input_buffer,
			    sound_generator_framesiz * sound_channels );

  while( data.input_frames ) {
    error = src_process( src_state, &data );
    if( error ) {
      ui_error( UI_ERROR_ERROR, "hifi sound downsample error %s",
		src_strerror( error ) );
      sound_end();
      return;
    }

    src_float_to_short_array( convert_output_buffer, ( short * ) sound_buf,
			      data.output_frames_gen * sound_channels );

    sound_lowlevel_frame( sound_buf,
			  data.output_frames_gen * sound_channels );

    data.data_in += data.input_frames_used * sound_channels;
    data.input_frames -= data.input_frames_used;
  }
}
#endif /* #ifdef HAVE_SAMPLERATE */

void CAY8910::sound_frame( void )
{
#if 0
  libspectrum_signed_word *ptr, *tptr;
  int f, bchan;
  int ampl = AMPL_BEEPER;

  if( !sound_enabled )
    return;

/* fill in remaining beeper/tape sound */
  ptr =
    sound_buf + ( sound_stereo ? sound_fillpos[0] * 2 : sound_fillpos[0] );
  for( bchan = 0; bchan < 2; bchan++ ) {
    for( f = sound_fillpos[ bchan ]; f < sound_generator_framesiz; f++ )
      SOUND_WRITE_BUF( bchan, ptr, sound_oldval[ bchan ] );

    ptr = tape_buf + sound_fillpos[1];
    ampl = AMPL_TAPE;
  }

/* overlay tape sound */
  ptr = sound_buf;
  tptr = tape_buf;
  for( f = 0; f < sound_generator_framesiz; f++, tptr++ ) {
    ( *ptr++ ) += *tptr;
    if( sound_stereo )
      ( *ptr++ ) += *tptr;
  }
#endif

/* overlay AY sound */
  sound_ay_overlay();

#ifdef HAVE_SAMPLERATE
/* resample from generated frequency down to output frequency if required */
  if( settings_current.sound_hifi )
    sound_resample();
  else
#endif /* #ifdef HAVE_SAMPLERATE */
#if 0
    sound_lowlevel_frame( sound_buf,
			  sound_generator_framesiz * sound_channels );
#endif

#if 0
  sound_oldpos[0] = sound_oldpos[1] = -1;
  sound_fillpos[0] = sound_fillpos[1] = 0;
#endif

  ay_change_count = 0;
}

#if 0
/* two beepers are supported - the real beeper (call with is_tape==0)
 * and a `fake' beeper which lets you hear when a tape is being played.
 */
void
sound_beeper( int is_tape, int on )
{
  libspectrum_signed_word *ptr;
  int newpos, subpos;
  int val, subval;
  int f;
  int bchan = ( is_tape ? 1 : 0 );
  int ampl = ( is_tape ? AMPL_TAPE : AMPL_BEEPER );
  int vol = ampl * 2;

  if( !sound_enabled )
    return;

  val = ( on ? -ampl : ampl );

  if( val == sound_oldval_orig[ bchan ] )
    return;

/* XXX a lookup table might help here, but would need to regenerate it
 * whenever cycles_per_frame were changed (i.e. when machine type changed).
 */
  newpos =
    ( tstates * sound_generator_framesiz ) /
    machine_current->timings.tstates_per_frame;
  subpos =
    ( ( ( libspectrum_signed_qword ) tstates ) * sound_generator_framesiz *
      vol ) / ( machine_current->timings.tstates_per_frame ) - vol * newpos;

/* if we already wrote here, adjust the level.
 */
  if( newpos == sound_oldpos[ bchan ] ) {
    /* adjust it as if the rest of the sample period were all in
     * the new state. (Often it will be, but if not, we'll fix
     * it later by doing this again.)
     */
    if( on )
      beeper_last_subpos[ bchan ] += vol - subpos;
    else
      beeper_last_subpos[ bchan ] -= vol - subpos;
  } else
    beeper_last_subpos[ bchan ] = ( on ? vol - subpos : subpos );

  subval = ampl - beeper_last_subpos[ bchan ];

  if( newpos >= 0 ) {
    /* fill gap from previous position */
    if( is_tape )
      ptr = tape_buf + sound_fillpos[1];
    else
      ptr =
	sound_buf +
	( sound_stereo ? sound_fillpos[0] * 2 : sound_fillpos[0] );

    for( f = sound_fillpos[ bchan ];
	 f < newpos && f < sound_generator_framesiz;
	 f++ )
      SOUND_WRITE_BUF( bchan, ptr, sound_oldval[ bchan ] );

    if( newpos < sound_generator_framesiz ) {
      /* newpos may be less than sound_fillpos, so... */
      if( is_tape )
	ptr = tape_buf + newpos;
      else
	ptr = sound_buf + ( sound_stereo ? newpos * 2 : newpos );

      /* write subsample value */
      SOUND_WRITE_BUF( bchan, ptr, subval );
    }
  }

  sound_oldpos[ bchan ] = newpos;
  sound_fillpos[ bchan ] = newpos + 1;
  sound_oldval[ bchan ] = sound_oldval_orig[ bchan ] = val;
}
#endif

///////////////////////////////////////////////////////////////////////////////

// AY8910 interface

// TODO:
// . AY reset, eg. at end of Ultima3 tune

#include "CPU.h"	// For g_nCumulativeCycles

static CAY8910 g_AY8910[MAX_8910];
static unsigned __int64 g_uLastCumulativeCycles = 0;


void _AYWriteReg(int chip, int r, int v)
{
	libspectrum_dword uOffset = (libspectrum_dword) (g_nCumulativeCycles - g_uLastCumulativeCycles);
	g_AY8910[chip].sound_ay_write(r, v, uOffset);
}

void AY8910_reset(int chip)
{
	// Don't reset the AY CLK, as this is a property of the card (MB/Phasor), not the AY chip
	g_AY8910[chip].sound_ay_reset();	// Calls: sound_ay_init();
}

void AY8910Update(int chip, INT16** buffer, int nNumSamples)
{
	g_uLastCumulativeCycles = g_nCumulativeCycles;

	sound_generator_framesiz = nNumSamples;
	g_ppSoundBuffers = buffer;
	g_AY8910[chip].sound_frame();
}

void AY8910_InitAll(int nClock, int nSampleRate)
{
	for (UINT i=0; i<MAX_8910; i++)
	{
		g_AY8910[i].sound_init(NULL);	// Inits mainly static members (except ay_tick_incr)
		g_AY8910[i].sound_ay_init();
	}
}

void AY8910_InitClock(int nClock)
{
	CAY8910::SetCLK( (double)nClock );
	for (UINT i=0; i<MAX_8910; i++)
	{
		g_AY8910[i].sound_init(NULL);	// ay_tick_incr is dependent on AY_CLK
	}
}

BYTE* AY8910_GetRegsPtr(UINT uChip)
{
	if(uChip >= MAX_8910)
		return NULL;

	return g_AY8910[uChip].GetAYRegsPtr();
}

#endif