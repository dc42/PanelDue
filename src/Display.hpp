/*
 * Display.h
 *
 * Created: 04/11/2014 09:43:43
 *  Author: David
 */ 


#ifndef DISPLAY_H_
#define DISPLAY_H_

#include "ecv.h"
#include "UTFT.hpp"

typedef const uint8_t * array LcdFont;

const Color red = UTFT::fromRGB(255,0,0);
const Color yellow = UTFT::fromRGB(128,128,0);
const Color green = UTFT::fromRGB(0,255,0);
const Color blue = UTFT::fromRGB(0,0,255);
const Color white = 0xFFFF;
const Color black = 0x0000;

typedef uint16_t PixelNumber;
typedef uint16_t LcdColour;
typedef uint16_t Event;
const Event nullEvent = 0;

enum TextAlignment { Left, Centre, Right };
	
// Base class for a displayable field
class DisplayField
{
protected:
	PixelNumber y, x;							// Coordinates of top left pixel, counting from the top left corner
	PixelNumber width;							// number of pixels occupied in each direction
	LcdColour fcolour, bcolour;					// foreground and background colours
	Event evt;									// event number that is triggered by touching this field, or nullEvent if not touch sensitive
	LcdFont font;
	bool changed;
	
	static LcdFont defaultFont;
	static LcdColour defaultFcolour, defaultBcolour;
	
protected:
	DisplayField(PixelNumber py, PixelNumber px, PixelNumber pw)
		: y(py), x(px), width(pw), fcolour(defaultFcolour), bcolour(defaultBcolour), evt(nullEvent), font(defaultFont), changed(true), next(NULL)
	{
	}
	
	virtual PixelNumber GetHeight() const { return font[1]; }	// nasty - should fix this, but don't want to run into alignment issues

public:
	DisplayField * null next;					// link to next field in list

	virtual void Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset) { }		// would like to make this pure virtual but then we get 50K of library that we don't want
	void SetColours(LcdColour pf, LcdColour pb) { fcolour = pf; bcolour = pb; changed = true; }
	void SetEvent(Event e) { evt = e; }
	Event GetEvent() const { return evt; }
	void SetChanged() { changed = true; }
	PixelNumber GetMinX() const { return x; }
	PixelNumber GetMaxX() const { return x + width - 1; }
	PixelNumber GetMinY() const { return y; }
	PixelNumber GetMaxY() const { return y + GetHeight() - 1; }

	static void SetDefaultColours(LcdColour pf, LcdColour pb) { defaultFcolour = pf; defaultBcolour = pb; }
	static void SetDefaultFont(LcdFont pf) { defaultFont = pf; }
	static DisplayField * null FindEvent(int x, int y, DisplayField * null p);
};

class PopupField
{
private:
	PixelNumber height, width;
	DisplayField * null root;
	
public:
	PopupField(PixelNumber ph, PixelNumber pw);
	PixelNumber GetHeight() override const { return height; }
	PixelNumber GetWidth() override const { return width; }
	void Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset) override;
	void AddField(DisplayField *p);
	DisplayField * null FindEvent(int x, int y);
};

class DisplayManager
{
public:
	DisplayManager();
	void Init(LcdColour pb);
	void AddField(DisplayField *pd);
	void RefreshAll(bool full = 0);
	DisplayField * null FindEvent(PixelNumber x, PixelNumber y);
	bool HavePopup() const { return popupField != NULL; }
	void SetPopup(PopupField * null p, PixelNumber px = 0, PixelNumber py = 0);
	void AttachPopup(PopupField * pp, DisplayField *p);
	bool Visible(const DisplayField *p) const;
	
private:
	LcdColour backgroundColor;
	DisplayField * null root;
	PopupField * null popupField;
	PixelNumber popupX, popupY;
};

// Base class for a labeled field, comprising a fixed text label followed by variable data. The label can be null.
class LabelledField : public DisplayField
{
	const char * array null label;
protected:
	uint16_t labelColumns;
	
	void DoLabel(bool full, PixelNumber xOffset, PixelNumber yOffset);
	
	LabelledField(PixelNumber py, PixelNumber px, PixelNumber pw, const char * array pLabel)
		: DisplayField(py, px, pw), label(pLabel), labelColumns(0)
	{}
};

class TextField : public LabelledField
{
	const char* array text;
public:
	TextField(PixelNumber py, PixelNumber px, PixelNumber pw, const char * array pl, const char* array pt)
		: LabelledField(py, px, pw, pl), text(pt)
	{
	}

	void Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset) override;
};

class FloatField : public LabelledField
{
	const char * array null units;
	uint8_t numDecimals;
	float val;
public:
	FloatField(PixelNumber py, PixelNumber px, PixelNumber pw, const char * array pl, uint8_t pd = 0, const char * array null pu = NULL)
		: LabelledField(py, px, pw, pl), units(pu), numDecimals(pd), val(0.0)
	{
	}

	void Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset) override;
	
	void SetValue(float v)
	{
		val = v;
		changed = true;
	}
};

class IntegerField : public LabelledField
{
	const char *units;
	int val;
public:
	IntegerField(PixelNumber py, PixelNumber px, PixelNumber pw, const char *pl, const char *pu = NULL)
		: LabelledField(py, px, pw, pl), units(pu), val(0.0)
	{
	}

	void Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset) override;
	
	int GetValue() const { return val; }

	void SetValue(int v)
	{
		val = v;
		changed = true;
	}
	
	void Increment(int amount)
	{
		val += amount;
		changed = true;
	}
};

class StaticTextField : public DisplayField
{
	TextAlignment align;
	const char *text;
public:
	StaticTextField(PixelNumber py, PixelNumber px, PixelNumber pw, TextAlignment pa, const char *pt)
		: DisplayField(py, px, pw), align(pa), text(pt) {}

	void Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset) override;
};

class ProgressBar : public DisplayField
{
	uint8_t height;
	uint8_t percent;
	PixelNumber lastNumPixelsSet;
public:
	ProgressBar(uint16_t py, uint16_t px, uint8_t ph, uint16_t pw)
		: DisplayField(py, px, pw), height(ph), percent(0), lastNumPixelsSet(0)
	{
	}
	
	void Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset) override;
	
	virtual PixelNumber GetHeight() override const { return height; }

	void SetPercent(uint8_t pc)
	{
		percent = pc;
		changed = true;
	}
};

class IntegerSettingField : public IntegerField
{
public:
	IntegerSettingField(const char* array pc, PixelNumber py, PixelNumber px, PixelNumber pw, const char *pl, const char *pu = NULL);

	void Action();
	
private:
	const char * array cmd;
};

#endif /* DISPLAY_H_ */