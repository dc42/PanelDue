/*
 * RequestTimer.cpp
 *
 * Created: 06/11/2015 14:22:55
 *  Author: David
 */ 

#include "ecv.h"
#include "asf.h"
#include "RequestTimer.hpp"
#include "Hardware/SysTick.hpp"
#include "Hardware/SerialIo.hpp"

extern bool OkToSend();		// in PanelDue.cpp

RequestTimer::RequestTimer(uint32_t del, const char *cmd)
	: delayTime(del), command(cmd)
{
	timerState = stopped;
}

bool RequestTimer::Process()
{
	if (timerState == running)
	{
		uint32_t now = SystemTick::GetTickCount();
		if (now - startTime > delayTime)
		{
			timerState = ready;
		}
	}

	if (timerState == ready && OkToSend())
	{
		SerialIo::SendString(command);
		SerialIo::SendChar('\n');
		startTime = SystemTick::GetTickCount();
		timerState = running;
		return true;
	}
	return false;
}

// End
