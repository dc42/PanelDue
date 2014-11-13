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
#include <asf.h>
#include "mem.hpp"
#include "Display.hpp"
#include "UTFT.hpp"
#include "UTouch.hpp"
#include "SerialIo.hpp"

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

volatile uint32_t tickCount;

const uint32_t pollInterval = 2000;			// poll interval in milliseconds
const uint32_t beepLength = 10;				// beep length in ms

pwm_channel_t pwm_channel_instance;

static char machineName[15] = "dc42's Ormerod";

const Color activeBackColor = red;
const Color standbyBackColor = yellow;
const Color defaultBackColor = blue;
const Color selectableBackColor = black;

const Event evChangeTab = 1,
			evSetTemp = 2,
			evUp = 3,
			evDown = 4,
			evSet = 5;

uint16_t tux[0x400] =
{
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xE73C, 0x9CD3, 0x9CF3, 0xA514,   // 0x0010 (16)
	0x9CF3, 0x8C51, 0xAD75, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x0020 (32)
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xEF7D, 0x5AEB, 0x7BEF, 0x9CD3, 0x94B2,   // 0x0030 (48)
	0x94B2, 0x94B2, 0x4228, 0x7BEF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x0040 (64)
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFDF, 0xFFFF, 0x9CF3, 0x18E3, 0x630C, 0x4A49, 0x4A69,   // 0x0050 (80)
	0x4A69, 0x528A, 0x4A49, 0x0000, 0xC638, 0xFFFF, 0xFFDF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x0060 (96)
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFDF, 0xFFFF, 0x6B6D, 0x0000, 0x0020, 0x10A2, 0x1082,   // 0x0070 (112)
	0x0841, 0x0841, 0x0841, 0x0000, 0x630C, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x0080 (128)
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFDF, 0xFFFF, 0x528A, 0x4228, 0x8410, 0x0000, 0x0861,   // 0x0090 (144)
	0xAD55, 0xBDD7, 0x10A2, 0x0000, 0x2945, 0xF7BE, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x00A0 (160)
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFDF, 0xFFFF, 0x5ACB, 0x8C71, 0xE75D, 0x2126, 0x528B,   // 0x00B0 (176)
	0xE75D, 0xDEDB, 0x7BCF, 0x0000, 0x18E3, 0xE73C, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x00C0 (192)
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFDF, 0xFFFF, 0x6B6D, 0x4A4A, 0x6B2A, 0x8BE7, 0xA48A,   // 0x00D0 (208)
	0x6B09, 0x4A8A, 0x8431, 0x0000, 0x2104, 0xE73C, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x00E0 (224)
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFDF, 0xFFFF, 0x6B6E, 0x5204, 0xDE6A, 0xFFF7, 0xFFF8,   // 0x00F0 (240)
	0xD5AC, 0xBCAA, 0x5A66, 0x0000, 0x1082, 0xDEFB, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x0100 (256)
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFDF, 0xFFFF, 0x8C10, 0xC540, 0xFFED, 0xFF2C, 0xFEEC,   // 0x0110 (272)
	0xFECC, 0xFE66, 0x8260, 0x0000, 0x0000, 0xB596, 0xFFFF, 0xFFDF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x0120 (288)
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFDF, 0xFFFF, 0x94B3, 0x9C25, 0xFF20, 0xFE40, 0xFDA0,   // 0x0130 (304)
	0xFCC0, 0xF524, 0x836A, 0x0000, 0x0000, 0x630C, 0xFFFF, 0xFFDF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x0140 (320)
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFDF, 0xFFFF, 0x630C, 0x94B4, 0xFF13, 0xFD83, 0xF523,   // 0x0150 (336)
	0xE5CF, 0xF79E, 0xE71D, 0x0861, 0x0000, 0x0861, 0xDEDB, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x0160 (352)
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFDF, 0xFFFF, 0xCE59, 0x0841, 0xD69A, 0xFFFF, 0xFF7D, 0xF77D,   // 0x0170 (368)
	0xFFFF, 0xFFFF, 0xFFFF, 0x73AE, 0x0000, 0x0000, 0x4A69, 0xFFFF, 0xFFDF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x0180 (384)
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xF79E, 0x10A2, 0x8410, 0xFFFF, 0xFFDF, 0xFFFF, 0xFFFF,   // 0x0190 (400)
	0xFFFF, 0xFFDF, 0xFFFF, 0xCE59, 0x0000, 0x0000, 0x0000, 0x9492, 0xFFFF, 0xFFDF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x01A0 (416)
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x52AA, 0x0020, 0xF7BE, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x01B0 (432)
	0xFFDF, 0xFFDF, 0xF7BE, 0xFFDF, 0x3186, 0x0000, 0x0020, 0x0841, 0xCE79, 0xFFFF, 0xFFDF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x01C0 (448)
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFDF, 0xFFFF, 0xC638, 0x0000, 0x52AA, 0xFFFF, 0xFFDF, 0xFFFF, 0xFFFF, 0xFFDF,   // 0x01D0 (464)
	0xFFDF, 0xF7BE, 0xF79E, 0xFFFF, 0x9CF3, 0x0000, 0x0841, 0x0000, 0x39E7, 0xFFDF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x01E0 (480)
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x5ACB, 0x0000, 0xBDF7, 0xFFFF, 0xFFDF, 0xFFFF, 0xFFDF, 0xFFDF,   // 0x01F0 (496)
	0xF7BE, 0xF7BE, 0xF79E, 0xF79E, 0xEF7D, 0x3186, 0x0000, 0x0861, 0x0000, 0xAD55, 0xFFFF, 0xFFDF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x0200 (512)
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xE73C, 0x0861, 0x4A49, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFDF, 0xFFDF, 0xFFDF,   // 0x0210 (528)
	0xF7BE, 0xF79E, 0xEF7D, 0xEF5D, 0xFFDF, 0x8410, 0x0000, 0x1082, 0x0000, 0x39E7, 0xFFDF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x0220 (544)
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFDF, 0xFFFF, 0x94B2, 0x0000, 0xB596, 0xFFFF, 0xFFDF, 0xFFDF, 0xFFDF, 0xFFDF, 0xF7BE,   // 0x0230 (560)
	0xF79E, 0xEF7D, 0xEF7D, 0xE73C, 0xF79E, 0xAD55, 0x0861, 0x10A2, 0x0861, 0x0841, 0xCE59, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x0240 (576)
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xF79E, 0x3185, 0x10A2, 0xE71C, 0xFFFF, 0xFFDF, 0xFFDF, 0xFFDF, 0xF7BE, 0xF79E,   // 0x0250 (592)
	0xEF7D, 0xEF7D, 0xEF5D, 0xE73C, 0xEF5D, 0xBDF7, 0x18C3, 0x18C3, 0x18C3, 0x0000, 0x8C71, 0xFFFF, 0xFFDF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x0260 (608)
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFDF, 0xFFFF, 0x94B2, 0x0000, 0x39E7, 0xF7BE, 0xFFFF, 0xFFDF, 0xFFDF, 0xF7BE, 0xF79E, 0xEF7D,   // 0x0270 (624)
	0xEF7D, 0xEF5D, 0xE73C, 0xE71C, 0xE71C, 0xC618, 0x18E3, 0x10A2, 0x10A2, 0x0020, 0x6B4D, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x0280 (640)
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFDF, 0xFFFF, 0x8C51, 0x38E0, 0x4A27, 0xFFFF, 0xFFDF, 0xF7BE, 0xF7BE, 0xF79E, 0xEF7D, 0xEF7D,   // 0x0290 (656)
	0xEF5D, 0xE73C, 0xE71C, 0xDEFB, 0xDF1D, 0xBDF8, 0x39C7, 0x5ACB, 0x528A, 0x10A3, 0x738F, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x02A0 (672)
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xDD6C, 0xFE2B, 0xBC45, 0xA513, 0xFFFF, 0xF7BE, 0xF79E, 0xF79E, 0xEF7D, 0xEF5D,   // 0x02B0 (688)
	0xE73C, 0xE71C, 0xDEFB, 0xD6DC, 0xDD8E, 0xB3E4, 0x2124, 0x2965, 0x2945, 0x20C1, 0xB511, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x02C0 (704)
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xF77C, 0xE5CF, 0xF60B, 0xFF9B, 0xFF54, 0x8B02, 0x7BF0, 0xFFDF, 0xF79E, 0xEF5D, 0xEF5D, 0xE73C,   // 0x02D0 (720)
	0xE71C, 0xDEFB, 0xDEDB, 0xCE7A, 0xED89, 0xDDAD, 0x0842, 0x0000, 0x0000, 0xAC69, 0xDD6B, 0xEFBF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x02E0 (736)
	0xFFFF, 0xFFFF, 0xFFBE, 0xE5CB, 0xEDC9, 0xFE4B, 0xFF14, 0xFEF3, 0xFF35, 0xFE8D, 0x51C1, 0x634E, 0xE73C, 0xEF5D, 0xE73C, 0xE71C,   // 0x02F0 (752)
	0xDEFB, 0xDEDB, 0xD6DB, 0xCE59, 0xE58B, 0xFF98, 0xBD4F, 0x8B88, 0xCD90, 0xFFB7, 0xCCE8, 0xE73D, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x0300 (768)
	0xFFFF, 0xFFFF, 0xEF3B, 0xF583, 0xFF30, 0xFF11, 0xFECF, 0xFEEF, 0xFECF, 0xFF30, 0xDD46, 0x2903, 0x6B8E, 0xEF7D, 0xE71C, 0xDEFB,   // 0x0310 (784)
	0xDEDB, 0xD6BA, 0xD69A, 0xCE59, 0xE5AA, 0xFF11, 0xFF53, 0xFF73, 0xFF33, 0xFF12, 0xFE6C, 0xDDAD, 0xF79E, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x0320 (800)
	0xFFFF, 0xFFFF, 0xF79E, 0xEDC5, 0xFECB, 0xFECC, 0xFECC, 0xFEEC, 0xFECB, 0xFECC, 0xFEEA, 0x9BE5, 0x8432, 0xE73C, 0xDEDB, 0xDEDB,   // 0x0330 (816)
	0xD6BA, 0xD69A, 0xDEDB, 0xA4F3, 0xD547, 0xFF2E, 0xFECD, 0xFECE, 0xFEEE, 0xFEEE, 0xFF10, 0xFEAB, 0xE5A8, 0xEF7D, 0xFFFF, 0xFFFF,   // 0x0340 (832)
	0xFFFF, 0xFFFF, 0xF79E, 0xF603, 0xFEA2, 0xFEC7, 0xFEC7, 0xFEA4, 0xFE81, 0xFE61, 0xFEA4, 0xFE43, 0xDE33, 0xE75E, 0xE71C, 0xDEFB,   // 0x0350 (848)
	0xDEDB, 0xCE58, 0x8C72, 0x5247, 0xEDE4, 0xFF0A, 0xFECA, 0xFEC9, 0xFE84, 0xFE83, 0xFEE7, 0xFEA3, 0xB443, 0xD69B, 0xFFFF, 0xFFFF,   // 0x0360 (864)
	0xFFFF, 0xFFFF, 0xF75B, 0xFE60, 0xFF00, 0xFEC0, 0xFEC0, 0xFEA0, 0xFEA0, 0xFEC0, 0xFEA0, 0xFEE0, 0xE5C1, 0x9492, 0xA514, 0x9CD3,   // 0x0370 (880)
	0x8410, 0x630B, 0x4229, 0x6AE8, 0xFE80, 0xFEC1, 0xFEC1, 0xFEA0, 0xFEA0, 0xFEE0, 0xDD80, 0x9BE8, 0xB597, 0xFFDF, 0xFFFF, 0xFFFF,   // 0x0380 (896)
	0xFFFF, 0xFFFF, 0xF79E, 0xD589, 0xE600, 0xFEA0, 0xFF00, 0xFF40, 0xFF40, 0xFF00, 0xFF00, 0xFF20, 0xFEC0, 0x5267, 0x4229, 0x4A48,   // 0x0390 (912)
	0x4A49, 0x5289, 0x424A, 0x7B46, 0xFF20, 0xFEE0, 0xFEE0, 0xFF20, 0xFEE0, 0xB4A5, 0x9C92, 0xDEFD, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x03A0 (928)
	0xFFFF, 0xFFFF, 0xFFFF, 0xE71D, 0xBDB6, 0xB530, 0xBD0B, 0xCD65, 0xEE60, 0xFF40, 0xFFA0, 0xFF80, 0xBD03, 0x8410, 0xA514, 0xA534,   // 0x03B0 (944)
	0xAD75, 0xB596, 0xA555, 0x9C8F, 0xF6C0, 0xFFA0, 0xFFA0, 0xF6E0, 0xA449, 0xB5B8, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x03C0 (960)
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xEF7F, 0xD69C, 0xBD95, 0xBD4C, 0xCDC6, 0xB4E8, 0xAD35, 0xF7BF, 0xFFFF, 0xFFFF,   // 0x03D0 (976)
	0xFFFF, 0xFFFF, 0xFFFF, 0xF7BF, 0xCDD0, 0xCDC6, 0xCDA7, 0xA48D, 0xCE7B, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x03E0 (992)
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xDF1F, 0xB59A, 0xBDDA, 0xFFFF, 0xFFFF, 0xFFDF, 0xFFFF,   // 0x03F0 (1008)
	0xFFFF, 0xFFDF, 0xFFDF, 0xFFFF, 0xEF7F, 0xB59A, 0xAD59, 0xDF1D, 0xFFFF, 0xFFDF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,   // 0x0400 (1024)
};


static FloatField *bedCurrentTemp, *t1CurrentTemp, *t2CurrentTemp, *xPos, *yPos, *zPos, *spd, /* *extrPos,*/ *e1Percent, *e2Percent;
static IntegerSettingField *bedActiveTemp, *t1ActiveTemp, *t2ActiveTemp, *t1StandbyTemp, *t2StandbyTemp;
static IntegerField *bedStandbyTemp, *zProbe, /* *fanRPM,*/ *freeMem, *touchX, *touchY;
static ProgressBar *pbar;
static StaticTextField *head1State, *head2State, *bedState, *tabControl, *tabPrint, *tabFiles, *tabMsg, *tabInfo;
static DisplayField *currentTab, *fieldBeingAdjusted = NULL;
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

const PixelNumber row1 = 0;
const PixelNumber row2 = 22;
const PixelNumber row3 = 44;
const PixelNumber row4 = 66;
const PixelNumber row5 = 88;
const PixelNumber rowTabs = 120;

const PixelNumber columnTab1 = 2;
const PixelNumber columnTab2 = 82;
const PixelNumber columnTab3 = 162;
const PixelNumber columnTab4 = 242;
const PixelNumber columnTab5 = 322;
const PixelNumber columnTabWidth = 75;

const PixelNumber printingRow = 142;
const PixelNumber progressRow = 164;
const PixelNumber commandRow = 185;
const PixelNumber freeMemRow = 210;

void setup()
{
	// Setup the LCD
	lcd.InitLCD(Landscape);
	mgr.Init(blue);
	
	DisplayField::SetDefaultFont(glcd19x20);
	
	// Create display fields
	DisplayField::SetDefaultColours(white, red);
	
	mgr.AddField(new StaticTextField(row1, 0, lcd.getDisplayXSize(), Centre, machineName));
	DisplayField::SetDefaultColours(white, defaultBackColor);
	
	mgr.AddField(new StaticTextField(row2, column2, column3 - column2 - 4, Left, "Current"));
	mgr.AddField(new StaticTextField(row2, column3, column4 - column3 - 4, Left, "Active"));
	mgr.AddField(new StaticTextField(row2, column4, column5 - column4 - 4, Left, "St'by"));

	DisplayField::SetDefaultColours(white, selectableBackColor);
	mgr.AddField(head2State = new StaticTextField(row4, column1, column2 - column1 - 4, Left, "Head 2"));
	mgr.AddField(head1State = new StaticTextField(row3, column1, column2 - column1 - 4, Left, "Head 1"));

	mgr.AddField(t1ActiveTemp = new IntegerSettingField("G10 P1 S", row3, column3, column4 - column3 - 4, NULL, DEGREE_SYMBOL "C"));
	mgr.AddField(t1StandbyTemp = new IntegerSettingField("G10 P1 R", row3, column4, column5 - column4 - 4, NULL, DEGREE_SYMBOL "C"));
	mgr.AddField(t2ActiveTemp = new IntegerSettingField("G10 P2 S", row4, column3, column4 - column3 - 4, NULL, DEGREE_SYMBOL "C"));
	mgr.AddField(t2StandbyTemp = new IntegerSettingField("G10 P2 R", row4, column4, column5 - column4 - 4, NULL, DEGREE_SYMBOL "C"));
	mgr.AddField(bedActiveTemp = new IntegerSettingField("M140 S", row5, column3, column4 - column3 - 4, NULL, DEGREE_SYMBOL "C"));
	t1ActiveTemp->SetEvent(evSetTemp);
	t1StandbyTemp->SetEvent(evSetTemp);
	t2ActiveTemp->SetEvent(evSetTemp);
	t2StandbyTemp->SetEvent(evSetTemp);
	bedActiveTemp->SetEvent(evSetTemp);

	DisplayField::SetDefaultColours(white, defaultBackColor);
	mgr.AddField(t1CurrentTemp = new FloatField(row3, column2, column3 - column2 - 4, NULL, 1, DEGREE_SYMBOL "C"));
	mgr.AddField(t2CurrentTemp = new FloatField(row4, column2, column3 - column2 - 4, NULL, 1, DEGREE_SYMBOL "C"));

	mgr.AddField(bedState = new StaticTextField(row5, column1, column2 - column1 - 4, Left, "Bed"));
	mgr.AddField(bedCurrentTemp = new FloatField(row5, column2, column3 - column2 - 4, NULL, 1, DEGREE_SYMBOL "C"));
	mgr.AddField(bedStandbyTemp = new IntegerField(row5, column4, column5 - column4 - 4, NULL, DEGREE_SYMBOL "C"));

	mgr.AddField(new StaticTextField(row2, columnX, columnY - columnX - 2, Left, "X"));
	mgr.AddField(new StaticTextField(row2, columnY, columnEnd - columnY - 2, Left, "Y"));
	//  mgr.AddField(new StaticTextField(row2, columnZ, 400 - columnZ - 2, Left, "Z"));

	mgr.AddField(xPos = new FloatField(row3, columnX, columnY - columnX - 2, NULL, 1));
	mgr.AddField(yPos = new FloatField(row3, columnY, columnEnd - columnY - 2, NULL, 1));
	//  mgr.AddField(zPos = new FloatField(row3, columnZ, columnEnd - columnZ, NULL, 2));
	
	//  mgr.AddField(new StaticTextField(row4, columnX, columnY - columnX - 2, Left, "Extr"));
	mgr.AddField(new StaticTextField(row4, columnX, columnY - columnX - 2, Left, "Z"));
	mgr.AddField(new StaticTextField(row4, columnY, columnEnd - columnY - 2, Left, "Probe"));
	//  mgr.AddField(new StaticTextField(row4, columnZ, columnEnd - columnZ, Left, "Fan"));

	mgr.AddField(zPos = new FloatField(row5, columnX, columnY - columnX - 2, NULL, 1));
	//  mgr.AddField(extrPos = new FloatField(row5, columnX, columnY - columnX - 2, NULL, 1));
	mgr.AddField(zProbe = new IntegerField(row5, columnY, columnEnd - columnY - 2, NULL, NULL));
	//  mgr.AddField(fanRPM = new IntegerField(row5, columnZ, columnEnd - columnZ, NULL, NULL));
	
	DisplayField::SetDefaultColours(white, selectableBackColor);
	mgr.AddField(tabControl = new StaticTextField(rowTabs, columnTab1, columnTabWidth, Centre, "Control"));
	tabControl->SetEvent(evChangeTab);
	mgr.AddField(tabPrint = new StaticTextField(rowTabs, columnTab2, columnTabWidth, Centre, "Print"));
	tabPrint->SetEvent(evChangeTab);
	mgr.AddField(tabFiles = new StaticTextField(rowTabs, columnTab3, columnTabWidth, Centre, "Files"));
	tabFiles->SetEvent(evChangeTab);
	mgr.AddField(tabMsg = new StaticTextField(rowTabs, columnTab4, columnTabWidth, Centre, "Msg"));
	tabMsg->SetEvent(evChangeTab);
	mgr.AddField(tabInfo = new StaticTextField(rowTabs, columnTab5, columnTabWidth, Centre, "Info"));
	tabInfo->SetEvent(evChangeTab);

	DisplayField::SetDefaultColours(white, defaultBackColor);
	
#if 0
	mgr.AddField(spd = new FloatField(54, 0, 98, "Speed: ", 0, "%"));
	mgr.AddField(e1Percent = new FloatField(54, 100, 68, "E1: ", 0, "%"));
	mgr.AddField(e2Percent = new FloatField(54, 170, 68, "E2: ", 0, "%"));
#endif

	mgr.AddField(new TextField(printingRow, 0, lcd.getDisplayXSize(), "Printing ", "nozzleMount.gcode"));
	
	DisplayField::SetDefaultColours(white, UTFT::fromRGB(0, 160, 0));
	mgr.AddField(pbar = new ProgressBar(progressRow, 1, 8, lcd.getDisplayXSize() - 2));

	DisplayField::SetDefaultColours(white, blue);
	mgr.AddField(freeMem = new IntegerField(freeMemRow, 0, 195, "Free RAM: "));
	mgr.AddField(touchX = new IntegerField(freeMemRow, 200, 130, "Touch: ", ","));
	mgr.AddField(touchY = new IntegerField(freeMemRow, 330, 50, ""));
	
	commandBuffer[0] = '\0';
	mgr.AddField(commandField = new TextField(commandRow, 0, 400, "Command: ", commandBuffer));
	
	DisplayField::SetDefaultColours(white, UTFT::fromRGB(0, 160, 0));
	setTempPopup = new PopupField(110, 80);
	DisplayField *tp = new StaticTextField(10, 10, 60, Centre, "+");
	tp->SetEvent(evUp);
	setTempPopup->AddField(tp);
	tp = new StaticTextField(45, 10, 60, Centre, "Set");
	tp->SetEvent(evSet);
	setTempPopup->AddField(tp);
	tp = new StaticTextField(80, 10, 60, Centre, "-");
	tp->SetEvent(evDown);
	setTempPopup->AddField(tp);

	lcd.drawLine(column5, row2, column5, row5+15);
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

	touch.InitTouch(400, 240, InvLandscape, TpMedium);
	currentTab = tabPrint;
	currentTab->SetColours(red, black);
}

void changeTab(DisplayField *newTab)
{
	if (newTab != currentTab)
	{
		currentTab->SetColours(white, black);
		newTab->SetColours(red, black);
		currentTab = newTab;
	}
}

// Process a touch event
void ProcessTouch(DisplayField *f)
{
	switch(f->GetEvent())
	{
	case evChangeTab:
		changeTab(f);
		break;

	case evSetTemp:
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

	default:
		break;
	}
}

void SysTick_Handler()
{
	++tickCount;
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

void BuzzerInit()
{	
	pwm_channel_disable(PWM, PWM_CHANNEL_0);
	pwm_clock_t clock_setting = 
	{
		.ul_clka = 2000 * 100,
		.ul_clkb = 0,
		.ul_mck = SystemCoreClock
	};
	pwm_init(PWM, &clock_setting);
	pwm_channel_instance.ul_prescaler = PWM_CMR_CPRE_CLKA;
	pwm_channel_instance.ul_period = 100;
	pwm_channel_instance.ul_duty = 50;
	pwm_channel_instance.channel = PWM_CHANNEL_0;
	pwm_channel_init(PWM, &pwm_channel_instance);
	pio_configure(PIOB, PIO_PERIPH_A, PIO_PB0, 0);
	pio_configure(PIOB, PIO_PERIPH_B, PIO_PB5, 0);
}

void BuzzerOn()
{
	pwm_channel_enable(PWM, PWM_CHANNEL_0);	
}

void BuzzerOff()
{
	pwm_channel_disable(PWM, PWM_CHANNEL_0);
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
	pmc_enable_periph_clk(ID_PWM);		// enable the PWM clock
	
	SerialIo::init();
	BuzzerInit();
	
	setup();
//	pio_configure(PIOA, PIO_OUTPUT_0, PIO_PA8, 0);
	
	SysTick_Config(SystemCoreClock / 1000);

	uint32_t lastPollTime = tickCount;
	uint32_t lastBeepStartTime = tickCount;
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
		
		if (touch.dataAvailable())
		{
			touch.read();
			int x = touch.getX();
			int y = touch.getY();
			touchX->SetValue(x);	//debug
			touchY->SetValue(y);	//debug
			DisplayField * null f = mgr.FindEvent(x, y);
			if (f != NULL)
			{
				BuzzerOn();
				lastBeepStartTime = tickCount;
				ProcessTouch(f);
			}
		}
		freeMem->SetValue(getFreeMemory());
		mgr.RefreshAll(false);
		
		if (tickCount - lastPollTime >= pollInterval)
		{
			lastPollTime += pollInterval;
			WriteCommand("M105 S2\n");
		}
		
		if (tickCount - lastBeepStartTime >= beepLength)
		{
			BuzzerOff();
		}
		
		//    lcd.drawBitmap(300, 200, 32, 32, tux, 1);
		//    delay(100);
		//		pio_clear(PIOA, PIO_PA8);
		//		delay_ms(1000);
		//		pio_set(PIOA, PIO_PA8);
			//		delay_ms(500);
	}
}

// End
