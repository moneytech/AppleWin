#pragma once

#include "SoundDevice.h"

typedef CSoundDevice*(*FUNCCreateSoundDevice)(void);
typedef std::map<SoundDevice_e, FUNCCreateSoundDevice> MAPSndDev2CreateFunc;
typedef std::vector<CSoundDevice*> VECSoundDevice;

class CSoundDeviceFactory
{
public:
	CSoundDeviceFactory(void) {}
	virtual ~CSoundDeviceFactory(void)
	{
		for (VECSoundDevice::iterator it = m_vecSoundDevices.begin(); it != m_vecSoundDevices.end(); ++it)
			delete *it;
	}

	static CSoundDeviceFactory& Instance(void);
	void Register(SoundDevice_e Type, FUNCCreateSoundDevice pfnCreateSoundDevice);
	CSoundDevice* Create(SoundDevice_e SoundDevice);

private:
	MAPSndDev2CreateFunc m_mapSndDev2CreateFunc;
	VECSoundDevice m_vecSoundDevices;
};
