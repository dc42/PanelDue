/*
 * Buzzer.cpp
 *
 * Created: 13/11/2014 22:56:18
 *  Author: David
 */ 

#include "ecv.h"
#include "asf.h"
#include "buzzer.hpp"
#include "SysTick.hpp"

const uint32_t frequency = 2000;

static pwm_channel_t pwm_channel_instance;
static uint32_t beepTicksToGo = 0;
static bool inBuzzer = true;

void BuzzerInit()
{
	pwm_channel_disable(PWM, PWM_CHANNEL_0);
	pwm_clock_t clock_setting =
	{
		.ul_clka = frequency * 100,
		.ul_clkb = 0,
		.ul_mck = SystemCoreClock
	};
	pwm_init(PWM, &clock_setting);
	pwm_channel_instance.ul_prescaler = PWM_CMR_CPRE_CLKA;
	pwm_channel_instance.ul_period = 100;
	pwm_channel_instance.ul_duty = 50;
	pwm_channel_instance.channel = PWM_CHANNEL_0;
	pwm_channel_init(PWM, &pwm_channel_instance);
	pio_configure(PIOB, PIO_PERIPH_A, PIO_PB0, 0);		// enable HI output
	pio_configure(PIOB, PIO_PERIPH_B, PIO_PB5, 0);		// enable LO output
	beepTicksToGo = 0;
	inBuzzer = false;
}

void BuzzerBeep(uint32_t ms)
{
	inBuzzer = true;
	if (beepTicksToGo == 0)
	{
		pwm_channel_enable(PWM, PWM_CHANNEL_0);
	}
	beepTicksToGo = ms;
	inBuzzer = false;
}

// This is called from the tick ISR
void BuzzerTick()
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

// End
