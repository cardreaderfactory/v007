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

#ifndef _USART_H
#define _USART_H

/**
 * Initialise the Serial Port
 *
 * @param baud
 */
void usart_init(unsigned long baud);

/**
 *
 * @return bool     True if there is any character in the
 *                  receive buffer
 */
bool usart_gotChar(void);

/**
 * @return bool     True if Usart has no queueing chars in the
 *                  buffer
 */
bool usart_isIdle(void);

/*
 * reads a char into ch.
 * only blocks for the specified number of seconds
 *
 * if times out, returns false and ch content is left unchanged.
 */
bool getchar_withTimeout(char *ch, uint8_t timeOut);

#ifndef _DEBUG_TERMINAL_IO_
    // Get a character from the USART Receiver buffer
    #pragma used+
        char getchar(void);
        void putchar(char c);
    #pragma used-
#endif
#endif