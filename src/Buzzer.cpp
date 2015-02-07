/*
 * Buzzer.cpp
 *
 * Created: 13/11/2014 22:56:18
 *  Author: David
 */ 

#include "ecv.h"
#include "asf.h"
#include "Buzzer.hpp"
#include "SysTick.hpp"

namespace Buzzer
{
	static const uint32_t pwmClockFrequency = 2000000;		// 2MHz clock
	static pwm_channel_t pwm_channel_instance =
	{
		.channel = 0,
		.ul_prescaler = PWM_CMR_CPRE_CLKA		
	};
	static uint32_t beepTicksToGo = 0;
	static bool inBuzzer = true;

	void Init()
	{
		pwm_channel_disable(PWM, PWM_CHANNEL_0);
		pwm_clock_t clock_setting =
		{
			.ul_clka = pwmClockFrequency,
			.ul_clkb = 0,
			.ul_mck = SystemCoreClock
		};
		pwm_init(PWM, &clock_setting);
		pio_configure(PIOB, PIO_PERIPH_A, PIO_PB0, 0);		// enable HI output
		pio_configure(PIOB, PIO_PERIPH_B, PIO_PB5, 0);		// enable LO output
		beepTicksToGo = 0;
		inBuzzer = false;
	}

	static uint32_t volumeTable[MaxVolume] = { 3, 9, 20, 40, 80 };
		
	// Generate a beep of the given length and frequency. The volume goes from 0 to MaxVolume.
	void Beep(uint32_t frequency, uint32_t ms, uint32_t volume)
	{
		if (volume != 0)
		{
			if (volume > MaxVolume)
			{
				volume = MaxVolume;
			}
			
			inBuzzer = true;		// tell the tick interrupt to leave us alone
			if (beepTicksToGo == 0)
			{
				uint32_t period = pwmClockFrequency/frequency;
				// To get the maximum fundamental component, we want the dead time to be 1/6 of the period.
				// Larger dead times reduce the volume, at the expense of generating more high harmonics.
				uint32_t onTime = (period * volumeTable[volume - 1])/200;
				uint16_t deadTime = period/2 - onTime;
				pwm_channel_instance.ul_period = period;
				pwm_channel_instance.ul_duty = period/2;
				pwm_channel_init(PWM, &pwm_channel_instance);
				PWM->PWM_CH_NUM[PWM_CHANNEL_0].PWM_CMR |= PWM_CMR_DTE;
				PWM->PWM_CH_NUM[PWM_CHANNEL_0].PWM_DT = (deadTime << 16) | deadTime;
				PWM->PWM_CH_NUM[PWM_CHANNEL_0].PWM_DTUPD = (deadTime << 16) | deadTime;
				pwm_channel_enable(PWM, PWM_CHANNEL_0);
				beepTicksToGo = ms;
			}
			inBuzzer = false;
		}
	}

	// This is called from the tick ISR
	void Tick()
	{
		if (!inBuzzer && beepTicksToGo != 0)
		{
			--beepTicksToGo;
			if (beepTicksToGo == 0)
			{
				pwm_channel_disable(PWM, PWM_CHANNEL_0);
			}
		}
	}
	
	bool Noisy()
	{
		return beepTicksToGo != 0;
	}
}

// End
