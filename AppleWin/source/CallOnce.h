#pragma once

class CCallOnce
{
public:
	typedef void (*CallOnceFunc)(void);
	explicit CCallOnce(CallOnceFunc func)
	{
		func();
	}
};
