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

#ifndef _RTC_H
#define _RTC_H

extern uint32_t timestamp;
extern uint32_t uptime;

#ifdef _MEGA328P_INCLUDED_

    extern uint8_t rtcState;
    extern uint8_t t1_overflows;
    extern uint16_t clicksRemaining;
#ifdef DEBUG_SWITCH_CLOCK    
    extern uint16_t calcErr;
#else    
    #define calcErr (6450)
#endif    
        
    enum
    {
        OnT2,           // 0           
        OnT1,           // 1
    };
       
    void switchT2toT1();
    void switchT1toT2();
#endif


/* initialises the Real Time Clock */
void rtc_init(void);

/* calibrates the system clock based on the external 32khz quartz */
bool calibrateOsccal();

#endif