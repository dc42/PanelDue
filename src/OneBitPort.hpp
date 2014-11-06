/*
 * OneBitPort.hpp
 *
 * Created: 04/11/2014 11:28:28
 *  Author: David
 */ 


#ifndef ONEBITPORT_H_
#define ONEBITPORT_H_

#include "ecv.h"
#include "asf.h"

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
		
	void pulseHigh() const { setHigh(); setLow(); }
	void pulseLow() const { setLow(); setHigh(); }
		
	bool read() const
	{
#if defined(SAM3S)
		return (port->PIO_PDSR & mask) != 0;
#else
		return (port & mask) != 0;
#endif		
	}
	
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