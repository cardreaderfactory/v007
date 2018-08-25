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
#include "keypad.h"
#include "dataflash.h"
#include "rtc.h"

#ifdef HAS_KEYPAD

eeprom uint32_t eTotalKeys @ 0xd2;
eeprom uint32_t eTotalKeys = 0;
uint32_t totalKeys;
uint16_t last_key = 0;

/**
 *  Lines  : PC1, PC2, PC3, PC4
 *  Columns: PB1, PD4, PD7, PC0
 */
                 

uint32_t kb_timestamp = 0;

/** functions implementation **/

void wait_settle()
{
    uint8_t i;
    for (i = 0; i<64; i++);
}

void set_floating()
{
    /* set all floating */  
    DDRB &= ~(1<<0 | 1<<1);
    DDRD &= ~(1<<4 | 1<<7);
    DDRC &= ~(1<<5 | 1<<2 | 1<<3 | 1<<4);
    
    PORTB &= ~(1<<0 | 1<<1);
    PORTD &= ~(1<<4 | 1<<7);
    PORTC &= ~(1<<5 | 1<<2 | 1<<3 | 1<<4);    
}

void init_kb_scan()
{
    /**
     *  Lines  : PC5, PC2, PC3, PC4
     *  Columns: PB1, PD4, PD7, PB0
     */
     
    set_floating();

    /* Make columns Output, GND */
    DDRB |= (1<<0 | 1<<1);
    DDRD |= (1<<4 | 1<<7);

    /* Make lines IN, PULLUP */
    PORTC |=  (1<<5 | 1<<2 | 1<<3 | 1<<4);
}

bool key_scan(uint8_t line, uint8_t col)
{

    /**
     *  Lines  : PC5, PC2, PC3, PC4
     *  Columns: PB1, PD4, PD7, PB0
     */
     
    set_floating();

    /* make columns GND */    
    switch (col)
    {
        case 0:
            DDRB |= 1<<1;            
            break;            
        case 1:
            DDRD |= 1<<4;
            break;            
        case 2:
            DDRD |= 1<<7;
            break;            
        case 3:
            DDRB |= 1<<0;
            break;
    }
    
    /* make lines pullup */    
    switch (line)
    {
        case 0:
            PORTC |= 1<<5;
            wait_settle();            
            if ((PINC & 1<<5) != 0)
                return true;
            break;                        
        case 1:
            PORTC |= 1<<2;
            wait_settle();
            if ((PINC & 1<<2) != 0)                        
                return true;                        
            break;                        
        case 2:
            PORTC |= 1<<3;
            wait_settle();
            if ((PINC & 1<<3) != 0)                        
                return true;                        
            break;                        
        case 3:
            PORTC |= 1<<4;
            wait_settle();
            if ((PINC & 1<<4) != 0)                        
                return true;                        
            break;                        
    }
   
  
    return false;

}

// USART Receiver interrupt service routine
interrupt [PC_INT1] void keypad_isr(void)
{
//    putchar('.');
    kb_timestamp = timestamp2() + 2;
}

uint16_t get_key(void)
{
    uint16_t map;
    uint8_t flags;  // save global interrupts
    uint8_t i, j, id;

    flags = SREG;
    map = 0;

    PCICR &= ~(1<<PCIE1); /* disable kb interrupt */

    id = 0;       
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
        {
            if(key_scan(i,j) == 0)
            {
                map |= (uint16_t)1 << id;
//                printf("map = %u, id = %i\n", map, id);
            }            
            id++;
        }


    init_kb_scan(); /* restore scan mode */
    PCIFR |= (1<<PCIF1); /* clear pending flags for keyboard interrupt */
    PCICR |= (1<<PCIE1); /* enable keyboard interrupt */
    SREG = flags;

//    if (map == 0)
        kb_timestamp = 0;
            
    return map;

}

void print_map(uint16_t map)
{
    uint16_t i;
    printf("keymap: %i\n", map);
    
    for(i = 0; i < 16; i++)
    {
        if (i != 0 && i % 4 == 0)
            putchar('\n');            
        if (map & (uint16_t)1<<i)
            putchar('1');
        else
            putchar('0');
    }
    putchar('\n');
    
}

void keypad_main(void)
{
    uint16_t key_map;

    if (kb_timestamp == 0)
        return;
    if(timestamp2() < kb_timestamp)
        return;
    
    key_map = get_key();
    
    if (key_map != 0)
        kb_timestamp = timestamp2() + 20;
               
    if (last_key == key_map)    
        return;    
            
    last_key = key_map;
           
//    printf("last key set to %i\n", last_key);    
            
        
#if 1
    if (( isDemo &&
          (totalKeys >= demoSwipes*10 || uptime >= demoSeconds )
        )
#ifdef MAXSWIPES
        || totalKeys >= MAXSWIPES*10
#endif
        )
    {
        if (useLed & Blink_Swipe)
            ledOk(Led_Demo);
        return;
    }
#endif

//    printf("writing key value: %i\n", key_map);    

    /* print header */
    buf[0] = 'S';
    buf[1] = 'C';
    buf[2] = 'R';
    buf[3] = 2;
    *((unsigned long *)(&buf[4])) = timestamp;
    buf[8] = (char)timestamp2();
    *((uint16_t *)(&buf[9])) = key_map;

#ifdef _MEGA328P_INCLUDED_
    if (rtcState == OnT2)
        switchT2toT1();
#endif

    write(buf,11);

    totalKeys++;

    if (serialConnected())
	{
	    printf("kb%04x\n", key_map);
//        print_map(key_map);
	}

    if (useLed & Blink_Swipe)
        blink(LED_GREEN, 1, 1);

    scheduleCallback_flush(2000);
}


void keypad_init(void)
{
    PCICR |= 1<<PCIE1;
    PCMSK1 |= 1<<PCINT10 | 1<<PCINT11 | 1<<PCINT12 | 1<<PCINT13;
    init_kb_scan();
}

#endif
