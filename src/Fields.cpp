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

FloatField *bedCurrentTemp, *t1CurrentTemp, *t2CurrentTemp, *xPos, *yPos, *zPos, *fpHeightField, *fpLayerHeightField;
IntegerField *bedActiveTemp, *t1ActiveTemp, *t2ActiveTemp, *t1StandbyTemp, *t2StandbyTemp, *spd, *e1Percent, *e2Percent, *fanSpeed, *fanRpm;
IntegerField *bedStandbyTemp, *freeMem, *touchX, *touchY, *fpSizeField, *fpFilamentField, *baudRateField;
ProgressBar *printProgressBar;
StaticTextField *nameField, *statusField, *head1State, *head2State, *bedState, *tabControl, *tabPrint, *tabFiles, *tabMsg, *tabInfo, *touchCalibInstruction;
StaticTextField *filenameFields[numDisplayedFiles], *messageTextFields[numMessageRows], *messageTimeFields[numMessageRows], *scrollFilesLeftField, *scrollFilesRightField;
StaticTextField *homeFields[3], *homeAllField, *fwVersionField, *settingsNotSavedField, *areYouSureTextField;
StaticTextField *pauseButtonField, *resumeButtonField, *resetButtonField, *timeLeftField;
DisplayField *baseRoot, *commonRoot, *controlRoot, *printRoot, *filesRoot, *messageRoot, *infoRoot;
DisplayField * null currentTab = NULL;
DisplayField * null fieldBeingAdjusted = NULL;
PopupField *setTempPopup, *setXYPopup, *setZPopup, *filePopup, *baudPopup, *volumePopup, *areYouSurePopup;
//TextField *commandField;
TextField *zProbe, *fpNameField, *fpGeneratedByField, *printingField;

String<machineNameLength> machineName;
String<printingFileLength> printingFile;
String<zprobeBufLength> zprobeBuf;
String<generatedByTextLength> generatedByText;

namespace Fields
{

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
	
	// Create a popup bar with string parameters
	PopupField *CreateStringPopupBar(unsigned int numEntries, const char* const text[], const char* const params[], Event ev)
	{
		PopupField *pf = new PopupField(popupBarHeight, popupBarWidth, popupBackColour);
		DisplayField::SetDefaultColours(white, selectablePopupBackColour);
		PixelNumber entryWidth = (popupBarWidth + fieldSpacing - 2 * margin)/numEntries;
		for (unsigned int i = 0; i < numEntries; ++i)
		{
			StaticTextField *tp = new StaticTextField(popupBarFieldYoffset, margin + i * entryWidth, entryWidth - fieldSpacing, Centre, text[i]);
			tp->SetEvent(ev, params[i]);
			pf->AddField(tp);
		}
		return pf;
	}

	// Create a popup bar with integer parameters
	PopupField *CreateIntPopupBar(unsigned int numEntries, const char* const text[], const int params[], Event ev)
	{
		PopupField *pf = new PopupField(popupBarHeight, popupBarWidth, popupBackColour);
		DisplayField::SetDefaultColours(white, selectablePopupBackColour);
		PixelNumber entryWidth = (popupBarWidth + fieldSpacing - 2 * margin)/numEntries;
		for (unsigned int i = 0; i < numEntries; ++i)
		{
			StaticTextField *tp = new StaticTextField(popupBarFieldYoffset, margin + i * entryWidth, entryWidth - fieldSpacing, Centre, text[i]);
			tp->SetEvent(ev, params[i]);
			pf->AddField(tp);
		}
		return pf;
	}

	void CreateFields()
	{
		mgr.Init(defaultBackColor);
		DisplayField::SetDefaultFont(glcd19x20);
	
		// Create the fields that are displayed on more than one page
		DisplayField::SetDefaultColours(white, selectableBackColor);
		tabControl = AddCommandCell(rowTabs, 0, 5, "Control", evTabControl, nullptr);
		tabPrint = AddCommandCell(rowTabs, 1, 5, "Print", evTabPrint, nullptr);
		tabFiles = AddCommandCell(rowTabs, 2, 5, "Files", evTabFiles, nullptr);
		tabMsg = AddCommandCell(rowTabs, 3, 5, "Msg", evTabMsg, nullptr);
		tabInfo = AddCommandCell(rowTabs, 4, 5, "Setup", evTabInfo, nullptr);
	
		baseRoot = mgr.GetRoot();		// save the root of fields that we usually display
	
		DisplayField::SetDefaultColours(white, red);
	
		mgr.AddField(nameField = new StaticTextField(row1, 0, DisplayX - statusFieldWidth, Centre, machineName.c_str()));
		mgr.AddField(statusField = new StaticTextField(row1, DisplayX - statusFieldWidth, statusFieldWidth, Right, ""));
		DisplayField::SetDefaultColours(white, defaultBackColor);
	
		mgr.AddField(new StaticTextField(row2, column2, column3 - column2 - fieldSpacing, Left, "Current"));
		mgr.AddField(new StaticTextField(row2, column3, column4 - column3 - fieldSpacing, Left, "Active"));
		mgr.AddField(new StaticTextField(row2, column4, column5 - column4 - fieldSpacing, Left, "Idle"));

		DisplayField::SetDefaultColours(white, selectableBackColor);
		mgr.AddField(head1State = new StaticTextField(row3, column1, column2 - column1 - fieldSpacing, Left, THIN_SPACE "Head 1"));
		mgr.AddField(head2State = new StaticTextField(row4, column1, column2 - column1 - fieldSpacing, Left, THIN_SPACE "Head 2"));
		mgr.AddField(bedState = new StaticTextField(row5, column1, column2 - column1 - fieldSpacing, Left, THIN_SPACE "Bed"));
		head1State->SetEvent(evSelectHead, 1);
		head2State->SetEvent(evSelectHead, 2);
		bedState->SetEvent(evSelectHead, 0);

		mgr.AddField(t1ActiveTemp = new IntegerField(row3, column3, column4 - column3 - fieldSpacing, NULL, DEGREE_SYMBOL "C"));
		mgr.AddField(t1StandbyTemp = new IntegerField(row3, column4, column5 - column4 - fieldSpacing, NULL, DEGREE_SYMBOL "C"));
		mgr.AddField(t2ActiveTemp = new IntegerField(row4, column3, column4 - column3 - fieldSpacing, NULL, DEGREE_SYMBOL "C"));
		mgr.AddField(t2StandbyTemp = new IntegerField(row4, column4, column5 - column4 - fieldSpacing, NULL, DEGREE_SYMBOL "C"));
		mgr.AddField(bedActiveTemp = new IntegerField(row5, column3, column4 - column3 - fieldSpacing, NULL, DEGREE_SYMBOL "C"));
		t1ActiveTemp->SetEvent(evAdjustTemp, "G10 P1 S");
		t1StandbyTemp->SetEvent(evAdjustTemp, "G10 P1 R");
		t2ActiveTemp->SetEvent(evAdjustTemp, "G10 P2 S");
		t2StandbyTemp->SetEvent(evAdjustTemp, "G10 P2 R");
		bedActiveTemp->SetEvent(evAdjustTemp, "M140 S");

		DisplayField::SetDefaultColours(white, defaultBackColor);
		mgr.AddField(t1CurrentTemp = new FloatField(row3, column2, column3 - column2 - fieldSpacing, NULL, 1, DEGREE_SYMBOL "C"));
		mgr.AddField(t2CurrentTemp = new FloatField(row4, column2, column3 - column2 - fieldSpacing, NULL, 1, DEGREE_SYMBOL "C"));

		mgr.AddField(bedCurrentTemp = new FloatField(row5, column2, column3 - column2 - fieldSpacing, NULL, 1, DEGREE_SYMBOL "C"));
		mgr.AddField(bedStandbyTemp = new IntegerField(row5, column4, column5 - column4 - fieldSpacing, NULL, DEGREE_SYMBOL "C"));

		commonRoot = mgr.GetRoot();		// save the root of fields that we display on more than one page
		
		// Create the extra fields for the Control tab
		mgr.AddField(new StaticTextField(row2, columnX, columnY - columnX - fieldSpacing, Left, "X"));
		mgr.AddField(new StaticTextField(row2, columnY, DisplayX - columnY - margin, Left, "Y"));
		mgr.AddField(new StaticTextField(row4, columnX, columnY - columnX - fieldSpacing, Left, "Z"));
		mgr.AddField(new StaticTextField(row4, columnY, DisplayX - columnY - margin, Left, "Probe"));

		DisplayField::SetDefaultColours(white, selectableBackColor);
		mgr.AddField(xPos = new FloatField(row3, columnX, columnY - columnX - fieldSpacing, NULL, 1));
		mgr.AddField(yPos = new FloatField(row3, columnY, DisplayX - columnY - margin, NULL, 1));
		mgr.AddField(zPos = new FloatField(row5, columnX, columnY - columnX - fieldSpacing, NULL, 2));
		xPos->SetEvent(evXYPos, "G1 X");
		yPos->SetEvent(evXYPos, "G1 Y");
		zPos->SetEvent(evZPos, "G1 Z");

		zprobeBuf[0] = 0;
		DisplayField::SetDefaultColours(white, defaultBackColor);
		mgr.AddField(zProbe = new TextField(row5, columnY, DisplayX - columnY - margin, NULL, zprobeBuf.c_str()));
	
		DisplayField::SetDefaultColours(white, notHomedBackColour);
		homeAllField = AddCommandCell(row6, 0, 5, "Home all", evSendCommand, "G28");
		homeFields[0] = AddCommandCell(row6, 1, 5, "Home X", evSendCommand, "G28 X0");
		homeFields[1] = AddCommandCell(row6, 2, 5, "Home Y", evSendCommand, "G28 Y0");
		homeFields[2] = AddCommandCell(row6, 3, 5, "Home Z", evSendCommand, "G28 Z0");
	
		DisplayField::SetDefaultColours(white, selectableBackColor);
		AddCommandCell(row9, 0, 5, "G92 Z0", evSendCommand, "G92 Z0");
		AddCommandCell(row9, 1, 5, "G1 X0 Y0", evSendCommand, "G1 X0 Y0 F5000");
		AddCommandCell(row9, 2, 5, "G1 Z1", evSendCommand, "G1 Z1 F5000");
		AddCommandCell(row9, 3, 5, "G32", evSendCommand, "G32");
	
		controlRoot = mgr.GetRoot();

		// Create the fields for the Printing tab
		mgr.SetRoot(commonRoot);
		DisplayField::SetDefaultColours(white, defaultBackColor);
		mgr.AddField(new StaticTextField(row2, columnX, columnY - columnX - fieldSpacing, Left, "Fan"));
		mgr.AddField(new StaticTextField(row2, columnY, DisplayX - columnY - margin, Left, "RPM"));

		DisplayField::SetDefaultColours(white, selectableBackColor);
		mgr.AddField(fanSpeed = new IntegerField(row3, columnX, columnY - columnX - fieldSpacing, NULL));
		fanSpeed->SetEvent(evAdjustPercent, "M106 S");
		DisplayField::SetDefaultColours(white, defaultBackColor);
		mgr.AddField(fanRpm = new IntegerField(row3, columnY, DisplayX - columnY - margin, NULL));

		DisplayField::SetDefaultColours(white, pauseButtonBackColor);
		pauseButtonField = new StaticTextField(row5, columnX, DisplayX - columnX - margin, Centre, "Pause print");
		pauseButtonField->SetEvent(evPausePrint, "M25");
		mgr.AddField(pauseButtonField);

		DisplayField::SetDefaultColours(white, resumeButtonBackColor);
		resumeButtonField = new StaticTextField(row5, columnX, columnY - columnX - fieldSpacing, Centre, "Resume");
		resumeButtonField->SetEvent(evResumePrint, "M24");
		mgr.AddField(resumeButtonField);

		DisplayField::SetDefaultColours(white, resetButtonBackColor);
		resetButtonField = new StaticTextField(row5, columnY, DisplayX - columnY - margin, Centre, "Cancel");
		resetButtonField->SetEvent(evReset, "M0");
		mgr.AddField(resetButtonField);

		DisplayField::SetDefaultColours(white, defaultBackColor);
		mgr.AddField(new StaticTextField(row6, 0, 70, Left, "Speed"));
		DisplayField::SetDefaultColours(white, selectableBackColor);
		mgr.AddField(spd = new IntegerField(row6, 70, 60, "", "%"));
		spd->SetValue(100);
		spd->SetEvent(evAdjustPercent, "M220 S");
		DisplayField::SetDefaultColours(white, defaultBackColor);
		mgr.AddField(new StaticTextField(row6, 140, 30, Left, "E1"));
		DisplayField::SetDefaultColours(white, selectableBackColor);
		mgr.AddField(e1Percent = new IntegerField(row6, 170, 60, "", "%"));
		e1Percent->SetValue(100);
		e1Percent->SetEvent(evAdjustPercent, "M221 D0 S");
		DisplayField::SetDefaultColours(white, defaultBackColor);
		mgr.AddField(new StaticTextField(row6, 250, 30, Left, "E2"));
		DisplayField::SetDefaultColours(white, selectableBackColor);
		mgr.AddField(e2Percent = new IntegerField(row6, 280, 60, "", "%"));
		e2Percent->SetValue(100);
		e2Percent->SetEvent(evAdjustPercent, "M221 D1 S");
	
		DisplayField::SetDefaultColours(white, defaultBackColor);
		mgr.AddField(printingField = new TextField(row7, 0, DisplayX, "Printing ", printingFile.c_str()));
	
		DisplayField::SetDefaultColours(white, UTFT::fromRGB(0, 160, 0));
		mgr.AddField(printProgressBar = new ProgressBar(row8, margin, 8, DisplayX - 2*margin));
		mgr.Show(printProgressBar, false);
		
		DisplayField::SetDefaultColours(white, defaultBackColor);
		mgr.AddField(timeLeftField = new StaticTextField(row9, margin, DisplayX - 2 * margin, Left, ""));

		printRoot = mgr.GetRoot();

		// Create the fields for the Files tab
		mgr.SetRoot(baseRoot);
		DisplayField::SetDefaultColours(white, defaultBackColor);
		mgr.AddField(new StaticTextField(row1, 135, DisplayX - 2*135, Centre, "Files on SD card"));
		{
			const PixelNumber fileFieldWidth = (DisplayX + fieldSpacing - 2*margin)/numFileColumns;
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
		mgr.AddField(scrollFilesLeftField = new StaticTextField(row1, 80, 50, Centre, "<"));
		scrollFilesLeftField->SetEvent(evScrollFiles, -numFileRows);
		mgr.AddField(scrollFilesRightField = new StaticTextField(row1, DisplayX - (80 + 50), 50, Centre, ">"));
		scrollFilesRightField->SetEvent(evScrollFiles, numFileRows);
		filesRoot = mgr.GetRoot();

		// Create the fields for the Message tab
		mgr.SetRoot(baseRoot);
		DisplayField::SetDefaultColours(white, defaultBackColor);
		mgr.AddField(new StaticTextField(row1, 135, DisplayX - 2*135, Centre, "Messages"));
		{
			for (unsigned int r = 0; r < numMessageRows; ++r)
			{
				StaticTextField *t = new StaticTextField(((r + 1) * rowHeight) + 8, margin, messageTimeWidth, Left, "");
				mgr.AddField(t);
				messageTimeFields[r] = t;
				t = new StaticTextField(((r + 1) * rowHeight) + 8, messageTextX, messageTextWidth, Left, "");
				mgr.AddField(t);
				messageTextFields[r] = t;
			}
		}
		messageRoot = mgr.GetRoot();

		// Create the fields for the Setup tab
		mgr.SetRoot(baseRoot);
		DisplayField::SetDefaultColours(white, defaultBackColor);
		// The firmware version field doubles up as an area for displaying debug messages, so make it the full width of the display
		mgr.AddField(fwVersionField = new StaticTextField(row1, margin, DisplayX, Left, "Panel Due firmware version " VERSION_TEXT));
		mgr.AddField(freeMem = new IntegerField(row2, margin, 195, "Free RAM: "));
		mgr.AddField(touchX = new IntegerField(row2, 200, 130, "Touch: ", ","));
		mgr.AddField(touchY = new IntegerField(row2, 330, 50, ""));
		mgr.AddField(baudRateField = new IntegerField(row3, margin, 195, "Baud rate: "));
		mgr.AddField(settingsNotSavedField = new StaticTextField(row4, margin, 300, Left, ""));

		DisplayField::SetDefaultColours(white, selectableBackColor);
		AddCommandCell(row6, 0, 3, "Baud rate", evSetBaudRate, nullptr);
		AddCommandCell(row6, 1, 3, "Beep volume", evSetVolume, nullptr);
		AddCommandCell(row6, 2, 3, "Calibrate touch", evCalTouch, nullptr);
		AddCommandCell(row7, 0, 3, "Invert display", evInvertDisplay, nullptr);
		AddCommandCell(row7, 1, 3, "Save settings", evSaveSettings, nullptr);
		AddCommandCell(row7, 2, 3, "Factory reset", evFactoryReset, nullptr);
	
		DisplayField::SetDefaultColours(white, defaultBackColor);
		infoRoot = mgr.GetRoot();
	
		mgr.SetRoot(commonRoot);
	
		touchCalibInstruction = new StaticTextField(DisplayY/2 - 10, 0, DisplayX, Centre, "");		// the text is filled in within CalibrateTouch

		// Create the popup window used to adjust temperatures and fan speed
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
		static const char * const xyPopupText[] = {"-50", "-5", "-0.5", "+0.5", "+5", "+50" };
		static const char * const xyPopupParams[] = {"-50", "-5", "-0.5", "0.5", "5", "50" };
		setXYPopup = CreateStringPopupBar(6, xyPopupText, xyPopupParams, evAdjustPosition);

		// Create the popup window used to adjust Z position
		static const char * const zPopupText[] = {"-5", "-0.5", "-0.05", "+0.05", "+0.5", "+5" };
		static const char * const zPopupParams[] = {"-5", "-0.5", "-0.05", "0.05", "0.5", "5" };
		setZPopup = CreateStringPopupBar(6, zPopupText, zPopupParams, evAdjustPosition);
	
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
		tp = new StaticTextField(10 + 7 * rowHeight, 10, filePopupWidth/3 - 20, Centre, "Print");
		tp->SetEvent(evPrint, 0);
		filePopup->AddField(tp);
		tp = new StaticTextField(10 + 7 * rowHeight, filePopupWidth/3 + 10, filePopupWidth/3 - 20, Centre, "Cancel");
		tp->SetEvent(evCancelPrint, 0);
		filePopup->AddField(tp);
		tp = new StaticTextField(10 + 7 * rowHeight, (2 * filePopupWidth)/3 + 10, filePopupWidth/3 - 20, Centre, "Delete");
		tp->SetEvent(evDeleteFile, 0);
		filePopup->AddField(tp);

		// Create the baud rate adjustment popup
		static const char* const baudPopupText[] = { "9600", "19200", "38400", "57600", "115200" };
		static const int baudPopupParams[] = { 9600, 19200, 38400, 57600, 115200 };
		baudPopup = CreateIntPopupBar(5, baudPopupText, baudPopupParams, evAdjustBaudRate);
	
		// Create the volume adjustment popup
		static const char* const volumePopupText[Buzzer::MaxVolume + 1] = { "Off", "1", "2", "3", "4", "5" };
		static const int volumePopupParams[Buzzer::MaxVolume + 1] = { 0, 1, 2, 3, 4, 5 };
		volumePopup = CreateIntPopupBar(Buzzer::MaxVolume + 1, volumePopupText, volumePopupParams, evAdjustVolume);
		
		// Create the "Are you sure?" popup
		areYouSurePopup = new PopupField(areYouSurePopupHeight, areYouSurePopupWidth, popupBackColour);
		DisplayField::SetDefaultColours(black, popupBackColour);
		areYouSurePopup->AddField(areYouSureTextField = new StaticTextField(10, margin, areYouSurePopupWidth - 2 * margin, Centre, ""));
		areYouSurePopup->AddField(new StaticTextField(10 + rowHeight, margin, areYouSurePopupWidth - 2 * margin, Centre, "Are you sure?"));

		DisplayField::SetDefaultColours(white, selectablePopupBackColour);
		tp = new StaticTextField(10 + 2 * rowHeight, 10, areYouSurePopupWidth/2 - 20, Centre, "Yes");
		tp->SetEvent(evYes, 0);
		areYouSurePopup->AddField(tp);
		tp = new StaticTextField(10 + 2 * rowHeight, areYouSurePopupWidth/2 + 10, areYouSurePopupWidth/2 - 20, Centre, "Cancel");
		tp->SetEvent(evCancel, 0);
		areYouSurePopup->AddField(tp);
		
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
	}
	
	// Update the field that warns about unsaved settings
	void SettingsAreSaved(bool areSaved)
	{
		settingsNotSavedField->SetValue((areSaved) ? "" : "Some settings are not saved!");
		settingsNotSavedField->SetColours(white, (areSaved) ? defaultBackColor : errorBackColour);
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

