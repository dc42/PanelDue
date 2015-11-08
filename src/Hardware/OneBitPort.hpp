/*
 * OneBitPort.hpp
 *
 * Created: 04/11/2014 11:28:28
 *  Author: David
 */ 


#ifndef ONEBITPORT_H_
#define ONEBITPORT_H_

#include "ecv.h"
#undef array
#undef result
#include "asf.h"
#define array _ecv_array
#define result _ecv_result

class OneBitPort
{
public:
	enum PortMode { Output, Input, InputPullup };
		
	OneBitPort(unsigned int pin);
		
	void setMode(PortMode mode);
		
	void setLow() const
	{
#if defined(SAM3S)
		pio_clear(port, mask);
#else
		*port &= ~mask;
#endif
	}
		
	void setHigh() const
	{
#if defined(SAM3S)
		pio_set(port, mask);
#else
		port |= mask;
#endif
	}
		
	// Pulse the pin high. On the SAM3S the pulse is about 400ns wide.
	void pulseHigh() const
	{
		setHigh();
		setLow();
	}

	// Pulse the pin high. On the SAM3S the pulse is about 400ns wide.
	void pulseLow() const
	{
		setLow();
		setHigh();
	}
		
	bool read() const
	{
#if defined(SAM3S)
		return (port->PIO_PDSR & mask) != 0;
#else
		return (port & mask) != 0;
#endif		
	}
	
	static void delay(uint8_t del);
	
	static const uint8_t delay_100ns = 1;		// delay argument for 100ns
	static const uint8_t delay_200ns = 2;		// delay argument for 200ns

private:

#if defined(SAM3S)
	Pio *port;			// PIO address
	uint32_t mask;		// bit mask
#else						// else assume Arduino
	regtype *port;		// port address
	regsize mask;		// bit number
#endif
};
	

#endif /* ONEBITPORT_H_ */