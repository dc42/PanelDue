/*
 * Fields.hpp
 *
 * Created: 04/02/2015 11:43:53
 *  Author: David
 */ 


#ifndef FIELDS_H_
#define FIELDS_H_

#include "Display.hpp"

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

#if DISPLAY_X == 480

const unsigned int maxHeaters = 5;

const PixelNumber margin = 2;
const PixelNumber textButtonMargin = 1;
const PixelNumber iconButtonMargin = 1;
const PixelNumber outlinePixels = 2;
const PixelNumber fieldSpacing = 6;
const PixelNumber statusFieldWidth = 200;
const PixelNumber bedColumn = 114;

const PixelNumber xyFieldWidth = 80;
const PixelNumber zFieldWidth = 90;

const PixelNumber rowTextHeight = 21;	// height of the font we use
const PixelNumber rowHeight = 28;
const PixelNumber moveButtonRowSpacing = 12;
const PixelNumber fileButtonRowSpacing = 8;

const PixelNumber speedTextWidth = 70;
const PixelNumber efactorTextWidth = 30;
const PixelNumber percentageWidth = 60;
const PixelNumber e1FactorXpos = 140, e2FactorXpos = 250;

const PixelNumber messageTimeWidth = 60;

const PixelNumber popupY = 192;
const PixelNumber popupSideMargin = 10;
const PixelNumber popupTopMargin = 10;
const PixelNumber popupFieldSpacing = 10;

const PixelNumber axisLabelWidth = 26;

const PixelNumber touchCalibMargin = 15;

extern uint8_t glcd19x21[];				// declare which fonts we will be using
#define DEFAULT_FONT	glcd19x21

#elif DISPLAY_X == 800

const unsigned int maxHeaters = 7;

const PixelNumber margin = 4;
const PixelNumber textButtonMargin = 1;
const PixelNumber iconButtonMargin = 2;
const PixelNumber outlinePixels = 3;
const PixelNumber fieldSpacing = 12;
const PixelNumber statusFieldWidth = 350;
const PixelNumber bedColumn = 160;

const PixelNumber xyFieldWidth = 120;
const PixelNumber zFieldWidth = 140;

const PixelNumber rowTextHeight = 32;	// height of the font we use
const PixelNumber rowHeight = 48;
const PixelNumber moveButtonRowSpacing = 20;
const PixelNumber fileButtonRowSpacing = 12;

const PixelNumber speedTextWidth = 105;
const PixelNumber efactorTextWidth = 45;
const PixelNumber percentageWidth = 90;
const PixelNumber e1FactorXpos = 220, e2FactorXpos = 375;

const PixelNumber messageTimeWidth = 90;

const PixelNumber popupY = 345;
const PixelNumber popupSideMargin = 20;
const PixelNumber popupTopMargin = 20;
const PixelNumber popupFieldSpacing = 20;

const PixelNumber axisLabelWidth = 40;

const PixelNumber touchCalibMargin = 22;

extern uint8_t glcd28x32[];				// declare which fonts we will be using
extern uint8_t glcd28x32[];				// declare which fonts we will be using
#define DEFAULT_FONT	glcd28x32

#else

#error Unsupported DISPLAY_X value

#endif

const PixelNumber buttonHeight = rowTextHeight + 4;
const PixelNumber tempButtonWidth = (DISPLAY_X + fieldSpacing - bedColumn)/maxHeaters - fieldSpacing;

const PixelNumber row1 = 0;										// we don't need a top margin
const PixelNumber row2 = row1 + rowHeight - 2;					// the top row never has buttons so it can be shorter
const PixelNumber row3 = row2 + rowHeight;
const PixelNumber row4 = row3 + rowHeight;
const PixelNumber row5 = row4 + rowHeight;
const PixelNumber row6 = row5 + rowHeight;
const PixelNumber row6p3 = row6 + (rowHeight/3);
const PixelNumber row7 = row6 + rowHeight;
const PixelNumber row7p7 = row7 + ((2 * rowHeight)/3);
const PixelNumber row8 = row7 + rowHeight;
const PixelNumber row8p7 = row8 + ((2 * rowHeight)/3);
const PixelNumber row9 = row8 + rowHeight;
const PixelNumber rowTabs = DisplayY - rowTextHeight;			// place at bottom of screen with no margin
const PixelNumber labelRowAdjust = 2;							// how much to drop non-button fields to line up with buttons

const PixelNumber columnX = margin;
const PixelNumber columnY = columnX + xyFieldWidth + fieldSpacing;
const PixelNumber columnZ = columnY + xyFieldWidth + fieldSpacing;
const PixelNumber columnProbe = columnZ + zFieldWidth + fieldSpacing;
const PixelNumber probeFieldWidth = DisplayX - columnProbe - margin;

const PixelNumber speedColumn = margin;
const PixelNumber fanColumn = DISPLAY_X/4 + 20;
const PixelNumber pauseColumn = DISPLAY_X/2 + 10 + fieldSpacing;
const PixelNumber resumeColumn = pauseColumn;
const PixelNumber cancelColumn = pauseColumn + (DISPLAY_X - pauseColumn - fieldSpacing - margin)/2 + fieldSpacing;

const PixelNumber fullPopupWidth = DisplayX - (2 * margin);
const PixelNumber fullPopupHeight = DisplayY - (2 * margin);
const PixelNumber fullWidthPopupX = (DisplayX - fullPopupWidth)/2;
const PixelNumber popupBarHeight = buttonHeight + (2 * popupTopMargin);

const PixelNumber tempPopupBarWidth = (3 * fullPopupWidth)/4;
const PixelNumber tempPopupX = (DisplayX - tempPopupBarWidth)/2;
const PixelNumber filePopupWidth = fullPopupWidth - (4 * margin),
				  filePopupHeight = (8 * rowHeight) + (2 * popupTopMargin);
const PixelNumber areYouSurePopupWidth = DisplayX - 80,
				  areYouSurePopupHeight = (3 * rowHeight) + (2 * popupTopMargin);

const PixelNumber movePopupWidth = fullPopupWidth;
const PixelNumber movePopupHeight = (5 * buttonHeight) + (4 * moveButtonRowSpacing) + (2 * popupTopMargin);
const PixelNumber movePopupX = (DisplayX - movePopupWidth)/2;
const PixelNumber movePopupY = (DisplayY - movePopupHeight)/2;

const PixelNumber keyboardButtonWidth = DisplayX/5;
const PixelNumber keyboardPopupWidth = fullPopupWidth;
const PixelNumber keyButtonWidth = (keyboardPopupWidth - 2 * popupSideMargin)/16;
const PixelNumber keyButtonHStep = (keyboardPopupWidth - 2 * popupSideMargin - keyButtonWidth)/11;
const PixelNumber keyButtonVStep = buttonHeight + fileButtonRowSpacing;
const PixelNumber keyboardPopupHeight = (6 * buttonHeight) + (4 * fileButtonRowSpacing) + (2 * popupTopMargin);
const PixelNumber keyboardPopupX = fullWidthPopupX, keyboardPopupY = margin;

const unsigned int numFileColumns = 2;
const unsigned int numFileRows = (fullPopupHeight - (2 * popupTopMargin) + fileButtonRowSpacing)/(buttonHeight + fileButtonRowSpacing) - 1;
const unsigned int numDisplayedFiles = numFileColumns * numFileRows;
const PixelNumber fileListPopupWidth = fullPopupWidth;
const PixelNumber fileListPopupHeight = ((numFileRows + 1) * buttonHeight) + (numFileRows * fileButtonRowSpacing) + (2 * popupTopMargin);
const PixelNumber fileListPopupX = (DisplayX - fileListPopupWidth)/2;
const PixelNumber fileListPopupY = (DisplayY - fileListPopupHeight)/2;

const uint32_t numMessageRows = (rowTabs - margin - rowHeight)/rowTextHeight;
const PixelNumber messageTextX = margin + messageTimeWidth + 2;
const PixelNumber messageTextWidth = DisplayX - margin - messageTextX;

// Colours
const Colour titleBarTextColour = white;
const Colour titleBarBackColour = red;
const Colour labelTextColour = black;
const Colour infoTextColour = black;
const Colour infoBackColour = UTFT::fromRGB(224, 224, 255);
const Colour defaultBackColour = white; //UTFT::fromRGB(220, 220, 220);
const Colour activeBackColour = UTFT::fromRGB(255, 128, 128);			// light red
const Colour standbyBackColour = UTFT::fromRGB(255, 255, 128);			// light yellow
const Colour errorTextColour = white;
const Colour errorBackColour = magenta;

const Colour popupBackColour = UTFT::fromRGB(224, 224, 255);			// light blue
const Colour popupTextColour = black;
const Colour popupButtonTextColour = black;
const Colour popupButtonBackColour = white;
const Colour popupInfoTextColour = black;
const Colour popupInfoBackColour = white;

const Colour buttonTextColour = black;
const Colour buttonPressedTextColour = black;
const Colour buttonBackColour = white;
const Colour buttonGradColour = UTFT::fromRGB(8, 4, 8);
const Colour buttonPressedBackColour = UTFT::fromRGB(192, 255, 192);
const Colour buttonPressedGradColour = UTFT::fromRGB(8, 8, 8);
const Colour buttonBorderColour = black;
const Colour homedButtonBackColour = UTFT::fromRGB(224, 224, 255);		// light blue
const Colour notHomedButtonBackColour = UTFT::fromRGB(255, 224, 192);	// light orange
const Colour pauseButtonBackColour = UTFT::fromRGB(255, 224, 192);		// light orange
const Colour resumeButtonBackColour = UTFT::fromRGB(255, 255, 128);		// light yellow
const Colour resetButtonBackColour = UTFT::fromRGB(255, 192, 192);		// light red

const Colour progressBarColour = UTFT::fromRGB(0, 160, 0);
const Colour progressBarBackColour = white;

const Colour touchSpotColour = black;

namespace Fields
{
	extern void CreateFields(uint32_t language);
	extern void SettingsAreSaved(bool areSaved);
	extern void ShowPauseButton();
	extern void ShowFilesButton();
	extern void ShowResumeAndCancelButtons();
}

const size_t machineNameLength = 30;
const size_t printingFileLength = 40;
const size_t zprobeBufLength = 12;
const size_t generatedByTextLength = 50;
const size_t maxUserCommandLength = 40;			// max length of a user gcode command
const size_t numUserCommandBuffers = 6;			// number of command history buffers plus one

const unsigned int numLanguages = 3;
extern const char* const longLanguageNames[];

extern String<machineNameLength> machineName;
extern String<printingFileLength> printingFile;
extern String<zprobeBufLength> zprobeBuf;
extern String<generatedByTextLength> generatedByText;
extern String<maxUserCommandLength> userCommandBuffers[numUserCommandBuffers];
extern size_t currentUserCommandBuffer;

extern FloatField *currentTemps[maxHeaters], *fpHeightField, *fpLayerHeightField;
extern FloatField *xPos, *yPos, *zPos;
extern IntegerButton *activeTemps[maxHeaters], *standbyTemps[maxHeaters];
extern IntegerButton *spd, *fanSpeed, *baudRateButton, *volumeButton;
extern IntegerButton *extrusionFactors[maxHeaters];
extern IntegerField *freeMem, *touchX, *touchY, *fpSizeField, *fpFilamentField, *fanRpm;
extern ProgressBar *printProgressBar;
extern SingleButton *tabControl, *tabPrint, *tabFiles, *tabMsg, *tabSetup;
extern SingleButton *moveButton, *extrudeButton, *fanButton, *macroButton;
extern TextButton *filenameButtons[numDisplayedFiles], *languageButton;
extern SingleButton *scrollFilesLeftButton, *scrollFilesRightButton;
extern SingleButton *homeButtons[3], *homeAllButton;
extern TextButton *bedCompButton;
extern StaticTextField *nameField, *statusField, *filePopupTitleField;
extern SingleButton *heaterStates[maxHeaters];
extern StaticTextField *touchCalibInstruction;
extern StaticTextField *messageTextFields[numMessageRows], *messageTimeFields[numMessageRows];
extern StaticTextField *fwVersionField, *areYouSureTextField, *areYouSureQueryField;
extern TextField *timeLeftField;
extern DisplayField *baseRoot, *commonRoot, *controlRoot, *printRoot, *filesRoot, *messageRoot, *infoRoot;
extern ButtonBase * null currentTab;
extern ButtonPress fieldBeingAdjusted;
extern ButtonPress currentButton;
extern PopupWindow *setTempPopup, *movePopup, *fileListPopup, *filePopup, *baudPopup, *volumePopup, *areYouSurePopup, *keyboardPopup, *languagePopup;
extern TextField *zProbe, *fpNameField, *fpGeneratedByField, *printingField, *userCommandField;

// Event numbers, used to say what we need to do when a field is touched
// *** MUST leave value 0 free to mean "no event"
enum Event : uint8_t
{
	evNull = 0,						// value must match nullEvent declared in Display.hpp

	// Page selection
	evTabControl, evTabPrint, evTabMsg, evTabInfo,

	// Heater control
	evSelectHead, evAdjustActiveTemp, evAdjustStandbyTemp,
	
	// Control functions
	evMove, evExtrude, evFan, evListMacros,
	evMoveX, evMoveY, evMoveZ,
	
	// Print functions
	evExtrusionFactor,
	evAdjustFan,
	evAdjustInt,
	evSetInt,
	evListFiles,

	evFile, evMacro,
	evPrint, evCancelPrint,
	evSendCommand,
	evFactoryReset,
	evAdjustSpeed,
	
	evScrollFiles,
	
	evKeyboard,

	// Setup functions
	evCalTouch, evSetBaudRate, evInvertDisplay, evAdjustBaudRate, evSetVolume, evSaveSettings, evAdjustVolume, evReset,

	evYes,
	evCancel,
	evDeleteFile,
	evPausePrint,
	evResumePrint,
	
	evKey, evBackspace, evSendKeyboardCommand, evUp, evDown,
	
	evAdjustLanguage, evSetLanguage,
	
	evRestart
};

#endif /* FIELDS_H_ */