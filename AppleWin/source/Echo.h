#pragma once

#include "Common.h"
#include "TMS5220.h"

extern class CEcho sg_Echo;

class CEcho
{
public:
	CEcho(const int nSampleRate);
	virtual ~CEcho(void);

	void Initialize(LPBYTE pCxRomPeripheral, UINT uSlot);
	void Uninitialize(void);
	void Reset(void);
	short* AudioRequest(const UINT uNumSamples);
	static BYTE __stdcall IORead(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nCyclesLeft);
	static BYTE __stdcall IOWrite(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nCyclesLeft);

private:
	CEcho(void);

private:
	UINT m_uSlot;
	int m_nPlaybackRate;
	CTMS5220 m_TMS5220;
	int m_nAudioCounter;
	short* m_pBuffer;
};
