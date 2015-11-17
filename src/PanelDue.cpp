// Controller for Ormerod to run on SAM3S2B
// Coding rules:
//
// 1. Must compile with no g++ warnings, when all warning are enabled.
// 2. Dynamic memory allocation using 'new' is permitted during the initialization phase only. No use of 'new' anywhere else,
//    and no use of 'delete', 'malloc' or 'free' anywhere.
// 3. No pure virtual functions. This is because in release builds, having pure virtual functions causes huge amounts of the C++ library to be linked in
//    (possibly because it wants to print a message if a pure virtual function is called).

#include "ecv.h"
#undef array
#undef result
#include "asf.h"
#define array _ecv_array
#define result _ecv_result

#include <cstring>

#include "Hardware/Mem.hpp"
#include "Display.hpp"
#include "Hardware/UTFT.hpp"
#include "Hardware/UTouch.hpp"
#include "Hardware/SerialIo.hpp"
#include "Hardware/Buzzer.hpp"
#include "Hardware/SysTick.hpp"
#include "Hardware/Reset.hpp"
#include "Library/Misc.hpp"
#include "Library/Vector.hpp"
#include "Hardware/FlashStorage.hpp"
#include "PanelDue.hpp"
#include "Configuration.hpp"
#include "Fields.hpp"
#include "FileManager.hpp"
#include "RequestTimer.hpp"
#include "MessageLog.hpp"

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

UTFT lcd(DISPLAY_CONTROLLER, TMode16bit, 16, 17, 18, 19);

UTouch touch(23, 24, 22, 21, 20);
MainWindow mgr;

const char* array null currentFile;					// file whose info is displayed in the file info popup

static uint32_t lastTouchTime;
static uint32_t ignoreTouchTime;
static uint32_t lastPollTime;
static uint32_t lastResponseTime = 0;
static bool gotMachineName = false;
static bool isDelta = false;
static bool gotGeometry = false;
static bool axisHomed[3] = {false, false, false};
static bool allAxesHomed = false;
static int beepFrequency = 0, beepLength = 0;
static unsigned int numHeads = 1;
static unsigned int messageSeq = 0;
static unsigned int newMessageSeq = 0;

static OneBitPort BacklightPort(33);				// PB1 (aka port 33) controls the backlight on the prototype

static bool restartNeeded = false;

static int timesLeft[3];
static String<50> timesLeftText;

struct FlashData
{
	static const uint32_t magicVal = 0x3AB629D1;
	static const uint32_t muggleVal = 0xFFFFFFFF;

	uint32_t magic;
	uint32_t baudRate;
	uint16_t xmin;
	uint16_t xmax;
	uint16_t ymin;
	uint16_t ymax;
	DisplayOrientation lcdOrientation;
	DisplayOrientation touchOrientation;
	uint32_t touchVolume;
	uint32_t language;
	char dummy;
	
	FlashData() : magic(muggleVal) { }
	bool operator==(const FlashData& other);
	bool operator!=(const FlashData& other) { return !operator==(other); }
	bool Valid() const { return magic == magicVal; }
	void SetInvalid() { magic = muggleVal; }
	void SetDefaults();
	void Load();
	void Save() const;
};

static_assert(sizeof(FlashData) <= FLASH_DATA_LENGTH, "Flash data too large");

FlashData nvData, savedNvData;

enum class PrinterStatus
{
	connecting = 0,
	idle = 1,
	printing = 2,
	stopped = 3,
	configuring = 4,
	paused = 5,
	busy = 6,
	pausing = 7,
	resuming = 8
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

static PrinterStatus status = PrinterStatus::connecting;

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

static Event eventToConfirm = evNull;

const size_t numHeaters = 3;
int heaterStatus[numHeaters];

bool OkToSend();		// forward declaration

RequestTimer macroListTimer(FileInfoRequestTimeout, "M20 S2 P/macros");
RequestTimer filesListTimer(FileInfoRequestTimeout, "M20 S2 P/gcodes");
RequestTimer fileInfoTimer(FileInfoRequestTimeout, "M36");
RequestTimer machineConfigTimer(FileInfoRequestTimeout, "M408 S1");

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
		&& touchVolume == other.touchVolume
		&& language == other.language;
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
	language = 0;
	magic = magicVal;
}

// Load parameters from flash memory
void FlashData::Load()
{
	FlashStorage::read(0, &(this->magic), &(this->dummy) - (char*)(&(this->magic)));
}

// Save parameters to flash memory
void FlashData::Save() const
{
	FlashStorage::write(0, &(this->magic), &(this->dummy) - (const char*)(&(this->magic)));
}

struct FileList
{
	int listNumber;
	size_t scrollOffset;
	String<100> path;
};

bool PrintInProgress()
{
	return status == PrinterStatus::printing || status == PrinterStatus::paused || status == PrinterStatus::pausing || status == PrinterStatus::resuming;
}

// Search an ordered table for a matching string
ReceivedDataEvent bsearch(const ReceiveDataTableEntry array table[], size_t numElems, const char* key)
{
	size_t low = 0u, high = numElems;
	while (high > low)
	{
		const size_t mid = (high - low)/2 + low;
		const int t = strcasecmp(key, table[mid].varName);
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
	return (low < numElems && strcasecmp(key, table[low].varName) == 0) ? table[low].rde : rcvUnknown;
}

// Return true if sending a command or file list request to the printer now is a good idea.
// We don't want to send these when the printer is busy with a previous command, because they will block normal status requests.
bool OkToSend()
{
	return status == PrinterStatus::idle || status == PrinterStatus::printing || status == PrinterStatus::paused;
}

void ChangeTab(ButtonBase *newTab)
{
	if (newTab != currentTab)
	{
		if (currentTab != NULL)
		{
			currentTab->Press(false, 0);
		}
		newTab->Press(true, 0);
		currentTab = newTab;
		switch(newTab->GetEvent())
		{
		case evTabControl:
			mgr.SetRoot(controlRoot);
			nameField->SetValue(machineName.c_str());
			break;
		case evTabPrint:
			mgr.SetRoot(printRoot);
			nameField->SetValue(PrintInProgress() ? printingFile.c_str() : machineName.c_str());
			break;
		case evTabMsg:
			mgr.SetRoot(messageRoot);
			break;
		case evTabSetup:
			mgr.SetRoot(setupRoot);
			break;
		default:
			mgr.SetRoot(commonRoot);
			break;
		}
		mgr.ClearAll();
	}
	if (currentButton.GetButton() == newTab)
	{
		currentButton.Clear();			// to prevent it being released
	}
	mgr.Refresh(true);
}

void InitLcd(DisplayOrientation dor, bool is24bit, uint8_t language)
{
	lcd.InitLCD(dor, is24bit);					// set up the LCD
	Fields::CreateFields(language);		// create all the fields
	mgr.Refresh(true);					// redraw everything

	currentTab = NULL;
}

// Ignore touches for a long time
void DelayTouchLong()
{
	lastTouchTime = SystemTick::GetTickCount();
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
void DoTouchCalib(PixelNumber x, PixelNumber y, PixelNumber altX, PixelNumber altY, bool wantY, uint16_t& rawRslt)
{
	const PixelNumber touchCircleRadius = DisplayY/32;
	const PixelNumber touchCalibMaxError = DisplayY/6;
	
	lcd.setColor(touchSpotColour);
	lcd.fillCircle(x, y, touchCircleRadius);
	
	for (;;)
	{
		uint16_t tx, ty, rawX, rawY;
		if (touch.read(tx, ty, &rawX, &rawY))
		{
			if (   (abs((int)tx - (int)x) <= touchCalibMaxError || abs((int)tx - (int)altX) <= touchCalibMaxError)
				&& (abs((int)ty - (int)y) <= touchCalibMaxError || abs((int)ty - (int)altY) <= touchCalibMaxError)
			   ) 
			{
				TouchBeep();
				rawRslt = (wantY) ? rawY : rawX;
				break;
			}
		}
	}
	
	lcd.setColor(defaultBackColour);
	lcd.fillCircle(x, y, touchCircleRadius);
}

void CalibrateTouch()
{
	DisplayField *oldRoot = mgr.GetRoot();
	touchCalibInstruction->SetValue("Touch the spot");				// in case the user didn't need to press the reset button last time
	mgr.SetRoot(touchCalibInstruction);
	mgr.ClearAll();
	mgr.Refresh(true);

	touch.init(DisplayX, DisplayY, DefaultTouchOrientAdjust);				// initialize the driver and clear any existing calibration
	
	// Draw spots on the edges of the screen, one at a time, and ask the user to touch them.
	// For the first two, we allow for the touch panel being the wrong way round.
	DoTouchCalib(DisplayX/2, touchCalibMargin, DisplayX/2, DisplayY - 1 - touchCalibMargin, true, nvData.ymin);
	if (nvData.ymin >= 4096/2)
	{
		touch.adjustOrientation(ReverseY);
		nvData.ymin = 4095 - nvData.ymin;
	}
	DoTouchCalib(DisplayX - touchCalibMargin - 1, DisplayY/2, touchCalibMargin, DisplayY/2, false, nvData.xmax);
	if (nvData.xmax < 4096/2)
	{
		touch.adjustOrientation(ReverseX);
		nvData.xmax = 4095 - nvData.xmax;
	}
	DoTouchCalib(DisplayX/2, DisplayY - 1 - touchCalibMargin, DisplayX/2, DisplayY - 1 - touchCalibMargin, true, nvData.ymax);
	DoTouchCalib(touchCalibMargin, DisplayY/2, touchCalibMargin, DisplayY/2, false, nvData.xmin);
	
	nvData.touchOrientation = touch.getOrientation();
	touch.calibrate(nvData.xmin, nvData.xmax, nvData.ymin, nvData.ymax, touchCalibMargin);
	
	mgr.SetRoot(oldRoot);
	mgr.ClearAll();
	mgr.Refresh(true);
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
	Buzzer::Beep(touchBeepFrequency, 400, Buzzer::MaxVolume);		// long beep to acknowledge it
	while (Buzzer::Noisy()) { }
	Restart();														// reset the processor
}

// Save settings
void SaveSettings()
{
	while (Buzzer::Noisy()) { }
	nvData.Save();
	// To make sure it worked, load the settings again
	savedNvData.Load();
	CheckSettingsAreSaved();
}

void PopupAreYouSure(Event ev, const char* text, const char* query = "Are you sure?")
{
	eventToConfirm = ev;
	areYouSureTextField->SetValue(text);
	areYouSureQueryField->SetValue(query);
	mgr.SetPopup(areYouSurePopup, (DisplayX - areYouSurePopupWidth)/2, (DisplayY - areYouSurePopupHeight)/2);
}

void PopupRestart()
{
	PopupAreYouSure(evRestart, "Restart required", "Restart now?");
}

void Adjusting(ButtonPress bp)
{
	fieldBeingAdjusted = bp;
	if (bp == currentButton)
	{
		currentButton.Clear();		// to stop it being released
	}
}

void StopAdjusting()
{
	if (fieldBeingAdjusted.IsValid())
	{
		mgr.Press(fieldBeingAdjusted, false);
		fieldBeingAdjusted.Clear();
	}
}

void CurrentButtonReleased()
{
	if (currentButton.IsValid())
	{
		mgr.Press(currentButton, false);
		currentButton.Clear();	
	}
}

// Process a touch event
void ProcessTouch(ButtonPress bp)
{
	if (bp.IsValid())
	{
		ButtonBase *f = bp.GetButton();
		currentButton = bp;
		mgr.Press(bp, true);
		Event ev = (Event)(f->GetEvent());
		switch(ev)
		{
		case evTabControl:
		case evTabPrint:
		case evTabMsg:
		case evTabSetup:
			ChangeTab(f);
			break;

		case evAdjustActiveTemp:
		case evAdjustStandbyTemp:
			if (static_cast<IntegerButton*>(f)->GetValue() < 0)
			{
				static_cast<IntegerButton*>(f)->SetValue(0);
			}
			Adjusting(bp);
			mgr.SetPopup(setTempPopup, tempPopupX, popupY);
			break;

		case evAdjustSpeed:
		case evExtrusionFactor:
		case evAdjustFan:
			Adjusting(bp);
			mgr.SetPopup(setTempPopup, tempPopupX, popupY);
			break;

		case evSetInt:
			if (fieldBeingAdjusted.IsValid())
			{
				int val = static_cast<const IntegerButton*>(fieldBeingAdjusted.GetButton())->GetValue();
				switch(fieldBeingAdjusted.GetEvent())
				{
				case evAdjustActiveTemp:
					{
						int heater = fieldBeingAdjusted.GetIParam();
						if (heater == 0)
						{
							SerialIo::SendString("M140 S");
							SerialIo::SendInt(val);
							SerialIo::SendChar('\n');
						}
						else
						{
							SerialIo::SendString("G10 P");
							SerialIo::SendInt(heater - 1);
							SerialIo::SendString(" S");
							SerialIo::SendInt(val);
							SerialIo::SendChar('\n');
						}
					}
					break;
					
				case evAdjustStandbyTemp:
					{
						int heater = fieldBeingAdjusted.GetIParam();
						if (heater > 0)
						{
							SerialIo::SendString("G10 P");
							SerialIo::SendInt(heater - 1);
							SerialIo::SendString(" R");
							SerialIo::SendInt(val);
							SerialIo::SendChar('\n');
						}
					}
					break;
				
				case evExtrusionFactor:
					{
						int heater = fieldBeingAdjusted.GetIParam();
						SerialIo::SendString("M221 P");
						SerialIo::SendInt(heater);
						SerialIo::SendString(" S");
						SerialIo::SendInt(val);
						SerialIo::SendChar('\n');
					}
					break;
					
				case evAdjustFan:
					SerialIo::SendString("M106 S");
					SerialIo::SendInt((256 * val)/100);
					SerialIo::SendChar('\n');
					break;

				default:
					{
						const char* null cmd = fieldBeingAdjusted.GetSParam();
						if (cmd != NULL)
						{
							SerialIo::SendString(cmd);
							SerialIo::SendInt(val);
							SerialIo::SendChar('\n');
						}
					}
					break;
				}
				mgr.ClearPopup();
				StopAdjusting();
			}
			break;

		case evAdjustInt:
			if (fieldBeingAdjusted.IsValid())
			{
				IntegerButton *ib = static_cast<IntegerButton*>(fieldBeingAdjusted.GetButton());
				int newValue = ib->GetValue() + bp.GetIParam();
				switch(fieldBeingAdjusted.GetEvent())
				{
				case evAdjustActiveTemp:
				case evAdjustStandbyTemp:
					newValue = max<int>(0, min<int>(300, newValue));
					break;

				case evAdjustFan:
					newValue = max<int>(0, min<int>(100, newValue));
					break;

				default:
					break;
				}
				ib->SetValue(newValue);
				ShortenTouchDelay();
			}
			break;

		case evMove:
			mgr.SetPopup(movePopup, movePopupX, movePopupY);
			break;
		
		case evMoveX:
		case evMoveY:
		case evMoveZ:
			SerialIo::SendString("G91\nG1 ");
			SerialIo::SendChar((ev == evMoveX) ? 'X' : (ev == evMoveY) ? 'Y' : 'Z');
			SerialIo::SendString(bp.GetSParam());
			SerialIo::SendString(" F6000\nG90\n");
			break;

		case evListFiles:
			FileManager::DisplayFilesList();
			break;

		case evListMacros:
			FileManager::DisplayMacrosList();
			break;

		case evCalTouch:
			CalibrateTouch();
			CheckSettingsAreSaved();
			break;

		case evFactoryReset:
			PopupAreYouSure(ev, "Confirm factory reset");
			break;

		case evRestart:
			PopupAreYouSure(ev, "Confirm restart");
			break;

		case evSaveSettings:
			SaveSettings();
			if (restartNeeded)
			{
				PopupRestart();
			}
			break;

		case evSelectHead:
			{
				int head = bp.GetIParam();
				if (head == 0)
				{
					// There is no command to switch the bed to standby temperature, so we always set it to the active temperature
					SerialIo::SendString("M140 S");
					SerialIo::SendInt(activeTemps[0]->GetValue());
					SerialIo::SendChar('\n');
				}
				else if (head < (int)maxHeaters)
				{
					if (heaterStatus[head] == 2)		// if head is active
					{
						SerialIo::SendString("T-1\n");
					}
					else
					{
						SerialIo::SendChar('T');
						SerialIo::SendInt(head - 1);
						SerialIo::SendChar('\n');
					}
				}
			}
			break;
	
		case evFile:
			{
				const char *fileName = bp.GetSParam();
				if (fileName != nullptr)
				{
					currentFile = fileName;
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
					mgr.SetPopup(filePopup, (DisplayX - fileInfoPopupWidth)/2, (DisplayY - fileInfoPopupHeight)/2);
				}
				else
				{
					ErrorBeep();
				}
			}
			break;

		case evMacro:
			{
				const char *fileName = bp.GetSParam();
				if (fileName != nullptr)
				{
					if (fileName[0] == '*')		// if it's a directory
					{
					
					}
					else
					{
						SerialIo::SendString("M98 P/macros/");
						SerialIo::SendString(fileName);
						SerialIo::SendChar('\n');
					} 
				}
				else
				{
					ErrorBeep();
				}
			}
			break;

		case evPrint:
			mgr.ClearPopup();			// clear the file info popup
			mgr.ClearPopup();			// clear the file list popup
			if (currentFile != nullptr)
			{
				SerialIo::SendString("M32 ");
				SerialIo::SendString(currentFile);
				SerialIo::SendChar('\n');
				printingFile.copyFrom(currentFile);
				currentFile = nullptr;							// allow the file list to be updated
				CurrentButtonReleased();
				ChangeTab(tabPrint);
			}
			break;

		case evCancelPrint:
			CurrentButtonReleased();
			mgr.ClearPopup();
			currentFile = nullptr;								// allow the file list to be updated
			break;

		case evDeleteFile:
			CurrentButtonReleased();;
			PopupAreYouSure(ev, "Confirm file delete");
			break;

		case evSendCommand:
		case evPausePrint:
		case evResumePrint:
		case evReset:
			SerialIo::SendString(bp.GetSParam());
			SerialIo::SendChar('\n');
			break;

		case evScrollFiles:
			FileManager::Scroll(bp.GetIParam());
			ShortenTouchDelay();				
			break;

		case evKeyboard:
			mgr.SetPopup(keyboardPopup, keyboardPopupX, keyboardPopupY);
			break;

		case evInvertX:
			nvData.lcdOrientation = static_cast<DisplayOrientation>(nvData.lcdOrientation ^ (ReverseX));
			lcd.InitLCD(nvData.lcdOrientation);
			CalibrateTouch();
			CheckSettingsAreSaved();
			break;

		case evInvertY:
			nvData.lcdOrientation = static_cast<DisplayOrientation>(nvData.lcdOrientation ^ (ReverseX | ReverseY | InvertText | InvertBitmap));
			lcd.InitLCD(nvData.lcdOrientation);
			CalibrateTouch();
			CheckSettingsAreSaved();
			break;

		case evSetBaudRate:
			Adjusting(bp);
			mgr.SetPopup(baudPopup, fullWidthPopupX, popupY);
			break;

		case evAdjustBaudRate:
			nvData.baudRate = bp.GetIParam();
			SerialIo::Init(nvData.baudRate);
			baudRateButton->SetValue(nvData.baudRate);
			CheckSettingsAreSaved();
			CurrentButtonReleased();
			mgr.ClearPopup();
			StopAdjusting();
			break;

		case evSetVolume:
			Adjusting(bp);
			mgr.SetPopup(volumePopup, fullWidthPopupX, popupY);
			break;

		case evAdjustVolume:
			nvData.touchVolume = bp.GetIParam();
			volumeButton->SetValue(nvData.touchVolume);
			TouchBeep();									// give audible feedback of the touch at the new volume level
			CheckSettingsAreSaved();
			break;

		case evSetLanguage:
			Adjusting(bp);
			mgr.SetPopup(languagePopup, fullWidthPopupX, popupY);
			break;

		case evAdjustLanguage:
			nvData.language = bp.GetIParam();
			languageButton->SetText(longLanguageNames[nvData.language]);
			CheckSettingsAreSaved();						// not sure we need this because we are going to reset anyway
			break;

		case evYes:
			CurrentButtonReleased();
			mgr.ClearPopup();
			switch (eventToConfirm)
			{
			case evFactoryReset:
				FactoryReset();
				break;

			case evDeleteFile:
				if (currentFile != nullptr)
				{
					SerialIo::SendString("M30 ");
					SerialIo::SendString(currentFile);
					SerialIo::SendChar('\n');
					filesListTimer.SetPending();
					currentFile = nullptr;
				}
				break;

			case evRestart:
				if (nvData != savedNvData)
				{
					SaveSettings();
				}
				Restart();
				break;

			default:
				break;
			}
			eventToConfirm = evNull;
			currentFile = NULL;
			break;

		case evCancel:
			eventToConfirm = evNull;
			currentFile = nullptr;
			CurrentButtonReleased();
			mgr.ClearPopup();
			break;

		case evKey:
			if (!userCommandBuffers[currentUserCommandBuffer].full())
			{
				userCommandBuffers[currentUserCommandBuffer].add((char)bp.GetIParam());
				userCommandField->SetChanged();
			}
			break;

		case evBackspace:
			if (!userCommandBuffers[currentUserCommandBuffer].isEmpty())
			{
				userCommandBuffers[currentUserCommandBuffer].erase(userCommandBuffers[currentUserCommandBuffer].size() - 1);
				userCommandField->SetChanged();
				ShortenTouchDelay();
			}
			break;

		case evUp:
			currentUserCommandBuffer = (currentUserCommandBuffer == 0) ? numUserCommandBuffers - 1 : currentUserCommandBuffer - 1;
			userCommandField->SetLabel(userCommandBuffers[currentUserCommandBuffer].c_str());
			break;

		case evDown:
			currentUserCommandBuffer = (currentUserCommandBuffer + 1) % numUserCommandBuffers;
			userCommandField->SetLabel(userCommandBuffers[currentUserCommandBuffer].c_str());
			break;

		case evSendKeyboardCommand:
			if (userCommandBuffers[currentUserCommandBuffer].size() != 0)
			{
				SerialIo::SendString(userCommandBuffers[currentUserCommandBuffer].c_str());
				SerialIo::SendChar('\n');
				currentUserCommandBuffer = (currentUserCommandBuffer + 1) % numUserCommandBuffers;
				userCommandBuffers[currentUserCommandBuffer].clear();
				userCommandField->SetLabel(userCommandBuffers[currentUserCommandBuffer].c_str());
			}
			break;

		default:
			break;
		}
	}
}

// Process a touch event outside the popup on the field being adjusted
void ProcessTouchOutsidePopup()
{
	switch(fieldBeingAdjusted.GetEvent())
	{
	case evAdjustActiveTemp:
	case evAdjustStandbyTemp:
	case evSetBaudRate:
	case evSetVolume:
	case evAdjustSpeed:
	case evExtrusionFactor:
		mgr.ClearPopup();
		StopAdjusting();
		break;

	case evSetLanguage:
		mgr.ClearPopup();
		StopAdjusting();
		if (nvData.language != savedNvData.language)
		{
			restartNeeded = true;
			PopupRestart();
		}
		break;
	
	default:
		break;
	}
}

// Update an integer field, provided it isn't the one being adjusted
void UpdateField(IntegerButton *f, int val)
{
	if (f != fieldBeingAdjusted.GetButton())
	{
		f->SetValue(val);
	}
}

void UpdatePrintingFields()
{
	if (status == PrinterStatus::printing)
	{
		Fields::ShowPauseButton();
	}
	else if (status == PrinterStatus::paused)
	{
		Fields::ShowResumeAndCancelButtons();
	}
	else
	{
		Fields::ShowFilesButton();
	}
	
	mgr.Show(printProgressBar, PrintInProgress());
//	mgr.Show(printingField, PrintInProgress());

	// Don't enable the time left field when we start printing, instead this will get enabled when we receive a suitable message
	if (!PrintInProgress())
	{
		mgr.Show(timeLeftField, false);	
	}
	
	statusField->SetValue(statusText[(unsigned int)status]);
}

// This is called when the status changes
void SetStatus(char c)
{
	PrinterStatus newStatus;
	switch(c)
	{
	case 'A':
		newStatus = PrinterStatus::paused;
		fileInfoTimer.SetPending();
		break;
	case 'B':
		newStatus = PrinterStatus::busy;
		break;
	case 'C':
		newStatus = PrinterStatus::configuring;
		break;
	case 'D':
		newStatus = PrinterStatus::pausing;
		break;
	case 'I':
		newStatus = PrinterStatus::idle;
		printingFile.clear();
		break;
	case 'P':
		newStatus = PrinterStatus::printing;
		fileInfoTimer.SetPending();
		break;
	case 'R':
		newStatus = PrinterStatus::resuming;
		break;
	case 'S':
		newStatus = PrinterStatus::stopped;
		break;
	default:
		newStatus = status;		// leave the status alone if we don't recognize it
		break;
	}
	
	if (newStatus != status)
	{
		switch (newStatus)
		{
		case PrinterStatus::printing:
			if (status != PrinterStatus::paused && status != PrinterStatus::resuming)
			{
				// Starting a new print, so clear the times
				timesLeft[0] = timesLeft[1] = timesLeft[2] = 0;			
			}	
			// no break
		case PrinterStatus::paused:
		case PrinterStatus::pausing:
		case PrinterStatus::resuming:
			if (status == PrinterStatus::connecting)
			{
				ChangeTab(tabPrint);
			}
			else if (currentTab == tabPrint)
			{
				nameField->SetValue(printingFile.c_str());
			}
			break;
			
		default:
			nameField->SetValue(machineName.c_str());
			break;
		}
		
		if (status == PrinterStatus::configuring || (status == PrinterStatus::connecting && newStatus != PrinterStatus::configuring))
		{
			MessageLog::AppendMessage("Connected");
			MessageLog::DisplayNewMessage();
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
	MessageLog::BeginNewMessage();
	FileManager::BeginNewMessage();
}

void EndReceivedMessage()
{
	lastResponseTime = SystemTick::GetTickCount();

	if (newMessageSeq != messageSeq)
	{
		messageSeq = newMessageSeq;
		MessageLog::DisplayNewMessage();
	}	
	FileManager::EndReceivedMessage(currentFile != nullptr);	
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
				if (GetInteger(data, ival) && index < (int)maxHeaters)
				{
					UpdateField(activeTemps[index], ival);
				}
			}
			break;

		case rcvStandby:
			{
				int ival;
				if (GetInteger(data, ival) && index < (int)maxHeaters && index != 0)
				{
					UpdateField(standbyTemps[index], ival);
				}
			}
			break;
		
		case rcvHeaters:
			{
				float fval;
				if (GetFloat(data, fval) && index < (int)maxHeaters)
				{
					currentTemps[index]->SetValue(fval);
					if (index == (int)numHeads + 1)
					{
						mgr.Show(currentTemps[index], true);
						mgr.Show(activeTemps[index], true);
						mgr.Show(standbyTemps[index], true);
						mgr.Show(extrusionFactors[index - 1], true);
						++numHeads;
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
					Colour c = (ival == 1) ? standbyBackColour : (ival == 2) ? activeBackColour : (ival == 3) ? errorBackColour : defaultBackColour;
					currentTemps[index]->SetColours(infoTextColour, c);
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
				if (GetInteger(data, ival) && index + 1 < (int)maxHeaters)
				{
					UpdateField(extrusionFactors[index], ival);
				}
			}
			break;
		
		case rcvFiles:
			if (index == 0)
			{
				FileManager::BeginReceivingFiles();
			}
			FileManager::ReceiveFile(data);
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
						homeButtons[index]->SetColours(buttonTextColour, (isHomed) ? homedButtonBackColour : notHomedButtonBackColour);
						bool allHomed = axisHomed[0] && axisHomed[1] && axisHomed[2];
						if (allHomed != allAxesHomed)
						{
							allAxesHomed = allHomed;
							homeAllButton->SetColours(buttonTextColour, (allAxesHomed) ? homedButtonBackColour : notHomedButtonBackColour);
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
				if (b && i >= 0 && i < 10 * 24 * 60 * 60 && PrintInProgress())
				{
					timesLeft[index] = i;
					timesLeftText.copyFrom("filament ");
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
			if (status != PrinterStatus::configuring && status != PrinterStatus::connecting)
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
			if (!printingFile.similar(data))
			{
				printingFile.copyFrom(data);
				if (currentTab == tabPrint && PrintInProgress())
				{
					nameField->SetChanged();
				}
			}
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
			if (status != PrinterStatus::configuring && status != PrinterStatus::connecting)
			{
				isDelta = (strcasecmp(data, "delta") == 0);
				gotGeometry = true;
				if (gotMachineName)
				{
					machineConfigTimer.Stop();
				}
				for (size_t i = 0; i < 3; ++i)
				{
					mgr.Show(homeButtons[i], !isDelta);
				}
				bedCompButton->SetText((isDelta) ? "Auto cal" : "Bed Comp");
			}
			break;
		
		case rcvSeq:
			GetUnsignedInteger(data, newMessageSeq);
			break;
		
		case rcvResponse:
			MessageLog::AppendMessage(data);
			break;
		
		case rcvDir:
			FileManager::ReceiveDirectoryName(data);
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
	currentTemps[0]->SetValue(129.0);
	currentTemps[1]->SetValue(299.0);
	currentTemps[2]->SetValue(299.0);
	activeTemps[0]->SetValue(120);
	activeTemps[1]->SetValue(280);
	activeTemps[2]->SetValue(280);
	standbyTemps[1]->SetValue(280);
	standbyTemps[2]->SetValue(280);
	xPos->SetValue(220.9);
	yPos->SetValue(220.9);
	zPos->SetValue(199.99);
//	extrPos->SetValue(999.9);
	zProbe->SetValue("1023 (1023)");
//	fanRPM->SetValue(9999);
	spd->SetValue(169);
	extrusionFactors[0]->SetValue(169);
	extrusionFactors[1]->SetValue(169);
}

void SendRequest(const char *s, bool includeSeq = false)
{
	SerialIo::SendString(s);
	if (includeSeq)
	{
		SerialIo::SendInt(messageSeq);
	}
	SerialIo::SendChar('\n');
	lastPollTime = SystemTick::GetTickCount();
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
	lastTouchTime = SystemTick::GetTickCount();
	
	SysTick_Config(SystemCoreClock / 1000);

	// On prototype boards we need to turn the backlight on
	BacklightPort.setMode(OneBitPort::Output);
	BacklightPort.setHigh();
	
	// Read parameters from flash memory
	nvData.Load();
	if (nvData.Valid())
	{
		// The touch panel has already been calibrated
		InitLcd(nvData.lcdOrientation, is24BitLcd, nvData.language);
		touch.init(DisplayX, DisplayY, nvData.touchOrientation);
		touch.calibrate(nvData.xmin, nvData.xmax, nvData.ymin, nvData.ymax, touchCalibMargin);
		savedNvData = nvData;
	}
	else
	{
		// The touch panel has not been calibrated, and we do not know which way up it is
		nvData.SetDefaults();
		InitLcd(nvData.lcdOrientation, is24BitLcd, nvData.language);
		CalibrateTouch();					// this includes the touch driver initialization
		SaveSettings();
	}
	
	// Set up the baud rate
	SerialIo::Init(nvData.baudRate);
	baudRateButton->SetValue(nvData.baudRate);
	volumeButton->SetValue(nvData.touchVolume);
	
	MessageLog::Init();

	UpdatePrintingFields();

	lastPollTime = SystemTick::GetTickCount() - printerPollInterval;	// allow a poll immediately
	
	// Hide the buttons that are ot implemented yet
	extrudeButton->Show(false);
	fanButton->Show(false);
	
	// Hide the Head 2 parameters until we know we have a second head
	for (unsigned int i = 2; i < maxHeaters; ++i)
	{
		currentTemps[i]->Show(false);
		activeTemps[i]->Show(false);
		standbyTemps[i]->Show(false);
		extrusionFactors[i - 1]->Show(false);
	}
	
	mgr.Show(standbyTemps[0], false);		// currently, we always hide the bed standby temperature because it doesn't do anything
	
	// Display the Control tab
	ChangeTab(tabControl);
	lastResponseTime = SystemTick::GetTickCount();		// pretend we just received a response
	
	machineConfigTimer.SetPending();		// we need to fetch the machine name and configuration
	
	for (;;)
	{
		// 1. Check for input from the serial port and process it.
		// This calls back into functions StartReceivedMessage, ProcessReceivedValue, ProcessArrayLength and EndReceivedMessage.
		SerialIo::CheckInput();
		
		// 2. if displaying the message log, update the times
		if (currentTab == tabMsg)
		{
			MessageLog::UpdateMessages(false);
		}
		
		// 3. Check for a touch on the touch panel.
		if (SystemTick::GetTickCount() - lastTouchTime >= ignoreTouchTime)
		{
			if (currentButton.IsValid())
			{
				CurrentButtonReleased();
			}

			uint16_t x, y;
			if (touch.read(x, y))
			{
				touchX->SetValue((int)x);	//debug
				touchY->SetValue((int)y);	//debug
				ButtonPress bp = mgr.FindEvent(x, y);
				if (bp.IsValid())
				{
					DelayTouchLong();		// by default, ignore further touches for a long time
					if (bp.GetEvent() != evAdjustVolume)
					{
						TouchBeep();		// give audible feedback of the touch, unless adjusting the volume	
					}
					ProcessTouch(bp);
				}
				else
				{
					bp = mgr.FindEventOutsidePopup(x, y);
					if (bp.IsValid() && bp == fieldBeingAdjusted)
					{
						DelayTouchLong();	// by default, ignore further touches for a long time
						TouchBeep();
						ProcessTouchOutsidePopup();					
					}
				}
			}
		}
		
		// 4. Refresh the display
		UpdateDebugInfo();
		mgr.Refresh(false);
		
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
		uint32_t now = SystemTick::GetTickCount();
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
					SendRequest("M408 S0 R", true);					// normal poll response
				}
			}
			else if (now - lastPollTime >= printerPollTimeout)		// if we're giving up on getting a response to the last poll
			{
				SendRequest("M408 S0");								// just send a normal poll message, don't ask for the last response
			}
		}
	}
}

void PrintDebugText(const char *x)
{
	fwVersionField->SetValue(x);
}

// End
