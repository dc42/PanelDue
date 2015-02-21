/*
 * Fields.hpp
 *
 * Created: 04/02/2015 11:43:53
 *  Author: David
 */ 


#ifndef FIELDS_H_
#define FIELDS_H_

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
const PixelNumber statusFieldWidth = 150;

const PixelNumber column1 = margin;
const PixelNumber column2 = 72;			// current temp
const PixelNumber column3 = 146;		// active temp
const PixelNumber column4 = 208;		// standby temp
const PixelNumber column5 = 270;

const PixelNumber columnX = 275;
const PixelNumber columnY = 333;

const PixelNumber rowHeight = 22;

#elif DISPLAY_X >= 480

const PixelNumber margin = 2;
const PixelNumber outlinePixels = 2;
const PixelNumber fieldSpacing = 5;
const PixelNumber statusFieldWidth = 200;

const PixelNumber column1 = margin;
const PixelNumber column2 = 83;			// current temp
const PixelNumber column3 = 167;		// active temp
const PixelNumber column4 = 232;		// standby temp
const PixelNumber column5 = 297;

const PixelNumber columnX = 306;
const PixelNumber columnY = 397;

const PixelNumber rowHeight = 24;


#endif

const PixelNumber row1 = margin;
const PixelNumber row2 = row1 + rowHeight;
const PixelNumber row3 = row2 + rowHeight;
const PixelNumber row4 = row3 + rowHeight;
const PixelNumber row5 = row4 + rowHeight;
const PixelNumber row6 = row5 + rowHeight + 10;				// leave a gap between the two panels
const PixelNumber row7 = row6 + rowHeight;
const PixelNumber row8 = row7 + rowHeight;
const PixelNumber row9 = row8 + rowHeight;
const PixelNumber rowTabs = DisplayY - rowHeight - margin;	// place at bottom of screen with a margin

const PixelNumber xyPopupX = 3, xyPopupY = 195;
const PixelNumber tempPopupX = 35, tempPopupY = 195;
const PixelNumber filePopupWidth = DisplayX - 40, filePopupHeight = 8 * rowHeight + 20;
const PixelNumber areYouSurePopupWidth = DisplayX - 80, areYouSurePopupHeight = 3 * rowHeight + 20;

const PixelNumber popupBarWidth = DisplayX - 6;
const PixelNumber popupBarHeight = 40;
const PixelNumber popupBarFieldYoffset = 10;

const uint32_t numFileColumns = 2;
const uint32_t numFileRows = (DisplayY - margin)/rowHeight - 3;
const uint32_t numDisplayedFiles = numFileColumns * numFileRows;

const uint32_t numMessageRows = (DisplayY - margin)/rowHeight - 3;
const PixelNumber messageTimeWidth = 60;
const PixelNumber messageTextX = margin + messageTimeWidth + 2;
const PixelNumber messageTextWidth = DisplayX - margin - messageTextX;

// Declare which fonts we will be using
extern uint8_t glcd19x20[];

const Color activeBackColor = red;
const Color standbyBackColor = yellow;
const Color defaultBackColor = blue;
const Color errorBackColour = magenta;
const Color selectableBackColor = black;
const Color outlineColor = green;
const Color popupBackColour = green;
const Color selectablePopupBackColour = UTFT::fromRGB(0, 128, 0);		// dark green
const Color homedBackColour = UTFT::fromRGB(0, 0, 100);					// dark blue
const Color notHomedBackColour = UTFT::fromRGB(255, 128, 0);			// orange
const Color pauseButtonBackColor = UTFT::fromRGB(255, 128, 0);			// orange
const Color resumeButtonBackColor = UTFT::fromRGB(0, 200, 0);			// green but not too bright
const Color resetButtonBackColor = red;

namespace Fields
{
	extern void CreateFields();
	extern void SettingsAreSaved(bool areSaved);
	extern void ShowPauseButton();
	extern void ShowResumeAndCancelButtons();
	extern void HidePauseResumeCancelButtons();
}

const size_t machineNameLength = 30;
const size_t printingFileLength = 40;
const size_t zprobeBufLength = 12;
const size_t generatedByTextLength = 30;

extern String<machineNameLength> machineName;
extern String<printingFileLength> printingFile;
extern String<zprobeBufLength> zprobeBuf;
extern String<generatedByTextLength> generatedByText;

extern FloatField *bedCurrentTemp, *t1CurrentTemp, *t2CurrentTemp, *xPos, *yPos, *zPos, *fpHeightField, *fpLayerHeightField;
extern IntegerField *bedActiveTemp, *t1ActiveTemp, *t2ActiveTemp, *t1StandbyTemp, *t2StandbyTemp, *spd, *e1Percent, *e2Percent, *fanSpeed, *fanRpm;
extern IntegerField *bedStandbyTemp, *freeMem, *touchX, *touchY, *fpSizeField, *fpFilamentField, *baudRateField;
extern ProgressBar *printProgressBar;
extern StaticTextField *nameField, *statusField, *head1State, *head2State, *bedState, *tabControl, *tabPrint, *tabFiles, *tabMsg, *tabInfo, *touchCalibInstruction;
extern StaticTextField *filenameFields[numDisplayedFiles], *messageTextFields[numMessageRows], *messageTimeFields[numMessageRows], *scrollFilesLeftField, *scrollFilesRightField;
extern StaticTextField *homeFields[3], *homeAllField, *fwVersionField, *areYouSureTextField, *timeLeftField;
extern DisplayField *baseRoot, *commonRoot, *controlRoot, *printRoot, *filesRoot, *messageRoot, *infoRoot;
extern DisplayField * null currentTab;
extern DisplayField * null fieldBeingAdjusted;
extern PopupField *setTempPopup, *setXYPopup, *setZPopup, *filePopup, *baudPopup, *volumePopup, *areYouSurePopup;
//extern TextField *commandField;
extern TextField *zProbe, *fpNameField, *fpGeneratedByField, *printingField;

// Event numbers, used to say what we need to do when a field is touched
const Event
	// leave 0 free for nullEvent declared in Display.hpp
	evTabControl = 1,
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
	evFactoryReset = 18,
	evAdjustPercent = 19,
	evScrollFiles = 20,
	evSetBaudRate = 21,
	evInvertDisplay = 22,
	evAdjustBaudRate = 23,
	evSetVolume = 24,
	evSaveSettings = 25,
	evAdjustVolume = 26,
	evYes = 27,
	evCancel = 28,
	evDeleteFile = 29,
	evPausePrint = 30,
	evResumePrint = 31,
	evReset = 32;

#endif /* FIELDS_H_ */