/*
 * Fields.cpp
 *
 * Created: 04/02/2015 11:43:36
 *  Author: David
 */ 

#include "Configuration.hpp"
#include "Vector.hpp"
#include "Display.hpp"
#include "PanelDue.hpp"
#include "Buzzer.hpp"
#include "Fields.hpp"
#include "Icons/Icons_21h.hpp"

FloatField *bedCurrentTemp, *t1CurrentTemp, *t2CurrentTemp, *fpHeightField, *fpLayerHeightField;
FloatButton *xPos, *yPos, *zPos;
IntegerButton *bedActiveTemp, *bedStandbyTemp, *t1ActiveTemp, *t2ActiveTemp, *t1StandbyTemp, *t2StandbyTemp;
IntegerButton *spd, *e1Percent, *e2Percent, *fanSpeed, *baudRateButton, *volumeButton;
IntegerField *fanRpm, *freeMem, *touchX, *touchY, *fpSizeField, *fpFilamentField;
ProgressBar *printProgressBar;
Button *tabControl, *tabPrint, *tabFiles, *tabMsg, *tabSetup;
TextButton *filenameButtons[numDisplayedFiles], *macroButtons[numDisplayedMacros];
Button *scrollFilesLeftButton, *scrollFilesRightButton;
Button *homeButtons[3], *homeAllButton, *head1State, *head2State, *bedState;
TextButton *bedCompButton;
StaticTextField *nameField, *statusField, *touchCalibInstruction;
StaticTextField *messageTextFields[numMessageRows], *messageTimeFields[numMessageRows];
StaticTextField *fwVersionField, *settingsNotSavedField, *areYouSureTextField;
TextButton *pauseButtonField, *resumeButtonField, *resetButtonField;
StaticTextField *timeLeftField;
DisplayField *baseRoot, *commonRoot, *controlRoot, *printRoot, *filesRoot, *messageRoot, *infoRoot;
Button * null currentTab = NULL;
Button * null fieldBeingAdjusted = NULL;
Button * null currentButton = NULL;
PopupField *setTempPopup, *setXYPopup, *setZPopup, *filePopup, *baudPopup, *volumePopup, *areYouSurePopup;
TextField *zProbe, *fpNameField, *fpGeneratedByField, *printingField;

String<machineNameLength> machineName;
String<printingFileLength> printingFile;
String<zprobeBufLength> zprobeBuf;
String<generatedByTextLength> generatedByText;

const int defaultTemperatures[7] = { 0, 0, 0, 0, 0, 0, 0 };
	
const Icon heaterIcons[7] = { IconBed_21h, IconNozzle1_21h, IconNozzle2_21h, IconNozzle1_21h, IconNozzle1_21h, IconNozzle1_21h, IconNozzle1_21h };

namespace Fields
{

	// Add a text button
	TextButton *AddTextButton(PixelNumber row, unsigned int col, unsigned int numCols, const char* array text, Event evt, const char* param)
	{
		PixelNumber width = (DisplayX - 2 * margin + fieldSpacing)/numCols - fieldSpacing;
		PixelNumber xpos = col * (width + fieldSpacing) + margin;
		TextButton *f = new TextButton(row - 2, xpos, width, text);
		f->SetEvent(evt, param);
		mgr.AddField(f);
		return f;
	}

	// Add an icon button
	IconButton *AddIconButton(PixelNumber row, unsigned int col, unsigned int numCols, Icon ic, Event evt, const char* param)
	{
		PixelNumber width = (DisplayX - 2 * margin + fieldSpacing)/numCols - fieldSpacing;
		PixelNumber xpos = col * (width + fieldSpacing) + margin;
		IconButton *f = new IconButton(row - 2, xpos, width, ic);
		f->SetEvent(evt, param);
		mgr.AddField(f);
		return f;
	}

	// Add an integer button
	IntegerButton *AddIntegerButton(PixelNumber row, unsigned int col, unsigned int numCols, const char * array null label, const char * array null units, Event evt)
	{
		PixelNumber width = (DisplayX - 2 * margin + fieldSpacing)/numCols - fieldSpacing;
		PixelNumber xpos = col * (width + fieldSpacing) + margin;
		IntegerButton *f = new IntegerButton(row - 2, xpos, width, label, units);
		f->SetEvent(evt, 0);
		mgr.AddField(f);
		return f;
	}
	
	void CreateIntButtonRow(PopupField * null pf, PixelNumber top, PixelNumber left, PixelNumber buttonWidth, PixelNumber spacing, unsigned int numButtons,
								const int * array values, const char * array null label, const char* array null units, Event evt)
	{
		for (unsigned int i = 0; i < numButtons; ++i)
		{
			IntegerButton *tp = new IntegerButton(top, left + i * (buttonWidth + spacing), buttonWidth, label, units);
			tp->SetValue(values[i]);
			tp->SetEvent(evt, i);
			if (pf == nullptr)
			{
				mgr.AddField(tp);
			}
			else
			{
				pf->AddField(tp);
			}		
		}	
	}

	void CreateIconButtonRow(PopupField * null pf, PixelNumber top, PixelNumber left, PixelNumber buttonWidth, PixelNumber spacing, unsigned int numButtons,
								const Icon * array icons, Event evt)
	{
		for (unsigned int i = 0; i < numButtons; ++i)
		{
			IconButton *tp = new IconButton(top, left + i * (buttonWidth + spacing), buttonWidth, icons[i]);
			tp->SetEvent(evt, i);
			if (pf == nullptr)
			{
				mgr.AddField(tp);
			}
			else
			{
				pf->AddField(tp);
			}
		}
	}

	// Create a popup bar with string parameters
	PopupField *CreateStringPopupBar(PixelNumber width, unsigned int numEntries, const char* const text[], const char* const params[], Event ev)
	{
		PopupField *pf = new PopupField(popupBarHeight, width, popupBackColour);
		DisplayField::SetDefaultColours(popupButtonTextColour, popupButtonBackColour);
		PixelNumber entryWidth = (width - 2)/numEntries;
		PixelNumber extra = (width - entryWidth * numEntries + popupFieldSpacing)/2;
		for (unsigned int i = 0; i < numEntries; ++i)
		{
			TextButton *tp = new TextButton(popupBarFieldYoffset, extra + i * entryWidth, entryWidth - popupFieldSpacing, text[i]);
			tp->SetEvent(ev, params[i]);
			pf->AddField(tp);
		}
		return pf;
	}

	// Create a popup bar with integer parameters
	PopupField *CreateIntPopupBar(PixelNumber width, unsigned int numEntries, const char* const text[], const int params[], Event ev, Event zeroEv)
	{
		PopupField *pf = new PopupField(popupBarHeight, width, popupBackColour);
		DisplayField::SetDefaultColours(popupButtonTextColour, popupButtonBackColour);
		PixelNumber entryWidth = (width - 2)/numEntries;
		PixelNumber extra = (width - entryWidth * numEntries + popupFieldSpacing)/2;
		for (unsigned int i = 0; i < numEntries; ++i)
		{
			TextButton *tp = new TextButton(popupBarFieldYoffset, extra + i * entryWidth, entryWidth - popupFieldSpacing, text[i]);
			tp->SetEvent((params[i] == 0) ? zeroEv : ev, params[i]);
			pf->AddField(tp);
		}
		return pf;
	}

	void CreateFields()
	{
		mgr.Init(defaultBackColour);
		DisplayField::SetDefaultFont(DEFAULT_FONT);
	
		// Create the fields that are displayed on more than one page
		DisplayField::SetDefaultColours(buttonTextColour, buttonBackColour, buttonBorderColour, buttonGradColour, buttonPressedBackColour, buttonPressedGradColour);
		tabControl = AddTextButton(rowTabs, 0, 5, "Control", evTabControl, nullptr);
		tabPrint = AddTextButton(rowTabs, 1, 5, "Print", evTabPrint, nullptr);
		tabFiles = AddTextButton(rowTabs, 2, 5, "Files", evTabFiles, nullptr);
		tabMsg = AddTextButton(rowTabs, 3, 5, "Msg", evTabMsg, nullptr);
		tabSetup = AddTextButton(rowTabs, 4, 5, "Setup", evTabInfo, nullptr);
	
		baseRoot = mgr.GetRoot();		// save the root of fields that we usually display
	
		DisplayField::SetDefaultColours(titleBarTextColour, titleBarBackColour);
		mgr.AddField(nameField = new StaticTextField(row1, 0, DisplayX - statusFieldWidth, TextAlignment::Centre, machineName.c_str()));
		mgr.AddField(statusField = new StaticTextField(row1, DisplayX - statusFieldWidth, statusFieldWidth, TextAlignment::Right, ""));

#if 0
		DisplayField::SetDefaultColours(labelTextColour, defaultBackColour);

		DisplayField::SetDefaultColours(buttonTextColour, buttonBackColour);
		CreateIconButtonRow(nullptr, row1, bedColumn, tempButtonWidth, fieldSpacing, maxHeads + 1, headIcons, evSelectHead);
		CreateIntButtonRow(nullptr, row2, bedColumn, tempButtonWidth, fieldSpacing, maxHeads + 1, defaultTemperatures, nullptr, DEGREE_SYMBOL "C", evAdjustTemp);
		CreateIntButtonRow(nullptr, row3, bedColumn, tempButtonWidth, fieldSpacing, maxHeads + 1, defaultTemperatures, nullptr, DEGREE_SYMBOL "C", evAdjustTemp);
#else
		DisplayField::SetDefaultColours(labelTextColour, defaultBackColour);	
		mgr.AddField(new StaticTextField(row2, column2, column3 - column2 - fieldSpacing, TextAlignment::Centre, "Current"));
		mgr.AddField(new StaticTextField(row2, column3, column4 - column3 - fieldSpacing, TextAlignment::Centre, "Active"));
		mgr.AddField(new StaticTextField(row2, column4, column5 - column4 - fieldSpacing, TextAlignment::Centre, "Idle"));

		DisplayField::SetDefaultColours(buttonTextColour, buttonBackColour);
		mgr.AddField(head1State = new IconButton(row3 - 2, column1, column2 - column1 - fieldSpacing, heaterIcons[1]));
		mgr.AddField(head2State = new IconButton(row4 - 2, column1, column2 - column1 - fieldSpacing, heaterIcons[2]));
		mgr.AddField(bedState = new IconButton(row5 - 2, column1, column2 - column1 - fieldSpacing, heaterIcons[0]));
		head1State->SetEvent(evSelectHead, 1);
		head2State->SetEvent(evSelectHead, 2);
		bedState->SetEvent(evSelectHead, 0);

		mgr.AddField(t1ActiveTemp = new IntegerButton(row3 - 2, column3, column4 - column3 - fieldSpacing, nullptr, DEGREE_SYMBOL "C"));
		mgr.AddField(t1StandbyTemp = new IntegerButton(row3 - 2, column4, column5 - column4 - fieldSpacing, nullptr, DEGREE_SYMBOL "C"));
		mgr.AddField(t2ActiveTemp = new IntegerButton(row4 - 2, column3, column4 - column3 - fieldSpacing, nullptr, DEGREE_SYMBOL "C"));
		mgr.AddField(t2StandbyTemp = new IntegerButton(row4 - 2, column4, column5 - column4 - fieldSpacing, nullptr, DEGREE_SYMBOL "C"));
		mgr.AddField(bedActiveTemp = new IntegerButton(row5 - 2, column3, column4 - column3 - fieldSpacing, nullptr, DEGREE_SYMBOL "C"));
		mgr.AddField(bedStandbyTemp = new IntegerButton(row5 - 2, column4, column5 - column4 - fieldSpacing, nullptr, DEGREE_SYMBOL "C"));
		t1ActiveTemp->SetEvent(evAdjustTemp, "G10 P0 S");
		t1StandbyTemp->SetEvent(evAdjustTemp, "G10 P0 R");
		t2ActiveTemp->SetEvent(evAdjustTemp, "G10 P1 S");
		t2StandbyTemp->SetEvent(evAdjustTemp, "G10 P1 R");
		bedActiveTemp->SetEvent(evAdjustTemp, "M140 S");

		DisplayField::SetDefaultColours(infoTextColour, defaultBackColour);
		mgr.AddField(t1CurrentTemp = new FloatField(row3, column2, column3 - column2 - fieldSpacing, TextAlignment::Centre, 1, nullptr, DEGREE_SYMBOL "C"));
		mgr.AddField(t2CurrentTemp = new FloatField(row4, column2, column3 - column2 - fieldSpacing, TextAlignment::Centre, 1, nullptr, DEGREE_SYMBOL "C"));
		mgr.AddField(bedCurrentTemp = new FloatField(row5, column2, column3 - column2 - fieldSpacing, TextAlignment::Centre, 1, nullptr, DEGREE_SYMBOL "C"));
#endif
		commonRoot = mgr.GetRoot();		// save the root of fields that we display on more than one page
		
		// Create the extra fields for the Control tab
		mgr.AddField(new StaticTextField(row2, columnX, columnY - columnX - fieldSpacing, TextAlignment::Centre, "X"));
		mgr.AddField(new StaticTextField(row2, columnY, DisplayX - columnY - margin, TextAlignment::Centre, "Y"));
		mgr.AddField(new StaticTextField(row4, columnX, columnY - columnX - fieldSpacing, TextAlignment::Centre, "Z"));
		mgr.AddField(new StaticTextField(row4, columnY, DisplayX - columnY - margin, TextAlignment::Centre, "Probe"));

		DisplayField::SetDefaultColours(buttonTextColour, buttonBackColour);
		mgr.AddField(xPos = new FloatButton(row3 - 2, columnX, columnY - columnX - fieldSpacing, 1));
		mgr.AddField(yPos = new FloatButton(row3 - 2, columnY, DisplayX - columnY - margin, 1));
		mgr.AddField(zPos = new FloatButton(row5 - 2, columnX, columnY - columnX - fieldSpacing, 2));
		xPos->SetEvent(evXYPos, "G1 X");
		yPos->SetEvent(evXYPos, "G1 Y");
		zPos->SetEvent(evZPos, "G1 Z");

		zprobeBuf[0] = 0;
		DisplayField::SetDefaultColours(infoTextColour, defaultBackColour);
		mgr.AddField(zProbe = new TextField(row5, columnY, DisplayX - columnY - margin, TextAlignment::Centre, NULL, zprobeBuf.c_str()));
	
		DisplayField::SetDefaultColours(buttonTextColour, notHomedButtonBackColour);
		homeAllButton = AddTextButton(row6 - 2, 0, 5, "Home all", evSendCommand, "G28");
		homeButtons[0] = AddTextButton(row6 - 2, 1, 5, "Home X", evSendCommand, "G28 X0");
		homeButtons[1] = AddTextButton(row6 - 2, 2, 5, "Home Y", evSendCommand, "G28 Y0");
		homeButtons[2] = AddTextButton(row6 - 2, 3, 5, "Home Z", evSendCommand, "G28 Z0");
		DisplayField::SetDefaultColours(buttonTextColour, buttonBackColour);
		bedCompButton = AddTextButton(row6 - 2, 4, 5, "Bed comp", evSendCommand, "G32");
	
		DisplayField::SetDefaultColours(buttonTextColour, buttonBackColour);

		// Create the macro fields
		{
			const PixelNumber macroFieldWidth = (DisplayX + fieldSpacing - 2*margin)/numMacroColumns;
			unsigned int macroNum = 0;
			for (unsigned int c = 0; c < numMacroColumns; ++c)
			{
				for (unsigned int r = 0; r < numMacroRows; ++r)
				{
					TextButton *t = new TextButton((r * macroRowHeight) + firstMacroRow - 1, (macroFieldWidth * c) + margin, macroFieldWidth - fieldSpacing, "");
					t->Show(false);
					mgr.AddField(t);
					macroButtons[macroNum] = t;
					++macroNum;
				}
			}
		}

		controlRoot = mgr.GetRoot();

		// Create the fields for the Printing tab
		mgr.SetRoot(commonRoot);
		DisplayField::SetDefaultColours(labelTextColour, defaultBackColour);
		mgr.AddField(new StaticTextField(row2, columnX, columnY - columnX - fieldSpacing, TextAlignment::Centre, "Fan"));
		mgr.AddField(new StaticTextField(row2, columnY, DisplayX - columnY - margin, TextAlignment::Centre, "RPM"));

		DisplayField::SetDefaultColours(buttonTextColour, buttonBackColour);
		mgr.AddField(fanSpeed = new IntegerButton(row3 - 2, columnX, columnY - columnX - fieldSpacing));
		fanSpeed->SetEvent(evAdjustPercent, "M106 S");
		DisplayField::SetDefaultColours(infoTextColour, defaultBackColour);
		mgr.AddField(fanRpm = new IntegerField(row3, columnY, DisplayX - columnY - margin, TextAlignment::Centre));

		DisplayField::SetDefaultColours(buttonTextColour, pauseButtonBackColour);
		pauseButtonField = new TextButton(row5 - 2, columnX, DisplayX - columnX - margin, "Pause print");
		pauseButtonField->SetEvent(evPausePrint, "M25");
		mgr.AddField(pauseButtonField);

		DisplayField::SetDefaultColours(buttonTextColour, resumeButtonBackColour);
		resumeButtonField = new TextButton(row5 - 2, columnX, columnY - columnX - fieldSpacing, "Resume");
		resumeButtonField->SetEvent(evResumePrint, "M24");
		mgr.AddField(resumeButtonField);

		DisplayField::SetDefaultColours(buttonTextColour, resetButtonBackColour);
		resetButtonField = new TextButton(row5 - 2, columnY, DisplayX - columnY - margin, "Cancel");
		resetButtonField->SetEvent(evReset, "M0");
		mgr.AddField(resetButtonField);

		DisplayField::SetDefaultColours(labelTextColour, defaultBackColour);
		mgr.AddField(new StaticTextField(row6, margin, speedTextWidth, TextAlignment::Left, "Speed"));
		DisplayField::SetDefaultColours(buttonTextColour, buttonBackColour);
		mgr.AddField(spd = new IntegerButton(row6 - 2, speedTextWidth, percentageWidth, nullptr, "%"));
		spd->SetValue(100);
		spd->SetEvent(evAdjustPercent, "M220 S");
		DisplayField::SetDefaultColours(labelTextColour, defaultBackColour);
		mgr.AddField(new StaticTextField(row6, e1FactorXpos, efactorTextWidth, TextAlignment::Left, "E1"));
		DisplayField::SetDefaultColours(buttonTextColour, buttonBackColour);
		mgr.AddField(e1Percent = new IntegerButton(row6 - 2, e1FactorXpos + efactorTextWidth, percentageWidth, nullptr, "%"));
		e1Percent->SetValue(100);
		e1Percent->SetEvent(evAdjustPercent, "M221 D0 S");
		DisplayField::SetDefaultColours(labelTextColour, defaultBackColour);
		mgr.AddField(new StaticTextField(row6, e2FactorXpos, efactorTextWidth, TextAlignment::Left, "E2"));
		DisplayField::SetDefaultColours(buttonTextColour, buttonBackColour);
		mgr.AddField(e2Percent = new IntegerButton(row6 - 2, e2FactorXpos + efactorTextWidth, percentageWidth, nullptr, "%"));
		e2Percent->SetValue(100);
		e2Percent->SetEvent(evAdjustPercent, "M221 D1 S");
	
		DisplayField::SetDefaultColours(labelTextColour, defaultBackColour);
		mgr.AddField(printingField = new TextField(row7, margin, DisplayX, TextAlignment::Left, "Printing ", printingFile.c_str()));
	
		DisplayField::SetDefaultColours(progressBarColour, progressBarBackColour);
		mgr.AddField(printProgressBar = new ProgressBar(row8, margin, 8, DisplayX - 2 * margin));
		mgr.Show(printProgressBar, false);
		
		DisplayField::SetDefaultColours(labelTextColour, defaultBackColour);
		mgr.AddField(timeLeftField = new StaticTextField(row9, margin, DisplayX - 2 * margin, TextAlignment::Left, ""));

		printRoot = mgr.GetRoot();

		// Create the fields for the Files tab
		mgr.SetRoot(baseRoot);
		DisplayField::SetDefaultColours(labelTextColour, defaultBackColour);
		mgr.AddField(new StaticTextField(row1 + 2, 135, DisplayX - 2*135, TextAlignment::Centre, "Files on SD card"));
		DisplayField::SetDefaultColours(buttonTextColour, buttonBackColour);
		{
			const PixelNumber fileFieldWidth = (DisplayX + fieldSpacing - 2*margin)/numFileColumns;
			unsigned int fileNum = 0;
			for (unsigned int c = 0; c < numFileColumns; ++c)
			{
				for (unsigned int r = 0; r < numFileRows; ++r)
				{
					TextButton *t = new TextButton((r * rowHeight) + firstFileRow, (fileFieldWidth * c) + margin, fileFieldWidth - fieldSpacing, "");
					t->Show(false);
					mgr.AddField(t);
					filenameButtons[fileNum] = t;
					++fileNum;
				}
			}
		}
		DisplayField::SetDefaultColours(buttonTextColour, buttonBackColour);
		mgr.AddField(scrollFilesLeftButton = new TextButton(row1, 80, 50, "<"));
		scrollFilesLeftButton->SetEvent(evScrollFiles, -numFileRows);
		scrollFilesLeftButton->Show(false);
		mgr.AddField(scrollFilesRightButton = new TextButton(row1, DisplayX - (80 + 50), 50, ">"));
		scrollFilesRightButton->SetEvent(evScrollFiles, numFileRows);
		scrollFilesRightButton->Show(false);
		filesRoot = mgr.GetRoot();

		// Create the fields for the Message tab
		mgr.SetRoot(baseRoot);
		DisplayField::SetDefaultColours(labelTextColour, defaultBackColour);
		mgr.AddField(new StaticTextField(row1, 135, DisplayX - 2*135, TextAlignment::Centre, "Messages"));
		{
			for (unsigned int r = 0; r < numMessageRows; ++r)
			{
				StaticTextField *t = new StaticTextField(((r + 1) * rowHeight) + 8, margin, messageTimeWidth, TextAlignment::Left, "");
				mgr.AddField(t);
				messageTimeFields[r] = t;
				t = new StaticTextField(((r + 1) * rowHeight) + 8, messageTextX, messageTextWidth, TextAlignment::Left, "");
				mgr.AddField(t);
				messageTextFields[r] = t;
			}
		}
		messageRoot = mgr.GetRoot();

		// Create the fields for the Setup tab
		mgr.SetRoot(baseRoot);
		DisplayField::SetDefaultColours(labelTextColour, defaultBackColour);
		// The firmware version field doubles up as an area for displaying debug messages, so make it the full width of the display
		mgr.AddField(fwVersionField = new StaticTextField(row1, margin, DisplayX, TextAlignment::Left, "Panel Due firmware version " VERSION_TEXT));
		mgr.AddField(freeMem = new IntegerField(row2, margin, DisplayX/2 - margin, TextAlignment::Left, "Free RAM: "));
		mgr.AddField(touchX = new IntegerField(row2, DisplayX/2, DisplayX/4, TextAlignment::Left, "Touch: ", ","));
		mgr.AddField(touchY = new IntegerField(row2, (DisplayX * 3)/4, DisplayX/4, TextAlignment::Left));
		
		DisplayField::SetDefaultColours(errorTextColour, errorBackColour);
		mgr.AddField(settingsNotSavedField = new StaticTextField(row4, margin, DisplayX - 2 * margin, TextAlignment::Left, "Some settings are not saved!"));
		settingsNotSavedField->Show(false);

		DisplayField::SetDefaultColours(buttonTextColour, buttonBackColour);
		baudRateButton = AddIntegerButton(row5, 0, 3, nullptr, " baud", evSetBaudRate);
		volumeButton = AddIntegerButton(row5, 1, 3, "Volume ", nullptr, evSetVolume);
		AddTextButton(row5, 2, 3, "Calibrate touch", evCalTouch, nullptr);
		AddTextButton(row6, 0, 3, "Invert display", evInvertDisplay, nullptr);
		AddTextButton(row6, 1, 3, "Save settings", evSaveSettings, nullptr);
		AddTextButton(row6, 2, 3, "Factory reset", evFactoryReset, nullptr);
	
		DisplayField::SetDefaultColours(labelTextColour, defaultBackColour);
		infoRoot = mgr.GetRoot();
	
		mgr.SetRoot(NULL);
	
		touchCalibInstruction = new StaticTextField(DisplayY/2 - 10, 0, DisplayX, TextAlignment::Centre, "");		// the text is filled in within CalibrateTouch

		// Create the popup window used to adjust temperatures and fan speed
		static const char* const tempPopupText[] = {"-5", "-1", "Set", "+1", "+5"};
		static const int tempPopupParams[] = { -5, -1, 0, 1, 5 };
		setTempPopup = CreateIntPopupBar(tempPopupBarWidth, 5, tempPopupText, tempPopupParams, evAdjustInt, evSetInt);

		// Create the popup window used to adjust XY position
		static const char * const xyPopupText[] = {"-50", "-5", "-0.5", "+0.5", "+5", "+50" };
		static const char * const xyPopupParams[] = {"-50", "-5", "-0.5", "0.5", "5", "50" };
		setXYPopup = CreateStringPopupBar(xyPopupBarWidth, 6, xyPopupText, xyPopupParams, evAdjustPosition);

		// Create the popup window used to adjust Z position
		static const char * const zPopupText[] = {"-5", "-0.5", "-0.05", "+0.05", "+0.5", "+5" };
		static const char * const zPopupParams[] = {"-5", "-0.5", "-0.05", "0.05", "0.5", "5" };
		setZPopup = CreateStringPopupBar(zPopupBarWidth, 6, zPopupText, zPopupParams, evAdjustPosition);
	
		// Create the popup window used to display the file dialog
		filePopup = new PopupField(filePopupHeight, filePopupWidth, popupBackColour);
		DisplayField::SetDefaultColours(popupTextColour, popupBackColour);

		fpNameField = new TextField(10, 10, filePopupWidth - 20, TextAlignment::Left, "Filename: ");
		fpSizeField = new IntegerField(10 + rowHeight, 10, filePopupWidth - 20, TextAlignment::Left, "Size: ", " bytes");
		fpLayerHeightField = new FloatField(10 + 2 * rowHeight, 10, filePopupWidth - 20, TextAlignment::Left, 1, "Layer height: ","mm");
		fpHeightField = new FloatField(10 + 3 * rowHeight, 10, filePopupWidth - 20, TextAlignment::Left, 1, "Object height: ", "mm");
		fpFilamentField = new IntegerField(10 + 4 * rowHeight, 10, filePopupWidth - 20, TextAlignment::Left, "Filament needed: ", "mm");
		fpGeneratedByField = new TextField(10 + 5 * rowHeight, 10, filePopupWidth - 20, TextAlignment::Left, "Sliced by: ", generatedByText.c_str());
		filePopup->AddField(fpNameField);
		filePopup->AddField(fpSizeField);
		filePopup->AddField(fpLayerHeightField);
		filePopup->AddField(fpHeightField);
		filePopup->AddField(fpFilamentField);
		filePopup->AddField(fpGeneratedByField);

		DisplayField::SetDefaultColours(popupButtonTextColour, popupButtonBackColour);
		Button *tp = new TextButton(10 + 7 * rowHeight, 10, filePopupWidth/3 - 20, "Print");
		tp->SetEvent(evPrint, 0);
		filePopup->AddField(tp);
		tp = new TextButton(10 + 7 * rowHeight, filePopupWidth/3 + 10, filePopupWidth/3 - 20, "Cancel");
		tp->SetEvent(evCancelPrint, 0);
		filePopup->AddField(tp);
		tp = new TextButton(10 + 7 * rowHeight, (2 * filePopupWidth)/3 + 10, filePopupWidth/3 - 20, "Delete");
		tp->SetEvent(evDeleteFile, 0);
		filePopup->AddField(tp);

		// Create the baud rate adjustment popup
		static const char* const baudPopupText[] = { "9600", "19200", "38400", "57600", "115200" };
		static const int baudPopupParams[] = { 9600, 19200, 38400, 57600, 115200 };
		baudPopup = CreateIntPopupBar(fullPopupBarWidth, 5, baudPopupText, baudPopupParams, evAdjustBaudRate, evAdjustBaudRate);
	
		// Create the volume adjustment popup
		static const char* const volumePopupText[Buzzer::MaxVolume + 1] = { "Off", "1", "2", "3", "4", "5" };
		static const int volumePopupParams[Buzzer::MaxVolume + 1] = { 0, 1, 2, 3, 4, 5 };
		volumePopup = CreateIntPopupBar(fullPopupBarWidth, Buzzer::MaxVolume + 1, volumePopupText, volumePopupParams, evAdjustVolume, evAdjustVolume);
		
		// Create the "Are you sure?" popup
		areYouSurePopup = new PopupField(areYouSurePopupHeight, areYouSurePopupWidth, popupBackColour);
		DisplayField::SetDefaultColours(popupTextColour, popupBackColour);
		areYouSurePopup->AddField(areYouSureTextField = new StaticTextField(10, margin, areYouSurePopupWidth - 2 * margin, TextAlignment::Centre, ""));
		areYouSurePopup->AddField(new StaticTextField(10 + rowHeight, margin, areYouSurePopupWidth - 2 * margin, TextAlignment::Centre, "Are you sure?"));

		DisplayField::SetDefaultColours(popupButtonTextColour, popupButtonBackColour);
		tp = new TextButton(10 + 2 * rowHeight, 10, areYouSurePopupWidth/2 - 20, "Yes");
		tp->SetEvent(evYes, 0);
		areYouSurePopup->AddField(tp);
		tp = new TextButton(10 + 2 * rowHeight, areYouSurePopupWidth/2 + 10, areYouSurePopupWidth/2 - 20, "Cancel");
		tp->SetEvent(evCancel, 0);
		areYouSurePopup->AddField(tp);
		
		// Set initial values
		bedCurrentTemp->SetValue(0.0);
		t1CurrentTemp->SetValue(0.0);
		t2CurrentTemp->SetValue(0.0);
		bedActiveTemp->SetValue(0);
		t1ActiveTemp->SetValue(0);
		t2ActiveTemp->SetValue(0);
		t1StandbyTemp->SetValue(0);
		t2StandbyTemp->SetValue(0);
		xPos->SetValue(0.0);
		yPos->SetValue(0.0);
		zPos->SetValue(0.0);
		fanSpeed->SetValue(0);
		fanRpm->SetValue(0);
		spd->SetValue(100);
		e1Percent->SetValue(100);
		e2Percent->SetValue(100);
	}
	
	// Show or hide the field that warns about unsaved settings
	void SettingsAreSaved(bool areSaved)
	{
		mgr.Show(settingsNotSavedField, !areSaved);
	}
	
	void ShowPauseButton()
	{
		mgr.Show(resumeButtonField, false);
		mgr.Show(resetButtonField, false);
		mgr.Show(pauseButtonField, true);
	}
	
	void ShowResumeAndCancelButtons()
	{
		mgr.Show(pauseButtonField, false);
		mgr.Show(resumeButtonField, true);
		mgr.Show(resetButtonField, true);
	}
	
	void HidePauseResumeCancelButtons()
	{
		mgr.Show(pauseButtonField, false);
		mgr.Show(resumeButtonField, false);
		mgr.Show(resetButtonField, false);
	}
}

// End

