/* 
DueFlashStorage saves non-volatile data for Arduino Due.
The library is made to be similar to EEPROM library
Uses flash block 1 per default.

Note: uploading new software will erase all flash so data written to flash
using this library will not survive a new software upload. 

Inspiration from Pansenti at https://github.com/Pansenti/DueFlash
Rewritten and modified by Sebastian Nilsson
Further modified up by David Crocker
*/


#ifndef FLASHSTORAGE_H
#define FLASHSTORAGE_H

#include "asf.h"
//#include "flash_efc.h"
//#include "efc.h"

// 1Kb of data
#define DATA_LENGTH   (512)		// 512 bytes of storage

extern int __flash_start__,	__flash_end__;

// Choose a start address close to the top of the Flash memory space
#define  FLASH_START  ((uint8_t *)(__flash_end__ - DATA_LENGTH))

//  FLASH_DEBUG can be enabled to get debugging information displayed.
//#define FLASH_DEBUG

#ifdef FLASH_DEBUG
#define _FLASH_DEBUG(x) Serial.print(x);
#else
#define _FLASH_DEBUG(x)
#endif

//  DueFlash is the main namespace for flash functions
namespace FlashStorage
{
	void init();
  
	// write() writes the specified amount of data into flash.
	// flashStart is the address in memory where the write should start
	// data is a pointer to the data to be written
	// dataLength is length of data in bytes
  
	uint8_t read(uint32_t address);
	void read(uint32_t address, void *data, uint32_t dataLength);
	bool write(uint32_t address, uint8_t value);
	bool write(uint32_t address, const void *data, uint32_t dataLength);
};

#endif
