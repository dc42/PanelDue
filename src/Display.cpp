/*
 * Display.cpp
 *
 * Created: 04/11/2014 09:42:47
 *  Author: David
 */ 

#include "Display.hpp"
#include <algorithm>

extern UTFT lcd;
extern uint8_t glcd19x20[];

extern void WriteCommand(const char* array s);
extern void WriteCommand(int i);
extern void WriteCommand(char c);


// Static fields of class DisplayField
LcdFont DisplayField::defaultFont = glcd19x20;
Color DisplayField::defaultFcolour = 0xFFFF;
Color DisplayField::defaultBcolour = 0;

DisplayField::DisplayField(PixelNumber py, PixelNumber px, PixelNumber pw)
	: y(py), x(px), width(pw), fcolour(defaultFcolour), bcolour(defaultBcolour),
		evt(nullEvent), font(defaultFont), changed(true), visible(true), next(NULL)
{
	param.sParam = NULL;
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
	const int maxXerror = 10, maxYerror = 10;		// set these to how close we need to be
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

void DisplayField::SetColours(Color pf, Color pb)
{
	if (fcolour != pf || bcolour != pb)
	{
		fcolour = pf;
		bcolour = pb;
		changed = true;	
	}
}

DisplayManager::DisplayManager()
	: backgroundColor(0), root(NULL), popupField(NULL)
{
}

void DisplayManager::Init(Color bc)
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

bool DisplayManager::Visible(const DisplayField *p) const
{
	return p->IsVisible() &&
			(    !HavePopup()
			  || (   p->GetMaxY() < popupY || p->GetMinY() >= popupY + popupField->GetHeight()
				  || p->GetMaxX() < popupX || p->GetMinX() >= popupX + popupField->GetWidth()
			     )
			);
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
			lcd.fillRect(popupX, popupY, popupX + popupField->GetWidth() - 1, popupY + popupField->GetHeight() - 1);
			
			// Re-display the background fields
			for (DisplayField * null pp = root; pp != NULL; pp = pp->next)
			{
				if (!Visible(pp))
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
void DisplayManager::Outline(DisplayField *f, Color c, PixelNumber numPixels)
{
	lcd.setColor(c);
	for (PixelNumber i = 1; i <= numPixels; ++i)
	{
		lcd.drawRect(f->GetMinX() - i, f->GetMinY() - i, f->GetMaxX() + i, f->GetMaxY() + i);	
	}
}

//TODO: make this work properly when field f is obscured by a popup
void DisplayManager::Show(DisplayField *f, bool v)
{
	if (f->IsVisible() != v)
	{
		f->Show(v);

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

// Set the font and colours, print the label (if any) and leave the cursor at the correct position for the data
void LabelledField::DoLabel(bool full, PixelNumber xOffset, PixelNumber yOffset)
pre(full || changed)
{
	lcd.setFont(font);
	lcd.setColor(fcolour);
	lcd.setBackColor(bcolour);
	if (label != NULL && (full || changed))
	{
		lcd.setTextPos(x + xOffset, y + yOffset, x + xOffset + width);
		lcd.print(label);
		labelColumns = lcd.getTextX() + 1 - x - xOffset;
	}
	else
	{
		lcd.setTextPos(x + xOffset + labelColumns, y + yOffset, x + xOffset + width);
	}
}

void TextField::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (full || changed)
	{
		DoLabel(full, xOffset, yOffset);
		lcd.print(text);
		lcd.clearToMargin();
		changed = false;
	}
}

void FloatField::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (full || changed)
	{
		DoLabel(full, xOffset, yOffset);
		lcd.setTranslation(".", "\x80");
		lcd.print(val, numDecimals);
		lcd.setTranslation(NULL, NULL);
		if (units != NULL)
		{
			lcd.print(units);
		}
		lcd.clearToMargin();
		changed = false;
	}
}

void IntegerField::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (full || changed)
	{
		DoLabel(full, xOffset, yOffset);
		lcd.setTranslation(".", "\x16");
		lcd.print(val);
		lcd.setTranslation(NULL, NULL);
		if (units != NULL)
		{
			lcd.print(units);
		}
		lcd.clearToMargin();
		changed = false;
	}
}

void StaticTextField::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (full || changed)
	{
		lcd.setFont(font);
		lcd.setColor(fcolour);
		lcd.setBackColor(bcolour);
		lcd.setTextPos(x + xOffset, y + yOffset, x + xOffset + width);
		if (align == Left)
		{
			lcd.print(text);
			lcd.clearToMargin();
		}
		else
		{
			lcd.clearToMargin();
			lcd.setTextPos(0, 9999, width);
			lcd.print(text);    // dummy print to get text width
			PixelNumber spare = width - lcd.getTextX();
			lcd.setTextPos(x + xOffset + ((align == Centre) ? spare/2 : spare), y + yOffset, x + xOffset + width);
			lcd.print(text);
		}
		changed = false;
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

PopupField::PopupField(PixelNumber ph, PixelNumber pw, Color pb)
	: height(ph), width(pw), backgroundColour(pb), root(NULL)
{
}

void PopupField::Refresh(bool full, PixelNumber xOffset, PixelNumber yOffset)
{
	if (full)
	{
		// Draw a black border
		lcd.setColor(black);
		lcd.drawRect(xOffset, yOffset, xOffset + width - 1, yOffset + height - 1);
		
		// Draw a rectangle inside that one
		lcd.setColor(green);
		lcd.fillRect(xOffset + 1, yOffset + 1, xOffset + width - 2, yOffset + height - 2);
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
