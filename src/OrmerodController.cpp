/**
 * \file
 *
 * \brief Empty user application template
 *
 */

/**
 * \mainpage User Application template doxygen documentation
 *
 * \par Empty user application template
 *
 * Bare minimum empty user application template
 *
 * \par Content
 *
 * -# Include the ASF header files (through asf.h)
 * -# "Insert system clock initialization code here" comment
 * -# Minimal main function that starts with a call to board_init()
 * -# "Insert application code here" comment
 *
 */

/*
 * Include header files for all drivers that have been imported from
 * Atmel Software Framework (ASF).
 */
#include "asf.h"
#include "mem.hpp"
#include "Display.hpp"
#include "UTFT.hpp"
#include "UTouch.hpp"
#include "SerialIo.hpp"
#include "Buzzer.hpp"
#include "SysTick.hpp"

// Controller for Ormerod to run on SAM3S2B
// Coding rules:
//
// 1. Must compile with no g++ warnings, when all warning are enabled..
// 2. Dynamic memory allocation using 'new' is permitted during the initialization phase only. No use of 'new' anywhere else,
//    and no use of 'delete', 'malloc' or 'free' anywhere.
// 3. No pure virtual functions. This is because in release builds, having pure virtual functions causes huge amounts of the C++ library to be linked in
//    (possibly because it wants to print a message if a pure virtual function is called).

// Declare which fonts we will be using
//extern uint8_t glcd10x10[];
extern uint8_t glcd16x16[];
extern uint8_t glcd19x20[];

#define DEGREE_SYMBOL	"\x81"

UTFT lcd(HX8352A, TMode16bit, 16, 17, 18, 19);   // Remember to change the model parameter to suit your display module!
UTouch touch(23, 24, 22, 21, 20);
DisplayManager mgr;

const uint32_t printerPollInterval = 2000;			// poll interval in milliseconds
const uint32_t beepLength = 10;						// beep length in ms
const uint32_t ignoreTouchTime = 200;				// how long we ignore new touches for after pressing Set

static uint32_t lastTouchTime;

static char machineName[15] = "dc42's Ormerod";

const Color activeBackColor = red;
const Color standbyBackColor = yellow;
const Color defaultBackColor = blue;
const Color selectableBackColor = black;

const Event evTabControl = 1,
			evTabPrint = 2,
			evTabFiles = 3,
			evTabMsg = 4,
			evTabInfo = 5,
			evSetVal = 6,
			evUp = 7,
			evDown = 8,
			evSet = 9,
			evCalTouch = 10;

static FloatField *bedCurrentTemp, *t1CurrentTemp, *t2CurrentTemp, *xPos, *yPos, *zPos;
static IntegerSettingField *bedActiveTemp, *t1ActiveTemp, *t2ActiveTemp, *t1StandbyTemp, *t2StandbyTemp, *spd, *e1Percent, *e2Percent;
static IntegerField *bedStandbyTemp, *zProbe, /* *fanRPM,*/ *freeMem, *touchX, *touchY;
static ProgressBar *pbar;
static StaticTextField *head1State, *head2State, *bedState, *tabControl, *tabPrint, *tabFiles, *tabMsg, *tabInfo, *touchCalibInstruction;
static DisplayField *commonRoot, *controlRoot, *printRoot, *filesRoot, *messageRoot, *infoRoot;
static DisplayField * null currentTab = NULL;
static DisplayField * null fieldBeingAdjusted = NULL;
static PopupField *setTempPopup;
static TextField *commandField;

char commandBuffer[80];

const PixelNumber column1 = 0;
const PixelNumber column2 = 71;
const PixelNumber column3 = 145;
const PixelNumber column4 = 207;
const PixelNumber column5 = 269;

const PixelNumber columnX = 274;
const PixelNumber columnY = 332;

const PixelNumber columnEnd = 400;

const PixelNumber rowCommon1 = 0;
const PixelNumber rowCommon2 = 22;
const PixelNumber rowCommon3 = 44;
const PixelNumber rowCommon4 = 66;
const PixelNumber rowCommon5 = 88;
const PixelNumber rowTabs = 120;
const PixelNumber rowCustom1 = rowTabs + 32;
const PixelNumber rowCustom2 = rowTabs + 54;
const PixelNumber rowCustom3 = rowTabs + 76;
const PixelNumber rowCustom4 = rowTabs + 98;	//120 + 98 = 218, +22 = 240 so it just fits in a 400x240 display

const PixelNumber columnTab1 = 2;
const PixelNumber columnTab2 = 82;
const PixelNumber columnTab3 = 162;
const PixelNumber columnTab4 = 242;
const PixelNumber columnTab5 = 322;
const PixelNumber columnTabWidth = 75;

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
		lcd.setColor(defaultBackColor);
		lcd.fillRect(0, rowCustom1, lcd.getDisplayXSize() - 1, lcd.getDisplayYSize() - 1);
		switch(newTab->GetEvent())
		{
			case evTabControl:
			mgr.SetRoot(controlRoot);
			break;
			case evTabPrint:
			mgr.SetRoot(printRoot);
			break;
			case evTabFiles:
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
	}
	mgr.RefreshAll(true);
}

void InitLcd()
{
	// Setup the LCD
	lcd.InitLCD(Landscape);
	mgr.Init(defaultBackColor);
	
	// Create the fields that are always displayed
	DisplayField::SetDefaultFont(glcd19x20);
	DisplayField::SetDefaultColours(white, red);
	
	mgr.AddField(new StaticTextField(rowCommon1, 0, lcd.getDisplayXSize(), Centre, machineName));
	DisplayField::SetDefaultColours(white, defaultBackColor);
	
	mgr.AddField(new StaticTextField(rowCommon2, column2, column3 - column2 - 4, Left, "Current"));
	mgr.AddField(new StaticTextField(rowCommon2, column3, column4 - column3 - 4, Left, "Active"));
	mgr.AddField(new StaticTextField(rowCommon2, column4, column5 - column4 - 4, Left, "St'by"));

	DisplayField::SetDefaultColours(white, selectableBackColor);
	mgr.AddField(head2State = new StaticTextField(rowCommon4, column1, column2 - column1 - 4, Left, "Head 2"));
	mgr.AddField(head1State = new StaticTextField(rowCommon3, column1, column2 - column1 - 4, Left, "Head 1"));

	mgr.AddField(t1ActiveTemp = new IntegerSettingField("G10 P1 S", rowCommon3, column3, column4 - column3 - 4, NULL, DEGREE_SYMBOL "C"));
	mgr.AddField(t1StandbyTemp = new IntegerSettingField("G10 P1 R", rowCommon3, column4, column5 - column4 - 4, NULL, DEGREE_SYMBOL "C"));
	mgr.AddField(t2ActiveTemp = new IntegerSettingField("G10 P2 S", rowCommon4, column3, column4 - column3 - 4, NULL, DEGREE_SYMBOL "C"));
	mgr.AddField(t2StandbyTemp = new IntegerSettingField("G10 P2 R", rowCommon4, column4, column5 - column4 - 4, NULL, DEGREE_SYMBOL "C"));
	mgr.AddField(bedActiveTemp = new IntegerSettingField("M140 S", rowCommon5, column3, column4 - column3 - 4, NULL, DEGREE_SYMBOL "C"));
	t1ActiveTemp->SetEvent(evSetVal);
	t1StandbyTemp->SetEvent(evSetVal);
	t2ActiveTemp->SetEvent(evSetVal);
	t2StandbyTemp->SetEvent(evSetVal);
	bedActiveTemp->SetEvent(evSetVal);

	DisplayField::SetDefaultColours(white, defaultBackColor);
	mgr.AddField(t1CurrentTemp = new FloatField(rowCommon3, column2, column3 - column2 - 4, NULL, 1, DEGREE_SYMBOL "C"));
	mgr.AddField(t2CurrentTemp = new FloatField(rowCommon4, column2, column3 - column2 - 4, NULL, 1, DEGREE_SYMBOL "C"));

	mgr.AddField(bedState = new StaticTextField(rowCommon5, column1, column2 - column1 - 4, Left, "Bed"));
	mgr.AddField(bedCurrentTemp = new FloatField(rowCommon5, column2, column3 - column2 - 4, NULL, 1, DEGREE_SYMBOL "C"));
	mgr.AddField(bedStandbyTemp = new IntegerField(rowCommon5, column4, column5 - column4 - 4, NULL, DEGREE_SYMBOL "C"));

	mgr.AddField(new StaticTextField(rowCommon2, columnX, columnY - columnX - 2, Left, "X"));
	mgr.AddField(new StaticTextField(rowCommon2, columnY, columnEnd - columnY - 2, Left, "Y"));

	mgr.AddField(xPos = new FloatField(rowCommon3, columnX, columnY - columnX - 2, NULL, 1));
	mgr.AddField(yPos = new FloatField(rowCommon3, columnY, columnEnd - columnY - 2, NULL, 1));
	
	mgr.AddField(new StaticTextField(rowCommon4, columnX, columnY - columnX - 2, Left, "Z"));
	mgr.AddField(new StaticTextField(rowCommon4, columnY, columnEnd - columnY - 2, Left, "Probe"));

	mgr.AddField(zPos = new FloatField(rowCommon5, columnX, columnY - columnX - 2, NULL, 1));
	mgr.AddField(zProbe = new IntegerField(rowCommon5, columnY, columnEnd - columnY - 2, NULL, NULL));
	
	DisplayField::SetDefaultColours(white, selectableBackColor);
	mgr.AddField(tabControl = new StaticTextField(rowTabs, columnTab1, columnTabWidth, Centre, "Control"));
	tabControl->SetEvent(evTabControl);
	mgr.AddField(tabPrint = new StaticTextField(rowTabs, columnTab2, columnTabWidth, Centre, "Print"));
	tabPrint->SetEvent(evTabPrint);
	mgr.AddField(tabFiles = new StaticTextField(rowTabs, columnTab3, columnTabWidth, Centre, "Files"));
	tabFiles->SetEvent(evTabFiles);
	mgr.AddField(tabMsg = new StaticTextField(rowTabs, columnTab4, columnTabWidth, Centre, "Msg"));
	tabMsg->SetEvent(evTabMsg);
	mgr.AddField(tabInfo = new StaticTextField(rowTabs, columnTab5, columnTabWidth, Centre, "Info"));
	tabInfo->SetEvent(evTabInfo);
	
	commonRoot = mgr.GetRoot();		// save the root of fields that we always display
	
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
	mgr.AddField(spd = new IntegerSettingField("M220 S", rowCustom3, 70, 60, "", "%"));
	spd->SetValue(100);
	spd->SetEvent(evSetVal);
	DisplayField::SetDefaultColours(white, defaultBackColor);
	mgr.AddField(new StaticTextField(rowCustom3, 140, 30, Left, "E1"));
	DisplayField::SetDefaultColours(white, selectableBackColor);
	mgr.AddField(e1Percent = new IntegerSettingField("M221 D0 S", rowCustom3, 170, 60, "", "%"));
	e1Percent->SetValue(100);
	e1Percent->SetEvent(evSetVal);
	DisplayField::SetDefaultColours(white, defaultBackColor);
	mgr.AddField(new StaticTextField(rowCustom3, 250, 30, Left, "E2"));
	DisplayField::SetDefaultColours(white, selectableBackColor);
	mgr.AddField(e2Percent = new IntegerSettingField("M221 D1 S", rowCustom3, 280, 60, "", "%"));
	e2Percent->SetValue(100);
	e2Percent->SetEvent(evSetVal);
	
	printRoot = mgr.GetRoot();

	// Create the fields for the Files tab
	mgr.SetRoot(commonRoot);
	DisplayField::SetDefaultColours(white, defaultBackColor);
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
	touchCal->SetEvent(evCalTouch);
	mgr.AddField(touchCal);

	DisplayField::SetDefaultColours(white, defaultBackColor);
	infoRoot = mgr.GetRoot();
	
	mgr.SetRoot(commonRoot);
	
	touchCalibInstruction = new StaticTextField(lcd.getDisplayYSize()/2 - 10, 0, lcd.getDisplayXSize(), Centre, "Touch the circle");

	// Create the popup window used to adjust values
	DisplayField::SetDefaultColours(white, UTFT::fromRGB(0, 160, 0));
	setTempPopup = new PopupField(130, 80);
	DisplayField *tp = new StaticTextField(10, 10, 60, Centre, "+");
	tp->SetEvent(evUp);
	setTempPopup->AddField(tp);
	tp = new StaticTextField(55, 10, 60, Centre, "Set");
	tp->SetEvent(evSet);
	setTempPopup->AddField(tp);
	tp = new StaticTextField(100, 10, 60, Centre, "-");
	tp->SetEvent(evDown);
	setTempPopup->AddField(tp);
	

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
 #if 0
	spd->SetValue(169);
	e1Percent->SetValue(169);
	e2Percent->SetValue(169);
 #endif
#else
	// Typical values
	bedCurrentTemp->SetValue(19.8);
	t1CurrentTemp->SetValue(181.5);
	t2CurrentTemp->SetValue(150.1);
	bedActiveTemp->SetValue(65);
	t1ActiveTemp->SetValue(180);
	t2ActiveTemp->SetValue(180);
	t1StandbyTemp->SetValue(150);
	t2StandbyTemp->SetValue(150);
	xPos->SetValue(92.4);
	yPos->SetValue(101.0);
	zPos->SetValue(4.80);
	//  extrPos->SetValue(43.6);
	zProbe->SetValue(535);
	//  fanRPM->SetValue(2354);
 #if 0
	spd->SetValue(100);
	e1Percent->SetValue(96);
	e2Percent->SetValue(100);
 #endif
#endif

	touch.init(400, 240, InvLandscape, TpMedium);
	lastTouchTime = GetTickCount();
	currentTab = NULL;
	changeTab(tabControl);
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
		mgr.AttachPopup(setTempPopup, f);
		fieldBeingAdjusted = f;
		break;

	case evSet:
		if (fieldBeingAdjusted != NULL)
		{
			commandBuffer[0] = '\0';
			((IntegerSettingField*)fieldBeingAdjusted)->Action();
			commandField->SetChanged();
		}
		mgr.SetPopup(NULL);
		fieldBeingAdjusted = NULL;
		lastTouchTime = GetTickCount();		// ignore touches for a little while
		break;

	case evUp:
		if (fieldBeingAdjusted != NULL)
		{
			((IntegerField*)fieldBeingAdjusted)->Increment(1);
		}
		break;

	case evDown:
		if (fieldBeingAdjusted != NULL)
		{
			((IntegerField*)fieldBeingAdjusted)->Increment(-1);
		}
		break;

	case evCalTouch:
		CalibrateTouch();
		break;

	default:
		break;
	}
}

void WriteCommand(char c)
{
//	size_t len = strlen(commandBuffer);
//	commandBuffer[len] = c;
//	commandBuffer[len + 1] = '\0';
	SerialIo::putChar(c);
}

void WriteCommand(const char* array s)
{
	while (*s != 0)
	{
		WriteCommand(*s++);
	}
}

void WriteCommand(int i)
decrease(i < 0; i)
{
	if (i < 0)
	{
		WriteCommand('-');
		i = -i;
	}
	if (i >= 10)
	{
		WriteCommand(i/10);
		i %= 10;
	}
	WriteCommand((char)((char)i + '0'));
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
					bedActiveTemp->SetValue(ival);
					break;
				case 1:
					t1ActiveTemp->SetValue(ival);
					break;
				case 2:
					t2ActiveTemp->SetValue(ival);
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
					bedStandbyTemp->SetValue(ival);
					break;
				case 1:
					t1StandbyTemp->SetValue(ival);
					break;
				case 2:
					t2StandbyTemp->SetValue(ival);
					break;
				default:
					break;
				}
			}
		}
		else if (strcmp(id, "current") == 0)
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
	}
	else
	{
		// Handle non-array values
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
	
	SerialIo::init();
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
		
		t1CurrentTemp->SetColours(white, activeBackColor);
		t2CurrentTemp->SetColours(white, standbyBackColor);
		bedActiveTemp->SetColours(white, yellow);
		
		SerialIo::checkInput();
		
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
		}
		freeMem->SetValue(getFreeMemory());
		mgr.RefreshAll(false);
		
		if (GetTickCount() - lastPollTime >= printerPollInterval)
		{
			lastPollTime += printerPollInterval;
			WriteCommand("M105 S2\n");
		}
	}
}

// End
