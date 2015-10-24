/*
 * Display.cpp
 *
 * Created: 04/11/2014 09:42:47
 *  Author: David
 */ 

#include "Display.hpp"
#undef array
#undef result
#include <algorithm>
#define array _ecv_array
#define result _ecv_result

extern UTFT lcd;

extern void WriteCommand(const char* array s);
extern void WriteCommand(int i);
extern void WriteCommand(char c);


// Static fields of class DisplayField
LcdFont DisplayField::defaultFont = NULL;
Colour DisplayField::defaultFcolour = white;
Colour DisplayField::defaultBcolour = black;
Colour DisplayField::defaultButtonBorderColour = black;
Colour DisplayField::defaultGradColour = 0;
Colour DisplayField::defaultPressedBackColour = black;
Colour DisplayField::defaultPressedGradColour = 0;

DisplayField::DisplayField(PixelNumber py, PixelNumber px, PixelNumber pw)
	: y(py), x(px), width(pw), fcolour(defaultFcolour), bcolour(defaultBcolour),
		changed(true), visible(true), next(NULL)
{
}

/*static*/ void DisplayField::SetDefaultColours(Colour pf, Colour pb, Colour pbb, Colour pg, Colour pbp, Colour pgp)
{
	defaultFcolour = pf;
	defaultBcolour = pb;
	defaultButtonBorderColour = pbb;
	defaultGradColour = pg;
	defaultPressedBackColour = pbp;
	defaultPressedGradColour = pgp;
}

void DisplayField::Show(bool v)
{
	if (visible != v)
	{
		visible = changed = v;
	}
}
	
// Find the best match to a touch event in a list of fields
DisplayField * null DisplayField::FindEvent(int x, int y, DisplayField * null p)
{	
	const int maxXerror = 8, maxYerror = 8;		// set these to how close we need to be
	int bestError = maxXerror + maxYerror;
	DisplayField * null best = NULL;
	while (p != NULL)
	{
		if (p->visible && p->GetEvent() != nullEvent)
		{
			int xError = (x < (int)p->GetMinX()) ? (int)p->GetMinX() - x
									: (x > (int)p->GetMaxX()) ? x - (int)p->GetMaxX()
										: 0;
			if (xError < maxXerror)
			{
				int yError = (y < (int)p->GetMinY()) ? (int)p->GetMinY() - y
										: (y > (int)p->GetMaxY()) ? y - (int)p->GetMaxY()
											: 0;
				if (yError < maxYerror && xError + yError < bestError)
				{
					bestError = xError + yError;
					best = p;
				}
			}
		}
		p = p->next;
	}
	return best;
}

void DisplayField::SetColours(Colour pf, Colour pb)
{
	if (fcolour != pf || bcolour != pb)
	{
		fcolour = pf;
		bcolour = pb;
		changed = true;	
	}
}

DisplayManager::DisplayManager()
	: root(NULL), popupField(NULL), backgroundColor(0)
{
}

void DisplayManager::Init(Colour bc)
{
	backgroundColor = bc;
	ClearAll();
}

void DisplayManager::ClearAll()
{
	lcd.fillScr(backgroundColor);
}

// Append a field to the list of displayed fields
void DisplayManager::AddField(DisplayField *d)
{
	d->next = root;
	root = d;
}

// Refresh all fields. If 'full' is true then we rewrite them all, else we just rewrite those that have changed.
void DisplayManager::RefreshAll(bool full)
{
	for (DisplayField * null pp = root; pp != NULL; pp = pp->next)
	{
		if (Visible(pp))
		{
			pp->Refresh(full, 0, 0);
		}		
	}
	if (HavePopup())
	{
		popupField->Refresh(full, popupX, popupY);
	}
}

bool DisplayManager::ObscuredByPopup(const DisplayField *p) const
{
	return HavePopup()
			&& p->GetMaxY() >= popupY && p->GetMinY() < popupY + popupField->GetHeight()
			&& p->GetMaxX() >= popupX && p->GetMinX() < popupX + popupField->GetWidth();
}

bool DisplayManager::Visible(const DisplayField *p) const
{
	return p->IsVisible() && !ObscuredByPopup(p);
}

// Get the field that has been touched, or null if we can't find one
DisplayField * null DisplayManager::FindEvent(PixelNumber x, PixelNumber y)
{
	return (HavePopup()) ? popupField->FindEvent((int)x - (int)popupX, (int)y - (int)popupY) : DisplayField::FindEvent((int)x, (int)y, root);
}

// Get the field that has been touched, but search only outside the popup
DisplayField * null DisplayManager::FindEventOutsidePopup(PixelNumber x, PixelNumber y)
{
	if (!HavePopup()) return NULL;
	
	DisplayField * null f = DisplayField::FindEvent((int)x, (int)y, root);
	return (f != NULL && Visible(f)) ? f : NULL;
}

void DisplayManager::SetPopup(PopupField * null p, PixelNumber px, PixelNumber py)
{
	if (popupField != p)
	{		
		if (popupField != NULL)
		{
			lcd.setColor(backgroundColor);
			lcd.fillRoundRect(popupX, popupY, popupX + popupField->GetWidth() - 1, popupY + popupField->GetHeight() - 1);
			
			// Re-display the background fields
			for (DisplayField * null pp = root; pp != NULL; pp = pp->next)
			{
				if (!Visible(pp) && pp->IsVisible())
				{
					pp->Refresh(true, 0, 0);
				}
			}		
		}
		popupField = p;
		if (p != NULL)
		{
			popupX = px;
			popupY = py;
			p->Refresh(true, popupX, popupY);
		}
	}
}

void DisplayManager::AttachPopup(PopupField * pp, DisplayField *p)
{
	const PixelNumber margin = 10;	// don't let the popup get too close to the screen edges where touch position is less reliable
	
	// Work out the Y coordinate to place the popup level with the field
	PixelNumber h = pp->GetHeight()/2;
	PixelNumber hy = (p->GetMinY() + p->GetMaxY() + 1)/2;
	PixelNumber y = (hy + h > lcd.getDisplayYSize() - margin) ? lcd.getDisplayYSize() - pp->GetHeight() - margin
					: (hy - h > margin) ? hy - h 
						: margin;
	
	PixelNumber x = (p->GetMaxX() + 5 + pp->GetWidth() < lcd.getDisplayXSize()) ? p->GetMaxX() + 5
						: p->GetMinX() - pp->GetWidth() - 5;
	SetPopup(pp, x, y);
}

// Draw an outline around a field. The field and 1 pixel around it are assumed to be visible.
// Not sure what will happen of the field goes right up to one of the edges of the display! better avoid that situation.
void DisplayManager::Outline(DisplayField *f, Colour c, PixelNumber numPixels)
{
	lcd.setColor(c);
	for (PixelNumber i = 1; i <= numPixels; ++i)
	{
		lcd.drawRect(f->GetMinX() - i, f->GetMinY() - i, f->GetMaxX() + i, f->GetMaxY() + i);	
	}
}

void DisplayManager::Show(DisplayField *f, bool v)
{
	if (f->IsVisible() != v)
	{
		f->Show(v);

		if (!ObscuredByPopup(f))
		{
			// Check whether the field is currently in the display list, if so then show or hide it
			for (DisplayField *p = root; p != NULL; p = p->next)
			{
				if (p == f)
				{
					if (v)
					{
						f->Refresh(true, 0, 0);
					}
					else
					{
						lcd.setColor(backgroundColor);
						lcd.fillRect(f->GetMinX(), f->GetMinY(), f->GetMaxX(), f->GetMaxY());
					}
					break;
				}
			}
		}
	}
}

void DisplayManager::Press(Button *f, bool v)
{
	f->Press(v);

	if (HavePopup())
	{
		for (DisplayField *p = popupField->GetRoot(); p != NULL; p = p->next)
		{
			if (p == f)
			{
				f->Refresh(true, popupX, popupY);
				return;
			}
		}
	}

	if (!ObscuredByPopup(f))
	{
		for (DisplayField *p = root; p != NULL; p = p->next)
		if (p == f)
		{
			f->Refresh(true, 0, 0);
			return;
		}
	}	
}

void FieldWithText::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (full || changed)
	{
		lcd.setFont(font);
		lcd.setColor(fcolour);
		lcd.setBackColor(bcolour);
		lcd.setTextPos(x + xOffset, y + yOffset, x + xOffset + width);
		if (align == TextAlignment::Left)
		{
			PrintText();
			lcd.clearToMargin();
		}
		else
		{
			lcd.clearToMargin();
			lcd.setTextPos(0, 9999, width);
			PrintText();    // dummy print to get text width
			PixelNumber spare = width - lcd.getTextX();
			lcd.setTextPos(x + xOffset + ((align == TextAlignment::Centre) ? spare/2 : spare), y + yOffset, x + xOffset + width);
			PrintText();
		}
		changed = false;
	}
}

void TextField::PrintText() const
{
	if (label != NULL)
	{
		lcd.print(label);
	}
	if (text != NULL)
	{
		lcd.print(text);
	}
}

void FloatField::PrintText() const
{
	if (label != NULL)
	{
		lcd.print(label);
	}
	lcd.print(val, numDecimals);
	if (units != NULL)
	{
		lcd.print(units);
	}
}

void IntegerField::PrintText() const
{
	if (label != NULL)
	{
		lcd.print(label);
	}
	lcd.print(val);
	if (units != NULL)
	{
		lcd.print(units);
	}
}

void StaticTextField::PrintText() const
{
	lcd.print(text);
}

Button::Button(PixelNumber py, PixelNumber px, PixelNumber pw)
	: DisplayField(py, px, pw), borderColour(defaultButtonBorderColour), gradColour(defaultGradColour),
	  pressedBackColour(defaultPressedBackColour), pressedGradColour(defaultPressedGradColour), evt(nullEvent),
	  pressed(false)
{
	param.sParam = NULL;
}

void Button::DrawOutline(PixelNumber xOffset, PixelNumber yOffset) const
{
	lcd.setColor((pressed) ? pressedBackColour : bcolour);
	// Note that we draw the filled rounded rectangle with the full width but 2 pixels less height than the border.
	// This means that we start with the requested colour inside the border.
	lcd.fillRoundRect(x + xOffset, y + yOffset + 1, x + xOffset + width - 1, y + yOffset + GetHeight() - 2,
	(pressed) ? pressedGradColour : gradColour, buttonGradStep);
	lcd.setColor(borderColour);
	lcd.drawRoundRect(x + xOffset, y + yOffset, x + xOffset + width - 1, y + yOffset + GetHeight() - 1);	
}

void ButtonWithText::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (full || changed)
	{
		DrawOutline(xOffset, yOffset);
		lcd.setTransparentBackground(true);
		lcd.setColor(fcolour);
		lcd.setFont(font);
		lcd.setTextPos(0, 9999, width - 6);
		PrintText();							// dummy print to get text width
		PixelNumber spare = width - 6 - lcd.getTextX();
		lcd.setTextPos(x + xOffset + 3 + spare/2, y + yOffset + 2, x + xOffset + width - 3);	// text is always centre-aligned
		PrintText();
		lcd.setTransparentBackground(false);
		changed = false;
	}
}

void TextButton::PrintText() const
{
	lcd.print(text);
}

void IconButton::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (full || changed)
	{
		DrawOutline(xOffset, yOffset);
		const uint16_t sx = GetIconWidth(icon), sy = GetIconHeight(icon);
		lcd.drawBitmap(x + (width - sx)/2, y + (GetHeight() - sy)/2, sx, sy, GetIconData(icon));
		changed = false;
	}
}

void IntegerButton::PrintText() const
{
	if (label != NULL)
	{
		lcd.print(label);
	}
	lcd.print(val);
	if (units != NULL)
	{
		lcd.print(units);
	}
}

void FloatButton::PrintText() const
{
	lcd.print(val, numDecimals);
	if (units != NULL)
	{
		lcd.print(units);
	}
}

void ProgressBar::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (full || changed)
	{
		PixelNumber pixelsSet = ((width - 2) * percent)/100;
		if (full)
		{
			lcd.setColor(fcolour);
			lcd.drawLine(x + xOffset, y, x + xOffset + width - 1, y + yOffset);
			lcd.drawLine(x + xOffset, y + yOffset + height - 1, x + xOffset + width - 1, y + yOffset + height - 1);
			lcd.drawLine(x + xOffset + width - 1, y + yOffset + 1, x + xOffset + width - 1, y + yOffset + height - 2);

			lcd.fillRect(x + xOffset, y + yOffset + 1, x + xOffset + pixelsSet, y + yOffset + height - 2);
			if (pixelsSet < width - 2)
			{
				lcd.setColor(bcolour);
				lcd.fillRect(x + xOffset + pixelsSet + 1, y + yOffset + 1, x + xOffset + width - 2, y + yOffset + height - 2);
			}
		}
		else if (pixelsSet > lastNumPixelsSet)
		{
			lcd.setColor(fcolour);
			lcd.fillRect(x + xOffset + lastNumPixelsSet, y + yOffset + 1, x + xOffset + pixelsSet, y + yOffset + height - 2);
		}
		else if (pixelsSet < lastNumPixelsSet)
		{
			lcd.setColor(bcolour);
			lcd.fillRect(x + xOffset + pixelsSet + 1, y + yOffset + 1, x + xOffset + lastNumPixelsSet, y + yOffset + height - 2);	
		}
		changed = false;
		lastNumPixelsSet = pixelsSet;
	}
}

PopupField::PopupField(PixelNumber ph, PixelNumber pw, Colour pb)
	: root(NULL), height(ph), width(pw), backgroundColour(pb)
{
}

void PopupField::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (full)
	{
		// Draw a rectangle inside the border
		lcd.setColor(green);
		lcd.fillRoundRect(xOffset, yOffset + 1, xOffset + width - 1, yOffset + height - 2);

		// Draw a black border
		lcd.setColor(black);
		lcd.drawRoundRect(xOffset, yOffset, xOffset + width - 1, yOffset + height - 1);
	}
	
	for (DisplayField * null p = root; p != NULL; p = p->next)
	{
		p->Refresh(full, xOffset, yOffset);
	}
}

void PopupField::AddField(DisplayField *p)
{
	p->next = root;
	root = p;	
}

DisplayField *PopupField::FindEvent(int px, int py)
{
	return DisplayField::FindEvent(px, py, root);
}

// End
