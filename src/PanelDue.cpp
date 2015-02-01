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

// From the display type, we determine the display controller type and touch screen orientation adjustment
#if DISPLAY_TYPE == DISPLAY_TYPE_ITDB02_32WD

# define DISPLAY_CONTROLLER		HX8352A
const DisplayOrientation DefaultDisplayOrientAdjust = static_cast<DisplayOrientation>(SwapXY | ReverseY | InvertBitmap);
const DisplayOrientation DefaultTouchOrientAdjust = static_cast<DisplayOrientation>(ReverseY);
# define DISPLAY_X				(400)
# define DISPLAY_Y				(240)

#elif DISPLAY_TYPE == DISPLAY_TYPE_ITDB02_43

# define DISPLAY_CONTROLLER		SSD1963_480
const DisplayOrientation DefaultDisplayOrientAdjust = static_cast<DisplayOrientation>(SwapXY | ReverseY | InvertBitmap);
const DisplayOrientation DefaultTouchOrientAdjust = SwapXY;
# define DISPLAY_X				(480)
# define DISPLAY_Y				(272)

#elif DISPLAY_TYPE == DISPLAY_TYPE_INVERTED_43

# define DISPLAY_CONTROLLER		SSD1963_480
const DisplayOrientation DefaultDisplayOrientAdjust = static_cast<DisplayOrientation>(SwapXY | ReverseX | InvertText);
const DisplayOrientation DefaultTouchOrientAdjust = static_cast<DisplayOrientation>(SwapXY);
# define DISPLAY_X				(480)
# define DISPLAY_Y				(272)

#elif DISPLAY_TYPE == DISPLAY_TYPE_ITDB02_50

# define DISPLAY_CONTROLLER		SSD1963_800
const DisplayOrientation DefaultDisplayOrientAdjust = static_cast<DisplayOrientation>(SwapXY | ReverseX | InvertText);
const DisplayOrientation DefaultTouchOrientAdjust = static_cast<DisplayOrientation>(SwapXY | ReverseY);
# define DISPLAY_X				(800)
# define DISPLAY_Y				(480)

#else
# error DISPLAY_TYPE is not defined correctly
#endif

const PixelNumber DisplayX = DISPLAY_X;
const PixelNumber DisplayY = DISPLAY_Y;

// Define the row and column positions. Leave a gap of at least 1 pixel from the edges of the screen, so that we can highlight
// a field by drawing an outline.

#if DISPLAY_X == 400

const PixelNumber margin = 1;
const PixelNumber outlinePixels = 1;
const PixelNumber fieldSpacing = 4;

const PixelNumber column1 = margin;
const PixelNumber column2 = 72;
const PixelNumber column3 = 146;
const PixelNumber column4 = 208;
const PixelNumber column5 = 270;

const PixelNumber columnX = 275;
const PixelNumber columnY = 333;

const PixelNumber columnEnd = DisplayX;

const PixelNumber rowCommon1 = margin;
const PixelNumber rowHeight = 22;

const PixelNumber rowTabs = DisplayY - 22 - margin;	// place at bottom of screen with a 1-pixel margin

#elif DISPLAY_X >= 480

const PixelNumber margin = 2;
const PixelNumber outlinePixels = 2;
const PixelNumber fieldSpacing = 5;

const PixelNumber column1 = margin;
const PixelNumber column2 = 83;
const PixelNumber column3 = 167;
const PixelNumber column4 = 230;
const PixelNumber column5 = 311;

const PixelNumber columnX = 326;
const PixelNumber columnY = 400;

const PixelNumber columnEnd = DisplayX;

const PixelNumber rowCommon1 = margin;
const PixelNumber rowHeight = 24;

const PixelNumber rowTabs = DisplayY - 22 - margin;	// place at bottom of screen with a 2-pixel margin

#endif

const PixelNumber rowCommon2 = rowCommon1 + rowHeight;
const PixelNumber rowCommon3 = rowCommon2 + rowHeight;
const PixelNumber rowCommon4 = rowCommon3 + rowHeight;
const PixelNumber rowCommon5 = rowCommon4 + rowHeight;
const PixelNumber rowCustom1 = rowCommon5 + rowHeight + 4;
const PixelNumber rowCustom2 = rowCustom1 + rowHeight;
const PixelNumber rowCustom3 = rowCustom2 + rowHeight;
const PixelNumber rowCustom4 = rowCustom3 + rowHeight;

const PixelNumber columnTabWidth = (DisplayX - 2*margin - 4*fieldSpacing)/5;

const PixelNumber columnTab1 = margin;
const PixelNumber columnTab2 = columnTab1 + columnTabWidth + fieldSpacing;
const PixelNumber columnTab3 = columnTab2 + columnTabWidth + fieldSpacing;
const PixelNumber columnTab4 = columnTab3 + columnTabWidth + fieldSpacing;
const PixelNumber columnTab5 = columnTab4 + columnTabWidth + fieldSpacing;

const PixelNumber xyPopupX = 3, xyPopupY = 195;
const PixelNumber tempPopupX = 35, tempPopupY = 195;
const PixelNumber filePopupWidth = DisplayX - 40, filePopupHeight = 8 * rowHeight + 20;

const uint32_t numFileColumns = 2;
const uint32_t numFileRows = (DisplayY - margin)/rowHeight - 3;
const uint32_t numDisplayedFiles = numFileColumns * numFileRows;

// Declare which fonts we will be using
//extern uint8_t glcd16x16[];
extern uint8_t glcd19x20[];

#define DEGREE_SYMBOL	"\x81"

UTFT lcd(DISPLAY_CONTROLLER, TMode16bit, 16, 17, 18, 19);

UTouch touch(23, 24, 22, 21, 20);
DisplayManager mgr;

const uint32_t printerPollInterval = 2000;			// poll interval in milliseconds
const uint32_t printerPollTimeout = 5000;			// poll timeout in milliseconds
const uint32_t touchBeepLength = 20;				// beep length in ms
const uint32_t touchBeepFrequency = 4500;			// beep frequency in Hz. Resonant frequency of the piezo sounder is 4.5kHz.
const uint32_t longTouchDelay = 200;				// how long we ignore new touches for after pressing Set
const uint32_t shortTouchDelay = 80;				// how long we ignore new touches while pressing up/down, to get a reasonable repeat rate

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

String<15> machineName;
String<40> printingFile;
String<10> zprobeBuf;
String<30> generatedByText;
Vector<char, 2048> fileList;						// we use a Vector instead of a String because we store multiple null-terminated strings in it
Vector<const char* array, 100> fileIndex;			// pointers into the individual filenames in the list
static const char* array null currentFile = NULL;	// file whose info is displayed in the popup menu

static OneBitPort BacklightPort(33);				// PB1 (aka port 33) controls the backlight on the prototype

struct FlashData
{
	uint32_t magic;
	uint32_t baudRate;
	int16_t xmin;
	int16_t xmax;
	int16_t ymin;
	int16_t ymax;
	DisplayOrientation lcdOrientation;
	DisplayOrientation touchOrientation;
};

// When we can switch to C++11, uncomment this line
//static_assert(sizeof(FlashData) <= FLASH_DATA_LENGTH);

FlashData nvData;
const uint32_t magicVal = 0x3AB629D1;
const uint32_t muggleVal = 0xFFFFFFFF;

enum PrinterStatus
{
	psUnknown = 0,
	psIdle = 1,
	psPrinting = 2,
	psStopped = 3,
	psConfiguring = 4
};

static PrinterStatus status = psUnknown;

const Color activeBackColor = red;
const Color standbyBackColor = yellow;
const Color defaultBackColor = blue;
const Color errorBackColour = magenta;
const Color selectableBackColor = black;
const Color outlineColor = green;
const Color popupBackColour = green;
const Color selectablePopupBackColour = UTFT::fromRGB(0, 128, 0);		// dark green
const Color homedBackColour = UTFT::fromRGB(0, 0, 100);					// dark blue
const Color notHomedBackColour = UTFT::fromRGB(128, 64, 9);				// orange

// Event numbers, used to say what we need to do when s field is touched
const Event evTabControl = 1,
			evTabPrint = 2,
			evTabFiles = 3,
			evTabMsg = 4,
			evTabInfo = 5,
			evAdjustTemp = 6,
			evAdjustInt = 7,
			evAdjustPosition = 8,
			evSetInt = 9,
			evCalTouch = 10,
			evSelectHead = 11,
			evXYPos = 12,
			evZPos = 13,
			evFile = 14,
			evPrint = 15, evCancelPrint = 16,
			evSendCommand = 17,
			evCalClear = 18,
			evAdjustPercent = 19,
			evScrollFiles = 20,
			evSetBaudRate = 21,
			evInvertDisplay = 22,
			evAdjustBaudRate = 23;

static FloatField *bedCurrentTemp, *t1CurrentTemp, *t2CurrentTemp, *xPos, *yPos, *zPos, *fpHeightField, *fpLayerHeightField;
static IntegerField *bedActiveTemp, *t1ActiveTemp, *t2ActiveTemp, *t1StandbyTemp, *t2StandbyTemp, *spd, *e1Percent, *e2Percent;
static IntegerField *bedStandbyTemp, /* *fanRPM,*/ *freeMem, *touchX, *touchY, *fpSizeField, *fpFilamentField;
static ProgressBar *printProgressBar;
static StaticTextField *nameField, *head1State, *head2State, *bedState, *tabControl, *tabPrint, *tabFiles, *tabMsg, *tabInfo, *touchCalibInstruction;
static StaticTextField *filenameFields[numDisplayedFiles], *scrollFilesLeftField, *scrollFilesRightField;
static StaticTextField *homeFields[3], *homeAllField, *fwVersionField;
static DisplayField *baseRoot, *commonRoot, *controlRoot, *printRoot, *filesRoot, *messageRoot, *infoRoot;
static DisplayField * null currentTab = NULL;
static DisplayField * null fieldBeingAdjusted = NULL;
static PopupField *setTempPopup, *setXYPopup, *setZPopup, *filePopup, *baudPopup;
//static TextField *commandField;
static TextField *zProbe, *fpNameField, *fpGeneratedByField, *printingField;

//char commandBuffer[80];

const size_t numHeaters = 3;
int heaterStatus[numHeaters];

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
	mgr.Show(scrollFilesRightField, fileScrollOffset + numFileRows < fileIndex.size());
	
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

void changeTab(DisplayField *newTab)
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

// Add a command cell
StaticTextField *AddCommandCell(PixelNumber row, unsigned int col, unsigned int numCols, const char* text, Event evt, const char* param)
{
	PixelNumber width = (DisplayX - 2 * margin + fieldSpacing)/numCols - fieldSpacing;
	PixelNumber xpos = col * (width + fieldSpacing) + margin;
	StaticTextField *f = new StaticTextField(row, xpos, width, Centre, text);
	f->SetEvent(evt, param);
	mgr.AddField(f);
	return f;
}

void InitLcd(DisplayOrientation dor)
{
	// Setup the LCD
	lcd.InitLCD(dor);
	mgr.Init(defaultBackColor);
	DisplayField::SetDefaultFont(glcd19x20);
	
	// Create the fields that are always displayed
	DisplayField::SetDefaultColours(white, selectableBackColor);
	tabControl = AddCommandCell(rowTabs, 0, 5, "Control", evTabControl, nullptr);
	tabPrint = AddCommandCell(rowTabs, 1, 5, "Print", evTabPrint, nullptr);
	tabFiles = AddCommandCell(rowTabs, 2, 5, "Files", evTabFiles, nullptr);
	tabMsg = AddCommandCell(rowTabs, 3, 5, "Msg", evTabMsg, nullptr);
	tabInfo = AddCommandCell(rowTabs, 4, 5, "Setup", evTabInfo, nullptr);
	
	baseRoot = mgr.GetRoot();		// save the root of fields that we usually display
	
	DisplayField::SetDefaultColours(white, red);
	
	mgr.AddField(nameField = new StaticTextField(rowCommon1, 0, lcd.getDisplayXSize(), Centre, machineName.c_str()));
	DisplayField::SetDefaultColours(white, defaultBackColor);
	
	mgr.AddField(new StaticTextField(rowCommon2, column2, column3 - column2 - fieldSpacing, Left, "Current"));
	mgr.AddField(new StaticTextField(rowCommon2, column3, column4 - column3 - fieldSpacing, Left, "Active"));
	mgr.AddField(new StaticTextField(rowCommon2, column4, column5 - column4 - fieldSpacing, Left, "St'by"));

	DisplayField::SetDefaultColours(white, selectableBackColor);
	mgr.AddField(head2State = new StaticTextField(rowCommon4, column1, column2 - column1 - fieldSpacing, Left, "Head 2"));
	mgr.AddField(head1State = new StaticTextField(rowCommon3, column1, column2 - column1 - fieldSpacing, Left, "Head 1"));
	mgr.AddField(bedState = new StaticTextField(rowCommon5, column1, column2 - column1 - fieldSpacing, Left, "Bed"));
	head1State->SetEvent(evSelectHead, 1);
	head2State->SetEvent(evSelectHead, 2);
	bedState->SetEvent(evSelectHead, 0);

	mgr.AddField(t1ActiveTemp = new IntegerField(rowCommon3, column3, column4 - column3 - fieldSpacing, NULL, DEGREE_SYMBOL "C"));
	mgr.AddField(t1StandbyTemp = new IntegerField(rowCommon3, column4, column5 - column4 - fieldSpacing, NULL, DEGREE_SYMBOL "C"));
	mgr.AddField(t2ActiveTemp = new IntegerField(rowCommon4, column3, column4 - column3 - fieldSpacing, NULL, DEGREE_SYMBOL "C"));
	mgr.AddField(t2StandbyTemp = new IntegerField(rowCommon4, column4, column5 - column4 - fieldSpacing, NULL, DEGREE_SYMBOL "C"));
	mgr.AddField(bedActiveTemp = new IntegerField(rowCommon5, column3, column4 - column3 - fieldSpacing, NULL, DEGREE_SYMBOL "C"));
	t1ActiveTemp->SetEvent(evAdjustTemp, "G10 P1 S");
	t1StandbyTemp->SetEvent(evAdjustTemp, "G10 P1 R");
	t2ActiveTemp->SetEvent(evAdjustTemp, "G10 P2 S");
	t2StandbyTemp->SetEvent(evAdjustTemp, "G10 P2 R");
	bedActiveTemp->SetEvent(evAdjustTemp, "M140 S");

	DisplayField::SetDefaultColours(white, defaultBackColor);
	mgr.AddField(t1CurrentTemp = new FloatField(rowCommon3, column2, column3 - column2 - fieldSpacing, NULL, 1, DEGREE_SYMBOL "C"));
	mgr.AddField(t2CurrentTemp = new FloatField(rowCommon4, column2, column3 - column2 - fieldSpacing, NULL, 1, DEGREE_SYMBOL "C"));

	mgr.AddField(bedCurrentTemp = new FloatField(rowCommon5, column2, column3 - column2 - fieldSpacing, NULL, 1, DEGREE_SYMBOL "C"));
	mgr.AddField(bedStandbyTemp = new IntegerField(rowCommon5, column4, column5 - column4 - fieldSpacing, NULL, DEGREE_SYMBOL "C"));

	mgr.AddField(new StaticTextField(rowCommon2, columnX, columnY - columnX - fieldSpacing, Left, "X"));
	mgr.AddField(new StaticTextField(rowCommon2, columnY, columnEnd - columnY - fieldSpacing, Left, "Y"));
	mgr.AddField(new StaticTextField(rowCommon4, columnX, columnY - columnX - fieldSpacing, Left, "Z"));
	mgr.AddField(new StaticTextField(rowCommon4, columnY, columnEnd - columnY - fieldSpacing, Left, "Probe"));

	DisplayField::SetDefaultColours(white, selectableBackColor);
	mgr.AddField(xPos = new FloatField(rowCommon3, columnX, columnY - columnX - fieldSpacing, NULL, 1));
	mgr.AddField(yPos = new FloatField(rowCommon3, columnY, columnEnd - columnY - fieldSpacing, NULL, 1));
	mgr.AddField(zPos = new FloatField(rowCommon5, columnX, columnY - columnX - fieldSpacing, NULL, 2));
	xPos->SetEvent(evXYPos, "G1 X");
	yPos->SetEvent(evXYPos, "G1 Y");
	zPos->SetEvent(evZPos, "G1 Z");

	zprobeBuf[0] = 0;
	DisplayField::SetDefaultColours(white, defaultBackColor);
	mgr.AddField(zProbe = new TextField(rowCommon5, columnY, columnEnd - columnY - fieldSpacing, NULL, zprobeBuf.c_str()));
	
	commonRoot = mgr.GetRoot();		// save the root of fields that we usually display
	
	// Create the fields for the Control tab
	DisplayField::SetDefaultColours(white, notHomedBackColour);
	homeAllField = AddCommandCell(rowCustom1, 0, 5, "Home all", evSendCommand, "G28");
	homeFields[0] = AddCommandCell(rowCustom1, 1, 5, "Home X", evSendCommand, "G28 X0");
	homeFields[1] = AddCommandCell(rowCustom1, 2, 5, "Home Y", evSendCommand, "G28 Y0");
	homeFields[2] = AddCommandCell(rowCustom1, 3, 5, "Home Z", evSendCommand, "G28 Z0");
	
	DisplayField::SetDefaultColours(white, selectableBackColor);
	AddCommandCell(rowCustom4, 0, 5, "G92 Z0", evSendCommand, "G92 Z0");
	AddCommandCell(rowCustom4, 1, 5, "G1 X0 Y0", evSendCommand, "G1 X0 Y0 F5000");
	AddCommandCell(rowCustom4, 2, 5, "G1 Z1", evSendCommand, "G1 Z1 F5000");
	AddCommandCell(rowCustom4, 3, 5, "G32", evSendCommand, "G32");
	
	controlRoot = mgr.GetRoot();

	// Create the fields for the Printing tab
	mgr.SetRoot(commonRoot);
	DisplayField::SetDefaultColours(white, defaultBackColor);
	mgr.AddField(new StaticTextField(rowCustom1, 0, 70, Left, "Speed"));
	DisplayField::SetDefaultColours(white, selectableBackColor);
	mgr.AddField(spd = new IntegerField(rowCustom1, 70, 60, "", "%"));
	spd->SetValue(100);
	spd->SetEvent(evAdjustPercent, "M220 S");
	DisplayField::SetDefaultColours(white, defaultBackColor);
	mgr.AddField(new StaticTextField(rowCustom1, 140, 30, Left, "E1"));
	DisplayField::SetDefaultColours(white, selectableBackColor);
	mgr.AddField(e1Percent = new IntegerField(rowCustom1, 170, 60, "", "%"));
	e1Percent->SetValue(100);
	e1Percent->SetEvent(evAdjustPercent, "M221 D0 S");
	DisplayField::SetDefaultColours(white, defaultBackColor);
	mgr.AddField(new StaticTextField(rowCustom1, 250, 30, Left, "E2"));
	DisplayField::SetDefaultColours(white, selectableBackColor);
	mgr.AddField(e2Percent = new IntegerField(rowCustom1, 280, 60, "", "%"));
	e2Percent->SetValue(100);
	e2Percent->SetEvent(evAdjustPercent, "M221 D1 S");
	
	DisplayField::SetDefaultColours(white, defaultBackColor);
	printingFile.copyFrom("(unknown)");
	mgr.AddField(printingField = new TextField(rowCustom2, 0, DisplayX, "Printing ", printingFile.c_str()));
	
	DisplayField::SetDefaultColours(white, UTFT::fromRGB(0, 160, 0));
	mgr.AddField(printProgressBar = new ProgressBar(rowCustom3, margin, 8, DisplayX - 2*margin));
	mgr.Show(printProgressBar, false);

	printRoot = mgr.GetRoot();

	// Create the fields for the Files tab
	mgr.SetRoot(baseRoot);
	DisplayField::SetDefaultColours(white, defaultBackColor);
	mgr.AddField(new StaticTextField(rowCommon1, 135, DisplayX - 2*135, Centre, "Files on SD card"));
	{
		PixelNumber fileFieldWidth = (DisplayX + fieldSpacing - 2*margin)/numFileColumns;
		unsigned int fileNum = 0;
		for (unsigned int c = 0; c < numFileColumns; ++c)
		{
			for (unsigned int r = 0; r < numFileRows; ++r)
			{
				StaticTextField *t = new StaticTextField(((r + 1) * rowHeight) + 8, (fileFieldWidth * c) + margin, fileFieldWidth - fieldSpacing, Left, "");
				mgr.AddField(t);
				filenameFields[fileNum] = t;
				++fileNum;
			}
		}
	}
	DisplayField::SetDefaultColours(white, selectableBackColor);
	mgr.AddField(scrollFilesLeftField = new StaticTextField(rowCommon1, 80, 50, Centre, "<"));
	scrollFilesLeftField->SetEvent(evScrollFiles, -numFileRows);
	mgr.AddField(scrollFilesRightField = new StaticTextField(rowCommon1, DisplayX - (80 + 50), 50, Centre, ">"));
	scrollFilesRightField->SetEvent(evScrollFiles, numFileRows);
	filesRoot = mgr.GetRoot();

	// Create the fields for the Message tab
	mgr.SetRoot(commonRoot);
	DisplayField::SetDefaultColours(white, defaultBackColor);
	messageRoot = mgr.GetRoot();

	// Create the fields for the Setup tab
	mgr.SetRoot(baseRoot);
	DisplayField::SetDefaultColours(white, defaultBackColor);
	// The firmware version field doubles up as an area for displaying debug messages, so make it the full width of the display
	mgr.AddField(fwVersionField = new StaticTextField(rowCommon1, margin, DisplayX, Left,"Panel Due firmware version " VERSION_TEXT));
	mgr.AddField(freeMem = new IntegerField(rowCommon2, margin, 195, "Free RAM: "));
	mgr.AddField(touchX = new IntegerField(rowCommon2, 200, 130, "Touch: ", ","));
	mgr.AddField(touchY = new IntegerField(rowCommon2, 330, 50, ""));

	DisplayField::SetDefaultColours(white, selectableBackColor);
	DisplayField *touchCal = new StaticTextField(rowCustom1, DisplayX/2 - 75, 150, Centre, "Calibrate touch");
	touchCal->SetEvent(evCalTouch, 0);
	mgr.AddField(touchCal);

	AddCommandCell(rowCustom3, 0, 3, "Baud rate", evSetBaudRate, nullptr);
	AddCommandCell(rowCustom3, 1, 3, "Factory reset", evCalClear, nullptr);
	AddCommandCell(rowCustom3, 2, 3, "Invert display", evInvertDisplay, nullptr);
	
	DisplayField::SetDefaultColours(white, defaultBackColor);
	infoRoot = mgr.GetRoot();
	
	mgr.SetRoot(commonRoot);
	
	touchCalibInstruction = new StaticTextField(DisplayY/2 - 10, 0, DisplayX, Centre, "");		// the text is filled in within CalibrateTouch

	// Create the popup window used to adjust temperatures
	setTempPopup = new PopupField(40, 330, popupBackColour);
	DisplayField::SetDefaultColours(white, selectablePopupBackColour);
	DisplayField *tp = new StaticTextField(10, 5, 60, Centre, "-10");
	tp->SetEvent(evAdjustInt, -10);
	setTempPopup->AddField(tp);
	tp = new StaticTextField(10, 70, 60, Centre, "-1");
	tp->SetEvent(evAdjustInt, -1);
	setTempPopup->AddField(tp);
	tp = new StaticTextField(10, 135, 60, Centre, "Set");
	tp->SetEvent(evSetInt, 0);
	setTempPopup->AddField(tp);
	tp = new StaticTextField(10, 200, 60, Centre, "+1");
	tp->SetEvent(evAdjustInt, 1);
	setTempPopup->AddField(tp);
	tp = new StaticTextField(10, 265, 60, Centre, "+10");
	tp->SetEvent(evAdjustInt, 10);
	setTempPopup->AddField(tp);

	// Create the popup window used to adjust XY position
	setXYPopup = new PopupField(40, 395, popupBackColour);
	DisplayField::SetDefaultColours(white, selectablePopupBackColour);
	
	tp = new StaticTextField(10, 5, 60, Centre, "-50");
	tp->SetEvent(evAdjustPosition, "-50");
	setXYPopup->AddField(tp);
	tp = new StaticTextField(10, 70, 60, Centre, "-5");
	tp->SetEvent(evAdjustPosition, "-5");
	setXYPopup->AddField(tp);
	tp = new StaticTextField(10, 135, 60, Centre, "-0.5");
	tp->SetEvent(evAdjustPosition, "-0.5");
	setXYPopup->AddField(tp);
	tp = new StaticTextField(10, 200, 60, Centre, "+0.5");
	tp->SetEvent(evAdjustPosition, "0.5");
	setXYPopup->AddField(tp);
	tp = new StaticTextField(10, 265, 60, Centre, "+5");
	tp->SetEvent(evAdjustPosition, "5");
	setXYPopup->AddField(tp);
	tp = new StaticTextField(10, 330, 60, Centre, "+50");
	tp->SetEvent(evAdjustPosition, "50");
	setXYPopup->AddField(tp);

	// Create the popup window used to adjust Z position
	setZPopup = new PopupField(40, 395, popupBackColour);
	DisplayField::SetDefaultColours(white, selectablePopupBackColour);
	
	tp = new StaticTextField(10, 5, 60, Centre, "-5");
	tp->SetEvent(evAdjustPosition, "-5");
	setZPopup->AddField(tp);
	tp = new StaticTextField(10, 70, 60, Centre, "-0.5");
	tp->SetEvent(evAdjustPosition, "-0.5");
	setZPopup->AddField(tp);
	tp = new StaticTextField(10, 135, 60, Centre, "-0.05");
	tp->SetEvent(evAdjustPosition, "-0.05");
	setZPopup->AddField(tp);
	tp = new StaticTextField(10, 200, 60, Centre, "+0.05");
	tp->SetEvent(evAdjustPosition, "0.05");
	setZPopup->AddField(tp);
	tp = new StaticTextField(10, 265, 60, Centre, "+0.5");
	tp->SetEvent(evAdjustPosition, "0.5");
	setZPopup->AddField(tp);
	tp = new StaticTextField(10, 330, 60, Centre, "+5");
	tp->SetEvent(evAdjustPosition, "5");
	setZPopup->AddField(tp);
	
	// Create the popup window used to display the file dialog
	filePopup = new PopupField(filePopupHeight, filePopupWidth, popupBackColour);
	DisplayField::SetDefaultColours(black, popupBackColour);

	fpNameField = new TextField(10, 10, filePopupWidth - 20, "Filename: ", "");
	fpSizeField = new IntegerField(10 + rowHeight, 10, filePopupWidth - 20, "Size: ", " bytes");
	fpLayerHeightField = new FloatField(10 + 2 * rowHeight, 10, filePopupWidth - 20, "Layer height: ", 1, "mm");
	fpHeightField = new FloatField(10 + 3 * rowHeight, 10, filePopupWidth - 20, "Object height: ", 1, "mm");
	fpFilamentField = new IntegerField(10 + 4 * rowHeight, 10, filePopupWidth - 20, "Filament needed: ", "mm");
	fpGeneratedByField = new TextField(10 + 5 * rowHeight, 10, filePopupWidth - 20, "Generated by: ", generatedByText.c_str());
	filePopup->AddField(fpNameField);
	filePopup->AddField(fpSizeField);
	filePopup->AddField(fpLayerHeightField);
	filePopup->AddField(fpHeightField);
	filePopup->AddField(fpFilamentField);
	filePopup->AddField(fpGeneratedByField);

	DisplayField::SetDefaultColours(white, selectablePopupBackColour);
	tp = new StaticTextField(10 + 7 * rowHeight, 10, filePopupWidth/2 - 20, Centre, "Print");
	tp->SetEvent(evPrint, 0);
	filePopup->AddField(tp);
	tp = new StaticTextField(10 + 7 * rowHeight, filePopupWidth/2 + 10, filePopupWidth/2 - 20, Centre, "Cancel");
	tp->SetEvent(evCancelPrint, 0);
	filePopup->AddField(tp);

	// Create the baud rate adjustment popup
	baudPopup = new PopupField(40, 395, popupBackColour);
	DisplayField::SetDefaultColours(black, popupBackColour);
	tp = new StaticTextField(10, 5, 70, Centre, "9600");
	tp->SetEvent(evAdjustBaudRate, 9600);
	baudPopup->AddField(tp);
	tp = new StaticTextField(10, 80, 70, Centre, "19200");
	tp->SetEvent(evAdjustBaudRate, 19200);
	baudPopup->AddField(tp);
	tp = new StaticTextField(10, 155, 70, Centre, "38400");
	tp->SetEvent(evAdjustBaudRate, 38400);
	baudPopup->AddField(tp);
	tp = new StaticTextField(10, 230, 70, Centre, "57600");
	tp->SetEvent(evAdjustBaudRate, 57600);
	baudPopup->AddField(tp);
	tp = new StaticTextField(10, 305, 70, Centre, "115200");
	tp->SetEvent(evAdjustBaudRate, 115200);
	baudPopup->AddField(tp);
	
	// Redraw everything
	mgr.RefreshAll(true);

	// Set initial values
	bedCurrentTemp->SetValue(0.0);
	t1CurrentTemp->SetValue(0.0);
	t2CurrentTemp->SetValue(0.01);
	bedActiveTemp->SetValue(0);
	t1ActiveTemp->SetValue(0);
	t2ActiveTemp->SetValue(0);
	t1StandbyTemp->SetValue(0);
	t2StandbyTemp->SetValue(0);
	xPos->SetValue(0.0);
	yPos->SetValue(0.0);
	zPos->SetValue(0.0);
	//  extrPos->SetValue(43.6);
	//  fanRPM->SetValue(2354);
	spd->SetValue(100);
	e1Percent->SetValue(100);
	e2Percent->SetValue(100);

	currentTab = NULL;
}

// Ignore touches for a little while
void delayTouch(uint32_t ms)
{
	lastTouchTime = GetTickCount();
	ignoreTouchTime = ms;
}

void TouchBeep()
{
	Buzzer::Beep(touchBeepFrequency, touchBeepLength);	
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

void ClearNvData()
{
	nvData.baudRate = DEFAULT_BAUD_RATE;
	nvData.xmin = 0;
	nvData.xmax = DisplayX - 1;
	nvData.ymin = 0;
	nvData.ymax = DisplayY - 1;
	nvData.lcdOrientation = DefaultDisplayOrientAdjust;
	nvData.touchOrientation = DefaultTouchOrientAdjust;
	nvData.magic = magicVal;
}

void WriteNvData()
{
	FlashStorage::write(0, &nvData, sizeof(nvData));	
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
	
	// Writing flash storage - or even re-locking it - sometimes crashes deep in the ASF, although the memory gets written anyway.
	// So wait for the beep to finish, instruct the user to press the reset button, and then write the flash
	while (Buzzer::Noisy()) { }
	touchCalibInstruction->SetValue("Press reset button, or disconnect/reconnect power");
	mgr.RefreshAll();

	WriteNvData();
	
	mgr.SetRoot(oldRoot);
	mgr.ClearAll();
	mgr.RefreshAll(true);
}

// Clear the touch calibration
void ClearCalibration()
{
	while (Buzzer::Noisy()) { }
	nvData.magic = muggleVal;
	FlashStorage::write(0, &nvData, sizeof(nvData));
	Buzzer::Beep(touchBeepFrequency, 400);			// long beep to acknowledge it
}

// Process a touch event
void ProcessTouch(DisplayField *f)
{
	switch(f->GetEvent())
	{
	case evTabControl:
	case evTabPrint:
	case evTabFiles:
	case evTabMsg:
	case evTabInfo:
		changeTab(f);
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
		delayTouch(longTouchDelay);
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
			delayTouch(longTouchDelay);
		}
		break;

	case evAdjustInt:
		if (fieldBeingAdjusted != NULL)
		{
			static_cast<IntegerField*>(fieldBeingAdjusted)->Increment(f->GetIParam());
			delayTouch(shortTouchDelay);
		}
		break;

	case evAdjustPosition:
		if (fieldBeingAdjusted != NULL)
		{
			SerialIo::SendString("G91\n");
			SerialIo::SendString(fieldBeingAdjusted->GetSParam());
			SerialIo::SendString(f->GetSParam());
			SerialIo::SendString(" F6000\nG90\n");
			delayTouch(longTouchDelay);
		}
		break;

	case evCalTouch:
		CalibrateTouch();
		break;

	case evCalClear:
		ClearCalibration();
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

		delayTouch(longTouchDelay);
		break;
		
	case evXYPos:
		mgr.Outline(f, outlineColor, outlinePixels);
		mgr.SetPopup(setXYPopup, xyPopupX, xyPopupY);
		fieldBeingAdjusted = f;
		delayTouch(longTouchDelay);
		break;

	case evZPos:
		mgr.Outline(f, outlineColor, outlinePixels);
		mgr.SetPopup(setZPopup, xyPopupX, xyPopupY);
		fieldBeingAdjusted = f;
		delayTouch(longTouchDelay);
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
		mgr.SetPopup(filePopup, (lcd.getDisplayXSize() - filePopupWidth)/2, (lcd.getDisplayYSize() - filePopupHeight)/2);
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
			changeTab(tabPrint);
		}
		break;

	case evCancelPrint:
		mgr.SetPopup(NULL);
		currentFile = NULL;				// allow the file list to be updated
		break;

	case evSendCommand:
		SerialIo::SendString(f->GetSParam());
		SerialIo::SendChar('\n');
		delayTouch(longTouchDelay);
		break;

	case evScrollFiles:
		fileScrollOffset += f->GetIParam();
		fileListChanged = true;
		delayTouch(shortTouchDelay);
		break;

	case evInvertDisplay:
		delayTouch(longTouchDelay);
		nvData.lcdOrientation = static_cast<DisplayOrientation>(nvData.lcdOrientation ^ (ReverseX | ReverseY | InvertText | InvertBitmap));
		lcd.InitLCD(nvData.lcdOrientation);
		CalibrateTouch();
		break;

	case evSetBaudRate:
		mgr.Outline(f, outlineColor, outlinePixels);
		mgr.SetPopup(baudPopup, xyPopupX, xyPopupY);
		fieldBeingAdjusted = f;
		delayTouch(longTouchDelay);
		break;

	case evAdjustBaudRate:
		nvData.baudRate = f->GetIParam();
		SerialIo::Init(nvData.baudRate);
		WriteNvData();
		mgr.RemoveOutline(fieldBeingAdjusted, outlinePixels);
		mgr.SetPopup(NULL);
		fieldBeingAdjusted = NULL;
		delayTouch(longTouchDelay);
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
		mgr.RemoveOutline(fieldBeingAdjusted, outlinePixels);
		mgr.SetPopup(NULL);
		fieldBeingAdjusted = NULL;
		delayTouch(longTouchDelay);
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

	// Read parameters from flash memory (currently, just the touch calibration)
	nvData.magic = 0;
	FlashStorage::read(0, &nvData, sizeof(nvData));
	if (nvData.magic == magicVal)
	{
		// The touch panel has already been calibrated
		InitLcd(nvData.lcdOrientation);
		touch.init(DisplayX, DisplayY, nvData.touchOrientation);
		touch.calibrate(nvData.xmin, nvData.xmax, nvData.ymin, nvData.ymax);
	}
	else
	{
		// The touch panel has not been calibrated, and we do not know which way up it is
		ClearNvData();
		InitLcd(nvData.lcdOrientation);
		CalibrateTouch();					// this includes the touch driver initialization, and it writes the flash data
	}
	
	SerialIo::Init(nvData.baudRate);

	uint32_t lastPollTime = GetTickCount() - printerPollInterval;
	lastResponseTime = GetTickCount();		// pretend we just received a response
	changeTab(tabControl);
	
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
					TouchBeep();		// give audible feedback of the touch
					ProcessTouch(f);
				}
				else
				{
					f = mgr.FindEventOutsidePopup(x, y);
					if (f != NULL && f == fieldBeingAdjusted)
					{
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
				Buzzer::Beep(beepFrequency, beepLength);
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
