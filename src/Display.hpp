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

// Unicode strings for special characters in our font
#define DECIMAL_POINT	"\xC2\xB7"		// Unicode middle-dot
#define DEGREE_SYMBOL	"\xC2\xB0"		// Unicode degree-symbol
#define THIN_SPACE		"\xC2\x80"		// Unicode control character, we use it as thin space

const Colour red = UTFT::fromRGB(255,0,0);
const Colour yellow = UTFT::fromRGB(128,128,0);
const Colour green = UTFT::fromRGB(0,255,0);
const Colour turquoise = UTFT::fromRGB(0,128,128);
const Colour blue = UTFT::fromRGB(0,0,255);
const Colour magenta = UTFT::fromRGB(128,0,128);
const Colour white = 0xFFFF;
const Colour black = 0x0000;

typedef uint16_t PixelNumber;
typedef uint16_t Event;
const Event nullEvent = 0;

enum TextAlignment { Left, Centre, Right };
	
// Base class for a displayable field
class DisplayField
{
protected:
	PixelNumber y, x;							// Coordinates of top left pixel, counting from the top left corner
	PixelNumber width;							// number of pixels occupied in each direction
	Colour fcolour, bcolour;						// foreground and background colours
	Event evt;									// event number that is triggered by touching this field, or nullEvent if not touch sensitive
	LcdFont font;
	bool changed;
	bool visible;
	
	union
	{
		const char* null sParam;
		int iParam;
//		float fParam;
	} param;
	
	static LcdFont defaultFont;
	static Colour defaultFcolour, defaultBcolour;
	static Colour defaultButtonBorderColour, defaultGradColour, defaultPressedBackColour, defaultPressedGradColour;
	
protected:
	DisplayField(PixelNumber py, PixelNumber px, PixelNumber pw);
	
	virtual PixelNumber GetHeight() const;

public:
	DisplayField * null next;					// link to next field in list

	virtual bool IsButton() const { return false; }
	bool IsVisible() const { return visible; }
	void Show(bool v);
	virtual void Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset) { }		// would like to make this pure virtual but then we get 50K of library that we don't want
	void SetColours(Colour pf, Colour pb);
	void SetEvent(Event e, const char* null sp ) { evt = e; param.sParam = sp; }
	void SetEvent(Event e, int ip ) { evt = e; param.iParam = ip; }
//	void SetEvent(Event e, double fp ) { evt = e; param.fParam = fp; }
	Event GetEvent() const { return evt; }
	void SetChanged() { changed = true; }
	const char* null GetSParam() const { return param.sParam; }
	int GetIParam() const { return param.iParam; }
//	float GetFParam() const { return param.fParam; }
	PixelNumber GetMinX() const { return x; }
	PixelNumber GetMaxX() const { return x + width - 1; }
	PixelNumber GetMinY() const { return y; }
	PixelNumber GetMaxY() const { return y + GetHeight() - 1; }

	static void SetDefaultColours(Colour pf, Colour pb) { defaultFcolour = pf; defaultBcolour = pb; }
	static void SetDefaultColours(Colour pf, Colour pb, Colour pbb, Colour pg, Colour pbp, Colour pgp);
	static void SetDefaultFont(LcdFont pf) { defaultFont = pf; }
	static DisplayField * null FindEvent(int x, int y, DisplayField * null p);
};

class PopupField
{
private:
	PixelNumber height, width;
	Colour backgroundColour;
	DisplayField * null root;
	
public:
	PopupField(PixelNumber ph, PixelNumber pw, Colour pb);
	PixelNumber GetHeight() const { return height; }
	PixelNumber GetWidth() const { return width; }
	void Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset);
	void AddField(DisplayField *p);
	DisplayField * null FindEvent(int x, int y);
	DisplayField * null GetRoot() const { return root; }
};

class Button : public DisplayField
{
	Colour borderColour, gradColour, pressedBackColour, pressedGradColour;
	bool pressed;

protected:
	virtual PixelNumber GetHeight() const override;

	virtual void PrintText() const {}		// ideally would be pure virtual

	Button(PixelNumber py, PixelNumber px, PixelNumber pw)
		: DisplayField(py, px, pw), borderColour(defaultButtonBorderColour), gradColour(defaultGradColour),
		  pressedBackColour(defaultPressedBackColour), pressedGradColour(defaultPressedGradColour),
		  pressed(false) {}

public:
	bool IsButton() const override final { return true; }

	void Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset) override final;
	
	void Press(bool p)
	{
		if (p != pressed)
		{
			pressed = p;
			changed = true;
		}
	}
};

class DisplayManager
{
public:
	DisplayManager();
	void Init(Colour pb);
	void ClearAll();
	void AddField(DisplayField *pd);
	void RefreshAll(bool full = 0);
	DisplayField * null FindEvent(PixelNumber x, PixelNumber y);
	DisplayField * null FindEventOutsidePopup(PixelNumber x, PixelNumber y);
	bool HavePopup() const { return popupField != NULL; }
	void SetPopup(PopupField * null p, PixelNumber px = 0, PixelNumber py = 0);
	void AttachPopup(PopupField * pp, DisplayField *p);
	bool ObscuredByPopup(const DisplayField *p) const;
	bool Visible(const DisplayField *p) const;
	DisplayField * null GetRoot() const { return root; }
	void SetRoot(DisplayField * null r) { root = r; }
	void Outline(DisplayField *f, Colour c, PixelNumber numPixels);
	void RemoveOutline(DisplayField *f, PixelNumber numPixels) { Outline(f, backgroundColor, numPixels); }
	void Show(DisplayField *f, bool v);
	void Press(Button *f, bool v);

private:
	Colour backgroundColor;
	DisplayField * null root;
	PopupField * null popupField;
	PixelNumber popupX, popupY;
};

// base class for most types of field
class RegularField : public DisplayField
{
	TextAlignment align;
	
protected:
	virtual void PrintText() const {}		// would ideally be pure virtual

	RegularField(PixelNumber py, PixelNumber px, PixelNumber pw, TextAlignment pa)
		: DisplayField(py, px, pw), align(pa)
	{
	}
		
public:
	void Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset) override final;
};
	
class TextField : public RegularField
{
	const char* array null label;
	const char* array null text;
	
protected:
	void PrintText() const override;

public:
	TextField(PixelNumber py, PixelNumber px, PixelNumber pw, TextAlignment pa, const char * array pl, const char* array pt = NULL)
		: RegularField(py, px, pw, pa), label(pl), text(pt)
	{
	}

	void SetValue(const char* array s)
	{
		text = s;
		changed = true;
	}
};

class FloatField : public RegularField
{
	const char* array null label;
	const char* array null units;
	uint8_t numDecimals;
	float val;

protected:
	void PrintText() const override;

public:
	FloatField(PixelNumber py, PixelNumber px, PixelNumber pw, TextAlignment pa, uint8_t pd, const char * array pl = NULL, const char * array null pu = NULL)
		: RegularField(py, px, pw, pa), label(pl), units(pu), numDecimals(pd), val(0.0)
	{
	}

	void SetValue(float v)
	{
		val = v;
		changed = true;
	}
};

class IntegerField : public RegularField
{
	const char* array null label;
	const char* array null units;
	int val;

protected:
	void PrintText() const override;

public:
	IntegerField(PixelNumber py, PixelNumber px, PixelNumber pw, TextAlignment pa, const char *pl = NULL, const char *pu = NULL)
		: RegularField(py, px, pw, pa), label(pl), units(pu), val(0.0)
	{
	}

	void SetValue(int v)
	{
		val = v;
		changed = true;
	}
};

class StaticTextField : public RegularField
{
	const char *text;

protected:
	void PrintText() const override;

public:
	StaticTextField(PixelNumber py, PixelNumber px, PixelNumber pw, TextAlignment pa, const char * array pt)
		: RegularField(py, px, pw,pa), text(pt) {}

	void SetValue(const char* array pt)
	{
		text = pt;
		changed = true;
	}
};

class TextButton : public Button
{
	const char *text;
	
protected:
	void PrintText() const override;

public:
	TextButton(PixelNumber py, PixelNumber px, PixelNumber pw, const char * array pt)
		: Button(py, px, pw), text(pt) {}

	void SetText(const char* array pt)
	{
		text = pt;
		changed = true;
	}
};

class IntegerButton : public Button
{
	const char* array null label;
	const char* array null units;
	int val;

protected:
	void PrintText() const override;

public:
	IntegerButton(PixelNumber py, PixelNumber px, PixelNumber pw, const char * array pl = NULL, const char * array pt = NULL)
		: Button(py, px, pw), label(pl), units(pt), val(0) {}

	int GetValue() const { return val; }

	void SetValue(int pv)
	{
		val = pv;
		changed = true;
	}

	void Increment(int amount)
	{
		val += amount;
		changed = true;
	}
};

class FloatButton : public Button
{
	const char * array null units;
	uint8_t numDecimals;
	float val;

protected:
	void PrintText() const override;

public:
	FloatButton(PixelNumber py, PixelNumber px, PixelNumber pw, uint8_t pd, const char * array pt = NULL)
		: Button(py, px, pw), units(pt), numDecimals(pd), val(0.0) {}

	float GetValue() const { return val; }

	void SetValue(float pv)
	{
		val = pv;
		changed = true;
	}

	void Increment(int amount)
	{
		val += amount;
		changed = true;
	}
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
	
	virtual PixelNumber GetHeight() const override { return height; }

	void SetPercent(uint8_t pc)
	{
		percent = pc;
		changed = true;
	}
};

#endif /* DISPLAY_H_ */