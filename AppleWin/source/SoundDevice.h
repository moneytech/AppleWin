#pragma once

enum SoundDevice_e
{
	eSndDev_Speaker,
	eSndDev_AY8910,
	eSndDev_SSI263,
	eSndDev_Votrax,
	eSndDev_TMS5220,
	eSndDev_8BitDAC,
};

class CSoundDevice
{
public:
	CSoundDevice(void) {}
	virtual ~CSoundDevice(void) {}

	//virtual Init(void) = 0;
	//virtual Reset(void) = 0;
	//virtual RequestData(UINT uNumSamplesReqd) = 0;
};
