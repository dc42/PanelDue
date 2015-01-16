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

	void Beep(uint32_t frequency, uint32_t ms)
	{
		inBuzzer = true;		// tell the tick interrupt to leave us alone
		if (beepTicksToGo == 0)
		{
			uint32_t period = pwmClockFrequency/frequency;
			pwm_channel_instance.ul_period = period;
			pwm_channel_instance.ul_duty = period/2;
			pwm_channel_init(PWM, &pwm_channel_instance);
			pwm_channel_enable(PWM, PWM_CHANNEL_0);
			beepTicksToGo = ms;
		}
		inBuzzer = false;
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
