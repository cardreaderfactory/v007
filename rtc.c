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
#include "global.h"
#include "rtc.h"
#include "profiler.h"
#include "reader.h"
#include "smth.h"

#define TCNT1 (*(unsigned int *) 0x84)


// how to calculate the calibration value
// 32khz with /128 prescaller = 256 clicks/sec
// internal click = XTAL / 256
// 32768 .. 8000000
// 128   .. x
// x = XTAL * 128 / 32768 = XTAL / 256


#if T2DIV == 8
    /* TCNT2 t2_overflows 16 times per second */
    #define T2VALUE (192/8)
    #define T1VALUE (T2VALUE * XTAL * 8 / 32768)
#elif T2DIV == 128
    /* TCNT2 t2_overflows once per second */
    #define T2VALUE (256/8)
    #define T1VALUE (T2VALUE * XTAL * 128 / 32768)
#else
    #error code not implemented for this divider
#endif

#if T1VALUE > 65535
  #error T2VALUE is too high therefore T1VALUE overflows
#endif


// timer 0 (8bit)   - not used
// timer 1 (16bit)  - used by RTC, osccal calibrator and profiler
// timer 2 (8bit)   - RTC from external quartz

uint32_t timestamp          = 0;    /* system's timestamp */
uint32_t uptime             = 0;
uint16_t timer2a_overflows  = 0;
uint16_t timer2b_overflows  = 0;
uint8_t  timer2a_compare    = 0;
uint8_t  timer2b_compare    = 0;

#if T2DIV == 8
uint8_t  t2_overflows          = 0;
#endif

#ifdef _MEGA328P_INCLUDED_

#define T1TOP (XTAL / 256) /* in order to overflow 256 times per second we need to use a TOP value for T1 of 31250 */

uint8_t rtcState = OnT2;
uint8_t t1_overflows = 0;
uint16_t clicksRemaining = 0;

interrupt [TIM1_COMPA] void timer1_compare_interrupt(void)
{
    t1_overflows++;
    if (t1_overflows == 0)
    {
//        putchar('+');
        ++timestamp;
        ++uptime;
    }
}

/* switch to T1 clock */
void switchT2toT1()
{
    uint32_t clicks;
    uint16_t tmp = 0;
    uint8_t t2;
    // atmega328p datasheet (8271E–AVR–07/2012) page 154
    // Reading of the TCNT2 Register shortly after wake-up from Power-save may give an incorrect result.
    // The recommended procedure for reading TCNT2 is thus as follows:
    TCCR2A = 0;    // Write any value to either of the registers OCR2x or TCCR2x,
    while((ASSR & (1<<TCR2AUB)) && tmp < 0xffff) tmp++; // Wait for the corresponding Update Busy Flag to be cleared.

//    if (tmp == 0xffff)
//        printf("timeout waiting for ASSR. maybe clock doesn't work?\n");
    t2 = TCNT2;                 // Read TCNT2

    rtcState = OnT1;
    // converting  (t2_overflows,t2) -> (t1_overflows, TCNT1)

#if OVERFLOWS_PER_SECOND == 16
    clicks = t2_overflows * (XTAL / 16) + (uint32_t)t2 * (XTAL/16) / 256 + clicksRemaining;  // t2_overflows * 1000000 + t2 * 500000 / 256
#elif OVERFLOWS_PER_SECOND == 1
    clicks = t2 * (uint32_t)(XTAL/OVERFLOWS_PER_SECOND) / 256 + clicksRemaining;
#else
    //clicks = t2 * T1TOP * 8;
    #error T2DIV value not supported. you would need to adjust timestamp here too.
#endif
//    printf("remaining = %u\n", clicksRemaining);
//    printf("switchT2toT1(). t1top: %u, ocr1a:%u / %i\n", (uint16_t)T1TOP, (uint16_t)OCR1AH<<8|OCR1AL, (uint16_t)t2);

    tmp = clicks/T1TOP;
    timestamp += (tmp >> 8);
    t1_overflows = tmp;
    clicks = clicks % T1TOP;

    PRR   &= ~(1<<PRTIM1); // start timer 1
    TCCR1B = 0;             // disconnect t1
    TIFR1 = 0xFF;           // delete all timer1 flags, including (1<<TOV1) overflow interrupt flag
    TCNT1H = (uint8_t)((uint16_t)clicks >> 8); // start from calculated t2 value, converted to t1
    TCNT1L = (uint8_t)clicks;                  // start from calculated t2 value, converted to t1
    OCR1AH = (uint8_t)((uint16_t)T1TOP >> 8);   // running until T1TOP
    OCR1AL = (uint8_t)(T1TOP);                  // running until T1TOP
    TCCR1B = (1<<WGM12 | 1<<CS10);              // start timer1 in CTC mode, with no prescalling
    TIMSK1 = (1<<OCIE1A);                       // activate compare interrupt
//    printf("switchT2toT1(). ocr1a:%u/%u, %u/%u TCNT1: %u/%u\n", OCR1AH, OCR1AL, T1TOP>>8, (uint8_t)T1TOP, TCNT1, (uint16_t)clicks);
}

void switchT1toT2()
{
    uint16_t t1 = TCNT1;
//    printf("switchT1toT2\n");

    if (hardwareFailure & Fail_Clock)
    {
//        printf("failed to switch to T2 due to RTC failure\n");
        return;
    }


    TCCR1B = 0; // disconnect t1 clock
    TIMSK1 = 0; // disconnect t1 interrupts
//    if (TIFR1 & (1<<OCF1B))
//    {
//        timer1_compare();
//        TIFR1 = (1<<OCF1B);
//        putchar('.');
//    }
//        printf("tifr1 = %i\n", TIFR1);
    PRR |= (1<<PRTIM1); // stop timer 1

    // convert  (t1_overflows,TCNT1) -> (t2_overflows, TCNT2)

#if OVERFLOWS_PER_SECOND == 16
    {
        uint32_t clicks;
        uint32_t tmp;
        uint8_t t2;

        clicks = (uint32_t)t1_overflows * T1TOP + t1;

        // (needs 3900 clocks to complete)
        t2_overflows = clicks / (XTAL/OVERFLOWS_PER_SECOND);
        tmp = clicks % (XTAL/OVERFLOWS_PER_SECOND);
        t2 = tmp * 256 / (XTAL/OVERFLOWS_PER_SECOND);
        clicksRemaining = tmp - (uint32_t)t2 * (XTAL/OVERFLOWS_PER_SECOND) / 256 + calcErr;

//      {
//        uint32_t clicks2;
//        cli();
//        clicks_stop = t1_overflows * (uint32_t)T1TOP + TCNT1;
//        SREG = flags; // restore global interrupts as they were
//        printf("t2_overflows = %u, clicks = %lu, tmp = %lu, t2 = %u\n", t2_overflows, clicks, tmp, t2);
//        printf("diff = %lu, remaining = %u\n", clicks2-clicks, clicksRemaining);
//      }
        TCNT2 = t2;
    }
#elif OVERFLOWS_PER_SECOND == 1
    TCNT2 = t1_overflows;
    clicksRemaining = ... to be seen;
#else
    #error T2DIV value not supported. you would need to adjust timestmap too
#endif

    rtcState = OnT2;

    // atmega328p datasheet (8271E–AVR–07/2012) page 153
    // When entering Power-save or ADC Noise Reduction mode after having written to TCNT2
    // the user must wait until the written register has been updated if Timer/Counter2 is used to wake up the device
    t1 = 0;
    while((ASSR & (1<<TCN2UB)) && t1 < 0xffff) t1++; // Wait for the corresponding Update Busy Flag to be cleared.
}
#endif


void startT2AComp()
{
//    printf("timer2a_overflows: 0, TCNT2 = %u, OCR2A = %u, compare = %u\n", TCNT2, OCR2A, timer2a_compare);
    OCR2A = timer2a_compare;
    TIFR2 = (1<<OCF2A); // clear the old interrupt handling flag for compare2a. note: in order to clear the flag we need to set the bit (strange, but this is atmel's idea of good programming)
    TIMSK2 |= (1<<OCIE2A);
}

void startT2BComp()
{
//    printf("timer2b_overflows: 0, TCNT2 = %u, OCR2B = %u, compare = %u\n", TCNT2, OCR2B, timer2b_compare);
    OCR2B = timer2b_compare;
    TIFR2 = (1<<OCF2B); // clear the old interrupt handling flag for compare2a. note: in order to clear the flag we need to set the bit (strange, but this is atmel's idea of good programming)
    TIMSK2 |= (1<<OCIE2B);
}


interrupt [TIM2_OVF] void timer2_ovf_interrupt(void)
{
    /**
     * The reason that we are using a /8 prescaller instead of /128
     * one is that the controller won't wake up until on int 1
     */

#ifdef _MEGA328P_INCLUDED_
    if (rtcState == OnT2)
#endif
    {
#if OVERFLOWS_PER_SECOND == 16
        /* 16 t2_overflows per second */
        ++t2_overflows;
        if (t2_overflows == 16)
        {
//            putchar('+');
            ++timestamp;
            ++uptime;
            t2_overflows = 0;
        }
#elif OVERFLOWS_PER_SECOND == 1
        /* one overflow per second */
        ++timestamp;
        ++uptime;
#else
        #error code not implemented for this divider
#endif
    }

    if (timer2a_overflows > 0)
    {
        timer2a_overflows--;
        if (timer2a_overflows == 0)
            startT2AComp();
    }

    if (timer2b_overflows > 0)
    {
        timer2b_overflows--;
        if (timer2b_overflows == 0)
            startT2BComp();
    }

    intRtc = true;
}

/* timer compare interrupt for scheduleCallback_reader() */
interrupt [TIM2_COMPA] void timer2_compa_isr(void)
{
    vChar('A');
    TIMSK2 &= ~(1 << OCIE2A);  // disable T2ACOMP interrupt

    readerTimeOut = true; // this will timeout the asic wait loop
    LED_OFF(LED_GREEN);

//    printf("TIM2_COMPA, TCNT2 = %u, OCR2A = %u, compare = %u\n", TCNT2, OCR2A, timer2a_compare);
}

/* starts a callback to shut down the led
 * note: to reduce the size of the code, there are 1024ms in a second */
void scheduleCallback_reader(uint16_t ms)
{
    TIMSK2 &= ~(1<<OCIE2A); // disable T2ACOMP interrupt
    // 4096     ... 1000ms
    // OCR2A    ... x ms

    // convert ms to cycles
#if CYCLES_PER_SECOND >= 1024
    ms = TCNT2 + ms * (CYCLES_PER_SECOND / 1024);
#else
    ms = TCNT2 + ms / (1024 / CYCLES_PER_SECOND);
#endif

    /* timer2a_overflows will be decreased by the overflow interrupt;
       when timer2a_overflows reaches 0, we will start the timer2a compare*/
    timer2a_overflows = (uint16_t)(ms >> 8);
    timer2a_compare = (uint8_t)ms;
    if (timer2a_overflows == 0)
        startT2AComp();

//    printf("scheduleCallback_reader(%u): TCNT2 = %u, timer2a overflow = %u, compare = %u\n", ms, TCNT2, timer2a_overflows, timer2a_compare);
}

/* timer compare interrupt for scheduleCallback_flush() */
interrupt [TIM2_COMPB] void timer2_compb_isr(void)
{
    vChar('B');
    TIMSK2 &= ~(1 << OCIE2B);  // disable T2BCOMP interrupt

    flush_pending = true;
    stayAwake = false;

//    printf("TIM2_COMPB, TCNT2 = %u, OCR2B = %u, compare = %u\n", TCNT2, OCR2B, timer2b_compare);

}

/* starts a callback to flush the dataflash and eeprom
 * note: to reduce the size of the code, there are 1024ms in a second */
void scheduleCallback_flush(uint16_t ms)
{
    if (hardwareFailure & Fail_Clock)
    {
        /* there's no clock to let us know when the timeout occurs. we need to write everything before we go to sleep */
        flush_pending = true;
        stayAwake = false;
        return;
    }

    TIMSK2 &= ~(1<<OCIE2B); // disable T2BCOMP interrupt

    flush_pending = false;
    stayAwake = slowWakeUp;

    // 4096     ... 1000ms
    // OCR2A    ... x ms
    // convert ms to cycles
#if CYCLES_PER_SECOND >= 1024
    ms = TCNT2 + ms * (CYCLES_PER_SECOND / 1024);
#else
    ms = TCNT2 + ms / (1024 / CYCLES_PER_SECOND);
#endif

    /* timer2b_overflows will be decreased by the overflow interrupt;
       when timer2b_overflows reaches 0, we will start the timer2b compare*/
    timer2b_overflows = ms >> 8;
    timer2b_compare = (uint8_t)ms;
    if (timer2b_overflows == 0)
        startT2BComp();

//    printf("scheduleCallback_flush(%u): TCNT2 = %u, timer2b overflow = %u, compare = %u\n", ms, TCNT2, timer2b_overflows, timer2b_compare);
}


/*
 * returns the timestamp in miliseconds * 3.90625
 * 32 units = 125ms
 * 64 units = 256ms
 */
unsigned long timestamp2(void)
{
#ifdef _MEGA328P_INCLUDED_
    if (rtcState == OnT2)
#endif

#if OVERFLOWS_PER_SECOND == 16
        return ( (timestamp<<8) | (t2_overflows<<4) | (TCNT2>>4) );
#elif OVERFLOWS_PER_SECOND == 1
        return ((timestamp<<8) | TCNT2);
#endif

#ifdef _MEGA328P_INCLUDED_
    else
        return ( (timestamp<<8) | t1_overflows );
#endif

}

void rtc_init(void)
{
//    return; /* disabled for testing purposes */

    // Timer/Counter 2 initialization
    // Clock source: Crystal on TOSC1 pin
    // Mode: Normal top=FFh
    // OC2A output: Disconnected
    // OC2B output: Disconnected
    ASSR    =   (1 << AS2); /* asynchronous Timer/Counter2 */
    TCCR2A  =   0x00;
#if T2DIV == 8
    /* 16 t2_overflows per second */
    TCCR2B  =   (1 << CS21);    /* 8 prescaller */
#elif T2DIV == 128
    /* one overflow per second */
    TCCR2B  =   (1 << CS22) | (1<<CS20);    /* 128 prescaller */
#elif
    #error code not implemented for this divider
#endif

    TCNT2   =   0x00;
    OCR2A   =   0x00;
    OCR2B   =   0x00;
    TIFR2   =   0xff;

    // Timer/Counter 2 Interrupt(s) initialization
    TIMSK2  =   (1 << TOIE2); /* timer/counter2 overflow interrupt enable */
}


#ifdef T2VALUE
/* returns the ticks timer 1 does in "T2VALUE" */
uint16_t countTimer1(char newOsccal)
{
    uint8_t b[4];
    uint8_t loop = 0;
    uint8_t cnt;
    uint16_t temp = 0xFFFF;

    b[0] = OSCCAL;
    b[1] = TIMSK1;
    b[2] = TCCR1B;
    b[3] = PRR;

    PRR   &= ~(1<<PRTIM1); // start timer 1
    OSCCAL = newOsccal;

    while (temp == 0xFFFF && loop < 5)
    {
        wdt_reset(); /* reset watchdog */
        //printf("try = %i\n", loop);
        /* timer 1 initialization */
        TIMSK1 = 0;             // delete any interrupt sources
        TCNT1H = 0;             // clear timer1 counter (need to write high byte first)
        TCNT1L = 0;             // clear timer1 counter
        //uint8_t flags = SREG; cli();

        //printf("t2start = %i, t2stop = %i, currentt2=%i\n", (uint8_t)(TCNT2+1), (uint8_t)(TCNT2+T2VALUE), TCNT2);
        TCCR1B = (1<<CS10);     // start timer1 with no prescaling

        /* syncronise with TCNT2 */
        cnt = TCNT2 + 1;
        TIFR1 = 0xFF;           // delete all timer1 flags, including (1<<TOV1) overflow interrupt flag
        while( (TCNT2 != cnt) && !(TIFR1 & (1<<TOV1)));

        /* then, wait the T2VALUE interval and count how long it takes in T1 clocks; it should be T1VALUE */
        cnt += T2VALUE;
        TIFR1 = 0xFF;           // delete all timer1 flags, including (1<<TOV1) overflow interrupt flag
        TCNT1H = 0;             // clear timer1 counter. cannot write directly into TCNT1 because CVAVR fails. (need to write high byte first)
        TCNT1L = 0;             // clear timer1 counter

        while( (TCNT2 != cnt) && !(TIFR1 & (1<<TOV1)));

        TCCR1B = 0; // stop timer1
        //SREG = flags;
        //printf("currentt2=%i\n", TCNT2);

        if (TIFR1 & (1<<TOV1))
        {
            //OSCCAL = b[0];
            //printf("timer1 overflowed\n");
            //OSCCAL = newOsccal;

            temp = 0xFFFF;      // if timer1 t2_overflows the quartz is not working, set the temp to 0xFFFF
            loop++;

        }
        else
        {   // read out the timer1 counter value
            temp = TCNT1;
        }
        //printf("counter1 = %lu\n", temp);
    }

    TIFR1 = 0xFF;   // delete all timer1 flags, ensuring that no interrupts for T1 caused by this routine will be called
    OSCCAL = b[0];
    TIMSK1 = b[1];
    TCCR1B = b[2];
    PRR    = b[3];
    //printf("osccal = %u, timer = %u\n", newOsccal, temp);
    return temp;
}

/* calcBestOsccal()
 *
 * calculates the best osccal value for the defined interval
 * however it assumes that all the values are sorted
 * as osccal ranges overlap, this routine will have to be called for each overlapping intervals
 *
 * start - the first value of the osccal
 * stop - the last value of the osccal (including this value)
 * value - the desired value
 * diff - if not NULL, it writes in here the difference to the desired value
 *
 */
uint8_t calcBestOsccal(uint8_t start, uint8_t stop, uint16_t desired_value, uint16_t *b_diff)
{
    uint8_t  index            = 0;
    uint8_t  old_index;
    uint8_t  best_index;
    uint16_t current_value;
    uint16_t diff;
    uint16_t best_diff        = 0xffff;

    best_index = start;

    oLog(("start = %i, stop = %i\n", start, stop));
    while (start != stop)
    {
        old_index = index;
        index = start + ((stop - start) / 2);
        if (index == old_index)
        {
            /* we are going to exit. there is however one value left to check: "stop" */
            index++;
            start = stop = index; /*ensure that the loop is going to exit; do not break here as we still need to check this new index */
        }

        current_value = countTimer1(index);
//        printf("countTimer1(%i) = %u\n", index, current_value);

        if (desired_value > current_value)
        {
            diff = desired_value - current_value;
            start = index;
        }
        else
        {
            diff = current_value - desired_value;
            stop = index;
        }


        if (diff < best_diff)
        {
            oLog(("[%i] diff=%u\n", index, diff));
            best_diff = diff;
            best_index = index;
        }
    }

    if (b_diff != NULL)
        *b_diff = best_diff;
    return best_index;
}

bool calibrateOsccal()
{
    uint16_t diff1, diff2;
    uint8_t o1, o2;

    oLog(("default: countTimer1(%i) = %u\n", OSCCAL, countTimer1(OSCCAL)));
    if (countTimer1(OSCCAL) == 0xffff)
    {
        oLog(("calibrating clock failed\n"));
        return false;
    }

    o1 = calcBestOsccal(0, 127, T1VALUE, &diff1);
    o2 = calcBestOsccal(128, 255, T1VALUE, &diff2);

    oLog(("OSCCAL = %i, diff = %i\n", o1, diff1));
    oLog(("OSCCAL = %i, diff = %i\n", o2, diff2));

    if (diff1 > diff2)
        o1 = o2;

    if (countTimer1(o1) != 0xffff)
    {
        oLog(("OSCCAL changed from %i to %i\n", OSCCAL, o1));
        OSCCAL = o1;
    }

//    printf("countTimer1(%i) = %u, desired = %u\n", OSCCAL, countTimer1(OSCCAL), T1VALUE);
//    printf("calcBestOsccal(0,127) = %u, expected = %u\n", calcBestOsccal(0,127,T1VALUE, NULL), T1VALUE);
//    printf("calcBestOsccal(127,255) = %u, expected = %u\n", calcBestOsccal(128,255,T1VALUE, NULL), T1VALUE);

/* as calibrate osccal has messed up the TCNT1, we need to restart the profiler */
    profiler_showReport(NULL);
    profiler_startTimer();

    return true;
}
#else
bool calibrateOsccal()
{
    return true;
}
#endif