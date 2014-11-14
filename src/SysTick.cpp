/*
 * SysTick.cpp
 *
 * Created: 13/11/2014 23:03:37
 *  Author: David
 */ 

#include "ecv.h"
#include "asf.h"
#include "SysTick.hpp"
#include "Buzzer.hpp"

volatile uint32_t tickCount;

uint32_t GetTickCount()
{
	return tickCount;
}

void SysTick_Handler()
{
	++tickCount;
	BuzzerTick();
}

// End
