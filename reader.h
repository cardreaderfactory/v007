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

#ifndef _READER_H
#define _READER_H

#define HEAD_STROBE 6       //pd2 - strobe
#define HEAD_DATA   3       //pe2 - data
#define HEAD_VCC    5       //pd3 - vcc

#define PORT_HS  PORTD.HEAD_STROBE
#define PORT_HD  PORTD.HEAD_DATA
#define PIN_HD   PIND.HEAD_DATA

#define SWIPE_TIME_DIFF 64 /* 250ms - reads below this value will not be considered as different wipes */

#define READER_HAS_DATA() (PIND.HEAD_DATA == 0)

/**
 * Starts the ASIC reader
 *
 */
#define asic_powerUp()                                          \
{                                                               \
    /* power up asic */                                         \
    DDRD |= (1<<HEAD_STROBE | 1<<HEAD_VCC);                     \
    PORTD |=  (1<<HEAD_STROBE | 1<<HEAD_VCC);                   \
}

/**
 * Shuts down the power to the ASIC reader
 */
#define asic_powerDown()                                        \
{                                                               \
    DDRD |= (1<<HEAD_STROBE | 1<<HEAD_VCC | 1<<HEAD_DATA);      \
    PORTD &= ~(1<<HEAD_STROBE | 1<<HEAD_VCC | 1<<HEAD_DATA);    \
}


extern bool readerTimeOut;

/**
 * Resets the asic.
 *
 * This function is required to initialise the new mode
 * (whenever this mode is wanted)
 */
void asic_reset(void);

/**
 * Reads the data from delta asic and writes it into memory

 */
void reader_main(void);

/**
 * initialises the reader library.
 *
 * @param[in] mode  the mode to use the asic
 * @return true     if delta asic is present
 */
void reader_init(bool mode);


/** sets the reader in the new or old mode
 *
 * @param[in]       true = newMode, false = oldMode
 */
void reader_setNewMode(bool mode);


#endif
