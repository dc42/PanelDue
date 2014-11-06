#ifndef HW_AVR_h
#define HW_AVR_h

// *** Hardware specific functions ***
void UTFT::LCD_Write_Bus(uint8_t VH, uint8_t VL)
{   
	switch (displayTransferMode)
	{
#ifndef DISABLE_SERIAL
	case TModeSerial4pin:
	case TModeSerial5pin:
		if (displayTransferMode == TModeSerial4pin)
		{
			if (VH==1)
			{
				portSDA.setHigh();
			}
			else
			{
				portSDA.setLow();
			}
			portSCL.pulseLow();
		}
		else
		{
			if (VH==1)
			{
				portRS.setHigh();
			}
			else
			{
				portRS.setLow();
			}
		}

		if (VL & 0x80)
		{
			portSDA.setHigh();
		}
		else
		{
			portSDA.setLow();
		}
		portSCL.pulseLow();
		if (VL & 0x40)
		{
			portSDA.setHigh();
		}
		else
		{
			portSDA.setLow();
		}
		portSCL.pulseLow();
		if (VL & 0x20)
		{
			portSDA.setHigh();
		}
		else
		{
			portSDA.setLow();
		}
		portSCL.pulseLow();
		if (VL & 0x10)
		{
			portSDA.setHigh();
		}
		else
		{
			portSDA.setLow();
		}
		portSCL.pulseLow();
		if (VL & 0x08)
		{
			portSDA.setHigh();
		}
		else
		{
			portSDA.setLow();
		}
		portSCL.pulseLow();
		if (VL & 0x04)
		{
			portSDA.setHigh();
		}
		else
		{
			portSDA.setLow();
		}
		portSCL.pulseLow();
		if (VL & 0x02)
		{
			portSDA.setHigh();
		}
		else
		{
			portSDA.setLow();
		}
		portSCL.pulseLow();
		if (VL & 0x01)
		{
			portSDA.setHigh();
		}
		else
		{
			portSDA.setLow();
		}
		portSCL.pulseLow();
		break;
#endif

#ifndef DISABLE_8BIT
	case TMode8bit:
#if defined(SAM3S)
#elif defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
		PORTA = VH;
		portWR.pulseLow();
		PORTA = VL;
		portWR.pulseLow();
#elif defined(__AVR_ATmega32U4__)
		/*
			Pin TFT   Leo   Uno
			-------------------
			D0  DB8   PD2   PD0
			D1  DB9   PD3   PD1
			D2  DB10  PD1   PD2
			D3  DB11  PD0   PD3
			D4  DB12  PD4   PD4
			D5  DB13  PC6   PD5
			D6  DB14  PD7   PD6
			D7  DB15  PE6   PD7
		*/
		if(VH & 0x01)
			PORTD |= (1<<2);
		else
			PORTD &= ~(1<<2);
		if(VH & 0x02)
			PORTD |= (1<<3);
		else
			PORTD &= ~(1<<3);
		if (VH & 0x04)
			PORTD |= (1<<1);
		else
			PORTD &= ~(1<<1);
		if (VH & 0x08)
			PORTD |= (1<<0);
		else
			PORTD &= ~(1<<0);
		if (VH & 0x10)
			PORTD |= (1<<4);
		else
			PORTD &= ~(1<<4);
		if (VH & 0x20)
			PORTC |= (1<<6);
		else
			PORTC &= ~(1<<6);
		if (VH & 0x40)
			PORTD |= (1<<7);
		else
			PORTD &= ~(1<<7);
		if (VH & 0x80)
			PORTE |= (1<<6);
		else
			PORTE &= ~(1<<6);
		portWR.pulseLow();
		if(VL & 0x01)
			PORTD |= (1<<2);
		else
			PORTD &= ~(1<<2);
		if(VL & 0x02)
			PORTD |= (1<<3);
		else
			PORTD &= ~(1<<3);
		if (VL & 0x04)
			PORTD |= (1<<1);
		else
			PORTD &= ~(1<<1);
		if (VL & 0x08)
			PORTD |= (1<<0);
		else
			PORTD &= ~(1<<0);
		if (VL & 0x10)
			PORTD |= (1<<4);
		else
			PORTD &= ~(1<<4);
		if (VL & 0x20)
			PORTC |= (1<<6);
		else
			PORTC &= ~(1<<6);
		if (VL & 0x40)
			PORTD |= (1<<7);
		else
			PORTD &= ~(1<<7);
		if (VL & 0x80)
			PORTE |= (1<<6);
		else
			PORTE &= ~(1<<6);	
		portWR.pulseLow();
#else
		PORTD = VH;
		portWR.pulseLow();
		PORTD = VL;
		portWR.pulseLow();
#endif
		break;
#endif

#ifndef DISABLE_9BIT
	case TMode9bit:
		// This is for using a display with a 16-bit parallel interface with one of the smaller Arduinos.
		// We use the same 8 pins to pass first the high byte and then the low byte. We latch the high byte in a 74HC373.
#if defined(SAM3S)
#elif defined(__AVR_ATmega32U4__)
		// Use PORTF 0-1, PORTD 0-1 and PORTF 4-7 to pass first the high byte and then the low byte (NB this is not pin-compatible with the Uno)
		// Write the high byte
		PORTF = VH;
		cli();				// disable interrupts in case an ISR writes to other bits of port D
		PORTD = (PORTD & 0xFC) | ((VH >> 2) & 0x03);
		sei();
		portSCL.pulseHigh();		// latch the high byte
		
		// Write the low byte
		PORTF = VL;
		cli();				// disable interrupts in case an ISR writes to other bits of port D
		PORTD = (PORTD & 0xFC) | ((VL >> 2) & 0x03);
		sei();
#else
		PORTD = VH;
		portSCL.pulseHigh();
		PORTD = VL;
#endif
		portWR.pulseLow();		
		break;
#endif

#ifndef DISABLE_16BIT
	case TMode16bit:
#if defined(SAM3S)
		pio_sync_output_write(PIOA, (VH << 8) | VL);
#elif defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
		PORTA = VH;
		PORTC = VL;
#else
		PORTD = VH;
		PORTC = (PORTC & 0xFC) | ((VL>>6) & 0x03);
		PORTB =  VL & 0x3F;
#endif
		portWR.pulseLow();
		break;
#endif
	default:
		break;		// to keep gcc happy
	}
}

// Write the previous 16-bit data again the specified number of times.
// Only supported in 9 and 16 bit modes. Used to speed up setting large blocks of pixels to the same colour. 
void UTFT::LCD_Write_Again(uint16_t num)
{   
	while (num != 0)
	{
		portWR.pulseLow();
		--num;
	}
}

void UTFT::_set_direction_registers()
{
#if defined(SAM3S)
	pio_configure(PIOA, PIO_OUTPUT_0, (displayTransferMode == TMode16bit) ? 0x0000FFFF : 0x000000FF, 0);
	pio_enable_output_write(PIOA, (displayTransferMode == TMode16bit) ? 0x0000FFFF : 0x000000FF);
#elif defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
	DDRA = 0xFF;
	if (displayTransferMode == TMode16bit)
	{
		DDRC = 0xFF;
	}
#elif defined(__AVR_ATmega32U4__)
	if (displayTransferMode == TMode9bit)
	{
		DDRF |= 0xF3;
		DDRD |= 0x03;
	}
	else
	{
		pinMode(0,OUTPUT);
		pinMode(1,OUTPUT);
		pinMode(2,OUTPUT);
		pinMode(3,OUTPUT);
		pinMode(4,OUTPUT);
		pinMode(5,OUTPUT);
		pinMode(6,OUTPUT);
		pinMode(7,OUTPUT);
	}
#else
	DDRD = 0xFF;
	if (displayTransferMode == TMode16bit)
	{
		DDRB |= 0x3F;
		DDRC |= 0x03;
	}
#endif

}

#endif
