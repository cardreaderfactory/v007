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
#include <string.h>

#ifdef PROFILER

uint32_t jiffies = 0;

#define PROFILER_SIZE 20
typedef struct {
    uint16_t    count;
    uint32_t    diff;
    int32_t     min;
    uint32_t    max;
    flash char *st;
} profiler_t;
profiler_t profiler[PROFILER_SIZE];

#if 0
uint32_t timerUnits()
{
   /** 8000000 interrupts/second =>
    *  1 interrupt = 1/8000000 = 0.000000125 seconds
    *  1 interrupt = 0.125 micro seconds
    *  1 microsecond = 1/0.125 = 8 interrupts
    *  elapsed microseconds = (jiffies * 65536 + timer) / 8
    *  elapsed microseconds = jiffies * 8192 + timer / 8
    */
   // return ((jiffies<<13) + (TCNT1H<<5) + (TCNT1L>>3));

   /* miliseconds:
      1 milisecond = 8000 interrupts
      elapsed miliseconds = jiffies *65536/8000
      aproximate = jiffies *8 + TNCNT1H >> 5 */
   //return ((jiffies<<3) + (TCNT1H>>5));

   /* 1 unit = 4 microseconds */
     return ((jiffies<<8) + (TCNT1H));
}
#endif /* 0 */

#define timerUnits() ( (jiffies<<8) + TCNT1H )  /* 1 unit = 4 microseconds */
#define tu2ms(VALUE) ( (VALUE)>>5 )  /* convert timer units to miliseconds */

void profiler_breakPoint(flash char *st)
{
   static uint32_t  lastClock = 0;
   uint32_t         currentClock;
   uint32_t         diff;
   int8_t           index;
   register int8_t  i;

   index        = -1;
   currentClock = timerUnits();

   /* look for buf slot */
   for (i = 0; i < PROFILER_SIZE; i++ )
   {
       if (profiler[i].st == st || profiler[i].st == NULL)
       {
           index = i;
           break;
       }
   }

   if (index == -1)
   {
       printf("too many breakpoints\n");
       return;
   }

   /* at this point, the slot that we've found in the profiler buffer is 'index' */

   diff = currentClock - lastClock;
   profiler[index].diff += diff;
   profiler[index].min = min( diff, profiler[index].min );
   profiler[index].max = max( diff, profiler[index].max );
   profiler[index].st = st;
   profiler[index].count++;

//       printf("%i sys=%lums, dif=%lums (%p)\n", line, systemMiliSeconds(), currentClock - lastClock, st);
   lastClock = timerUnits();
}

interrupt [TIM1_OVF] void timer1_ovf_interrupt(void)
{
    jiffies++;
}

void profiler_startTimer(void)
{
    int i;
    PRR    &= ~(1<<PRTIM1); // start timer 1

    TCNT1H = 0x00;
    TCNT1L = 0x00;

    TIMSK1 = (1<<TOIE1); // interrupt on compare A
    TIFR1 = 0xFF; // delete TIFR1 flags
    TCCR1A = 0x00;  // normal operation
    TCCR1B = (1<<CS10); // no divider

    memset(profiler, 0, sizeof(profiler));
    for (i = 0; i < PROFILER_SIZE; i++)
       profiler[i].min = -1;
}

bool profiler_showReport(char *st)
{
    register int8_t  i;

    /* look for buf slot */
    for (i = 0; i < PROFILER_SIZE; i++ )
    {
        if (profiler[i].st != NULL)
            printf("diff=%lums calls=%lu min=%lims avg=%lums max=%lums(%p)\n",
                    tu2ms(profiler[i].diff),
                    profiler[i].count,
                    tu2ms(profiler[i].min),
                    tu2ms(profiler[i].diff/profiler[i].count),
                    tu2ms(profiler[i].max),
                    profiler[i].st);
    }
    return true;
}

#endif /* PROFILER */