/********************************************************************************
* \copyright
* Copyright 2009-2017, Card Reader Factory.  All rights were reserved.
* From 2018 this code has been made PUBLIC DOMAIN.
* This means that there are no longer any ownership rights such as copyright, trademark, or patent over this code.
* This code can be modified, distributed, or sold even without any attribution by anyone.
*
* We would however be very grateful to anyone using this code in their product if you could add the line below into your product's documentation:
* Special thanks to Nicholas Alexander Michael Webber, Terry Botten & all the staff working for Operation (Police) Academy. Without these people this code would not have been made public and the existance of this very product would be very much in doubt.
*
*******************************************************************************/

#ifndef _DATAFLASH_H
#define _DATAFLASH_H

#include "aes.h"

#define DF_READY_bm (1<<7)
#define SPI_IF_bm (0x80)

#ifdef INCLUDE_ENCRYPTION
    #define write(__x,__y)  aes_write(__x,__y)
    #define flush()         aes_flush()
#else
    #define write(__x,__y)  df_write(__x,__y)
    #define flush()         df_flush()
#endif /* INCLUDE_ENCRYPTION */

/* handle chip select */
#define df_inactive() { PORTB |=  ( 1 << PinCs ); }
#define df_active() { PORTB &=  ~( 1 << PinCs ); }



/* Hardware specific stuff */
enum
{
    /* DataFlash, all on PortB */
    PinDfReset      = 0,
    PinCs           = 2,
    PinMosi         = 3,
    PinMiso         = 4,
    PinSck          = 5,
};

enum
{
//Dataflash opcodes
    FlashPageRead             =  0xd2,    // Main memory page read
    FlashToBuf1Transfer       =  0x53,    // Main memory page to buffer 1 transfer
    Buf1Read                  =  0xd4,    // Buffer 1 read
    FlashToBuf2Transfer       =  0x55,    // Main memory page to buffer 2 transfer
    Buf2Read                  =  0xd6,    // Buffer 2 read
    StatusReg                 =  0xd7,    // Status register
    AutoPageReWrBuf1          =  0x58,    // Auto page rewrite through buffer 1
    AutoPageReWrBuf2          =  0x59,    // Auto page rewrite through buffer 2
    FlashToBuf1Compare        =  0x60,    // Main memory page to buffer 1 compare
    FlashToBuf2Compare        =  0x61,    // Main memory page to buffer 2 compare
    ContArrayRead             =  0x0b,    // Continuous Array Read (Note: Only D-parts supported)
    FlashProgBuf1             =  0x82,    // Main memory page program through buffer 1
    Buf1ToFlashWE             =  0x83,    // Buffer 1 to main memory page program with built-in erase
    Buf1Write                 =  0x84,    // Buffer 1 write
    FlashProgBuf2             =  0x85,    // Main memory page program through buffer 2
    Buf2ToFlashWE             =  0x86,    // Buffer 2 to main memory page program with built-in erase
    Buf2Write                 =  0x87,    // Buffer 2 write
    Buf1ToFlash               =  0x88,    // Buffer 1 to main memory page program without built-in erase
    Buf2ToFlash               =  0x89,    // Buffer 2 to main memory page program without built-in erase
    PageErase                 =  0x81,    // Page erase
    BlockErase                =  0x50,    // Block erase
    SectorErase               =  0x7c,    // Sector erase
    ReadId                    =  0x9f,    // Device ReadId
    PowerDown                 =  0xb9,    // Device Power Down
    PowerUp                   =  0xab,    // Device Power Up

/* DataFlash ID */
    AtmelId                   =  0x1f,    // id taken from at45db161d.pdf
    DataFlashId               =  1<<5,    // id taken from at45db161d.pdf
    MaxRetries                =  0xfe,    // the number of retries

/* Empty Data signature */
    EmptyByte                 =  0xff,   // Dataflash seems to contain 0xff where it is erased
    EmptySignatureLen         =  0xff    // the number of EmptyBytes until we are sure that the memory is emtpy
};

extern uint8_t     df_index;
extern uint8_t     df_addrShift;
extern uint8_t     df_pageBits;
extern uint16_t    df_pageSize;
extern uint32_t    df_totalMemory;    /* memory size in bytes */
extern uint16_t    df_currentAddr;    /* current address in df ram buffer */
extern uint16_t    df_currentPage;    /* current page */
extern uint32_t    df_currentByte;    /* current byte */
extern uint32_t    df_usedMemory;     /* the memory used */
extern bool        df_dirty;          /* does it needs to be flushed? */
extern bool        df_powerOn;
extern bool        df_binaryPageSize;
extern bool        df_usingSecondBuffer;

/**
 * - Starts up the dataflash.
 * - Reads the memory used
 * - Formats the dataflash if initialisation fails
 *
 * @return bool
 */
bool df_init(void);

/**
 * Writes into the dataflash the data pointed by the buffer.
 *
 * @param buf
 * @param length
 *
 * @return uint16_t
 */
uint8_t df_write(const uint8_t *buf, uint8_t length);

/**
 *  Writes into the reserved memory the pointer to the next free byte,
 *  and flushes the current buffer which is always not written
 *
 *  If you abuse this function (use it more than 10000 times) and you don't refresh
 *  the entire first block (first 8 pages), you will loose the data in the fist block!
 *
 *  Use this function always when you are really finished writing in memory.
 *  Keep in mind that data that dosen't fill 1 page (df_pageSize) isn't written
 *  into memory until you use this function.
 *
 *  Shuts down the DF
 */
void df_flush(void);

/**
 * Sets the read/write cursor at the specified position
 *
 * @param index
 */
void df_seek(uint32_t index);

/**
 * erases all memory
 *
 * @param all   true to erase all memory
 */
void df_erase(bool all);

/**
 * Initiates a continuous read from a location in the DataFlash
 *
 * @param address       Address of byte where cont.read starts from
 */
void df_enableContinuousRead(uint32_t addr);

/**
 * Read and writes one byte from/to SPI master
 *
 * @param output    Byte to be written to SPI data register (any value)
 *
 * @return uint8_t  Byte read from SPI data register (any value)
 */
uint8_t rwSpi(uint8_t output);

/**
 * puts the device into deep sleep
 *
 */
void df_powerDown(void);

/**
 * wakes the device from deep sleep
 *
 */
void df_powerUp(void);

/**
 * Finds the amount of used memory...
 *
 * @return uint32_t
 */
uint32_t df_findUsedMemory(void);


/**
 * waits for the dataflash to become ready to execute commands.
 *
 * note even if the dataflash is busy:
 *  - you can still execute command that read or modify the dataflash's sram buffer
 *  - you cannot read, write, erase or powerdown the dataflash.
 */
void df_waitReady(void);

/**
 * returns true if the dataflash is ready to execute commands
 *
 * note even if the dataflash is busy:
 *  - you can still execute command that read or modify the dataflash's sram buffer
 *  - you cannot read, write, erase or powerdown the dataflash.
 */
bool df_isReady(void);


#endif
