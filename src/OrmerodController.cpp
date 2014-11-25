// Controller for Ormerod to run on SAM3S2B
// Coding rules:
//
// 1. Must compile with no g++ warnings, when all warning are enabled..
// 2. Dynamic memory allocation using 'new' is permitted during the initialization phase only. No use of 'new' anywhere else,
//    and no use of 'delete', 'malloc' or 'free' anywhere.
// 3. No pure virtual functions. This is because in release builds, having pure virtual functions causes huge amounts of the C++ library to be linked in
//    (possibly because it wants to print a message if a pure virtual function is called).

#include "asf.h"
#include "mem.hpp"
#include "Display.hpp"
#include "UTFT.hpp"
#include "UTouch.hpp"
#include "SerialIo.hpp"
#include "Buzzer.hpp"
#include "SysTick.hpp"
#include "Misc.hpp"
#include "Vector.hpp"

// Declare which fonts we will be using
extern uint8_t glcd16x16[];
extern uint8_t glcd19x20[];

#define DEGREE_SYMBOL	"\x81"

UTFT lcd(HX8352A, TMode16bit, 16, 17, 18, 19);		// Remember to change the model parameter to suit your display module!
UTouch touch(23, 24, 22, 21, 20);
DisplayManager mgr;

const uint32_t printerPollInterval = 2000;			// poll interval in milliseconds
const uint32_t beepLength = 10;						// beep length in ms
const uint32_t longTouchDelay = 200;				// how long we ignore new touches for after pressing Set
const uint32_t shortTouchDelay = 50;				// how long we ignore new touches while pressing up/down, to get a reasonable repeat rate

const uint32_t numFileColumns = 2;
const uint32_t numFileRows = 6;
const uint32_t fileFieldSpacing = 4;
const uint32_t numDisplayedFiles = numFileColumns * numFileRows;

static uint32_t lastTouchTime;
static uint32_t ignoreTouchTime;
static uint32_t fileScrollOffset;
static bool fileListChanged = true;
static bool gotMachineName = false;
static const char* array null currentFile = NULL;			// file whose info is displayed in the popup menu

String<15> machineName;
String<10> zprobeBuf;
String<30> generatedByText;
Vector<char, 2048> fileList;					// we use a Vector instead of a String because we store multiple null-terminated strings in it
Vector<const char* array, 100> fileIndex;		// pointers into the individual filenames in the list

const Color activeBackColor = red;
const Color standbyBackColor = yellow;
const Color defaultBackColor = blue;
const Color errorBackColour = magenta;
const Color selectableBackColor = black;
const Color outlineColor = green;
const Color popupBackColour = green;
const Color selectablePopupBackColour = UTFT::fromRGB(0, 160, 0);	// dark green

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
			evPrint = 15, evCancelPrint = 16;

static FloatField *bedCurrentTemp, *t1CurrentTemp, *t2CurrentTemp, *xPos, *yPos, *zPos, *fpHeightField, *fpLayerHeightField;
static IntegerField *bedActiveTemp, *t1ActiveTemp, *t2ActiveTemp, *t1StandbyTemp, *t2StandbyTemp, *spd, *e1Percent, *e2Percent;
static IntegerField *bedStandbyTemp, /* *fanRPM,*/ *freeMem, *touchX, *touchY, *fpSizeField, *fpFilamentField;
static ProgressBar *pbar;
static StaticTextField *nameField, *head1State, *head2State, *bedState, *tabControl, *tabPrint, *tabFiles, *tabMsg, *tabInfo, *touchCalibInstruction;
static StaticTextField *filenameFields[12];
static DisplayField *baseRoot, *commonRoot, *controlRoot, *printRoot, *filesRoot, *messageRoot, *infoRoot;
static DisplayField * null currentTab = NULL;
static DisplayField * null fieldBeingAdjusted = NULL;
static PopupField *setTempPopup, *setXYPopup, *setZPopup, *filePopup;
//static TextField *commandField;
static TextField *zProbe, *fpNameField, *fpGeneratedByField;

//char commandBuffer[80];

const size_t numHeaters = 3;
int heaterStatus[numHeaters];

// Define the row and column positions. Leave a gap of at least 1 pixel fro the edges of the screen, so that we can highlight
// a file by drawing an outline.
const PixelNumber column1 = 1;
const PixelNumber column2 = 72;
const PixelNumber column3 = 146;
const PixelNumber column4 = 208;
const PixelNumber column5 = 270;

const PixelNumber columnX = 275;
const PixelNumber columnY = 333;

const PixelNumber columnEnd = 400;	// should be the same as the display X size

const PixelNumber rowHeight = 22;
const PixelNumber rowCommon1 = 1;
const PixelNumber rowCommon2 = rowCommon1 + rowHeight;
const PixelNumber rowCommon3 = rowCommon2 + rowHeight;
const PixelNumber rowCommon4 = rowCommon3 + rowHeight;
const PixelNumber rowCommon5 = rowCommon4 + rowHeight;
const PixelNumber rowCustom1 = rowCommon5 + rowHeight;
const PixelNumber rowCustom2 = rowCustom1 + rowHeight;
const PixelNumber rowCustom3 = rowCustom2 + rowHeight;
const PixelNumber rowCustom4 = rowCustom3 + rowHeight;	// ends just before row 177
const PixelNumber rowTabs = 240 - 22 - 1;				// place at bottom of screen with a 1-pixel margin

const PixelNumber columnTab1 = 2;
const PixelNumber columnTab2 = 82;
const PixelNumber columnTab3 = 162;
const PixelNumber columnTab4 = 242;
const PixelNumber columnTab5 = 322;
const PixelNumber columnTabWidth = 75;

const PixelNumber xyPopupX = 3, xyPopupY = 195;
const PixelNumber filePopupWidth = 330, filePopupHeight = 8 * rowHeight + 20;

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
	lcd.InitLCD(Landscape);
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
	
	mgr.AddField(new StaticTextField(rowCommon2, column2, column3 - column2 - 4, Left, "Current"));
	mgr.AddField(new StaticTextField(rowCommon2, column3, column4 - column3 - 4, Left, "Active"));
	mgr.AddField(new StaticTextField(rowCommon2, column4, column5 - column4 - 4, Left, "St'by"));

	DisplayField::SetDefaultColours(white, selectableBackColor);
	mgr.AddField(head2State = new StaticTextField(rowCommon4, column1, column2 - column1 - 4, Left, "Head 2"));
	mgr.AddField(head1State = new StaticTextField(rowCommon3, column1, column2 - column1 - 4, Left, "Head 1"));
	mgr.AddField(bedState = new StaticTextField(rowCommon5, column1, column2 - column1 - 4, Left, "Bed"));
	head1State->SetEvent(evSelectHead, 1);
	head2State->SetEvent(evSelectHead, 2);
	bedState->SetEvent(evSelectHead, 0);

	mgr.AddField(t1ActiveTemp = new IntegerField(rowCommon3, column3, column4 - column3 - 4, NULL, DEGREE_SYMBOL "C"));
	mgr.AddField(t1StandbyTemp = new IntegerField(rowCommon3, column4, column5 - column4 - 4, NULL, DEGREE_SYMBOL "C"));
	mgr.AddField(t2ActiveTemp = new IntegerField(rowCommon4, column3, column4 - column3 - 4, NULL, DEGREE_SYMBOL "C"));
	mgr.AddField(t2StandbyTemp = new IntegerField(rowCommon4, column4, column5 - column4 - 4, NULL, DEGREE_SYMBOL "C"));
	mgr.AddField(bedActiveTemp = new IntegerField(rowCommon5, column3, column4 - column3 - 4, NULL, DEGREE_SYMBOL "C"));
	t1ActiveTemp->SetEvent(evSetVal, "G10 P1 S");
	t1StandbyTemp->SetEvent(evSetVal, "G10 P1 R");
	t2ActiveTemp->SetEvent(evSetVal, "G10 P2 S");
	t2StandbyTemp->SetEvent(evSetVal, "G10 P2 R");
	bedActiveTemp->SetEvent(evSetVal, "M140 S");

	DisplayField::SetDefaultColours(white, defaultBackColor);
	mgr.AddField(t1CurrentTemp = new FloatField(rowCommon3, column2, column3 - column2 - 4, NULL, 1, DEGREE_SYMBOL "C"));
	mgr.AddField(t2CurrentTemp = new FloatField(rowCommon4, column2, column3 - column2 - 4, NULL, 1, DEGREE_SYMBOL "C"));

	mgr.AddField(bedCurrentTemp = new FloatField(rowCommon5, column2, column3 - column2 - 4, NULL, 1, DEGREE_SYMBOL "C"));
	mgr.AddField(bedStandbyTemp = new IntegerField(rowCommon5, column4, column5 - column4 - 4, NULL, DEGREE_SYMBOL "C"));

	mgr.AddField(new StaticTextField(rowCommon2, columnX, columnY - columnX - 2, Left, "X"));
	mgr.AddField(new StaticTextField(rowCommon2, columnY, columnEnd - columnY - 2, Left, "Y"));
	mgr.AddField(new StaticTextField(rowCommon4, columnX, columnY - columnX - 2, Left, "Z"));
	mgr.AddField(new StaticTextField(rowCommon4, columnY, columnEnd - columnY - 2, Left, "Probe"));

	DisplayField::SetDefaultColours(white, selectableBackColor);
	mgr.AddField(xPos = new FloatField(rowCommon3, columnX, columnY - columnX - 2, NULL, 1));
	mgr.AddField(yPos = new FloatField(rowCommon3, columnY, columnEnd - columnY - 2, NULL, 1));
	mgr.AddField(zPos = new FloatField(rowCommon5, columnX, columnY - columnX - 2, NULL, 2));
	xPos->SetEvent(evXYPos, "G1 X");
	yPos->SetEvent(evXYPos, "G1 Y");
	zPos->SetEvent(evZPos, "G1 Z");

	zprobeBuf[0] = 0;
	DisplayField::SetDefaultColours(white, defaultBackColor);
	mgr.AddField(zProbe = new TextField(rowCommon5, columnY, columnEnd - columnY - 2, NULL, zprobeBuf.c_str()));
	
	commonRoot = mgr.GetRoot();		// save the root of fields that we usually display
	
	// Create the fields for the Control tab
	DisplayField::SetDefaultColours(white, defaultBackColor);
	controlRoot = mgr.GetRoot();

	// Create the fields for the Printing tab
	mgr.SetRoot(commonRoot);
	DisplayField::SetDefaultColours(white, defaultBackColor);
	mgr.AddField(new TextField(rowCustom1, 0, lcd.getDisplayXSize(), "Printing ", "nozzleMount.gcode"));
	
	DisplayField::SetDefaultColours(white, UTFT::fromRGB(0, 160, 0));
	mgr.AddField(pbar = new ProgressBar(rowCustom2, 1, 8, lcd.getDisplayXSize() - 2));

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
	mgr.AddField(new StaticTextField(rowCommon1, 1, lcd.getDisplayXSize() - 2, Centre, "Files on SD card"));
	{
		PixelNumber fileFieldWidth = (lcd.getDisplayXSize() + fileFieldSpacing - 2)/numFileColumns;
		unsigned int fileNum = 0;
		for (unsigned int r = 0; r < numFileRows; ++r)
		{
			for (unsigned int c = 0; c < numFileColumns; ++c)
			{
				StaticTextField *t = new StaticTextField(((r + 1) * rowHeight) + 1, (fileFieldWidth * c) + 1, fileFieldWidth - fileFieldSpacing, Left, "");
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
	mgr.AddField(freeMem = new IntegerField(rowCustom1, 1, 195, "Free RAM: "));
	mgr.AddField(touchX = new IntegerField(rowCustom1, 200, 130, "Touch: ", ","));
	mgr.AddField(touchY = new IntegerField(rowCustom1, 330, 50, ""));
	DisplayField::SetDefaultColours(white, selectableBackColor);
	DisplayField *touchCal = new StaticTextField(rowCustom3, lcd.getDisplayXSize()/2 - 75, 150, Centre, "Calibrate touch");
	touchCal->SetEvent(evCalTouch, 0);
	mgr.AddField(touchCal);

	DisplayField::SetDefaultColours(white, defaultBackColor);
	infoRoot = mgr.GetRoot();
	
	mgr.SetRoot(commonRoot);
	
	touchCalibInstruction = new StaticTextField(lcd.getDisplayYSize()/2 - 10, 0, lcd.getDisplayXSize(), Centre, "Touch the spot");

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

	touch.init(400, 240, InvLandscape, TpMedium);
	lastTouchTime = GetTickCount();
	currentTab = NULL;
	changeTab(tabControl);
}

// Ignore touches for a little while
void delayTouch(uint32_t ms)
{
	lastTouchTime = GetTickCount();
	ignoreTouchTime = ms;
}

int DoTouchCalib(PixelNumber x, PixelNumber y, bool wantY)
{
	const PixelNumber touchCircleRadius = 8;
	const PixelNumber touchCalibMaxError = 30;
	
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
				BuzzerBeep(beepLength);
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
	const PixelNumber touchCalibMargin = 30;

	DisplayField *oldRoot = mgr.GetRoot();
	mgr.SetRoot(touchCalibInstruction);
	mgr.ClearAll();
	mgr.RefreshAll(true);
	touch.calibrate(0, lcd.getDisplayXSize() - 1, 0, lcd.getDisplayYSize() - 1);

	int yLow = DoTouchCalib(lcd.getDisplayXSize()/2, touchCalibMargin, true);
	int xHigh = DoTouchCalib(lcd.getDisplayXSize() - touchCalibMargin, lcd.getDisplayYSize()/2, false);
	int yHigh = DoTouchCalib(lcd.getDisplayXSize()/2, lcd.getDisplayYSize() - touchCalibMargin, true);
	int xLow = DoTouchCalib(touchCalibMargin, lcd.getDisplayYSize()/2, false);
	
	// Extrapolate the values we read to the edges of the screen
	int xAdjust = (touchCalibMargin * lcd.getDisplayXSize())/(lcd.getDisplayXSize() - 2 * touchCalibMargin);
	int yAdjust = (touchCalibMargin * lcd.getDisplayYSize())/(lcd.getDisplayYSize() - 2 * touchCalibMargin);
	touch.calibrate(xLow - xAdjust, xHigh + xAdjust, yLow - yAdjust, yHigh + yAdjust);
	
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
		mgr.Outline(f, outlineColor);
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
			mgr.RemoveOutline(fieldBeingAdjusted);
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
		mgr.Outline(f, outlineColor);
		mgr.SetPopup(setXYPopup, xyPopupX, xyPopupY);
		fieldBeingAdjusted = f;
		delayTouch(longTouchDelay);
		break;

	case evZPos:
		mgr.Outline(f, outlineColor);
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
			currentFile = NULL;			// allow the file list to be updated		
		}
		break;

	case evCancelPrint:
		mgr.SetPopup(NULL);
		currentFile = NULL;				// allow the file list to be updated
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
		{
			const char* null cmd = fieldBeingAdjusted->GetSParam();
			if (cmd != NULL)
			{
				SerialIo::SendString(cmd);
				SerialIo::SendInt(static_cast<const IntegerField*>(fieldBeingAdjusted)->GetValue());
				SerialIo::SendChar('\n');
			}
		}
//		commandField->SetChanged();
		mgr.SetPopup(NULL);
		mgr.RemoveOutline(fieldBeingAdjusted);
		fieldBeingAdjusted = NULL;
		delayTouch(longTouchDelay);
		break;

	case evXYPos:
	case evZPos:
		mgr.RemoveOutline(fieldBeingAdjusted);
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
}

/**
 * \brief Application entry point.
 *
 * \return Unused (ANSI-C compatibility).
 */
int main(void)
{
    SystemInit();						// set up the click etc.	
	wdt_disable(WDT);					// disable watchdog for now
	
	matrix_set_system_io(CCFG_SYSIO_SYSIO4 | CCFG_SYSIO_SYSIO5 | CCFG_SYSIO_SYSIO6 | CCFG_SYSIO_SYSIO7);	// enable PB4=PB7 pins
	pmc_enable_periph_clk(ID_PIOA);		// enable the PIO clock
	pmc_enable_periph_clk(ID_PIOB);		// enable the PIO clock
	pmc_enable_periph_clk(ID_PWM);		// enable the PWM clock
	pmc_enable_periph_clk(ID_UART1);	// enable UART1 clock
	
	SerialIo::Init();
	BuzzerInit();
	InitLcd();
	
	SysTick_Config(SystemCoreClock / 1000);

	uint32_t lastPollTime = GetTickCount();
	unsigned int percent = 0;
	for (;;)
	{
		// Temporarily animate the progress bar so we can see that it is running
		pbar->SetPercent(percent);
		++percent;
		if (percent == 100)
		{
			percent = 0;
		}
		
		SerialIo::CheckInput();
		if (fileListChanged)
		{
			RefreshFileList();
			fileListChanged = false;
		}
		
		if (touch.dataAvailable() && GetTickCount() - lastTouchTime >= ignoreTouchTime)
		{
			touch.read();
			int x = touch.getX();
			int y = touch.getY();
			touchX->SetValue(x);	//debug
			touchY->SetValue(y);	//debug
			DisplayField * null f = mgr.FindEvent(x, y);
			if (f != NULL)
			{
				BuzzerBeep(beepLength);
				ProcessTouch(f);
			}
			else
			{
				f = mgr.FindEventOutsidePopup(x, y);
				if (f != NULL && f == fieldBeingAdjusted)
				{
					BuzzerBeep(beepLength);
					ProcessTouchOutsidePopup();					
				}
			}
		}
		freeMem->SetValue(getFreeMemory());
		mgr.RefreshAll(false);
		
		if (GetTickCount() - lastPollTime >= printerPollInterval)
		{
			lastPollTime += printerPollInterval;
			SerialIo::SendString((gotMachineName) ? "M105 S2\n" : "M105 S3\n");
		}
	}
}

// End
