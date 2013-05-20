#include "stdafx.h"
#include "SoundDeviceFactory.h"

CSoundDeviceFactory& CSoundDeviceFactory::Instance(void)
{
	static CSoundDeviceFactory SoundDeviceFactory;
	return SoundDeviceFactory;
}

void CSoundDeviceFactory::Register(SoundDevice_e SoundDevice, FUNCCreateSoundDevice CreateFunc)
{
	if (m_mapSndDev2CreateFunc.find(SoundDevice) != m_mapSndDev2CreateFunc.end())
		return;

	m_mapSndDev2CreateFunc[SoundDevice] = CreateFunc;
}

CSoundDevice* CSoundDeviceFactory::Create(SoundDevice_e SoundDevice)
{
	MAPSndDev2CreateFunc::iterator it = m_mapSndDev2CreateFunc.find(SoundDevice);
	if (m_mapSndDev2CreateFunc.find(SoundDevice) == m_mapSndDev2CreateFunc.end())
	{
		_ASSERT(0);
		return NULL;
	}

	FUNCCreateSoundDevice CreateFunc = it->second;
	CSoundDevice* pNewSoundDevice = CreateFunc();
	m_vecSoundDevices.push_back(pNewSoundDevice);
	return pNewSoundDevice;
}
