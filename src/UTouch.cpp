/*
  UTouch.cpp - library support for Color TFT LCD Touch screens on SAM3X 
  Originally based on Utouch library by Henning Karlsen.
  Rewritten by D Crocker using the approach described in TI app note http://www.ti.com/lit/pdf/sbaa036.
*/

#include "UTouch.hpp"

UTouch::UTouch(unsigned int tclk, unsigned int tcs, unsigned int din, unsigned int dout, unsigned int irq)
	: portCLK(tclk), portCS(tcs), portDIN(din), portDOUT(dout), portIRQ(irq)
{
}

void UTouch::init(uint16_t xp, uint16_t yp, DisplayOrientation orientationAdjust)
{
	orientAdjust			= orientationAdjust;
	touch_x_left			= 0;
	touch_x_right			= 4095;
	touch_y_top				= 0;
	touch_y_bottom			= 4095;
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
}

// If the panel is touched, return the coordinates in x and y and return true; else return false
bool UTouch::read(uint16_t &px, uint16_t &py)
{
	bool ret = false;
	if (!portIRQ.read())			// if screen is touched
	{
		portCS.setLow();
		uint16_t tx;
		if (getTouchData(false, tx))
		{
			uint16_t ty;
			if (getTouchData(true, ty))
			{
				if (!portIRQ.read())
				{
					int16_t valx = (orientAdjust & SwapXY) ? ty : tx;
					if (orientAdjust & ReverseX)
					{
						valx = 4096 - valx;
					}
						
					int16_t cx = (int16_t)(((int32_t)(valx - touch_x_left) * (int32_t)disp_x_size) / (touch_x_right - touch_x_left));
					px = (cx < 0) ? 0 : (cx >= disp_x_size) ? disp_x_size - 1 : cx;

					int16_t valy = (orientAdjust & SwapXY) ? tx : ty;
					if (orientAdjust & ReverseY)
					{
						valy = 4096 - valy;
					}
	
					int16_t cy = (int16_t)(((int32_t)(valy - touch_y_top) * (int32_t)disp_y_size) / (touch_y_bottom - touch_y_top));
					py = (cy < 0) ? 0 : (cy >= disp_y_size) ? disp_y_size - 1 : cy;
					ret = true;
				}				
			}
		}
		portCS.setHigh();
	}
	return ret;
}

// Get data from the touch chip. CS has already been set low.
// We need to allow the touch chip ADC input to settle. See TI app note http://www.ti.com/lit/pdf/sbaa036.
bool UTouch::getTouchData(bool wantY, uint16_t &rslt)
{
	uint8_t command = (wantY) ? 0xD0 : 0x90;		// start, channel 5 (y) or 1 (x), 12-bit, differential mode, power down between conversions
	touch_WriteCommand(command);					// send the command
	touch_ReadData(command);						// discard the first result and send the same command again

	const size_t numReadings = 4;
	const uint16_t maxDiff = 4;
	const unsigned int maxAttempts = 16;

	uint16_t ring[numReadings];
	uint16_t sum = 0;
	for (size_t i = 0; i < numReadings; ++i)
	{
		uint16_t val = touch_ReadData(command);
		ring[i] = val;
		sum += val;
	}

	uint16_t avg;
	bool ok;
	size_t last = 0;
	for (unsigned int i = 0; i < maxAttempts; ++i)
	{
		avg = sum/numReadings;
		ok = true;
		for (size_t i = 0; ok && i < numReadings; ++i)
		{
			ok = diff(avg, ring[i]) < maxDiff;
		}
		if (ok)
		{
			break;
		}
		sum -= ring[last];
		uint16_t val = touch_ReadData(command);
		ring[last] = val;
		sum += val;
		last = (last + 1) % numReadings;
	}
	
	touch_ReadData(0);
	rslt = avg;
	return ok;
}

// Send the first command in a chain. The chip latches the data bit on the rising edge of the clock. We have already set CS low.
void UTouch::touch_WriteCommand(uint8_t command)
{
	for(uint8_t count=0; count<8; count++)
	{
		if (command & 0x80)
		{
			portDIN.setHigh();
		}
		else
		{
			portDIN.setLow();
		}
		command <<= 1;
		portCLK.pulseHigh();
	}
}

// Read the data, and write another command at the same time. We have already set CS low.
// The chip produces its data bit after the falling edge of the clock. After sending 8 clocks, we can send a command again.
uint16_t UTouch::touch_ReadData(uint8_t command)
{
	uint16_t cmd = (uint16_t)command;
	uint16_t data = 0;

	for (uint8_t count=0; count<16; count++)
	{
		if (cmd & 0x8000)
		{
			portDIN.setHigh();
		}
		else
		{
			portDIN.setLow();
		}
		cmd <<= 1;
		OneBitPort::delay(OneBitPort::delay_100ns);					// need 100ns setup time from writing data to clock rising edge
		portCLK.pulseHigh();
		if (count < 12)
		{
			OneBitPort::delay(OneBitPort::delay_200ns);				// need 200ns setup time form clock falling edge to reading data
			data <<= 1;
			if (portDOUT.read())
			{
				data++;
			}
		}
	}
	
	return(data);
}

void UTouch::calibrate(int16_t xlow, int16_t xhigh, int16_t ylow, int16_t yhigh)
{
	touch_x_left = ((int32_t)xlow * 4096)/disp_x_size;
	touch_x_right = ((int32_t)xhigh * 4096)/disp_x_size;
	touch_y_top = ((int32_t)ylow * 4096)/disp_y_size;
	touch_y_bottom = ((int32_t)yhigh * 4096)/disp_y_size;
}

// End
