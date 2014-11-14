/*
  UTouch.cpp - Arduino/chipKit library support for Color TFT LCD Touch screens 
  Copyright (C)2010-2014 Henning Karlsen. All right reserved
  
  Basic functionality of this library are based on the demo-code provided by
  ITead studio. You can find the latest version of the library at
  http://www.henningkarlsen.com/electronics

  If you make any modifications or improvements to the code, I would appreciate
  that you share the code with me so that I might include it in the next release.
  I can be contacted through http://www.henningkarlsen.com/electronics/contact.php

  This library is free software; you can redistribute it and/or
  modify it under the terms of the CC BY-NC-SA 3.0 license.
  Please see the included documents for further information.

  Commercial use of this library requires you to buy a license that
  will allow commercial use. This includes using the library,
  modified or not, as a tool to sell products.

  The license applies to all part of the library including the 
  examples and tools supplied with the library.
*/

#include "UTouch.hpp"

UTouch::UTouch(unsigned int tclk, unsigned int tcs, unsigned int din, unsigned int dout, unsigned int irq)
	: portCLK(tclk), portCS(tcs), portDIN(din), portDOUT(dout), portIRQ(irq)
{
}

void UTouch::init(uint16_t xp, uint16_t yp, DisplayOrientation orientation, TouchPrecision p)
{
	orient					= orientation;
	nativeOrientation		= InvPortrait;
	touch_x_left			= 128;
	touch_x_right			= 3840;
	touch_y_top				= 255;
	touch_y_bottom			= 3840;
	disp_x_size				= xp;
	disp_y_size				= yp;
	
	portCLK.setMode(OneBitPort::Output);
	portCS.setMode(OneBitPort::Output);
	portDIN.setMode(OneBitPort::Output);
	portDOUT.setMode(OneBitPort::Input);
	portIRQ.setMode(OneBitPort::InputPullup);
	
	portCS.setHigh();
	portCLK.setHigh();
	portDIN.setHigh();
	
	setPrecision(p);
}

void UTouch::read()
{
	uint32_t tx = 0;
	uint32_t ty = 0;
	uint16_t minx = 9999, maxx = 0;
	uint16_t miny = 9999, maxy = 0;
	uint8_t datacount = 0;

	portCS.setLow();
	for (uint8_t i=0; i < prec; i++)
	{
		if (!portIRQ.read())
		{
			touch_WriteData(0x90);		// Start, A=1, 12 bit, differential, power down between conversions
			portCLK.pulseHigh();       
			uint16_t temp_x = touch_ReadData();

			if (!portIRQ.read())
			{
				touch_WriteData(0xD0);		// Start, A=5, 12 bit, differential, power down between conversions 
				portCLK.pulseHigh();     
				uint16_t temp_y = touch_ReadData();

				tx += temp_x;
				ty += temp_y;
				if (prec > 5)
				{
					if (temp_x < minx)
					{
						minx = temp_x;
					}
					if (temp_x > maxx)
					{
						maxx = temp_x;
					}
					if (temp_y < miny)
					{
						miny = temp_y;
					}
					if (temp_y > maxy)
					{
						maxy = temp_y;
					}
				}
				datacount++;
			}
		}
	}
	portCS.setHigh();

	if (datacount > 5)
	{
		tx = tx - (minx + maxx);
		ty = ty - (miny + maxy);
		datacount -= 2;
	}

	if ((datacount == prec - 2) || (prec == 1 && datacount == 1))
	{
		TP_X = (int16_t)(ty/datacount);
		TP_Y = (int16_t)(tx/datacount);
	}
	else
	{
		TP_X = -1;
		TP_Y = -1;
	}
}

bool UTouch::dataAvailable() const
{
	return !portIRQ.read();
}

int16_t UTouch::getX() const
{
	if ((TP_X == -1) || (TP_Y == -1))
		return -1;
	
	int16_t val = ((orient ^ nativeOrientation)	& SwapXY) ? TP_Y : TP_X;
	if ((orient ^ nativeOrientation) & ReverseX)
	{
		val = 4096 - val;
	}
	
	int16_t c = ((int32_t)(val - touch_x_left) * (int32_t)disp_x_size) / (int32_t)(touch_x_right - touch_x_left);
	return (c < 0) ? 0 : (c >= disp_x_size) ? disp_x_size - 1 : c;
}

int16_t UTouch::getY() const
{
	if ((TP_X == -1) || (TP_Y == -1))
	return -1;
	
	int16_t val = ((orient ^ nativeOrientation)	& SwapXY) ? TP_X : TP_Y;
	if ((orient ^ nativeOrientation) & ReverseY)
	{
		val = 4096 - val;
	}
	
	int16_t c = ((int32_t)(val - touch_y_top) * (int32_t)disp_y_size) / (int32_t)(touch_y_bottom - touch_y_top);
	return (c < 0) ? 0 : (c >= disp_y_size) ? disp_y_size - 1 : c;
}

void UTouch::setPrecision(TouchPrecision precision)
{
	switch (precision)
	{
	case TpLow:
		prec = 1;	// DO NOT CHANGE!
		break;
	case TpMedium:
		prec = 12;	// Iterations + 2
		break;
	case TpHigh:
		prec = 27;	// Iterations + 2
		break;
	case TpExtreme:
		prec = 102;	// Iterations + 2
		break;
	default:
		prec = 12;	// Iterations + 2
		break;
	}
}

void UTouch::touch_WriteData(uint8_t data)
{
	uint8_t temp = data;
	portCLK.setLow();

	for(uint8_t count=0; count<8; count++)
	{
		if (temp & 0x80)
		{
			portDIN.setHigh();
		}
		else
		{
			portDIN.setLow();
		}
		temp <<= 1;
		portCLK.pulseLow();
	}
}

uint16_t UTouch::touch_ReadData()
{
	uint16_t data = 0;

	for (uint8_t count=0; count<12; count++)
	{
		data <<= 1;
		portCLK.pulseHigh();
		if (portDOUT.read())
		{
			data++;
		}
	}

	// The chip datasheet says we must read another 4 bits before the chip is not busy	
	portCLK.pulseHigh();
	portCLK.pulseHigh();
	portCLK.pulseHigh();
	portCLK.pulseHigh();
	
	return(data);
}

void UTouch::calibrate(int16_t xlow, int16_t xhigh, int16_t ylow, int16_t yhigh)
{
	touch_x_left = (xlow * 4096)/disp_x_size;
	touch_x_right = (xhigh * 4096)/disp_x_size;
	touch_y_top = (ylow * 4096)/disp_y_size;
	touch_y_bottom = (yhigh * 4096)/disp_y_size;
}

// End
