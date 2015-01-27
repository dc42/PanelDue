/*
  UTouch.cpp - library support for Color TFT LCD Touch screens on SAM3X 
  Originally based on Utouch library by Henning Karlsen.
  Rewritten by D Crocker using the approach described in TI app note http://www.ti.com/lit/pdf/sbaa036.
*/

#ifndef UTouch_h
#define UTouch_h

#include "asf.h"
#include "OneBitPort.hpp"
#include "DisplayOrientation.hpp"

class UTouch
{
public:
	UTouch(unsigned int tclk, unsigned int tcs, unsigned int tdin, unsigned int dout, unsigned int irq);

	void	init(uint16_t xp, uint16_t yp, DisplayOrientation orientationAdjust = Default);
	bool	read(uint16_t &x, uint16_t &y);
	void	calibrate(int16_t xlow, int16_t xhigh, int16_t ylow, int16_t yhigh);
	void	adjustOrientation(DisplayOrientation a) { orientAdjust = (DisplayOrientation) (orientAdjust ^ a); }
	DisplayOrientation getOrientation() const { return orientAdjust; }
    
private:
	OneBitPort portCLK, portCS, portDIN, portDOUT, portIRQ;
	DisplayOrientation orientAdjust;
	uint16_t disp_x_size, disp_y_size;
	int16_t	touch_x_left, touch_x_right, touch_y_top, touch_y_bottom;

	bool	getTouchData(bool wantY, uint16_t &rslt);
	void	touch_WriteCommand(uint8_t command);
	uint16_t touch_ReadData(uint8_t command);
	uint16_t diff(uint16_t a, uint16_t b) { return (a < b) ? b - a : a - b; }
};

#endif