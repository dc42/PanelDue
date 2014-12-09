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

#define DISPLAY_TYPE_ITDB02_32WD	(0)					// Itead 3.2 inch widescreen display (400x240)
#define DISPLAY_TYPE_ITDB02_43		(1)					// Itead 4.3 inch display (480 x 272)
#define DISPLAY_TYPE_ITDB02_50		(2)					// Itead 5.0 inch display (800 x 480)

// Define DISPLAY_TYPE to be one of the above 3 types of display
//#define DISPLAY_TYPE	DISPLAY_TYPE_ITDB02_32WD
//#define DISPLAY_TYPE	DISPLAY_TYPE_ITDB02_43
#define DISPLAY_TYPE	DISPLAY_TYPE_ITDB02_50

// From the display type, we determine the display controller type and touch screen orientation adjustment
#if DISPLAY_TYPE == DISPLAY_TYPE_ITDB02_32WD

# define DISPLAY_CONTROLLER		HX8352A
const DisplayOrientation DisplayOrientAdjust = static_cast<DisplayOrientation>(SwapXY | ReverseY | InvertBitmap);
const DisplayOrientation TouchOrientAdjust = static_cast<DisplayOrientation>(SwapXY | ReverseY);
# define DISPLAY_X				(400)
# define DISPLAY_Y				(240)

#elif DISPLAY_TYPE == DISPLAY_TYPE_ITDB02_43

# define DISPLAY_CONTROLLER		SSD1963_480
const DisplayOrientation DisplayOrientAdjust = static_cast<DisplayOrientation>(SwapXY | ReverseY | InvertBitmap);
const DisplayOrientation TouchOrientAdjust = Default;
# define DISPLAY_X				(480)
# define DISPLAY_Y				(272)

#elif DISPLAY_TYPE == DISPLAY_TYPE_ITDB02_50

# define DISPLAY_CONTROLLER		SSD1963_800
const DisplayOrientation DisplayOrientAdjust = static_cast<DisplayOrientation>(SwapXY | ReverseX | InvertText);
const DisplayOrientation TouchOrientAdjust = static_cast<DisplayOrientation>(ReverseY);
# define DISPLAY_X				(800)
# define DISPLAY_Y				(480)

#else
# error DISPLAY_TYPE is not defined correctly
#endif

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

const PixelNumber columnEnd = DISPLAY_X;

const PixelNumber rowCommon1 = margin;
const PixelNumber rowHeight = 22;

const PixelNumber rowTabs = DISPLAY_Y - 22 - margin;	// place at bottom of screen with a 1-pixel margin

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

const PixelNumber columnEnd = DISPLAY_X;

const PixelNumber rowCommon1 = margin;
const PixelNumber rowHeight = 24;

const PixelNumber rowTabs = DISPLAY_Y - 22 - margin;	// place at bottom of screen with a 2-pixel margin

#endif

const PixelNumber rowCommon2 = rowCommon1 + rowHeight;
const PixelNumber rowCommon3 = rowCommon2 + rowHeight;
const PixelNumber rowCommon4 = rowCommon3 + rowHeight;
const PixelNumber rowCommon5 = rowCommon4 + rowHeight;
const PixelNumber rowCustom1 = rowCommon5 + rowHeight;
const PixelNumber rowCustom2 = rowCustom1 + rowHeight;
const PixelNumber rowCustom3 = rowCustom2 + rowHeight;
const PixelNumber rowCustom4 = rowCustom3 + rowHeight;

const PixelNumber columnTabWidth = (DISPLAY_X - 2*margin - 4*fieldSpacing)/5;

const PixelNumber columnTab1 = margin;
const PixelNumber columnTab2 = columnTab1 + columnTabWidth + fieldSpacing;
const PixelNumber columnTab3 = columnTab2 + columnTabWidth + fieldSpacing;
const PixelNumber columnTab4 = columnTab3 + columnTabWidth + fieldSpacing;
const PixelNumber columnTab5 = columnTab4 + columnTabWidth + fieldSpacing;

const PixelNumber xyPopupX = 3, xyPopupY = 195;
const PixelNumber filePopupWidth = DISPLAY_X - 40, filePopupHeight = 8 * rowHeight + 20;

const uint32_t numFileColumns = 2;
const uint32_t numFileRows = (DISPLAY_Y - margin)/rowHeight - 2;
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
const uint32_t shortTouchDelay = 50;				// how long we ignore new touches while pressing up/down, to get a reasonable repeat rate

static uint32_t lastTouchTime;
static uint32_t ignoreTouchTime;
static uint32_t lastResponseTime;
static uint32_t fileScrollOffset;
static bool fileListChanged = true;
static bool gotMachineName = false;
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

static OneBitPort BacklightPort(33);				// PB1 (aka port 33) controls the backlight

struct FlashData
{
	uint32_t magic;
	int16_t xmin;
	int16_t xmax;
	int16_t ymin;
	int16_t ymax;
};

FlashData nvData;
const uint32_t magicVal = 0x3AB629D1;

enum PrinterStatus
{
	psUnknown = 0,
	psIdle = 1,
	psPrinting = 2,
	psStopped = 3
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
			evSetVal = 6,
			evAdjustInt = 7,
			evAdjustFloat = 8,
			evSetInt = 9,
			evCalTouch = 10,
			evSelectHead = 11,
			evXYPos = 12,
			evZPos = 13,
			evFile = 14,
			evPrint = 15, evCancelPrint = 16,
			evSendCommand = 17;

static FloatField *bedCurrentTemp, *t1CurrentTemp, *t2CurrentTemp, *xPos, *yPos, *zPos, *fpHeightField, *fpLayerHeightField;
static IntegerField *bedActiveTemp, *t1ActiveTemp, *t2ActiveTemp, *t1StandbyTemp, *t2StandbyTemp, *spd, *e1Percent, *e2Percent;
static IntegerField *bedStandbyTemp, /* *fanRPM,*/ *freeMem, *touchX, *touchY, *fpSizeField, *fpFilamentField;
static ProgressBar *printProgressBar;
static StaticTextField *nameField, *head1State, *head2State, *bedState, *tabControl, *tabPrint, *tabFiles, *tabMsg, *tabInfo, *touchCalibInstruction;
static StaticTextField *filenameFields[numDisplayedFiles];
static StaticTextField *homeFields[3], *homeAllField;
static DisplayField *baseRoot, *commonRoot, *controlRoot, *printRoot, *filesRoot, *messageRoot, *infoRoot;
static DisplayField * null currentTab = NULL;
static DisplayField * null fieldBeingAdjusted = NULL;
static PopupField *setTempPopup, *setXYPopup, *setZPopup, *filePopup;
//static TextField *commandField;
static TextField *zProbe, *fpNameField, *fpGeneratedByField, *printingField;

//char commandBuffer[80];

const size_t numHeaters = 3;
int heaterStatus[numHeaters];

void RefreshFileList()
{
	for (size_t i = 0; i < numDisplayedFiles; ++i)
	{
		StaticTextField *f = filenameFields[i];
		if (i < fileIndex.size())
		{
			f->SetValue(fileIndex[i]);
			f->SetColours(white, selectableBackColor);
			f->SetEvent(evFile, i);
		}
		else
		{
			f->SetValue("");
			f->SetColours(white, defaultBackColor);
			f->SetEvent(nullEvent, i);
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
//		lcd.setColor(defaultBackColor);
//		lcd.fillRect(0, rowCustom1, lcd.getDisplayXSize() - 1, lcd.getDisplayYSize() - 1);
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
			fileScrollOffset = 0;					// scroll to top of file list
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

void InitLcd()
{
	// Setup the LCD
	lcd.InitLCD(DisplayOrientAdjust);
	mgr.Init(defaultBackColor);
	DisplayField::SetDefaultFont(glcd19x20);
	
	// Create the fields that are always displayed
	DisplayField::SetDefaultColours(white, selectableBackColor);
	mgr.AddField(tabControl = new StaticTextField(rowTabs, columnTab1, columnTabWidth, Centre, "Control"));
	tabControl->SetEvent(evTabControl, 0);
	mgr.AddField(tabPrint = new StaticTextField(rowTabs, columnTab2, columnTabWidth, Centre, "Print"));
	tabPrint->SetEvent(evTabPrint, 0);
	mgr.AddField(tabFiles = new StaticTextField(rowTabs, columnTab3, columnTabWidth, Centre, "Files"));
	tabFiles->SetEvent(evTabFiles, 0);
	mgr.AddField(tabMsg = new StaticTextField(rowTabs, columnTab4, columnTabWidth, Centre, "Msg"));
	tabMsg->SetEvent(evTabMsg, 0);
	mgr.AddField(tabInfo = new StaticTextField(rowTabs, columnTab5, columnTabWidth, Centre, "Info"));
	tabInfo->SetEvent(evTabInfo, 0);
	
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
	t1ActiveTemp->SetEvent(evSetVal, "G10 P1 S");
	t1StandbyTemp->SetEvent(evSetVal, "G10 P1 R");
	t2ActiveTemp->SetEvent(evSetVal, "G10 P2 S");
	t2StandbyTemp->SetEvent(evSetVal, "G10 P2 R");
	bedActiveTemp->SetEvent(evSetVal, "M140 S");

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
	homeFields[0] = new StaticTextField(rowCustom1, column1, 90, Centre, "Home X");
	homeFields[0]->SetEvent(evSendCommand, "G28 X0");
	mgr.AddField(homeFields[0]);
	homeFields[1] = new StaticTextField(rowCustom1, column1+100, 90, Centre, "Home Y");
	homeFields[1]->SetEvent(evSendCommand, "G28 Y0");
	mgr.AddField(homeFields[1]);
	homeFields[2] = new StaticTextField(rowCustom1, column1+200, 90, Centre, "Home Z");
	homeFields[2]->SetEvent(evSendCommand, "G28 Z0");
	mgr.AddField(homeFields[2]);
	homeAllField = new StaticTextField(rowCustom1, column1+300, 90, Centre, "Home all");
	homeAllField->SetEvent(evSendCommand, "G28");
	mgr.AddField(homeAllField);
	
	controlRoot = mgr.GetRoot();

	// Create the fields for the Printing tab
	mgr.SetRoot(commonRoot);
	DisplayField::SetDefaultColours(white, defaultBackColor);
	printingFile.copyFrom("(unknown)");
	mgr.AddField(printingField = new TextField(rowCustom1, 0, DISPLAY_X, "Printing ", printingFile.c_str()));
	
	DisplayField::SetDefaultColours(white, UTFT::fromRGB(0, 160, 0));
	mgr.AddField(printProgressBar = new ProgressBar(rowCustom2, margin, 8, DISPLAY_X - 2*margin));
	mgr.Show(printProgressBar, false);

	DisplayField::SetDefaultColours(white, defaultBackColor);
	mgr.AddField(new StaticTextField(rowCustom3, 0, 70, Left, "Speed"));
	DisplayField::SetDefaultColours(white, selectableBackColor);
	mgr.AddField(spd = new IntegerField(rowCustom3, 70, 60, "", "%"));
	spd->SetValue(100);
	spd->SetEvent(evSetVal, "M220 S");
	DisplayField::SetDefaultColours(white, defaultBackColor);
	mgr.AddField(new StaticTextField(rowCustom3, 140, 30, Left, "E1"));
	DisplayField::SetDefaultColours(white, selectableBackColor);
	mgr.AddField(e1Percent = new IntegerField(rowCustom3, 170, 60, "", "%"));
	e1Percent->SetValue(100);
	e1Percent->SetEvent(evSetVal, "M221 D0 S");
	DisplayField::SetDefaultColours(white, defaultBackColor);
	mgr.AddField(new StaticTextField(rowCustom3, 250, 30, Left, "E2"));
	DisplayField::SetDefaultColours(white, selectableBackColor);
	mgr.AddField(e2Percent = new IntegerField(rowCustom3, 280, 60, "", "%"));
	e2Percent->SetValue(100);
	e2Percent->SetEvent(evSetVal, "M221 D1 S");
	
	printRoot = mgr.GetRoot();

	// Create the fields for the Files tab
	mgr.SetRoot(baseRoot);
	DisplayField::SetDefaultColours(white, defaultBackColor);
	mgr.AddField(new StaticTextField(rowCommon1, margin, DISPLAY_X - 2*margin, Centre, "Files on SD card"));
	{
		PixelNumber fileFieldWidth = (DISPLAY_X + fieldSpacing - 2*margin)/numFileColumns;
		unsigned int fileNum = 0;
		for (unsigned int r = 0; r < numFileRows; ++r)
		{
			for (unsigned int c = 0; c < numFileColumns; ++c)
			{
				StaticTextField *t = new StaticTextField(((r + 1) * rowHeight) + 1, (fileFieldWidth * c) + margin, fileFieldWidth - fieldSpacing, Left, "");
				mgr.AddField(t);
				filenameFields[fileNum] = t;
				++fileNum;
			}
		}
	}
	filesRoot = mgr.GetRoot();

	// Create the fields for the Message tab
	mgr.SetRoot(commonRoot);
	DisplayField::SetDefaultColours(white, defaultBackColor);
	messageRoot = mgr.GetRoot();

	// Create the fields for the Info tab
	mgr.SetRoot(commonRoot);
	DisplayField::SetDefaultColours(white, defaultBackColor);
	mgr.AddField(freeMem = new IntegerField(rowCustom1, margin, 195, "Free RAM: "));
	mgr.AddField(touchX = new IntegerField(rowCustom1, 200, 130, "Touch: ", ","));
	mgr.AddField(touchY = new IntegerField(rowCustom1, 330, 50, ""));
	DisplayField::SetDefaultColours(white, selectableBackColor);
	DisplayField *touchCal = new StaticTextField(rowCustom3, DISPLAY_X/2 - 75, 150, Centre, "Calibrate touch");
	touchCal->SetEvent(evCalTouch, 0);
	mgr.AddField(touchCal);

	DisplayField::SetDefaultColours(white, defaultBackColor);
	infoRoot = mgr.GetRoot();
	
	mgr.SetRoot(commonRoot);
	
	touchCalibInstruction = new StaticTextField(DISPLAY_Y/2 - 10, 0, DISPLAY_X, Centre, "Touch the spot");

	// Create the popup window used to adjust temperatures
	setTempPopup = new PopupField(130, 80, popupBackColour);
	DisplayField::SetDefaultColours(white, selectablePopupBackColour);

	DisplayField *tp = new StaticTextField(10, 10, 60, Centre, "+");
	tp->SetEvent(evAdjustInt, 1);
	setTempPopup->AddField(tp);
	tp = new StaticTextField(55, 10, 60, Centre, "Set");
	tp->SetEvent(evSetInt, 0);
	setTempPopup->AddField(tp);
	tp = new StaticTextField(100, 10, 60, Centre, "-");
	tp->SetEvent(evAdjustInt, -1);
	setTempPopup->AddField(tp);

	// Create the popup window used to adjust XY position
	setXYPopup = new PopupField(40, 395, popupBackColour);
	DisplayField::SetDefaultColours(white, selectablePopupBackColour);
	
	tp = new StaticTextField(10, 5, 60, Centre, "-50");
	tp->SetEvent(evAdjustFloat, "-50");
	setXYPopup->AddField(tp);
	tp = new StaticTextField(10, 70, 60, Centre, "-5");
	tp->SetEvent(evAdjustFloat, "-5");
	setXYPopup->AddField(tp);
	tp = new StaticTextField(10, 135, 60, Centre, "-0.5");
	tp->SetEvent(evAdjustFloat, "-0.5");
	setXYPopup->AddField(tp);
	tp = new StaticTextField(10, 200, 60, Centre, "+0.5");
	tp->SetEvent(evAdjustFloat, "0.5");
	setXYPopup->AddField(tp);
	tp = new StaticTextField(10, 265, 60, Centre, "+5");
	tp->SetEvent(evAdjustFloat, "5");
	setXYPopup->AddField(tp);
	tp = new StaticTextField(10, 330, 60, Centre, "+50");
	tp->SetEvent(evAdjustFloat, "50");
	setXYPopup->AddField(tp);

	// Create the popup window used to adjust Z position
	setZPopup = new PopupField(40, 395, popupBackColour);
	DisplayField::SetDefaultColours(white, selectablePopupBackColour);
	
	tp = new StaticTextField(10, 5, 60, Centre, "-5");
	tp->SetEvent(evAdjustFloat, "-5");
	setZPopup->AddField(tp);
	tp = new StaticTextField(10, 70, 60, Centre, "-0.5");
	tp->SetEvent(evAdjustFloat, "-0.5");
	setZPopup->AddField(tp);
	tp = new StaticTextField(10, 135, 60, Centre, "-0.05");
	tp->SetEvent(evAdjustFloat, "-0.05");
	setZPopup->AddField(tp);
	tp = new StaticTextField(10, 200, 60, Centre, "+0.05");
	tp->SetEvent(evAdjustFloat, "0.05");
	setZPopup->AddField(tp);
	tp = new StaticTextField(10, 265, 60, Centre, "+0.5");
	tp->SetEvent(evAdjustFloat, "0.5");
	setZPopup->AddField(tp);
	tp = new StaticTextField(10, 330, 60, Centre, "+5");
	tp->SetEvent(evAdjustFloat, "5");
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

	// Redraw everything
	mgr.RefreshAll(true);

#if 0
	// Widest expected values
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
	extrPos->SetValue(999.9);
	zProbe->SetValue(1023);
	fanRPM->SetValue(9999);
	spd->SetValue(169);
	e1Percent->SetValue(169);
	e2Percent->SetValue(169);
#else
	// Initial values
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
#endif

	currentTab = NULL;
	changeTab(tabControl);
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

int DoTouchCalib(PixelNumber x, PixelNumber y, bool wantY)
{
	const PixelNumber touchCircleRadius = 8;
	const PixelNumber touchCalibMaxError = 40;
	
	lcd.setColor(white);
	lcd.fillCircle(x, y, touchCircleRadius);
	
	int tx, ty;
	
	for (;;)
	{
		if (touch.dataAvailable())
		{
			touch.read();
			tx = touch.getX();
			ty = touch.getY();
			if (abs(tx - x) <= touchCalibMaxError && abs(ty - y) <= touchCalibMaxError)
			{
				TouchBeep();
				break;
			}
		}
	}
	
	lcd.setColor(defaultBackColor);
	lcd.fillCircle(x, y, touchCircleRadius);
	return (wantY) ? ty : tx;
}

void CalibrateTouch()
{
	const PixelNumber touchCalibMargin = 25;

	DisplayField *oldRoot = mgr.GetRoot();
	mgr.SetRoot(touchCalibInstruction);
	mgr.ClearAll();
	mgr.RefreshAll(true);
	touch.calibrate(0, lcd.getDisplayXSize() - 1, 0, lcd.getDisplayYSize() - 1);

	int yLow = DoTouchCalib(lcd.getDisplayXSize()/2, touchCalibMargin, true);
	int xHigh = DoTouchCalib(lcd.getDisplayXSize() - touchCalibMargin - 1, lcd.getDisplayYSize()/2, false);
	int yHigh = DoTouchCalib(lcd.getDisplayXSize()/2, lcd.getDisplayYSize() - touchCalibMargin - 1, true);
	int xLow = DoTouchCalib(touchCalibMargin, lcd.getDisplayYSize()/2, false);
	
	// Extrapolate the values we read to the edges of the screen
	nvData.xmin = xLow - touchCalibMargin;
	nvData.xmax = xHigh + touchCalibMargin;
	nvData.ymin = yLow - touchCalibMargin;
	nvData.ymax = yHigh + touchCalibMargin;
	touch.calibrate(nvData.xmin, nvData.xmax, nvData.ymin, nvData.ymax);
	nvData.magic = magicVal;
	FlashStorage::write(0, &nvData, sizeof(nvData));
	
	mgr.SetRoot(oldRoot);
	mgr.ClearAll();
	mgr.RefreshAll(true);
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

	case evSetVal:
		mgr.Outline(f, outlineColor, outlinePixels);
		mgr.AttachPopup(setTempPopup, f);
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
//			commandField->SetChanged();
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

	case evAdjustFloat:
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

	default:
		break;
	}
}

// Process a touch event outside the popup on the field being adjusted
void ProcessTouchOutsidePopup()
{
	switch(fieldBeingAdjusted->GetEvent())
	{
	case evSetVal:
	case evXYPos:
	case evZPos:
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
		machineName.copyFrom(data);
		nameField->SetChanged();
		gotMachineName = true;
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
}

// Update those fields that display debug information
void updateDebugInfo()
{
	freeMem->SetValue(getFreeMemory());
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
	
	SerialIo::Init();
	Buzzer::Init();
	InitLcd();
	touch.init(lcd.getDisplayXSize(), lcd.getDisplayYSize(), TouchOrientAdjust, TpMedium);
	lastTouchTime = GetTickCount();
	
	SysTick_Config(SystemCoreClock / 1000);

	// Read parameters from flash memory (currently, just the touch calibration)
	flash_init(FLASH_ACCESS_MODE_128, 6);	
	nvData.magic = 0;
	FlashStorage::read(0, &nvData, sizeof(nvData));
	if (nvData.magic == magicVal)
	{
		touch.calibrate(nvData.xmin, nvData.xmax, nvData.ymin, nvData.ymax);
	}
	
	BacklightPort.setMode(OneBitPort::Output);
	BacklightPort.setHigh();				// turn the backlight on (no PWM for now, it only works with the 3.2" display anyway, and only on prototype boards)
	
	uint32_t lastPollTime = GetTickCount() - printerPollInterval;
	lastResponseTime = GetTickCount();		// pretend we just received a response
	
#if 0 //test
	uint32_t cc = 0;
	for (;;)
	{
		pio_sync_output_write(PIOA, cc);
		++cc;
	}
#else	
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
		if (touch.dataAvailable() && GetTickCount() - lastTouchTime >= ignoreTouchTime)
		{
			if (touch.read())
			{
				int x = touch.getX();
				int y = touch.getY();
				touchX->SetValue(x);	//debug
				touchY->SetValue(y);	//debug
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
#endif
}

// End
