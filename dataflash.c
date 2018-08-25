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
#include <io.h>
#include <string.h>
#include <stdio.h>
#include <delay.h>
#include "global.h"
#include "dataflash.h"
#include "aes.h"
#include "smth.h"
#include "usart.h"

//#define DEBUG_MEMORY

#ifdef DEBUG_MEMORY
    #define vLog(a) printf a
#else
    #define vLog(a)
#endif

#define UNKNOWN 0xffffffff

// as CVAVR clears global variables at program startup, there's no need to set the below variables to 0 and false
// by not setting them we keep the size of the firmware smaller

uint8_t     df_index;               /* the dataflash we're using; points to one of the indexes from the vectors below */
uint8_t     df_addrShift;           /* the current address shift - used to calculate the binary page size address */
uint8_t     df_pageBits;            /* the number of bits in the page */
uint16_t    df_pageSize;            /* the size of the page */
uint32_t    df_totalMemory;         /* memory size in bytes */
uint16_t    df_currentPage;         /* current page */

/* note: always keep in sync df_currentAddr and df_currentByte. One should not be modified without the other */
uint16_t    df_currentAddr;         /* current address in df ram buffer. */
uint32_t    df_currentByte;         /* current byte in memory */

uint32_t    df_usedMemory;          /* the memory used */
bool        df_dirty;               /* does it needs to be flushed? */
bool        df_powerOn;             /* true if the dataflash is turned on */
bool        df_binaryPageSize;      /* true if this dataflash is using a binary page*/
bool        df_usingSecondBuffer;   /* indicates if we're using the first or the second dataflash SRAM buffer - for double buffering */


// Constants
//Look-up table for these sizes ->      512k,  1M,   2M,   4M,   8M,  16M,  32M,  64M
flash unsigned char dfPageBits[]     = {   9,   9,    9,    9,    9,   10,   10,   11};    /* index of internal page address bits */
flash unsigned char dfAddrShift[]    = {   0,   0,    0,    0,    0,    1,    1,    2};    /* Address shift for binary page size. PageSize = 256 << dfAddrShift[i]  */
flash unsigned int  dfPageSize[]     = { 264, 264,  264,  264,  264,  528,  528, 1056};    /* index of pagesizes */
flash unsigned int  dfPages[]        = { 256, 512, 1024, 2048, 4096, 4096, 8192, 8192};
flash unsigned char dfSectors[]      = {   4,   4,    4,    8,   16 ,  16,   64,   32};  /* the number of sectors to erase */
flash unsigned char dfSector0Erase[] = {   4,   4,    4,    4,    4,    5,    5,    6};  /* first sector shift - which, courtesy of cretins from atmel is different than the rest - see table below */
flash unsigned char dfSectorsErase[] = {   8,   8,    8,    9,    9,   10,    9,   11};  /* all other sector shift */

/* sector erase:
0a,0b
---------------
011 = xxxxxPPP PPPPxxxx xxxxxxxx
021 = xxxxxPPP PPPPxxxx xxxxxxxx
041 = xxxxPPPP PPPPxxxx xxxxxxxx
081 = xxxPPPPP PPPPxxxx xxxxxxxx
161 = xxPPPPPP PPPxxxxx xxxxxxxx
321 = xPPPPPPP PPPxxxxx xxxxxxxx
642 = PPPPPPPP PPxxxxxx xxxxxxxx

rest of sectors
---------------
011 = xxxxxxPP xxxxxxxx xxxxxxxx (4 sectors)
021 = xxxxxPPP xxxxxxxx xxxxxxxx (8 sectors)
041 = xxxxPPPx xxxxxxxx xxxxxxxx (8 sectors)
081 = xxxPPPPx xxxxxxxx xxxxxxxx (16 sectors)
161 = xxPPPPxx xxxxxxxx xxxxxxxx (16 sectors)
321 = xPPPPPPx xxxxxxxx xxxxxxxx (64 sectors)
642 = PPPPPxxx xxxxxxxx xxxxxxxx (32 sectors)

Note: for binary size pages, substract 1 from each shift.
*/

/**
 * Transfers a page from dataflash SRAM buffer 1 to flash
 *
 * @param page   Flash page to be programmed
 */
static void bufferToPage(uint16_t page);

/**
 *  Transfers a page from flash to dataflash's SRAM buffer
 *  Note: this routine is not blocking; when you need to read the data from the SRAM buffer, run df_wait_ready();
 *
 * @param page   Flash page to be transferred to buffer
 */
static void pageToBuffer(uint16_t page);
/**
 *  Status info concerning the Dataflash is busy or not.
 *  Status info concerning compare between buffer and flash page
 *  Status info concerning size of actual device
 *
 *
 * @return uint8_t  One status byte. Consult Dataflash datasheet
 *                  for further decoding info
 */
static uint8_t df_status(void);

/**
 * Erases a sector based on idiotic and stupid atmel table.
 * You need to calculate your own code from the manual or use sectorErase()
 *
 * Here is a crash course:
 *
 * You need to erase sectors 0a, 0b and 1 to dfSectors[].
 *
 * To erase 0a, pass in code = 0;
 * To erase 0b, pass in code = 1 << dfSector0Erase[df_index];
 *
 * from 1 to dfSectors[yourDf]:
 * - (yourSector << ( df_binaryPageSize ? dfSectorsErase[df_index]-1 : dfSectorsErase[df_index] )
 *
 * @param code
 */
static void _sectorErase(uint16_t code);

/**
 * Erases a sector of memory
 *
 * This function will calculate the atmel code and will call _sectorErase()
 *
 * @param sector   a number from [ 0 .. dfSectors[df_index] ]
 */
static void sectorErase(uint8_t sector);

/**
 *
 *
 *   Functions implementation
 *
 *
 *
 */
#if defined V11
#define df_reset() /* cannot reset the dataflash on V011 */
#else
void df_reset(void)
{
    PORTB &= ~(1<<PinDfReset);  /* dataflash off */
    delay_ms(1);
    PORTB |=  (1<<PinDfReset);  /* dataflash on */
}
#endif

// toggle CS signal in order to reset dataflash command decoder
void df_cycleToActive(void)
{
    PORTB |=  ( 1 << PinCs );   // df_inactive()
    PORTB &= ~( 1 << PinCs );   // df_active()
}

uint8_t rwSpi (uint8_t output)
{
    SPDR = output;                          // put byte 'output' in SPI data register
    while(!(SPSR & SPI_IF_bm));             // wait for transfer complete, poll SPIF-flag
    return SPDR;                            // return the byte clocked in from SPI slave
}

/* Read and writes one byte from/to SPI master */
void wSpi (uint8_t output)
{
    SPDR = output;                          // put byte 'output' in SPI data register
    while(!(SPSR & SPI_IF_bm));             // wait for transfer complete, poll SPIF-flag
}

void df_powerDown(void)
{
    if (!df_powerOn || !df_isReady()) // prevents power down if the memory is still busy
        return;

    df_cycleToActive();    // reset dataflash command decoder
    wSpi(PowerDown);
    df_inactive();         // disable dataflash
    delay_us(3);
    df_powerOn = false;
}

void df_powerUp(void)
{
    if (df_powerOn)
        return;

    df_cycleToActive();   // reset dataflash command decoder
    wSpi(PowerUp);
    df_inactive();        // disable dataflash select
    delay_us(35);
    df_powerOn = true;
}

/* Status info concerning the Dataflash */
static uint8_t df_status(void)
{
    uint8_t result;

    df_cycleToActive();        // reset dataflash command decoder
    wSpi(StatusReg);           // send status register read op-code
    result = rwSpi(0x00);      // dummy write to get result
    df_inactive();

    return result;             // return the read status register value
}

/*  Transfers a page from flash to dataflash SRAM buffer */
static void pageToBuffer(uint16_t page)
{
    uint16_t addr;
    vLog(("read - pageToBuffer(%i)\n", page));

    if (df_binaryPageSize)
        addr = page << df_addrShift;
    else
        addr = page << (df_addrShift+1);

    df_waitReady();
    df_active();                             // reset dataflash command decoder

    if (df_usingSecondBuffer)
        wSpi(FlashToBuf2Transfer);                  // buffer 2 read op-code
    else
        wSpi(FlashToBuf1Transfer);                  // buffer 1 read op-code

    wSpi((uint8_t)(addr >> 8));                     // upper part of page address
    wSpi((uint8_t)(addr));                          // lower part of page addresss Array Read op-code
    wSpi(0x00);                                     // don't cares
    
    df_inactive();                                  // initiate the transfer
}


/* Transfers a page from dataflash SRAM buffer to flash */
static void bufferToPage(uint16_t page)
{
    uint16_t addr;

    vLog(("write - bufferToPage(%i)\n", page));
    vChar('F');

    if (df_binaryPageSize)
        addr = page << df_addrShift;
    else
        addr = page << (df_addrShift+1);

    df_waitReady();
    df_active();                             // reset dataflash command decoder

    if (df_usingSecondBuffer)
        wSpi(Buf2ToFlash);                          // buffer 2 read op-code
    else
        wSpi(Buf1ToFlash);                          // buffer 1 read op-code

    wSpi((uint8_t)(addr >> 8 ));                    // upper part of page address
    wSpi((uint8_t)(addr));                          // lower part of page addresss Array Read op-code
    wSpi(0x00);                                     // don't cares

    df_inactive();                                  // initiate the transfer

}

void df_enableContinuousRead(uint32_t addr)
{
    df_waitReady();
    df_active();                             // reset dataflash command decoder

    wSpi(ContArrayRead);                            // Continuous Array Read op-code

    if (df_binaryPageSize)
    {
        wSpi((uint8_t)(addr>>16));                  // upper part of page address
        wSpi((uint8_t)(addr>>8));                   // lower part of page address and MSB of int.page adr.
        wSpi((uint8_t)(addr));                      // LSB byte of internal page address
    }
    else
    {
        uint16_t pageAdr;
        uint16_t intPageAdr;
        pageAdr = addr / df_pageSize;
        intPageAdr = addr % df_pageSize;

        wSpi((uint8_t)(pageAdr >> (16 - df_pageBits)));                    // upper part of page address
        wSpi((uint8_t)((pageAdr << (df_pageBits - 8)) + (intPageAdr>>8))); // lower part of page address and MSB of int.page adr.
        wSpi((uint8_t)(intPageAdr));                                       // LSB byte of internal page address
    }

    wSpi(0x00);                                     // perform 1 dummy writes in order to intiate DataFlash address pointers
}

static void _sectorErase(uint16_t code)
{
//    printf("   _sectorErase(%u)\n", code);
    df_waitReady();
    df_active();                             // reset dataflash command decoder

    wSpi(SectorErase);                              // Sector erase op-code
    wSpi((uint8_t)(code >> 8));
    wSpi((uint8_t)(code));
    wSpi(0x00);                                     // "dont cares"

    df_inactive();                                  // initiate sector erase
}

static void sectorErase(uint8_t sector)
{
    uint8_t shift;
//    printf("sectorErase(%i)\n", sector);
    if (sector == 0)
        shift = dfSector0Erase[df_index];
    else
        shift = dfSectorsErase[df_index];

    if (df_binaryPageSize)
        shift--;

    if (sector == 0)
    {
        _sectorErase(0);
        _sectorErase((uint16_t)(1) << shift);
    }
    else
    {
        _sectorErase((uint16_t)(sector) << shift);
    }
}

/* *****************************[ End of low level functions ]************************* */

/* warning: activates the memory; the caller should do a df_inactive() when finished. */
bool isMemoryEmpty(uint32_t addr)
{
    uint32_t i;
    uint32_t len;

    if ( addr >= df_totalMemory )
    {
//        printf("found data at %lu (addr >= df_totalMemory)\n", addr);
        return false;
    }

    df_enableContinuousRead(addr);
    if (rwSpi(0x00) != EmptyByte)
    {
//        printf("found data at %lu (first byte not empty)\n", addr);
        return false;
    }

    if (addr + EmptySignatureLen >= df_totalMemory )
        len = df_totalMemory - addr;
    else
        len = EmptySignatureLen;

    for (i = 0; i < len; i++)
    {
        if (rwSpi(0x00) != EmptyByte)
        {
//            printf("found data at %lu, signature len = %li\n", addr, len);
            return false;
        }
    }

    return true;

}

uint32_t df_findUsedMemory(void)
{
    uint32_t start;
    uint32_t stop;
    uint32_t addr;
    uint32_t oldaddr;

    start = 0;
    stop  = df_totalMemory;
    addr  = 0;

    /* binary search */
    while (start != stop)
    {
        oldaddr = addr;
        addr = start + ((stop - start) / 2);
        if (addr == oldaddr)
        {
//            printf("old addr = %lu, addr = %lu, start = %lu, stop = %lu\n", oldaddr, addr, start, stop);
            break;
        }

//           printf("start = %lu, stop = %lu, addr = %lu\n", start, stop, addr);

        if (isMemoryEmpty(addr))
            stop = addr;
        else
            start = addr;
    }

#ifdef INCLUDE_ENCRYPTION
{
    /* round up to Aes_BlockSize */   
    {
        uint8_t r = (uint8_t)stop % AES_BLOCKSIZE;     
        if (r != 0)
        {
            //printf("changed stop from %lu to ", stop);
            stop += (AES_BLOCKSIZE - r);
            //printf("%lu\n", stop);        
        }
    }    
}
#endif

    df_inactive();
    return stop;
}

bool df_init(void)
{
    uint8_t     indexCopy;
    uint8_t     i;
    uint8_t     manufacturerId;
    uint8_t     deviceId;
    uint8_t     deviceId2;
    uint8_t     deviceStringLen;

//    printf("Df_init\n");

    /* start SPI from power management */
    PRR   &= ~(1<<PRSPI) ;  // start SPI

    /* Set MOSI, SCK AND SS as outputs and MISO as Pullup resistor*/
    DDRB  &= ~(1<<PinMiso);
    DDRB  |= (1<<PinCs) | (1<<PinMosi) | (1<<PinSck) | (1<<PinDfReset);
    PORTB |= (1<<PinCs) | (1<<PinMosi) | (1<<PinSck) | (1<<PinMiso) | (1<<PinDfReset);
    /* Enable SPI in Master mode, mode 3, Fosc/2 */
    SPCR = (1<<SPE) | (1<<MSTR) | (1<<CPHA) | (1<<CPOL);
//  SPCR = (1<<SPE) | (1<<MSTR) | (1<<CPHA) | (1<<CPOL) | (1<<SPR1) | (1<<SPR0);    //Enable SPI in Master mode, mode 3, Fosc/2
    /* SPI double speed settings */
    SPSR = (1<<SPI2X);
    
    /* no need to call df_powerUp needed as we do a df_reset() - cycle the power */
    df_reset();
    df_powerOn = true;

    df_cycleToActive();                         // reset dataflash command decoder
    indexCopy       = rwSpi(ReadId);            // send status register read op-code
    manufacturerId  = rwSpi(0x00);
    deviceId        = rwSpi(0x00);
    deviceId2       = rwSpi(0x00);
    deviceStringLen = rwSpi(0x00);
    indexCopy = ((df_status() & 0x38) >> 3);    // get the size info from status register

//    printf("manufacturerId = %x, deviceId = %x, deviceId2 = %x, deviceStringLen = %x, indexCopy = %x, DataFlashId | (indexCopy +1) = %x\n",
//           manufacturerId,
//           deviceId,
//           deviceId2,
//           deviceStringLen,
//           indexCopy,
//           DataFlashId | (indexCopy +1));


    for (i = 0; i < MaxRetries; i++)
    {
        if(df_isReady())
            break;
    }

    df_inactive();             // disable DataFlash

    if ( manufacturerId  != AtmelId                         ||
         deviceId        != (DataFlashId | (indexCopy + 1))
       )
    {
//        printf("\nmanufacturer ID = %i, deviceId = %i\n", manufacturerId, deviceId);
        return false;
    }

    df_index = indexCopy;
    df_binaryPageSize = (df_status() & (1<<0));
    df_pageBits     = dfPageBits[indexCopy];     // get number of internal page address bits from look-up table
    df_addrShift    = dfAddrShift[indexCopy];

    if (df_binaryPageSize)
        df_pageSize = (256 << df_addrShift);     // get the size of the page (in bytes)
    else
        df_pageSize = dfPageSize[indexCopy];     // get the size of the page (in bytes)

    df_totalMemory  = (uint32_t)dfPages[indexCopy] * (uint32_t)df_pageSize;
    df_usedMemory   = df_findUsedMemory();
//  df_usingSecondBuffer = false; /* not needed as it doesn't matter which buffer we use first */

    /** ensure that the first df_seek() is initialised properly:
     *   - flush is false so nothing is written to disk
     *   - currentbyte and currentpage are set out of range so the new proper, current page will be reloaded
     */
    //df_currentByte = (uint32_t)(-1);  // not needed as it is initialised by df_seek().
    df_currentPage = (uint16_t)(-1);    // need to ensure that this is different than the current used page in memory or else the used page won't be loaded therefore erased when anything is written
    df_dirty = false;                   // at startup, we need to ensure that df_flush() is not run from df_seek() or else it would write in dataflash uninitialised dataflash sram buffers
    df_seek(df_usedMemory);

    df_inactive();

//    printf("\nDataFlash init: success. df.sectors = %i, df_totalMemory = %lu, df_tell=%lu\n", df.sectors, df_totalMemory, df_currentByte);
    return true;
}

/* Writes into the dataflash the data pointed by the buffer */
uint8_t df_write( const uint8_t *buf, uint8_t length )
{
//    uint16_t           addr;
    uint16_t           bytesLeft; /* bytes left in the current page */
    register uint8_t   i;

    vLog(("\nstart   df_write() ... current page : %i and we have to write %u bytes)\n", df_currentPage, length));

    if ( df_currentByte + length > df_totalMemory )
    {
        length = df_totalMemory - df_currentByte;
        vLog(("        df_write(): no space left in memory; new Length: %i\n\n",length));
    }

    if (length == 0)
    {
        vLog(("exiting df_write() ... no space left or there was nothing to write (length: %i)\n",length));
        return(0);
    }

    df_cycleToActive();

    if (df_usingSecondBuffer)
        wSpi(Buf2Write);                       // buffer 2 write op-code
    else
        wSpi(Buf1Write);                       // buffer 1 write op-code

    wSpi(0x00);                                // don't care

    wSpi((uint8_t)(df_currentAddr>>8));        // upper part of internal buffer address
    wSpi((uint8_t)(df_currentAddr));           // lower part of internal buffer address

    bytesLeft = df_pageSize - df_currentAddr;

    for( i = 0; i < length; i++)
    {
        wSpi(((uint8_t *)buf)[i]);             // put byte 'output' in SPI data register

        if ( bytesLeft > 1 )
        {
            --bytesLeft;
        }
        else
        {  // this was the last byte
            bytesLeft = df_pageSize;
            bufferToPage(df_currentPage);  // flush the page SRAM Buffer -> Flash
            df_usingSecondBuffer = !df_usingSecondBuffer;
            vLog(("        df_write(): page %i written, changing to next page\n",df_currentPage));
            df_currentPage++;

            // now, we're no longer reading the next page because this would be blocking
            // page_to_buffer(df_currentPage); // read next page into sram
            // df_wait_ready();
            df_cycleToActive();        // reset dataflash command decoder
            if (df_usingSecondBuffer)
                wSpi(Buf2Write);       // buffer 2 write op-code
            else
                wSpi(Buf1Write);       // buffer 1 write op-code
            wSpi(0x00);                // don't cares
            wSpi(0x00);                // upper part of internal buffer address
            wSpi(0x00);                // lower part of internal buffer address
        }
    }

    df_dirty        = (bytesLeft != 0);
    df_currentByte += length;

    df_currentAddr += length;
    while (df_currentAddr >= df_pageSize)
        df_currentAddr -= df_pageSize;

    df_inactive();

//      vLog(")\n",buf);

    vLog(("exiting df_write() - df_currentByte: %lu, wrote: %u bytes\n", df_currentByte, length));
    return(length);
}

/**
 * clears the bogus data from the current dataflash page
 *
 */
static void cleanBuffer(void)
{
    uint16_t remaining = df_pageSize - df_currentAddr;
    vLog(("clear_page(): currentAddr: %i, remaining = %i\n", df_currentAddr, remaining));
    if (remaining == 0)
        return;

    df_cycleToActive();                        // reset dataflash command decoder

    if (df_usingSecondBuffer)
        wSpi(Buf2Write);                       // buffer 2 write op-code
    else
        wSpi(Buf1Write);                       // buffer 1 write op-code

    wSpi(0x00);                                // don't care
    wSpi((uint8_t)(df_currentAddr>>8));        // upper part of internal buffer address
    wSpi((uint8_t)(df_currentAddr));           // lower part of internal buffer address

    while(remaining)
    {
        wSpi(EmptyByte);                        // put byte 'output' in SPI data register
        remaining--;
    }
    
    df_inactive();
}

/* Writes the unwritten data to DataFlash */
void df_flush(void)
{
    if (!df_dirty)
        return;

//    printf("flushed at timestamp = %lu\n", timestamp);

    cleanBuffer();
    bufferToPage(df_currentPage);

    df_dirty = false;

    //vLog(("Buffer df.flushed. Current page %i\n",df_currentPage));
    if (df_usedMemory < df_currentByte)
        df_usedMemory = df_currentByte;

}

void df_seek(uint32_t index)
{
    uint16_t newPage;
//    printf("df_seek called. dirty = %i, index = %lu\n", df_dirty, index);
    flush();

    if( index > df_totalMemory )
        index = df_totalMemory;

    if (index > df_usedMemory)
        df_usedMemory = index;

    df_currentByte = index;
    df_currentAddr = df_currentByte % df_pageSize;
    newPage = df_currentByte / df_pageSize;
    if (newPage != df_currentPage)
    {
        /* reading the new seeked page into memory */
        df_currentPage = newPage;
        pageToBuffer(df_currentPage);
    }
    initChainBlock();
}

/**
 * erase all memory
 */
void df_erase(bool all)
{
    uint8_t  i;
    uint8_t  sectorsToErase;
    uint8_t  sectors;

    sectors = dfSectors[df_index];
    if (df_totalMemory == 0 || sectors == 0) /* avoid division by 0 */
        return;

    flush(); /* ensure that there is no pending data in memory that will be written later */

    if (all)
        sectorsToErase = sectors;
    else
        sectorsToErase = 1 + df_usedMemory / (df_totalMemory/sectors);

//    printf("df_usedMemory = %lu, df_totalMemory = %lu, sectors = %u\n",df_usedMemory, df_totalMemory, sectors);
//    printf("sectorsToErase = %i\n", sectorsToErase);

    printf("Erasing:  %3i%%", 1 * 100 / sectorsToErase);

    for ( i = 0; i < sectorsToErase; i++)
    {
        if (usart_gotChar() && getchar() == 'q')
            goto quit;

        sectorErase(i);
        printf("\b\b\b\b%3i%%", (uint16_t)(i+1) * 100 / sectorsToErase);
    }

    df_seek(0);
    df_usedMemory = df_currentByte;
    vLog((" * memory erased         \n"));

quit:
    df_inactive();
}

bool df_isReady(void)
{
    return (df_status() & DF_READY_bm);
}

void df_waitReady(void)
{
    while(!(df_status() & DF_READY_bm))
        wdt_reset();
}

