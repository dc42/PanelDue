// Controller for Ormerod to run on SAM3S2B
// Coding rules:
//
// 1. Must compile with no g++ warnings, when all warning are enabled..
// 2. Dynamic memory allocation using 'new' is permitted during the initialization phase only. No use of 'new' anywhere else,
//    and no use of 'delete', 'malloc' or 'free' anywhere.
// 3. No pure virtual functions. This is because in release builds, having pure virtual functions causes huge amounts of the C++ library to be linked in
//    (possibly because it wants to print a message if a pure virtual function is called).

#include "asf.h"
#include "Mem.hpp"
#include "Display.hpp"
#include "UTFT.hpp"
#include "UTouch.hpp"
#include "SerialIo.hpp"
#include "Buzzer.hpp"
#include "SysTick.hpp"
#include "Misc.hpp"
#include "Vector.hpp"
#include "FlashStorage.hpp"
#include "PanelDue.hpp"
#include "Configuration.hpp"
#include "Fields.hpp"

UTFT lcd(DISPLAY_CONTROLLER, TMode16bit, 16, 17, 18, 19);

UTouch touch(23, 24, 22, 21, 20);
DisplayManager mgr;

const uint32_t printerPollInterval = 2000;			// poll interval in milliseconds
const uint32_t printerPollTimeout = 5000;			// poll timeout in milliseconds
const uint32_t touchBeepLength = 20;				// beep length in ms
const uint32_t touchBeepFrequency = 4500;			// beep frequency in Hz. Resonant frequency of the piezo sounder is 4.5kHz.
const uint32_t longTouchDelay = 250;				// how long we ignore new touches for after pressing Set
const uint32_t shortTouchDelay = 100;				// how long we ignore new touches while pressing up/down, to get a reasonable repeat rate

static uint32_t lastTouchTime;
static uint32_t ignoreTouchTime;
static uint32_t lastResponseTime;
static uint32_t fileScrollOffset = 0;
static bool fileListChanged = true;
static bool gotMachineName = false;
static bool isDelta = false;
static bool gotGeometry = false;
static bool axisHomed[3] = {false, false, false};
static bool allAxesHomed = false;
static int beepFrequency = 0, beepLength = 0;
static unsigned int numHeads = 1;

Vector<char, 2048> fileList;						// we use a Vector instead of a String because we store multiple null-terminated strings in it
Vector<const char* array, 100> fileIndex;			// pointers into the individual filenames in the list
static const char* array null currentFile = NULL;	// file whose info is displayed in the popup menu

static OneBitPort BacklightPort(33);				// PB1 (aka port 33) controls the backlight on the prototype

struct FlashData
{
	static const uint32_t magicVal = 0x3AB629D1;
	static const uint32_t muggleVal = 0xFFFFFFFF;

	uint32_t magic;
	uint32_t baudRate;
	int16_t xmin;
	int16_t xmax;
	int16_t ymin;
	int16_t ymax;
	DisplayOrientation lcdOrientation;
	DisplayOrientation touchOrientation;
	uint32_t touchVolume;
	
	FlashData() : magic(muggleVal) { }
	bool operator==(const FlashData& other);
	bool Valid() const { return magic == magicVal; }
	void SetInvalid() { magic = muggleVal; }
	void SetDefaults();
	void Load();
	void Save() const;
};

// When we can switch to C++11, uncomment this line
//static_assert(sizeof(FlashData) <= FLASH_DATA_LENGTH);

FlashData nvData, savedNvData;

enum PrinterStatus
{
	psUnknown = 0,
	psIdle = 1,
	psPrinting = 2,
	psStopped = 3,
	psConfiguring = 4
};

static PrinterStatus status = psUnknown;

static Event eventToConfirm = nullEvent;

//char commandBuffer[80];

const size_t numHeaters = 3;
int heaterStatus[numHeaters];

bool FlashData::operator==(const FlashData& other)
{
	return magic == other.magic
		&& baudRate == other.baudRate
		&& xmin == other.xmin
		&& xmax == other.xmax
		&& ymin == other.ymin
		&& ymax == other.ymax
		&& lcdOrientation == other.lcdOrientation
		&& touchOrientation == other.touchOrientation
		&& touchVolume == other.touchVolume;
}

void FlashData::SetDefaults()
{
	baudRate = DEFAULT_BAUD_RATE;
	xmin = 0;
	xmax = DisplayX - 1;
	ymin = 0;
	ymax = DisplayY - 1;
	lcdOrientation = DefaultDisplayOrientAdjust;
	touchOrientation = DefaultTouchOrientAdjust;
	touchVolume = Buzzer::DefaultVolume;
	magic = magicVal;
}

void FlashData::Load()
{
	FlashStorage::read(0, &nvData, sizeof(nvData));
}

void FlashData::Save() const
{
	FlashStorage::write(0, this, sizeof(*this));
}

bool StringGreaterThan(const char* a, const char* b)
{
	return stricmp(a, b) > 0;
}

void RefreshFileList()
{
	// 1. Sort the file list
	fileIndex.sort(StringGreaterThan);
	
	// 2. Make sure the scroll position is still sensible
	if (fileScrollOffset >= fileIndex.size())
	{
		fileScrollOffset = (fileIndex.size()/numFileRows) * numFileRows;
	}
	
	// 3. Display the scroll buttons if needed
	mgr.Show(scrollFilesLeftField, fileScrollOffset != 0);
	mgr.Show(scrollFilesRightField, fileScrollOffset + (numFileRows * numFileColumns) < fileIndex.size());
	
	// 4. Display the file list
	for (size_t i = 0; i < numDisplayedFiles; ++i)
	{
		StaticTextField *f = filenameFields[i];
		if (i + fileScrollOffset < fileIndex.size())
		{
			f->SetValue(fileIndex[i + fileScrollOffset]);
			f->SetColours(white, selectableBackColor);
			f->SetEvent(evFile, i + fileScrollOffset);
		}
		else
		{
			f->SetValue("");
			f->SetColours(white, defaultBackColor);
			f->SetEvent(nullEvent, 0);
		}
	}
}

void ChangeTab(DisplayField *newTab)
{
	if (newTab != currentTab)
	{
		if (currentTab != NULL)
		{
			currentTab->SetColours(white, black);
		}
		newTab->SetColours(red, black);
		currentTab = newTab;
		switch(newTab->GetEvent())
		{
		case evTabControl:
			mgr.SetRoot(controlRoot);
			break;
		case evTabPrint:
			mgr.SetRoot(printRoot);
			break;
		case evTabFiles:
			SerialIo::SendString("M20 S2\n");		// ask for the list of files
			mgr.SetRoot(filesRoot);
			break;
		case evTabMsg:
			mgr.SetRoot(messageRoot);
			break;
		case evTabInfo:
			mgr.SetRoot(infoRoot);
			break;
		default:
			mgr.SetRoot(commonRoot);
			break;
		}
		mgr.ClearAll();
	}
	mgr.RefreshAll(true);
}

void InitLcd(DisplayOrientation dor)
{
	lcd.InitLCD(dor);				// set up the LCD
	Fields::CreateFields();			// create all the fields
	mgr.RefreshAll(true);			// redraw everything

	currentTab = NULL;
}

// Ignore touches for a long time
void DelayTouchLong()
{
	lastTouchTime = GetTickCount();
	ignoreTouchTime = longTouchDelay;
}

// Ignore touches for a short time instead of the long time we already asked for
void ShortenTouchDelay()
{
	ignoreTouchTime = shortTouchDelay;
}

void TouchBeep()
{
	Buzzer::Beep(touchBeepFrequency, touchBeepLength, nvData.touchVolume);	
}

// Draw a spot and wait until the user touches it, returning the touch coordinates in tx and ty.
// The alternative X and Y locations are so that the caller can allow for the touch panel being possibly inverted.
void DoTouchCalib(PixelNumber x, PixelNumber y, PixelNumber altX, PixelNumber altY, uint16_t& tx, uint16_t& ty)
{
	const PixelNumber touchCircleRadius = 8;
	const PixelNumber touchCalibMaxError = 40;
	
	lcd.setColor(white);
	lcd.fillCircle(x, y, touchCircleRadius);
	
	for (;;)
	{
		if (touch.read(tx, ty))
		{
			if (   (abs((int)tx - (int)x) <= touchCalibMaxError || abs((int)tx - (int)altX) <= touchCalibMaxError)
				&& (abs((int)ty - (int)y) <= touchCalibMaxError || abs((int)ty - (int)altY) <= touchCalibMaxError)
			   ) 
			{
				TouchBeep();
				break;
			}
		}
	}
	
	lcd.setColor(defaultBackColor);
	lcd.fillCircle(x, y, touchCircleRadius);
}

void CalibrateTouch()
{
	const PixelNumber touchCalibMargin = 15;

	DisplayField *oldRoot = mgr.GetRoot();
	touchCalibInstruction->SetValue("Touch the spot");				// in case the user didn't need to press the reset button last time
	mgr.SetRoot(touchCalibInstruction);
	mgr.ClearAll();
	mgr.RefreshAll(true);

	touch.init(DisplayX, DisplayY, DefaultTouchOrientAdjust);				// initialize the driver and clear any existing calibration

	// Draw spots on the edges of the screen, one at a time, and ask the user to touch them.
	// For the first two, we allow for the touch panel being the wrong way round.
	uint16_t xLow, xHigh, yLow, yHigh, dummy;
	DoTouchCalib(DisplayX/2, touchCalibMargin, DisplayX/2, DisplayY - 1 - touchCalibMargin,	dummy, yLow);
	if (yLow > DisplayY/2)
	{
		touch.adjustOrientation(ReverseY);
		yLow = DisplayY - 1 - yLow;
	}
	DoTouchCalib(DisplayX - touchCalibMargin - 1, DisplayY/2, touchCalibMargin, DisplayY/2, xHigh, dummy);
	if (xHigh < DisplayX/2)
	{
		touch.adjustOrientation(ReverseX);
		xHigh = DisplayX - 1 - xHigh;
	}
	DoTouchCalib(DisplayX/2, DisplayY - 1 - touchCalibMargin, DisplayX/2, DisplayY - 1 - touchCalibMargin, dummy, yHigh);
	DoTouchCalib(touchCalibMargin, DisplayY/2, touchCalibMargin, DisplayY/2, xLow, dummy);
	
	// Extrapolate the values we read to the edges of the screen
	nvData.xmin = (int)xLow - (int)touchCalibMargin;
	nvData.xmax = (int)xHigh + (int)touchCalibMargin;
	nvData.ymin = (int)yLow - (int)touchCalibMargin;
	nvData.ymax = (int)yHigh + (int)touchCalibMargin;
	nvData.touchOrientation = touch.getOrientation();
	touch.calibrate(nvData.xmin, nvData.xmax, nvData.ymin, nvData.ymax);
	
	mgr.SetRoot(oldRoot);
	mgr.ClearAll();
	mgr.RefreshAll(true);
}

void CheckSettingsAreSaved()
{
	Fields::SettingsAreSaved(nvData == savedNvData);
}

// Factory reset
void FactoryReset()
{
	while (Buzzer::Noisy()) { }
	nvData.SetInvalid();
	nvData.Save();
	savedNvData = nvData;
	Buzzer::Beep(touchBeepFrequency, 400, Buzzer::MaxVolume);			// long beep to acknowledge it
	while (Buzzer::Noisy()) { }
	rstc_start_software_reset(RSTC);									// reset the processor
}

// Save settings
void SaveSettings()
{
	while (Buzzer::Noisy()) { }
	nvData.Save();
	savedNvData = nvData;
	CheckSettingsAreSaved();
//	Buzzer::Beep(touchBeepFrequency, 400, Buzzer::MaxVolume);			// long beep to acknowledge it	
}

void PopupAreYouSure(Event ev, const char* text)
{
	eventToConfirm = ev;
	areYouSureTextField->SetValue(text);
	mgr.SetPopup(areYouSurePopup, (DisplayX - areYouSurePopupWidth)/2, (DisplayY - areYouSurePopupHeight)/2);
}

// Process a touch event
void ProcessTouch(DisplayField *f)
{
	Event ev = f->GetEvent();
	switch(ev)
	{
	case evTabControl:
	case evTabPrint:
	case evTabFiles:
	case evTabMsg:
	case evTabInfo:
		ChangeTab(f);
		break;

	case evAdjustTemp:
		if (static_cast<IntegerField*>(f)->GetValue() < 0)
		{
			static_cast<IntegerField*>(f)->SetValue(0);
		}
		// no break
	case evAdjustPercent:
		mgr.Outline(f, outlineColor, outlinePixels);
		mgr.SetPopup(setTempPopup, tempPopupX, tempPopupY);
		fieldBeingAdjusted = f;
		break;

	case evSetInt:
		if (fieldBeingAdjusted != NULL)
		{
			const char* null cmd = fieldBeingAdjusted->GetSParam();
			if (cmd != NULL)
			{
				SerialIo::SendString(cmd);
				SerialIo::SendInt(static_cast<const IntegerField*>(fieldBeingAdjusted)->GetValue());
				SerialIo::SendChar('\n');
			}
			mgr.SetPopup(NULL);
			mgr.RemoveOutline(fieldBeingAdjusted, outlinePixels);
			fieldBeingAdjusted = NULL;
		}
		break;

	case evAdjustInt:
		if (fieldBeingAdjusted != NULL)
		{
			static_cast<IntegerField*>(fieldBeingAdjusted)->Increment(f->GetIParam());
			ShortenTouchDelay();
		}
		break;

	case evAdjustPosition:
		if (fieldBeingAdjusted != NULL)
		{
			SerialIo::SendString("G91\n");
			SerialIo::SendString(fieldBeingAdjusted->GetSParam());
			SerialIo::SendString(f->GetSParam());
			SerialIo::SendString(" F6000\nG90\n");
		}
		break;

	case evCalTouch:
		CalibrateTouch();
		break;

	case evFactoryReset:
		PopupAreYouSure(ev, "Confirm factory reset");
		break;

	case evSaveSettings:
		SaveSettings();
		break;

	case evSelectHead:
		switch(f->GetIParam())
		{
		case 0:
			// There is no command to switch the bed to standby temperature, so we always set it to the active temperature
			SerialIo::SendString("M140 S");
			SerialIo::SendInt(bedActiveTemp->GetValue());
			SerialIo::SendChar('\n');
			break;
		
		case 1:
			SerialIo::SendString((heaterStatus[1] == 2) ? "T0\n" : "T1\n");
			break;
		
		case 2:
			SerialIo::SendString((heaterStatus[2] == 2) ? "T0\n" : "T2\n");
			break;
		
		default:
			break;
		}
		break;
		
	case evXYPos:
		mgr.Outline(f, outlineColor, outlinePixels);
		mgr.SetPopup(setXYPopup, xyPopupX, xyPopupY);
		fieldBeingAdjusted = f;
		break;

	case evZPos:
		mgr.Outline(f, outlineColor, outlinePixels);
		mgr.SetPopup(setZPopup, xyPopupX, xyPopupY);
		fieldBeingAdjusted = f;
		break;

	case evFile:
		currentFile = fileIndex[f->GetIParam()];
		SerialIo::SendString("M36 /gcodes/");			// ask for the file info
		SerialIo::SendString(currentFile);
		SerialIo::SendChar('\n');
		fpNameField->SetValue(currentFile);
		// Clear out the old field values, they relate to the previous file we looked at until we process the response
		fpSizeField->SetValue(0);						// would be better to make it blank
		fpHeightField->SetValue(0.0);					// would be better to make it blank
		fpLayerHeightField->SetValue(0.0);				// would be better to make it blank
		fpFilamentField->SetValue(0);					// would be better to make it blank
		generatedByText.clear();
		fpGeneratedByField->SetChanged();
		mgr.SetPopup(filePopup, (DisplayX - filePopupWidth)/2, (DisplayY - filePopupHeight)/2);
		break;

	case evPrint:
		mgr.SetPopup(NULL);
		if (currentFile != NULL)
		{
			SerialIo::SendString("M23 ");
			SerialIo::SendString(currentFile);
			SerialIo::SendString("\nM24\n");
			printingFile.copyFrom(currentFile);
			currentFile = NULL;							// allow the file list to be updated
			ChangeTab(tabPrint);
		}
		break;

	case evCancelPrint:
		mgr.SetPopup(NULL);
		currentFile = NULL;								// allow the file list to be updated
		break;

	case evDeleteFile:
		PopupAreYouSure(ev, "Confirm file delete");
		break;

	case evSendCommand:
		SerialIo::SendString(f->GetSParam());
		SerialIo::SendChar('\n');
		break;

	case evScrollFiles:
		fileScrollOffset += f->GetIParam();
		fileListChanged = true;
		ShortenTouchDelay();
		break;

	case evInvertDisplay:
		nvData.lcdOrientation = static_cast<DisplayOrientation>(nvData.lcdOrientation ^ (ReverseX | ReverseY | InvertText | InvertBitmap));
		lcd.InitLCD(nvData.lcdOrientation);
		CalibrateTouch();
		CheckSettingsAreSaved();
		break;

	case evSetBaudRate:
		mgr.Outline(f, outlineColor, outlinePixels);
		mgr.SetPopup(baudPopup, xyPopupX, xyPopupY);
		fieldBeingAdjusted = f;
		break;

	case evAdjustBaudRate:
		nvData.baudRate = f->GetIParam();
		SerialIo::Init(nvData.baudRate);
		baudRateField->SetValue(nvData.baudRate);
		CheckSettingsAreSaved();
		mgr.RemoveOutline(fieldBeingAdjusted, outlinePixels);
		mgr.SetPopup(NULL);
		fieldBeingAdjusted = NULL;
		break;

	case evSetVolume:
		mgr.Outline(f, outlineColor, outlinePixels);
		mgr.SetPopup(volumePopup, xyPopupX, xyPopupY);
		fieldBeingAdjusted = f;
		break;

	case evAdjustVolume:
		nvData.touchVolume = f->GetIParam();
		TouchBeep();									// give audible feedback of the touch at the new volume level
		CheckSettingsAreSaved();
		break;

	case evYes:
		switch (eventToConfirm)
		{
		case evFactoryReset:
			FactoryReset();
			break;

		case evDeleteFile:
			if (currentFile != NULL)
			{
				SerialIo::SendString("M30 ");
				SerialIo::SendString(currentFile);
				SerialIo::SendString("\nM20 S2\n");
				currentFile = NULL;
			}
			break;

		default:
			break;
		}
		eventToConfirm = nullEvent;
		currentFile = NULL;
		mgr.SetPopup(NULL);
		break;

	case evCancel:
		eventToConfirm = nullEvent;
		currentFile = NULL;
		mgr.SetPopup(NULL);
		break;

	default:
		break;
	}
}

// Process a touch event outside the popup on the field being adjusted
void ProcessTouchOutsidePopup()
{
	switch(fieldBeingAdjusted->GetEvent())
	{
	case evAdjustTemp:
	case evXYPos:
	case evZPos:
	case evSetBaudRate:
	case evSetVolume:
		mgr.RemoveOutline(fieldBeingAdjusted, outlinePixels);
		mgr.SetPopup(NULL);
		fieldBeingAdjusted = NULL;
		break;
	
	default:
		break;
	}
}

// Update an integer field, provided it isn't the one being adjusted
void UpdateField(IntegerField *f, int val)
{
	if (f != fieldBeingAdjusted)
	{
		f->SetValue(val);
	}
}

// Try to get an integer value from a string. if it is actually a floating point value, round it.
bool getInteger(const char s[], int &rslt)
{
	if (s[0] == 0) return false;			// empty string
	char* endptr;
	rslt = (int) strtol(s, &endptr, 10);
	if (*endptr == 0) return true;			// we parsed an integer
	double d = strtod(s, &endptr);			// try parsing a floating point number
	if (*endptr == 0)
	{
		rslt = (int)((d < 0.0) ? d - 0.5 : d + 0.5);
		return true;
	}
	return false;
}

// Try to get a floating point value from a string. if it is actually a floating point value, round it.
bool getFloat(const char s[], float &rslt)
{
	if (s[0] == 0) return false;			// empty string
	char* endptr;
	rslt = (float) strtod(s, &endptr);
	return *endptr == 0;					// we parsed an integer
}

// Public functions called by the SerialIo module
extern void processReceivedValue(const char id[], const char data[], int index)
{
	if (index >= 0)			// if this is an element of an array
	{
		if (strcmp(id, "active") == 0)
		{
			int ival;
			if (getInteger(data, ival))
			{
				switch(index)
				{
				case 0:
					UpdateField(bedActiveTemp, ival);
					break;
				case 1:
					UpdateField(t1ActiveTemp, ival);
					break;
				case 2:
					UpdateField(t2ActiveTemp, ival);
					break;
				default:
					break;
				}
			}
		}
		else if (strcmp(id, "standby") == 0)
		{
			int ival;
			if (getInteger(data, ival))
			{
				switch(index)
				{
				case 0:
					UpdateField(bedStandbyTemp, ival);
					break;
				case 1:
					UpdateField(t1StandbyTemp, ival);
					break;
				case 2:
					UpdateField(t2StandbyTemp, ival);
					break;
				default:
					break;
				}
			}
		}
		else if (strcmp(id, "heaters") == 0)
		{
			float fval;
			if (getFloat(data, fval))
			{
				switch(index)
				{
				case 0:
					bedCurrentTemp->SetValue(fval);
					break;
				case 1:
					t1CurrentTemp->SetValue(fval);
					break;
				case 2:
					t2CurrentTemp->SetValue(fval);
					if (numHeads == 1)
					{
						mgr.Show(t2CurrentTemp, true);
						mgr.Show(t2ActiveTemp, true);
						mgr.Show(t2StandbyTemp, true);
						++numHeads;
					}
					break;
				default:
					break;
				}
			}
		}
		else if (strcmp(id, "hstat") == 0)
		{
			int ival;
			if (getInteger(data, ival) && index >= 0 && index < (int)numHeaters)
			{
				heaterStatus[index] = ival;
				Color c = (ival == 1) ? standbyBackColor : (ival == 2) ? activeBackColor : (ival == 3) ? errorBackColour : defaultBackColor;
				switch(index)
				{
				case 0:
					bedCurrentTemp->SetColours(white, c);
					break;
				case 1:
					t1CurrentTemp->SetColours(white, c);
					break;
				case 2:
					t2CurrentTemp->SetColours(white, c);
					break;
				default:
					break;
				}
			}
		}
		else if (strcmp(id, "pos") == 0)
		{
			float fval;
			if (getFloat(data, fval))
			{
				switch(index)
				{
				case 0:
					xPos->SetValue(fval);
					break;
				case 1:
					yPos->SetValue(fval);
					break;
				case 2:
					zPos->SetValue(fval);
					break;
				default:
					break;
				}
			}
		}
		else if (strcmp(id, "efactor") == 0)
		{
			int ival;
			if (getInteger(data, ival))
			{
				switch(index)
				{
				case 0:
					UpdateField(e1Percent, ival);
					break;
				case 1:
					UpdateField(e2Percent, ival);
					break;
				default:
					break;	
				}
			}
		}
		else if (strcmp(id, "files") == 0)
		{
			static bool fileListLocked = false;
			if (index == 0)
			{
				if (currentFile == NULL)
				{
					fileList.clear();
					fileIndex.clear();
					fileListChanged = true;
					fileListLocked = false;
				}
				else				// don't destroy the file list if we are using a pointer into it
				{
					fileListLocked = true;
				}
			}
			if (!fileListLocked)
			{
				size_t len = strlen(data) + 1;		// we are going to copy the null terminator as well
				if (len + fileList.size() <= fileList.capacity() && fileIndex.size() < fileIndex.capacity())
				{
					fileIndex.add(fileList.c_ptr() + fileList.size());
					fileList.add(data, len);
				}
				fileListChanged = true;
			}
		}
		else if (strcmp(id, "filament") == 0)
		{
			static float totalFilament = 0.0;
			if (index == 0)
			{
				totalFilament = 0.0;
			}
			float f;
			if (getFloat(data, f))
			{
				totalFilament += f;
				fpFilamentField->SetValue((int)totalFilament);
			}
		}
		else if (strcmp(id, "homed") == 0)
		{
			int ival;
			if (index < 3 && getInteger(data, ival) && ival >= 0 && ival < 2) 
			{
				bool isHomed = (ival == 1);
				if (isHomed != axisHomed[index])
				{
					axisHomed[index] = isHomed;
					homeFields[index]->SetColours(white, (isHomed) ? homedBackColour : notHomedBackColour);
					bool allHomed = axisHomed[0] && axisHomed[1] && axisHomed[2];
					if (allHomed != allAxesHomed)
					{
						allAxesHomed = allHomed;
						homeAllField->SetColours(white, (allAxesHomed) ? homedBackColour : notHomedBackColour);
					}
				}
			}
		}
	}
	
	// Non-array values follow
	else if (strcmp(id, "sfactor") == 0)
	{
		int ival;
		if (getInteger(data, ival))
		{
			UpdateField(spd, ival);
		}
	}
	else if (strcmp(id, "probe") == 0)
	{
		zprobeBuf.copyFrom(data);
		zProbe->SetChanged();
	}
	else if (strcmp(id, "myName") == 0)
	{
		if (status != psConfiguring && status != psUnknown)
		{
			machineName.copyFrom(data);
			nameField->SetChanged();
			gotMachineName = true;		
		}
	}
	else if (strcmp(id, "fileName") == 0)
	{
		printingFile.copyFrom(data);
		printingField->SetChanged();
	}
	else if (strcmp(id, "size") == 0)
	{
		int sz;
		if (getInteger(data, sz))
		{
			fpSizeField->SetValue(sz);
		}
	}
	else if (strcmp(id, "height") == 0)
	{
		float f;
		if (getFloat(data, f))
		{
			fpHeightField->SetValue(f);
		}		
	}
	else if (strcmp(id, "layerHeight") == 0)
	{
		float f;
		if (getFloat(data, f))
		{
			fpLayerHeightField->SetValue(f);
		}	
	}
	else if (strcmp(id, "generatedBy") == 0)
	{
		generatedByText.copyFrom(data);
		fpGeneratedByField->SetChanged();
	}
	else if (strcmp(id, "fraction_printed") == 0)
	{
		float f;
		if (getFloat(data, f))
		{
			if (f >= 0.0 && f <= 1.0)
			{
				printProgressBar->SetPercent((uint8_t)((100.0 * f) + 0.5));
			}
		}
	}
	else if (strcmp(id, "status") == 0)
	{
		switch(data[0])
		{
		case 'C':
			status = psConfiguring;
			mgr.Show(printProgressBar, false);
			mgr.Show(printingField, false);
			break;
		case 'P':
			status = psPrinting;
			mgr.Show(printProgressBar, true);
			mgr.Show(printingField, true);
			break;
		case 'I':
			status = psIdle;
			mgr.Show(printProgressBar, false);
			mgr.Show(printingField, false);
			break;
		case 'S':
			status = psStopped;
			mgr.Show(printProgressBar, false);
			mgr.Show(printingField, false);
			break;
		default:
			status = psUnknown;
			break;
		}
	}
	else if (strcmp(id, "beep_freq") == 0)
	{
		getInteger(data, beepFrequency);
	}
	else if (strcmp(id, "beep_length") == 0)
	{
		getInteger(data, beepLength);
	}
	else if (strcmp(id, "geometry") == 0)
	{
		if (status != psConfiguring && status != psUnknown)
		{
			isDelta = (strcmp(data, "delta") == 0);
			gotGeometry = true;
			for (size_t i = 0; i < 3; ++i)
			{
				mgr.Show(homeFields[i], !isDelta);
			}
		}
	}
}

// Update those fields that display debug information
void updateDebugInfo()
{
	freeMem->SetValue(getFreeMemory());
}

void SelfTest()
{
	// Measure the 3.3V supply against the internal reference
	
	// Do internal and external loopback tests on the serial port

	// Initialize fields with the widest expected values so that we can make sure they fit
	bedCurrentTemp->SetValue(129.0);
	t1CurrentTemp->SetValue(299.0);
	t2CurrentTemp->SetValue(299.0);
	bedActiveTemp->SetValue(120);
	t1ActiveTemp->SetValue(280);
	t2ActiveTemp->SetValue(280);
	t1StandbyTemp->SetValue(280);
	t2StandbyTemp->SetValue(280);
	xPos->SetValue(220.9);
	yPos->SetValue(220.9);
	zPos->SetValue(199.99);
//	extrPos->SetValue(999.9);
	zProbe->SetValue("1023 (1023)");
//	fanRPM->SetValue(9999);
	spd->SetValue(169);
	e1Percent->SetValue(169);
	e2Percent->SetValue(169);
}

/**
 * \brief Application entry point.
 *
 * \return Unused (ANSI-C compatibility).
 */
int main(void)
{
    SystemInit();						// set up the clock etc.	
	wdt_disable(WDT);					// disable watchdog for now
	
	matrix_set_system_io(CCFG_SYSIO_SYSIO4 | CCFG_SYSIO_SYSIO5 | CCFG_SYSIO_SYSIO6 | CCFG_SYSIO_SYSIO7);	// enable PB4-PB7 pins
	pmc_enable_periph_clk(ID_PIOA);		// enable the PIO clock
	pmc_enable_periph_clk(ID_PIOB);		// enable the PIO clock
	pmc_enable_periph_clk(ID_PWM);		// enable the PWM clock
	pmc_enable_periph_clk(ID_UART1);	// enable UART1 clock
	
	Buzzer::Init();
	lastTouchTime = GetTickCount();
	
	SysTick_Config(SystemCoreClock / 1000);

	// On prototype boards we need to turn the backlight on
	BacklightPort.setMode(OneBitPort::Output);
	BacklightPort.setHigh();

	// Read parameters from flash memory
	nvData.Load();
	if (nvData.Valid())
	{
		// The touch panel has already been calibrated
		InitLcd(nvData.lcdOrientation);
		touch.init(DisplayX, DisplayY, nvData.touchOrientation);
		touch.calibrate(nvData.xmin, nvData.xmax, nvData.ymin, nvData.ymax);
	}
	else
	{
		// The touch panel has not been calibrated, and we do not know which way up it is
		nvData.SetDefaults();
		InitLcd(nvData.lcdOrientation);
		CalibrateTouch();					// this includes the touch driver initialization
		nvData.Save();
		savedNvData = nvData;
		CheckSettingsAreSaved();
	}
	
	SerialIo::Init(nvData.baudRate);
	baudRateField->SetValue(nvData.baudRate);

	uint32_t lastPollTime = GetTickCount() - printerPollInterval;
	
	// Hide the Head 2 parameters until we know we have a second head
	mgr.Show(t2CurrentTemp, false);
	mgr.Show(t2ActiveTemp, false);
	mgr.Show(t2StandbyTemp, false);
	
	mgr.Show(bedStandbyTemp, false);		// currently, we always hide the bed standby temperature because it doesn't do anything
	
	// Display the Control tab
	ChangeTab(tabControl);
	lastResponseTime = GetTickCount();		// pretend we just received a response
	
	for (;;)
	{
		// 1. Check for input from the serial port and process it.
		SerialIo::CheckInput();
		
		// 2. If the file list has changed, refresh it.
		if (fileListChanged)
		{
			RefreshFileList();
			fileListChanged = false;
		}
		
		// 3. Check for a touch on the touch panel.
		if (GetTickCount() - lastTouchTime >= ignoreTouchTime)
		{
			uint16_t x, y;
			if (touch.read(x, y))
			{
				touchX->SetValue((int)x);	//debug
				touchY->SetValue((int)y);	//debug
				DisplayField * null f = mgr.FindEvent(x, y);
				if (f != NULL)
				{
					DelayTouchLong();		// by default, ignore further touches for a long time
					if (f->GetEvent() != evAdjustVolume)
					{
						TouchBeep();		// give audible feedback of the touch, unless adjusting the volume	
					}
					ProcessTouch(f);
				}
				else
				{
					f = mgr.FindEventOutsidePopup(x, y);
					if (f != NULL && f == fieldBeingAdjusted)
					{
						DelayTouchLong();	// by default, ignore further touches for a long time
						TouchBeep();
						ProcessTouchOutsidePopup();					
					}
				}
			}
		}
		
		// 4. Refresh the display
		updateDebugInfo();
		mgr.RefreshAll(false);
		
		// 5. Generate a beep if asked to
		if (beepFrequency != 0 && beepLength != 0)
		{
			if (beepFrequency >= 100 && beepFrequency <= 10000 && beepLength > 0)
			{
				Buzzer::Beep(beepFrequency, beepLength, Buzzer::MaxVolume);
			}
			beepFrequency = beepLength = 0;
		}

		// 6. If it is time, poll the printer status.
		// When the printer is executing a homing move or other file macro, it may stop responding to polling requests.
		// Under these conditions, we slow down the rate of polling to avoid building up a large queue of them.
		uint32_t now = GetTickCount();
		if (   now - lastPollTime >= printerPollInterval			// if we haven't polled the printer too recently...
			&& (   now - lastPollTime > now - lastResponseTime		// ...and either we've had a response since the last poll...
				|| now - lastPollTime >= printerPollTimeout			// ...or we're giving up on getting a response to the last poll
			   )
		   )
		{
			lastPollTime += printerPollInterval;
			SerialIo::SendString((gotMachineName) ? "M105 S2\n" : "M105 S3\n");
		}
	}
}

void PrintDebugText(const char *x)
{
	fwVersionField->SetValue(x);
}

// End
