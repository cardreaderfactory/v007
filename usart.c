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
#include "global.h"
#include "usart.h"
#include "reader.h"
#include "rtc.h"
#include "stdio.h"

#define RX_BUFFER_SIZE 16
#define TX_BUFFER_SIZE 32

#define OVR     DOR0
#define UPE     UPE0
#define FE      FE0
#define RXCIE   RXCIE0
#define TXCIE   TXCIE0
#define TXEN    TXEN0
#define RXEN    RXEN0


#define FRAMING_ERROR (1<<FE)
#define PARITY_ERROR (1<<UPE)
#define DATA_OVERRUN (1<<OVR)
#define DATA_REGISTER_EMPTY (1<<UDRE0)
#define RX_COMPLETE (1<<RXC)

#if (TX_BUFFER_SIZE > 255 || RX_BUFFER_SIZE > 255)
    #error This file was written for buffers lower than 255
#endif


/* receive stuff */
bool                rx_overflow;
uint8_t             rx_counter;
uint8_t             rx_writeIndex;
uint8_t             rx_readIndex;
uint8_t             rx_buffer[RX_BUFFER_SIZE];

/* transmit stuff */
//bool                tx_overflow;
uint8_t             tx_counter;
uint8_t             tx_writeIndex;
uint8_t             tx_readIndex;
uint8_t             tx_buffer[TX_BUFFER_SIZE];

// USART Receiver interrupt service routine
interrupt [USART_RXC] void usart_rx_isr(void)
{
    char status;
    char data;

    status = UCSR0A;
    data   = UDR0;

    if((status & (FRAMING_ERROR | PARITY_ERROR | DATA_OVERRUN))==0)
    {
        rx_buffer[rx_writeIndex] = data;
        if(++rx_writeIndex == RX_BUFFER_SIZE)
            rx_writeIndex = 0;
        if(++rx_counter == RX_BUFFER_SIZE)
        {
            rx_counter  = 0;
            rx_overflow = true;
        };
    };
}

void write_next_byte()
{
    if(tx_counter != 0)
    {
        --tx_counter;
        UDR0 = tx_buffer[tx_readIndex];
        if(++tx_readIndex == TX_BUFFER_SIZE)
            tx_readIndex = 0;
    };
}

/* USART Transmitter interrupt service routine */
interrupt [USART_TXC] void usart_tx_isr(void)
{
    write_next_byte();
}

#ifndef _DEBUG_TERMINAL_IO_
/* Get a character from the USART Receiver buffer */
    #define _ALTERNATE_GETCHAR_
    #pragma used+
char getchar(void)
{
    unsigned char data;

    while(rx_counter==0)
        wdt_reset(); /* reset watchdog */

    data = rx_buffer[rx_readIndex];

    if(++rx_readIndex == RX_BUFFER_SIZE)
        rx_readIndex = 0;

    --rx_counter;

    return data;
}
    #pragma used-
#endif


#ifndef _DEBUG_TERMINAL_IO_
/* Write a character to the USART Transmitter buffer */
    #define _ALTERNATE_PUTCHAR_
    #pragma used+
void putchar(char c)
{
    uint8_t flags = SREG;  // save global interrupts

#ifdef DEBUG_INTERRUPTS
    if ((flags & 1<<SREG_I) == 0) // if the interrupts are disabled, write the data byte by byte.
    {
        while(tx_counter != 0)
        {
            while( (UCSR0A & 1<<UDRE0) == 0);
            write_next_byte();
        }
        while( (UCSR0A & 1<<UDRE0) == 0);
        UDR0 = c;
        while( (UCSR0A & 1<<UDRE0) == 0);
        UCSR0A |= (1<<TXC0);
        return;
    }
#endif

    while(tx_counter == TX_BUFFER_SIZE)
        wdt_reset(); /* reset watchdog */

    cli();

    if(tx_counter != 0 || ((UCSR0A & DATA_REGISTER_EMPTY)==0) )
    {
        tx_buffer[tx_writeIndex] = c;
        if(++tx_writeIndex == TX_BUFFER_SIZE)
            tx_writeIndex=0;

        ++tx_counter;
    }
    else
    {
        UDR0 = c;
    }

    SREG = flags; // restore global interrupts as they were

}
    #pragma used-
#endif


bool usart_gotChar(void)
{
    return(rx_counter != 0);
}

bool usart_isIdle(void)
{
    return(rx_counter == 0 && tx_counter == 0);
}

bool getchar_withTimeout(char *ch, uint8_t timeOut)
{
    uint32_t start;
    start = timestamp;
    while( !usart_gotChar() )
    {
        if(timestamp - start > timeOut )
            return false;

        wdt_reset(); /* reset watchdog */
    }

    *ch = getchar();
    return true;
}

void usart_init(unsigned long baud)
{
    unsigned int bauddiv;

    bauddiv = ( 2 * XTAL / 8 / baud / (2<<CLKPR) ) - 1 ; // for atmega329
    //bauddiv=XTAL/8/baud-1;

    /* start USART from power management */
    PRR &= ~( 1<<PRUSART0) ;  // start USART

    // USART initialization
    // Communication Parameters: 8 Data, 1 Stop, No Parity
    // USART Receiver: On
    // USART Transmitter: On
    // USART0 Mode: Asynchronous
    // USART Baud rate: baud (Double Speed Mode)
    UCSR0A = (1<<U2X0);
    UCSR0B = (1<<RXCIE | 1<<TXCIE | 1 << TXEN | 1 << RXEN );

    UCSR0C = 0x06; /* 8N1 */
    UBRR0H = 0x00;
    UBRR0H = bauddiv>>8;
    UBRR0L = bauddiv;
}

