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

const uint32_t printerPollInterval = 2000;			// poll interval in milliseconds
const uint32_t printerResponseInterval = 1500;		// shortest time after a response that we send another poll (gives printer time to catch up)
const uint32_t printerPollTimeout = 8000;			// poll timeout in milliseconds
const uint32_t FileInfoRequestTimeout = 8000;		// file info request timeout in milliseconds
const uint32_t touchBeepLength = 20;				// beep length in ms
const uint32_t touchBeepFrequency = 4500;			// beep frequency in Hz. Resonant frequency of the piezo sounder is 4.5kHz.
const uint32_t errorBeepLength = 100;
const uint32_t errorBeepFrequency = 2250;
const uint32_t longTouchDelay = 250;				// how long we ignore new touches for after pressing Set
const uint32_t shortTouchDelay = 100;				// how long we ignore new touches while pressing up/down, to get a reasonable repeat rate
const unsigned int maxMessageChars = 100;

UTFT lcd(DISPLAY_CONTROLLER, TMode16bit, 16, 17, 18, 19);

UTouch touch(23, 24, 22, 21, 20);
DisplayManager mgr;

static uint32_t lastTouchTime;
static uint32_t ignoreTouchTime;
static uint32_t lastPollTime;
static uint32_t lastResponseTime = 0;
static uint32_t fileScrollOffset = 0;
static bool fileListScrolled = false;
static bool gotMachineName = false;
static bool isDelta = false;
static bool gotGeometry = false;
static bool axisHomed[3] = {false, false, false};
static bool allAxesHomed = false;
static int beepFrequency = 0, beepLength = 0;
static unsigned int numHeads = 1;
static unsigned int messageSeq = 0;
static unsigned int newMessageSeq = 0;

typedef Vector<char, 2048> FileList;				// we use a Vector instead of a String because we store multiple null-terminated strings in it
typedef Vector<const char* array, 100> FileListIndex;

FileList fileLists[3];								// one for gcode file list, one for macro list, one for receiving new lists into
FileListIndex fileIndices[3];						// pointers into the individual filenames in the list
int filesFileList = -1, macrosFileList = -1;		// which file list we use for the files listing and the macros listing
int newFileList = -1;								// which file list we received a new listing into
static const char* array null currentFile = NULL;	// file whose info is displayed in the popup menu

String<100> fileDirectoryName;

static OneBitPort BacklightPort(33);				// PB1 (aka port 33) controls the backlight on the prototype

struct Message
{
	static const size_t rttLen = 5;					// number of chars we print for the message age
	uint32_t receivedTime;
	char receivedTimeText[rttLen];					// 5 characters plus null terminator
	char msg[maxMessageChars];
};

static Message messages[numMessageRows + 1];		// one extra slot for receiving new messages into
static unsigned int messageStartRow = 0;			// the row number at the top
static unsigned int newMessageStartRow = 0;			// the row number that we put a new message in

static int timesLeft[3];
static String<50> timesLeftText;

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
	psConnecting = 0,
	psIdle = 1,
	psPrinting = 2,
	psStopped = 3,
	psConfiguring = 4,
	psPaused = 5,
	psBusy = 6,
	psPausing = 7,
	psResuming = 8
};

// Map of the above status codes to text. The space at the end improves the appearance.
const char *statusText[] =
{
	"Connecting",
	"Idle ",
	"Printing ",
	"Halted (needs reset)",
	"Starting up ",
	"Paused ",
	"Busy ",
	"Pausing ",
	"Resuming "
};

static PrinterStatus status = psConnecting;

enum ReceivedDataEvent
{
	rcvUnknown = 0,
	rcvActive,
	rcvDir,
	rcvEfactor,
	rcvFilament,
	rcvFiles,
	rcvHeaters,
	rcvHomed,
	rcvHstat,
	rcvPos,
	rcvStandby,
	rcvBeepFreq,
	rcvBeepLength,
	rcvFilename,
	rcvFraction,
	rcvGeneratedBy,
	rcvGeometry,
	rcvHeight,
	rcvLayerHeight,
	rcvMyName,
	rcvProbe,
	rcvResponse,
	rcvSeq,
	rcvSfactor,
	rcvSize,
	rcvStatus,
	rcvTimesLeft
};

struct ReceiveDataTableEntry
{
	ReceivedDataEvent rde;
	const char* varName;
};

static Event eventToConfirm = nullEvent;

const size_t numHeaters = 3;
int heaterStatus[numHeaters];

bool OkToSend();		// forward declaration

class RequestTimer
{
	enum { stopped, running, ready } timerState;
	uint32_t startTime;
	uint32_t delayTime;
	const char *command;
	
public:
	RequestTimer(uint32_t del, const char *cmd);
	void SetPending() { timerState = ready; }
	void Stop() { timerState = stopped; }
	bool Process();
};

RequestTimer macroListTimer(FileInfoRequestTimeout, "M20 S2 P/macros");
RequestTimer filesListTimer(FileInfoRequestTimeout, "M20 S2 P/gcodes");
RequestTimer fileInfoTimer(FileInfoRequestTimeout, "M36");
RequestTimer machineConfigTimer(FileInfoRequestTimeout, "M105 S3");

RequestTimer::RequestTimer(uint32_t del, const char *cmd)
	: delayTime(del), command(cmd)
{
	timerState = stopped;
}

bool RequestTimer::Process()
{
	if (timerState == running)
	{
		uint32_t now = GetTickCount();
		if (now - startTime > delayTime)
		{
			timerState = ready;
		}
	}

	if (timerState == ready && OkToSend())
	{
		SerialIo::SendString(command);
		SerialIo::SendChar('\n');
		startTime = GetTickCount();
		timerState = running;
		return true;
	}
	return false;
}

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

// Load parameters from flash memory
void FlashData::Load()
{
	FlashStorage::read(0, &nvData, sizeof(nvData));
}

// Save parameters to flash memory
void FlashData::Save() const
{
	FlashStorage::write(0, this, sizeof(*this));
}

// Return true if the second string is alphabetically greater then the first, case insensitive
bool StringGreaterThan(const char* a, const char* b)
{
	return stricmp(a, b) > 0;
}

// Refresh the list of files on the Files tab
void RefreshFileList()
{
	if (filesFileList >= 0)
	{
		FileListIndex& fileIndex = fileIndices[filesFileList];
	
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
		fileListScrolled = false;
	}
}

// Refresh the list of files on the Files tab
void RefreshMacroList()
{
	if (macrosFileList >= 0)
	{
		FileListIndex& macroIndex = fileIndices[macrosFileList];
		// 1. Sort the file list
		macroIndex.sort(StringGreaterThan);
	
		// 2. Display the macro list
		for (size_t i = 0; i < numDisplayedMacros; ++i)
		{
			StaticTextField *f = macroFields[i];
			if (i < macroIndex.size())
			{
				f->SetValue(macroIndex[i]);
				f->SetColours(white, selectableBackColor);
				f->SetEvent(evMacro, i);
			}
			else
			{
				f->SetValue("");
				f->SetColours(white, defaultBackColor);
				f->SetEvent(nullEvent, 0);
			}
		}
	}
}

// Search a table for a matching string
ReceivedDataEvent bsearch(const ReceiveDataTableEntry array table[], size_t numElems, const char* key)
{
	size_t low = 0u, high = numElems;
	while (high > low)
	{
		const size_t mid = (high - low)/2 + low;
		const int t = stricmp(key, table[mid].varName);
		if (t == 0)
		{
			return table[mid].rde;
		}
		if (t > 0)
		{
			low = mid + 1u;
		}
		else
		{
			high = mid;
		}
	}
	return (low < numElems && stricmp(key, table[low].varName) == 0) ? table[low].rde : rcvUnknown;
}

// Return true if sending a command or file list request to the printer now is a good idea.
// We don't want to send these when the printer is busy with a previous command, because they will block normal status requests.
bool OkToSend()
{
	return status == psIdle || status == psPrinting || status == psPaused;
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
			macroListTimer.SetPending();								// refresh the list of macros
			mgr.SetRoot(controlRoot);
			break;
		case evTabPrint:
			mgr.SetRoot(printRoot);
			break;
		case evTabFiles:
			filesListTimer.SetPending();								// refresh the list of files
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

void ErrorBeep()
{
	while (Buzzer::Noisy()) { }
	Buzzer::Beep(errorBeepFrequency, errorBeepLength, nvData.touchVolume);
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
		mgr.SetPopup(setTempPopup, tempPopupX, popupY);
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
		CheckSettingsAreSaved();
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
		mgr.SetPopup(setXYPopup, xyPopupX, popupY);
		fieldBeingAdjusted = f;
		break;

	case evZPos:
		mgr.Outline(f, outlineColor, outlinePixels);
		mgr.SetPopup(setZPopup, xyPopupX, popupY);
		fieldBeingAdjusted = f;
		break;

	case evFile:
		{
			int fileNumber = f->GetIParam();
			if (fileNumber >= 0 && filesFileList >= 0 && (size_t)fileNumber < fileIndices[filesFileList].size())
			{
				currentFile = fileIndices[filesFileList][fileNumber];
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
			}
			else
			{
				ErrorBeep();
			}
		}
		break;

	case evMacro:
		{
			int macroNumber = f->GetIParam();
			if (macroNumber >= 0 && macrosFileList >= 0 && (size_t)macroNumber < fileIndices[macrosFileList].size())
			{
				SerialIo::SendString("M98 P/macros/");
				SerialIo::SendString(fileIndices[macrosFileList][macroNumber]);
				SerialIo::SendChar('\n');
			}
			else
			{
				ErrorBeep();
			}
		}
		break;

	case evPrint:
		mgr.SetPopup(NULL);
		if (currentFile != NULL)
		{
			SerialIo::SendString("M32 ");
			SerialIo::SendString(currentFile);
			SerialIo::SendChar('\n');
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
	case evPausePrint:
	case evResumePrint:
	case evReset:
		SerialIo::SendString(f->GetSParam());
		SerialIo::SendChar('\n');
		break;

	case evScrollFiles:
		fileScrollOffset += f->GetIParam();
		fileListScrolled = true;
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
		mgr.SetPopup(baudPopup, xyPopupX, popupY);
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
		mgr.SetPopup(volumePopup, xyPopupX, popupY);
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
				filesListTimer.SetPending();
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
	case evAdjustPercent:
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

// Update the messages on the message tab. If 'all' is true we do the times and the text, else we just do the times.
void UpdateMessages(bool all)
{
	size_t index = messageStartRow;
	for (size_t i = 0; i < numMessageRows; ++i)
	{
		Message *m = &messages[index];
		uint32_t tim = m->receivedTime;
		char* p = m->receivedTimeText;
		if (tim == 0)
		{
			p[0] = 0;
		}
		else
		{
			uint32_t age = (GetTickCount() - tim)/1000;	// age of message in seconds
			if (age < 10 * 60)
			{
				snprintf(p, Message::rttLen, "%lum%02lu", age/60, age%60);
			}
			else
			{
				age /= 60;		// convert to minutes
				if (age < 60)
				{
					snprintf(p, Message::rttLen, "%lum", age);
				}
				else if (age < 10 * 60)
				{
					snprintf(p, Message::rttLen, "%luh%02lu", age/60, age%60);
				}
				else
				{
					age /= 60;	// convert to hours
					if (age < 10)
					{
						snprintf(p, Message::rttLen, "%luh", age);
					}
					else if (age < 24 + 10)
					{
						snprintf(p, Message::rttLen, "%lud%02lu", age/24, age%24);
					}
					else
					{
						snprintf(p, Message::rttLen, "%lud", age/24);
					}
				}
			}
			messageTimeFields[i]->SetValue(p);			
		}
		if (all)
		{
			messageTextFields[i]->SetValue(m->msg);		
		}
		index = (index == numMessageRows) ? 0 : index + 1;
	}
}

void AppendMessage(const char* data)
{
	newMessageStartRow = (messageStartRow == 0) ? numMessageRows : messageStartRow - 1;
	safeStrncpy(messages[newMessageStartRow].msg, data, maxMessageChars);
	messages[newMessageStartRow].receivedTime = GetTickCount();
}

void DisplayNewMessage()
{
	if (newMessageStartRow != messageStartRow)
	{
		messageStartRow = newMessageStartRow;
		UpdateMessages(true);
	}
}

void UpdatePrintingFields()
{
	if (status == psPrinting)
	{
		Fields::ShowPauseButton();
	}
	else if (status == psPaused)
	{
		Fields::ShowResumeAndCancelButtons();
	}
	else
	{
		Fields::HidePauseResumeCancelButtons();
	}
	
	bool showPrintProgress = (status == psPrinting || status == psPaused || status == psPausing || status == psResuming);
	mgr.Show(printProgressBar, showPrintProgress);
	mgr.Show(printingField, showPrintProgress);

	// Don't enable the time left field when we start printing, instead this will get enabled when we receive a suitable message
	if (!showPrintProgress)
	{
		mgr.Show(timeLeftField, false);	
	}
	
	statusField->SetValue(statusText[status]);
}

// This is called when the status changes
void SetStatus(char c)
{
	PrinterStatus newStatus;
	switch(c)
	{
	case 'A':
		newStatus = psPaused;
		fileInfoTimer.SetPending();
		break;
	case 'B':
		newStatus = psBusy;
		break;
	case 'C':
		newStatus = psConfiguring;
		break;
	case 'D':
		newStatus = psPausing;
		break;
	case 'I':
		newStatus = psIdle;
		printingFile.clear();
		break;
	case 'P':
		newStatus = psPrinting;
		fileInfoTimer.SetPending();
		break;
	case 'R':
		newStatus = psResuming;
		break;
	case 'S':
		newStatus = psStopped;
		break;
	default:
		newStatus = status;		// leave the status alone if we don't recognize it
		break;
	}
	
	if (newStatus != status)
	{
		switch (newStatus)
		{
		case psPrinting:
			if (status != psPaused && status != psResuming)
			{
				// Starting a new print, so clear the times
				timesLeft[0] = timesLeft[1] = timesLeft[2] = 0;			
			}	
			// no break
		case psPaused:
		case psPausing:
		case psResuming:
			if (status == psConnecting)
			{
				ChangeTab(tabPrint);
			}
			break;
			
		default:
			break;
		}
		
		if (status == psConfiguring || (status == psConnecting && newStatus != psConfiguring))
		{
			AppendMessage("Connected");
			DisplayNewMessage();
		}
	
		status = newStatus;
		UpdatePrintingFields();
	}
}

// Append an amount of time to timesLeftText
void AppendTimeLeft(int t)
{
	if (t <= 0)
	{
		timesLeftText.catFrom("n/a");
	}
	else if (t < 60)
	{
		timesLeftText.scatf("%ds", t);
	}
	else if (t < 60 * 60)
	{
		timesLeftText.scatf("%dm %02ds", t/60, t%60);
	}
	else
	{
		t /= 60;
		timesLeftText.scatf("%dh %02dm", t/60, t%60);
	}
}

// Try to get an integer value from a string. If it is actually a floating point value, round it.
bool GetInteger(const char s[], int &rslt)
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

// Try to get an unsigned integer value from a string
bool GetUnsignedInteger(const char s[], unsigned int &rslt)
{
	if (s[0] == 0) return false;			// empty string
	char* endptr;
	rslt = (int) strtoul(s, &endptr, 10);
	return (*endptr == 0);
}

// Try to get a floating point value from a string. if it is actually a floating point value, round it.
bool GetFloat(const char s[], float &rslt)
{
	if (s[0] == 0) return false;			// empty string
	char* endptr;
	rslt = (float) strtod(s, &endptr);
	return *endptr == 0;					// we parsed an integer
}

// These tables must be kept in alphabetical order
const ReceiveDataTableEntry arrayDataTable[] =
{
	{ rcvActive,		"active" },
	{ rcvEfactor,		"efactor" },
	{ rcvFilament,		"filament" },
	{ rcvFiles,			"files" },
	{ rcvHeaters,		"heaters" },
	{ rcvHomed,			"homed" },
	{ rcvHstat,			"hstat" },
	{ rcvPos,			"pos" },
	{ rcvStandby,		"standby" },
	{ rcvTimesLeft,		"timesLeft" }
};

const ReceiveDataTableEntry nonArrayDataTable[] =
{
	{ rcvBeepFreq,		"beep_freq" },
	{ rcvBeepLength,	"beep_length" },
	{ rcvDir,			"dir" },
	{ rcvFilename,		"fileName" },
	{ rcvFraction,		"fraction_printed" },
	{ rcvGeneratedBy,	"generatedBy" },
	{ rcvGeometry,		"geometry" },
	{ rcvHeight,		"height" },
	{ rcvLayerHeight,	"layerHeight" },
	{ rcvMyName,		"myName" },
	{ rcvProbe,			"probe" },
	{ rcvResponse,		"resp" },
	{ rcvSeq,			"seq" },
	{ rcvSfactor,		"sfactor" },
	{ rcvSize,			"size" },
	{ rcvStatus,		"status" }
};

void StartReceivedMessage()
{
	newMessageSeq = messageSeq;
	newMessageStartRow = messageStartRow;
	fileDirectoryName.clear();
	newFileList = -1;
}

void EndReceivedMessage()
{
	lastResponseTime = GetTickCount();

	if (newMessageSeq != messageSeq && newMessageStartRow != messageStartRow)
	{
		messageSeq = newMessageSeq;
		DisplayNewMessage();
	}
	
	if (newFileList >= 0)
	{
		// We received a new file list
		if (fileDirectoryName.isEmpty() || fileDirectoryName.equalsIgnoreCase("/gcodes") || fileDirectoryName.equalsIgnoreCase("0:/gcodes/"))
		{
			if (currentFile == NULL)
			{
				filesFileList = newFileList;
				RefreshFileList();
				filesListTimer.Stop();
			}
		}
		else if (fileDirectoryName.equalsIgnoreCase("/macros"))
		{
			macrosFileList = newFileList;
			RefreshMacroList();
			macroListTimer.Stop();
		}
		newFileList = -1;
	}
}

// Public functions called by the SerialIo module
void ProcessReceivedValue(const char id[], const char data[], int index)
{
	if (index >= 0)			// if this is an element of an array
	{
		switch(bsearch(arrayDataTable, sizeof(arrayDataTable)/sizeof(arrayDataTable[0]), id))
		{
		case rcvActive:
			{
				int ival;
				if (GetInteger(data, ival))
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
			break;

		case rcvHeaters:
			{
				float fval;
				if (GetFloat(data, fval))
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
			break;

		case rcvHstat:
			{
				int ival;
				if (GetInteger(data, ival) && index >= 0 && index < (int)numHeaters)
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
			break;
			
		case rcvPos:
			{
				float fval;
				if (GetFloat(data, fval))
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
			break;
		
		case rcvEfactor:
			{
				int ival;
				if (GetInteger(data, ival))
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
			break;
		
		case rcvFiles:
			{
				if (index == 0)
				{
					// Find a free file list and index to receive the filenames into
					newFileList = 0;
					while (newFileList == filesFileList || newFileList == macrosFileList)
					{
						++newFileList;
					}
				
					_ecv_assert(0 <= newFileList && newFileList < 3);
					fileLists[newFileList].clear();
					fileIndices[newFileList].clear();
				}
				if (newFileList >= 0)
				{
					FileList& fileList = fileLists[newFileList];
					FileListIndex& fileIndex = fileIndices[newFileList];
					size_t len = strlen(data) + 1;		// we are going to copy the null terminator as well
					if (len + fileList.size() < fileList.capacity() && fileIndex.size() < fileIndex.capacity())
					{
						fileIndex.add(fileList.c_ptr() + fileList.size());
						fileList.add(data, len);
					}
				}
			}
			break;
		
		case rcvFilament:
			{
				static float totalFilament = 0.0;
				if (index == 0)
				{
					totalFilament = 0.0;
				}
				float f;
				if (GetFloat(data, f))
				{
					totalFilament += f;
					fpFilamentField->SetValue((int)totalFilament);
				}
			}
			break;
		
		case rcvHomed:
			{
				int ival;
				if (index < 3 && GetInteger(data, ival) && ival >= 0 && ival < 2)
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
			break;
		
		case rcvTimesLeft:
			if (index < 2)			// we ignore the layer-based time because it is often wildly inaccurate
			{
				int i;
				bool b = GetInteger(data, i);
				if (b && i >= 0 && i < 10 * 24 * 60 * 60 && (status == psPrinting || status == psPaused))
				{
					timesLeft[index] = i;
					timesLeftText.copyFrom("Estimated time left: filament ");
					AppendTimeLeft(timesLeft[1]);
					timesLeftText.catFrom(", file ");
					AppendTimeLeft(timesLeft[0]);
					timeLeftField->SetValue(timesLeftText.c_str());
					mgr.Show(timeLeftField, true);
				}
			}
			break;

		default:
			break;
		}
	}
	else
	{
		// Non-array values follow
		switch(bsearch(nonArrayDataTable, sizeof(nonArrayDataTable)/sizeof(nonArrayDataTable[0]), id))
		{
		case rcvSfactor:
			{
				int ival;
				if (GetInteger(data, ival))
				{
					UpdateField(spd, ival);
				}
			}
			break;

		case rcvProbe:
			zprobeBuf.copyFrom(data);
			zProbe->SetChanged();
			break;
		
		case rcvMyName:
			if (status != psConfiguring && status != psConnecting)
			{
				machineName.copyFrom(data);
				nameField->SetChanged();
				gotMachineName = true;
				if (gotGeometry)
				{
					machineConfigTimer.Stop();
				}
			}
			break;
		
		case rcvFilename:
			printingFile.copyFrom(data);
			printingField->SetChanged();
			fileInfoTimer.Stop();
			break;
		
		case rcvSize:
			{
				int sz;
				if (GetInteger(data, sz))
				{
					fpSizeField->SetValue(sz);
				}
			}
			break;
		
		case rcvHeight:
			{
				float f;
				if (GetFloat(data, f))
				{
					fpHeightField->SetValue(f);
				}
			}
			break;
		
		case rcvLayerHeight:
			{
				float f;
				if (GetFloat(data, f))
				{
					fpLayerHeightField->SetValue(f);
				}
			}
			break;
		
		case rcvGeneratedBy:
			generatedByText.copyFrom(data);
			fpGeneratedByField->SetChanged();
			break;
		
		case rcvFraction:
			{
				float f;
				if (GetFloat(data, f))
				{
					if (f >= 0.0 && f <= 1.0)
					{
						printProgressBar->SetPercent((uint8_t)((100.0 * f) + 0.5));
					}
				}
			}
			break;
		
		case rcvStatus:
			SetStatus(data[0]);
			break;
		
		case rcvBeepFreq:
			GetInteger(data, beepFrequency);
			break;
		
		case rcvBeepLength:
			GetInteger(data, beepLength);
			break;
		
		case rcvGeometry:
			if (status != psConfiguring && status != psConnecting)
			{
				isDelta = (strcmp(data, "delta") == 0);
				gotGeometry = true;
				if (gotMachineName)
				{
					machineConfigTimer.Stop();
				}
				for (size_t i = 0; i < 3; ++i)
				{
					mgr.Show(homeFields[i], !isDelta);
				}
			}
			break;
		
		case rcvSeq:
			GetUnsignedInteger(data, newMessageSeq);
			break;
		
		case rcvResponse:
			AppendMessage(data);
			break;
		
		case rcvDir:
			fileDirectoryName.copyFrom(data);
			break;

		default:
			break;
		}
	}
}

// Public function called when the serial I/O module finishes receiving an array of values
void ProcessArrayLength(const char id[], int length)
{
	// Nothing to do here at present
}

// Update those fields that display debug information
void UpdateDebugInfo()
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

void SendRequest(const char *s, bool includeSeq = false)
{
	SerialIo::SendString(s);
	if (includeSeq)
	{
		SerialIo::SendInt(messageSeq);
	}
	SerialIo::SendChar('\n');
	lastPollTime = GetTickCount();
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
		SaveSettings();
	}
	
	// Set up the baud rate
	SerialIo::Init(nvData.baudRate);
	baudRateField->SetValue(nvData.baudRate);

	// Clear the message log
	for (size_t i = 0; i <= numMessageRows; ++i)	// note we have numMessageRows+1 message slots
	{
		messages[i].receivedTime = 0;
		messages[i].msg[0] = 0;
	}
	UpdateMessages(true);
	UpdatePrintingFields();

	lastPollTime = GetTickCount() - printerPollInterval;	// allow a poll immediately
	
	// Hide the Head 2 parameters until we know we have a second head
	mgr.Show(t2CurrentTemp, false);
	mgr.Show(t2ActiveTemp, false);
	mgr.Show(t2StandbyTemp, false);
	
	mgr.Show(bedStandbyTemp, false);		// currently, we always hide the bed standby temperature because it doesn't do anything
	
	// Display the Control tab
	ChangeTab(tabControl);
	lastResponseTime = GetTickCount();		// pretend we just received a response
	
	machineConfigTimer.SetPending();		// we need to fetch the machine name and configuration
	
	for (;;)
	{
		// 1. Check for input from the serial port and process it.
		// This calls back into functions StartReceivedMessage, ProcessReceivedValue, ProcessArrayLength and EndReceivedMessage.
		SerialIo::CheckInput();
		
		// 2. if displaying the message log, update the times
		if (currentTab == tabMsg)
		{
			UpdateMessages(false);
		}
		
		// 3a. Check for a touch on the touch panel.
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
		
		// 3b. If the file list has changed due to scrolling, refresh it.
		if (fileListScrolled)
		{
			RefreshFileList();
		}
		
		// 4. Refresh the display
		UpdateDebugInfo();
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
			&& now - lastResponseTime >= printerResponseInterval	// and we haven't had a response too recently
		   )
		{
			if (now - lastPollTime > now - lastResponseTime)		// if we've had a response since the last poll
			{
				// First check for specific info we need to fetch
				bool done = machineConfigTimer.Process();
				if (!done)
				{
					done = macroListTimer.Process();
				}
				if (!done)
				{
					done = filesListTimer.Process();
				}
				if (!done)
				{
					done = fileInfoTimer.Process();
				}
				
				// Otherwise just send a normal poll command
				if (!done)
				{
					SendRequest("M105 S2 R", true);					// normal poll response
				}
			}
			else if (now - lastPollTime >= printerPollTimeout)		// if we're giving up on getting a response to the last poll
			{
				SendRequest("M105 S2");								// just send a normal poll message, don't ask for the last response
			}
		}
	}
}

void PrintDebugText(const char *x)
{
	fwVersionField->SetValue(x);
}

// End
