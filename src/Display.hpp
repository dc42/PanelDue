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

// Fonts are held as arrays of 8-bit data in flash.
typedef const uint8_t * array LcdFont;

// An icon is stored an array of uint16_t data normally held in flash memory. The first value is the width in pixels, the second is the height in pixels.
// After that comes the icon data, 16 bits per pixel, one row at a time.
typedef const uint16_t * array Icon;

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

const uint8_t buttonGradStep = 12;

typedef uint16_t PixelNumber;
typedef uint8_t event_t;
const event_t nullEvent = 0;

enum class TextAlignment : uint8_t { Left, Centre, Right };
	
// Base class for a displayable field
class DisplayField
{
protected:
	PixelNumber y, x;							// Coordinates of top left pixel, counting from the top left corner
	PixelNumber width;							// number of pixels occupied in each direction
	Colour fcolour, bcolour;					// foreground and background colours
	bool changed;
	bool visible;
	
	static LcdFont defaultFont;
	static Colour defaultFcolour, defaultBcolour;
	static Colour defaultButtonBorderColour, defaultGradColour, defaultPressedBackColour, defaultPressedGradColour;
	
protected:
	DisplayField(PixelNumber py, PixelNumber px, PixelNumber pw);
	
	virtual PixelNumber GetHeight() const { return 1; }		// would like to make this pure virtual but then we get 50K of library that we don't want

public:
	DisplayField * null next;					// link to next field in list

	virtual bool IsButton() const { return false; }
	bool IsVisible() const { return visible; }
	void Show(bool v);
	virtual void Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset) { }		// would like to make this pure virtual but then we get 50K of library that we don't want
	void SetColours(Colour pf, Colour pb);
	void SetChanged() { changed = true; }
	PixelNumber GetMinX() const { return x; }
	PixelNumber GetMaxX() const { return x + width - 1; }
	PixelNumber GetMinY() const { return y; }
	PixelNumber GetMaxY() const { return y + GetHeight() - 1; }
		
	virtual event_t GetEvent() const { return nullEvent; }

	static void SetDefaultColours(Colour pf, Colour pb) { defaultFcolour = pf; defaultBcolour = pb; }
	static void SetDefaultColours(Colour pf, Colour pb, Colour pbb, Colour pg, Colour pbp, Colour pgp);
	static void SetDefaultFont(LcdFont pf) { defaultFont = pf; }
	static DisplayField * null FindEvent(int x, int y, DisplayField * null p);
	
	// Icon management
	static PixelNumber GetIconWidth(Icon ic) { return ic[0]; }
	static PixelNumber GetIconHeight(Icon ic) { return ic[1]; }
	static const uint16_t * array GetIconData(Icon ic) { return ic + 2; }
};

class PopupField
{
private:
	DisplayField * null root;
	PixelNumber height, width;
	Colour backgroundColour;
	
public:
	PopupField(PixelNumber ph, PixelNumber pw, Colour pb);
	PixelNumber GetHeight() const { return height; }
	PixelNumber GetWidth() const { return width; }
	void Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset);
	void AddField(DisplayField *p);
	DisplayField * null FindEvent(int x, int y);
	DisplayField * null GetRoot() const { return root; }
	void Redraw(DisplayField *f, PixelNumber xOffset, PixelNumber yOffset);
};

class Button;

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
	DisplayField * null root;
	PopupField * null popupField;
	PixelNumber popupX, popupY;
	Colour backgroundColor;
};

// Base class for fields displaying text
class FieldWithText : public DisplayField
{
	LcdFont font;
	TextAlignment align;
	
protected:
	PixelNumber GetHeight() const override { return UTFT::GetFontHeight(font); }
	
	virtual void PrintText() const {}		// would ideally be pure virtual

	FieldWithText(PixelNumber py, PixelNumber px, PixelNumber pw, TextAlignment pa)
		: DisplayField(py, px, pw), font(DisplayField::defaultFont), align(pa)
	{
	}
		
public:
	void Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset) override final;
};
	
class TextField : public FieldWithText
{
	const char* array null label;
	const char* array null text;
	
protected:
	void PrintText() const override;

public:
	TextField(PixelNumber py, PixelNumber px, PixelNumber pw, TextAlignment pa, const char * array null pl, const char* array null pt = nullptr)
		: FieldWithText(py, px, pw, pa), label(pl), text(pt)
	{
	}

	void SetValue(const char* array s)
	{
		text = s;
		changed = true;
	}

	void SetLabel(const char* array s)
	{
		label = s;
		changed = true;
	}
};

class FloatField : public FieldWithText
{
	const char* array null label;
	const char* array null units;
	float val;
	uint8_t numDecimals;

protected:
	void PrintText() const override;

public:
	FloatField(PixelNumber py, PixelNumber px, PixelNumber pw, TextAlignment pa, uint8_t pd, const char * array pl = NULL, const char * array null pu = NULL)
		: FieldWithText(py, px, pw, pa), label(pl), units(pu), val(0.0), numDecimals(pd)
	{
	}

	void SetValue(float v)
	{
		val = v;
		changed = true;
	}
};

class IntegerField : public FieldWithText
{
	const char* array null label;
	const char* array null units;
	int val;

protected:
	void PrintText() const override;

public:
	IntegerField(PixelNumber py, PixelNumber px, PixelNumber pw, TextAlignment pa, const char *pl = NULL, const char *pu = NULL)
		: FieldWithText(py, px, pw, pa), label(pl), units(pu), val(0)
	{
	}

	void SetValue(int v)
	{
		val = v;
		changed = true;
	}
};

class StaticTextField : public FieldWithText
{
	const char *text;

protected:
	void PrintText() const override;

public:
	StaticTextField(PixelNumber py, PixelNumber px, PixelNumber pw, TextAlignment pa, const char * array pt)
		: FieldWithText(py, px, pw,pa), text(pt) {}

	void SetValue(const char* array pt)
	{
		text = pt;
		changed = true;
	}
};

class Button : public DisplayField
{
	union
	{
		const char* null sParam;
		int iParam;
		//float fParam;
	} param;

	Colour borderColour, gradColour, pressedBackColour, pressedGradColour;
	event_t evt;								// event number that is triggered by touching this field
	bool pressed;

protected:
	Button(PixelNumber py, PixelNumber px, PixelNumber pw);
	
	void DrawOutline(PixelNumber xOffset, PixelNumber yOffset) const;
	
	static PixelNumber textMargin;
	static PixelNumber iconMargin;

public:
	bool IsButton() const override final { return true; }

	void SetEvent(event_t e, const char* null sp ) { evt = e; param.sParam = sp; }
	void SetEvent(event_t e, int ip ) { evt = e; param.iParam = ip; }
	//void SetEvent(event_t e, float fp ) { evt = e; param.fParam = fp; }

	event_t GetEvent() const override { return evt; }

	const char* null GetSParam() const { return param.sParam; }
	int GetIParam() const { return param.iParam; }
	//float GetFParam() const { return param.fParam; }

	void Press(bool p)
	{
		if (p != pressed)
		{
			pressed = p;
			changed = true;
		}
	}
	
	static void SetTextMargin(PixelNumber p) { textMargin = p; }
	static void SetIconMargin(PixelNumber p) { iconMargin = p; }
};

class ButtonWithText : public Button
{
	LcdFont font;
	
protected:
	PixelNumber GetHeight() const override { return UTFT::GetFontHeight(font) + 2 * textMargin + 2; }

	virtual void PrintText() const {}		// ideally would be pure virtual

public:
	ButtonWithText(PixelNumber py, PixelNumber px, PixelNumber pw)
		: Button(py, px, pw), font(DisplayField::defaultFont) {}

	void Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset) override final;	
};

class CharButton : public ButtonWithText
{
	LcdFont font;
	char c;

public:
	CharButton(PixelNumber py, PixelNumber px, PixelNumber pw, char pc, event_t e);

protected:	
	void PrintText() const override;
};

class TextButton : public ButtonWithText
{
	const char *text;
	
protected:
	void PrintText() const override;

public:
	TextButton(PixelNumber py, PixelNumber px, PixelNumber pw, const char * array pt);
	TextButton(PixelNumber py, PixelNumber px, PixelNumber pw, const char * array pt, event_t e, int param);
	TextButton(PixelNumber py, PixelNumber px, PixelNumber pw, const char * array pt, event_t e, const char * array param);

	void SetText(const char* array pt)
	{
		text = pt;
		changed = true;
	}
};

class IconButton : public Button
{
	Icon icon;
	
protected:
	PixelNumber GetHeight() const override { return GetIconHeight(icon) + 2 * iconMargin + 2; }

public:
	IconButton(PixelNumber py, PixelNumber px, PixelNumber pw, Icon ic)
		: Button(py, px, pw), icon(ic) {}

	void Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset) override final;
};

class IntegerButton : public ButtonWithText
{
	const char* array null label;
	const char* array null units;
	int val;

protected:
	void PrintText() const override;

public:
	IntegerButton(PixelNumber py, PixelNumber px, PixelNumber pw, const char * array pl = NULL, const char * array pt = NULL)
		: ButtonWithText(py, px, pw), label(pl), units(pt), val(0) {}

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

class FloatButton : public ButtonWithText
{
	const char * array null units;
	float val;
	uint8_t numDecimals;

protected:
	void PrintText() const override;

public:
	FloatButton(PixelNumber py, PixelNumber px, PixelNumber pw, uint8_t pd, const char * array pt = NULL)
		: ButtonWithText(py, px, pw), units(pt), val(0.0), numDecimals(pd) {}

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
	PixelNumber lastNumPixelsSet;
	uint8_t height;
	uint8_t percent;
public:
	ProgressBar(uint16_t py, uint16_t px, uint8_t ph, uint16_t pw)
		: DisplayField(py, px, pw), lastNumPixelsSet(0), height(ph), percent(0)
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