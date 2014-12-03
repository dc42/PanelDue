/*
 * Buzzer.h
 *
 * Created: 13/11/2014 22:56:34
 *  Author: David
 */ 


#ifndef BUZZER_H_
#define BUZZER_H_

namespace Buzzer
{
	void Init();

	void Beep(uint32_t frequency, uint32_t ms);

	void Tick();
}

#endif /* BUZZER_H_ */