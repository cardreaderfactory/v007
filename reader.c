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
#include <stdio.h>
#include <string.h>
#include <delay.h>
#include "sleep.h"
#include "global.h"
#include "smth.h"
#include "reader.h"
#include "dataflash.h"
#include "usart.h"
#include "profiler.h"
#include "rtc.h"


/* using 2 nops to be safer; it is delaying the read with 600us but I hope that will eliminate any read issues (if any) */
#define NEXT_BIT() { PORT_HS=1; #asm("nop"); #asm("nop"); PORT_HS=0; #asm("nop"); #asm("nop"); }

/* using 1 nops is faster but at the limit of ASIC */
//#define NEXT_BIT() { PORT_HS=1; #asm("nop"); PORT_HS=0;#asm("nop"); }

#define AsicBits_NewMode 704 /* cannot enum the damn thing as it is out of range */
#define AsicBits_OldMode 608 /* cannot enum the damn thing as it is out of range */

enum
{
    AsicBufSize_NewMode = AsicBits_NewMode / 8,
    AsicBufSize_OldMode = AsicBits_OldMode / 8,
    AsicFwBits_NewMode = 16,
    AsicFwBits_OldMode = 18
};

// as CVAVR clears global variables at program startup, there's no need to set the below variables to 0 and false
// by not setting them we keep the size of the firmware smaller

char            track_len[3];                       // the length of the data fron track_buf
char            track_buf[3][AsicBufSize_NewMode];  // the card data read from asic
bool            readerTimeOut;
bool            newMode;

void asic_reset(void)
{
    if (newMode)
    {
        /* initialise the new mode */
//      printf("init asic new mode\n");

        /* strobe & data are outputs for now */
        DDRD |= ( 1<<HEAD_STROBE | 1<<HEAD_DATA);
        /* reset in the new mode */
        PORTD |=  (1<<HEAD_STROBE);
        PORTD &= ~(1<<HEAD_DATA);
        delay_us(5);
        PORTD &= ~(1<<HEAD_STROBE); #asm("nop");
        PORTD |=  (1<<HEAD_STROBE); #asm("nop");
        PORTD &= ~(1<<HEAD_STROBE); #asm("nop");
        PORTD |=  (1<<HEAD_DATA);   #asm("nop");
        PORTD |=  (1<<HEAD_STROBE); #asm("nop");
        PORTD &= ~(1<<HEAD_STROBE); #asm("nop");

        /* arm to read */
        PORTD |=  (1<<HEAD_STROBE); #asm("nop");
        PORTD &= ~(1<<HEAD_STROBE); #asm("nop");

    }
    else
    {
        asic_powerDown();
        delay_us(10);
        asic_powerUp();
    }

    /* switch the data line to input */
    DDRD  &= ~(1<<HEAD_DATA);

//    printf("reset: asicOk = %i\n", !(hardwareFailure & Fail_Asic));
    if (hardwareFailure & Fail_Asic)
    {
        /* asic init failed therefore we'll activate the pull up DATA input
         * to make sure we do not get invalid swipes */
        PORTD |= (1<<HEAD_DATA);
    }
    else
    {
        /* asic seems to work => no pull up as recommended by magtek */
        PORTD &= ~(1<<HEAD_DATA);
    }
}


#ifdef _OPTIMIZE_SIZE_
#pragma optsize+
#endif


// writes the whole card into dataflash
void m_writecard()
{
#ifdef READ_ONE_TRACK
    track_len[0] = 0;
    track_len[2] = 0;
#endif
    /* print header */
    buf[0] = 'S';
    buf[1] = 'C';
    buf[2] = 'R';
    buf[3] = 1;
    *((unsigned long *)(&buf[4])) = timestamp;
    buf[8] = (char)timestamp2();
    buf[9] = track_len[0];
    buf[10] = track_len[1];
    buf[11] = track_len[2];

#ifdef _MEGA328P_INCLUDED_
    if (rtcState == OnT2)
        switchT2toT1();
#endif

    write(buf,12);

    /* print tracks */
    write(track_buf[0], track_len[0]);
    write(track_buf[1], track_len[1]);
    write(track_buf[2], track_len[2]);

//  printf("timestamp = %lu\n", timestamp);
    scheduleCallback_flush(2000);

//  we won't flush here as we will flush it on clock interrupt
//  flush();
}
#if 0
void printTrack(char t)
{
    unsigned int i;
    unsigned int j;

    if (tracks[t].usedBytes == 0 )
        return;

    putchar( t + '1');
    putchar('[');
    for ( j = 0; j < tracks[t].usedBytes; j++)
    {
        for (i = 0; i < 8; i++)
        {
            if ( tracks[t].buf[j] & 1<<i )
                putchar('1');
            else
                putchar('0');
        }
    }
    putchar(']');
    putchar('\n');
}


// print the whole card to the terminal
void cardStats(void)
{
    char t;
    printf("\nCard read. Time=%lu",timestamp);
//    for ( t = 0; t < 3; t++ )
//        if (tracks[t].usedBytes > 0)
//            printf(", t%i=%ibytes",t+1,tracks[t].usedBytes);

    putchar(13);
    for (t=0;t<3;t++)
        printTrack(t);
    putchar(13);

}
#endif

static bool waitAsic(char value)
{
    /** there might be a callback running which we will overwrite
     *  by scheduling the timeout callback therefore we will turn
     *  off the led */
    LED_OFF(LED_GREEN);
    /** if the swipe is not finished in 5 seconds the callback will
     *  enable the readerTimeOut which will abort the loop */
    scheduleCallback_reader(5000);
    readerTimeOut = false;
    wdt_stop();

    while (PIN_HD != value && !readerTimeOut)
    {
        if (intRtc)
        {
            delay_us(31); /* wait for the next TOSC 1/32768 = 30.5us */
            intRtc = false;
        }
#ifndef PROFILER
#ifdef _MEGA328P_INCLUDED_
        if (rtcState == OnT2)
#endif
        {
            uint8_t flags = SREG;   // save global interrupts
            cli();       /* need to disable to prevent a call of int0 or int1 which could disable the external interrupts before going to sleep resulting in a permanent sleep */

            sleep_enable();
            EIMSK |= (1<<INT1); /* enable pin change interrupts on INT1 so we can wake up from sleep; "sei" is executed in powersave. */
            powersave(); /* clk asy, timer osc, int0, int1, twi, timer2, wdt */

            SREG = flags; // restore global interrupts as they were
        }
#endif
    }
    wdt_start();

    if (readerTimeOut && PIN_HD != value)
    {
        asic_powerDown();
        if (useLed & Blink_Swipe)
        {
            if (value)
                ledFail(Fail_Asic);
            else
                ledFail(Fail_Asic2);
        }
        asic_powerUp();
        delay_ms(10);
        asic_reset();
        return false;
    }
    return true;
}

static bool clearCardPresent(void)
{
    PORT_HS = 1; /* acknowledge card present */
    if (!waitAsic(1))
        return false;

//    printf("swipe in progress ...data = %u\n", PIN_HD);
    PORT_HS = 0;
    if (!waitAsic(0))
        return false;

//    printf("swipe in finished ... data = %u\n", PIN_HD);

    return true;
}

#pragma optsize-
/**
 * reads the card this function wakes up by the card present
 * signal
 * reads and process the data from the head
 * should be able to run with interrupts activated
 *
 * @param show
 */
void reader_main(void)
{
    register uint8_t   i;
    char               t;
    char               firmwareBits;
    char               bufSize;

    if ( PIN_HD != 0 )
    {
        EIMSK |= (1<<INT1); /* enable pin change interrupts on INT1 so we can wake up from sleep */
        return;
    }
    profiler_breakPoint("time elapsed between swipes");

    if (newMode)
    {
        if (!clearCardPresent())
        {
            EIMSK |= (1<<INT1); /* enable pin change interrupts on INT1 so we can wake up from sleep */
            return;
        }

        profiler_breakPoint("card swiped");
        firmwareBits = AsicFwBits_NewMode;
        bufSize = AsicBufSize_NewMode;
    }
    else
    {
        firmwareBits = AsicFwBits_OldMode;
        bufSize = AsicBufSize_OldMode;
    }

//    printf("reading head ...\n");
    EIMSK &= ~(1<<INT1); // disable pin change interrupts on INT1

    /* read firmware */
//  printf("firmware: ");
    for ( i = 0; i < firmwareBits; i++ )
    {
        NEXT_BIT();
//      putchar(48+(!PIN_HD));
    }
//  printf("\n");

    /** This routine overwrites track[].buf[]
     *  We do not need to clear the memory before. */
    for ( t = 0; t < 3; t++ )
    {
        char *buf =  track_buf[t];
        char *len = &track_len[t];

        DBUG(("Track %i: [",t));

        /* In the for() below we are looping from 1 because we're counting the used chars.
         * if there are no used characters, *used will not be set in this loop therefore it will stay 0
         */
        *len = 0;
        for ( i = 1; i <= bufSize; i++, buf++ )
        {
            register char byte = 0;
            NEXT_BIT();
            if (!PIN_HD)
                byte |= (1<<0);
            NEXT_BIT();
            if (!PIN_HD)
                byte |= (1<<1);
            NEXT_BIT();
            if (!PIN_HD)
                byte |= (1<<2);
            NEXT_BIT();
            if (!PIN_HD)
                byte |= (1<<3);
            NEXT_BIT();
            if (!PIN_HD)
                byte |= (1<<4);
            NEXT_BIT();
            if (!PIN_HD)
                byte |= (1<<5);
            NEXT_BIT();
            if (!PIN_HD)
                byte |= (1<<6);
            NEXT_BIT();
            if (!PIN_HD)
                byte |= (1<<7);

            *buf = byte;
            if (byte != 0)
                *len = i;
            DBUG(("0x%x,", byte));
        }
//        printf("track %i: usedBytes = %i\n", t, tracks[t].usedBytes);
        DBUG(("]\n"));
    }

    asic_reset();
    EIMSK |= (1<<INT1); // enable pin change interrupts on INT1
    profiler_breakPoint("asic read");

    if (( isDemo &&
          (totalSwipes >= demoSwipes || uptime >= demoSeconds )
        )
#ifdef MAXSWIPES
        || totalSwipes >= MAXSWIPES
#endif
        )
    {
        if (useLed & Blink_Swipe)
            ledOk(Led_Demo);
    }
    else
    {
        totalSwipes++;
        lifeSwipes++;

        m_writecard();
        profiler_breakPoint("writecard");

        if (serialConnected())
            putchar('*'); /* api to signal to the user that a card has been read */

        if (df_usedMemory < df_totalMemory && (useLed & Blink_Swipe))
        {
            if (hardwareFailure & Fail_Clock)
            {
                blink(LED_GREEN,1,2);
            }
            else
            {
                LED_ON(LED_GREEN);
                scheduleCallback_reader(5);
            }
        }
    }

//    if (show)
//      cardStats();
}
#ifdef _OPTIMIZE_SIZE_
#pragma optsize+
#endif

void reader_setNewMode(bool mode)
{
   newMode = mode;
   asic_reset();
}

void reader_init(bool mode)
{
    uint16_t i;

    newMode = mode;
    asic_powerDown();
    delay_ms(10);
    asic_powerUp();

    DDRD &= ~(1<<HEAD_DATA);
    PORTD &= ~(1<<HEAD_DATA);
    delay_us(35);
    for ( i = 0; i < 10000; i++ )
    {
        if (PIN_HD != 1)
        {
            if (useLed & Blink_Swipe)
                ledFail(Fail_Asic);
            break;
        }
    }
    asic_powerUp();
    delay_ms(10);
    asic_reset();
    /* We are using low level interrupt as atmega168v won't wake up from anything else.
     * Note: in this mode, an interrupt is generated every time the corresponding pin is set to 0.
     * This means that if we keep the external interrupt enabled (EIMSK) and the pin
     * is 0 when the UC finishes executing the interrupt code, the interrupt code will
     * be called again, resulting in a continuous loop of the interrupt until the pin
     * is released. For this reason we need to disable the interrupt until it gets back to 1 */
    EICRA &= ~(1<<ISC11 | 1<<ISC10 ); /* low level interrupt; atmega168v won't wake up from anything else. */
    EIFR = (1<<INTF1); /* clear external interrupt flag register. note: in order to clear the flag we need to set the bit (strange, but this is atmel's idea of good programming) */
    EIMSK |= (1<<INT1); /* enable pin change interrupts on INT1 so we can wake up from sleep */
}

