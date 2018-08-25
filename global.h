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

#ifndef GLOBAL_H
#define GLOBAL_H

/*  ------------------- device to build ------------------- */
//    #define V6
//    #define V7
//    #define V8
//    #define V9
//    #define V10
//    #define V11
    #define VTEST             /* checks the hardware and allows device testing (no encryption, no full mode)*/
//    #define VSHOW             /* doesn't initialize the eeprom and allows device testing (no encryption, no full mode)*/

/*  ------------------- firmware configuration ------------------- */
//    #define INTERNET_UPDATE   /* builds the internet firmware; it will run only if the eeprom was already initialized.*/
//    #define BOOTLOADER_4K     /* just changes the name of the cpu so crfsuite can get the proper update */
    #define VERSION   "7.15"    /* the firmware version */

/*  ------------------- start: device personalisation ------------------- */

#if 1
    #define USER_PASS               "1234"              /* default user password */
    #define ROOT_PASS               "456789"            /* root password */
    #define DEMO_PASS               "Fim2p1X2qKJdct5Z"  /* defaultDemoPass */
    #define BUILD                   "111111"
    #define AES_KEY                 "ThisIs128bitKey"  /* 16 bytes long: 5468697349733132386269744b657900 */
//    #define AES_KEY                 {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff} // ffffffffffffffffffffffffffffffff
#else
    #define USER_PASS               "USER"              /* default user password */
    #define ROOT_PASS               "[ROOT]"            /* root password */
    #define DEMO_PASS               "UpgradeKeyIsHere"  /* defaultDemoPass */
    #define BUILD                   "BUILD!"
    #define AES_KEY                 "123456789012345"
//    #define AES_KEY                 {0xab, 0x21, 0xc4, 0x94, 0x12, 0x02, 0x9f, 0x73, 0xf2, 0x65, 0x10, 0xb5, 0x2d, 0x9b, 0xbf, 0xb6} // ab21c49412029f73f26510b52d9bbfb6
#endif

    #define DEFAULT_PEERING         "0000"              /* bluetooth peering password */


/*  ------------------- start: device features  -------------------  */
#define EEPROM_GUARD_TEST           0x821c2cdb

#ifdef V6
    #define DEVICE_NAME             "MSRv006"
    #define READ_ONE_TRACK
    #define DISABLE_POWER_MANAGEMENT
    #define MAXSWIPES               2000
    #define EEPROM_GUARD            0xfd589575
#elif defined V7
    #define DEVICE_NAME             "MSRv007"
    #define INCLUDE_ENCRYPTION
    #define EEPROM_GUARD            0xf48474be
#elif defined V9
    #define DEVICE_NAME             "MSRv009"
    #define INCLUDE_ENCRYPTION
    #define EEPROM_GUARD            0x347c25b7
    #define VCC_LEDS
#elif defined V10
    #define DEVICE_NAME             "MSRv010"
    #define INCLUDE_ENCRYPTION
    #define EEPROM_GUARD            0x597b92cb
#elif defined V11
    #define DEVICE_NAME             "MSRv011"
    #define INCLUDE_ENCRYPTION
    #define WT41
    #define HAS_BLUETOOTH
    #define HAS_KEYPAD    
    #define VCC_LEDS
    #define EEPROM_GUARD            0xcb321a55
#elif defined VTEST
    /* test builds for factory testing.*/
    #define DEVICE_NAME             "MSRvTEST"
    #define MAXSWIPES               10
    #define EEPROM_GUARD            EEPROM_GUARD_TEST
    
    #define WT41                  // for v011 test builds
    #define HAS_BLUETOOTH         // for v011 test builds
    #define HAS_KEYPAD            // for v011 test builds    
    #define VCC_LEDS              // for v009 test builds
#elif defined VSHOW
    /* this will not initialize the eeprom. useful for viewing what is (was) in the eeprom of the device (eg. find the build number) */
    #define DEVICE_NAME             "MSRvSHOW"
    #define MAXSWIPES               10
    #define EEPROM_GUARD            0x2093a092
//    #define VCC_LEDS              // for v009 test builds
#else
    #error Unknown target device
#endif

/*  ------------------- start: debug parametters ------------------- */
//    #define PCB_1LED // the 1st PCB had one led. the 2nd PCB has two leds and controlable reset

//  #define DEBUG
//    #define DEBUG_INTERRUPTS  // enable this to be able to call printf() from interrupts
//    #define DEBUG_SWITCH_CLOCK
//    #define INCLUDE_ENCRYPTION // if this is defined, data will always be encrypted in DF
//    #define DEBUG_ASYNC_CALLS
//    #define DEBUG_OSCCAL
//    #define PROFILER // enables system clock but disables power management

/*  ------------------- start: hardware configuration ------------------- */
    #define XTAL 8000000
    #define EEPROM_GUARD_EMPTY (0xFFFFFFFF)

#if defined WT41
    #define COM_SPEED 250000
#else
    #define COM_SPEED 250000
#endif

    #define BOOTLOADER_FLAG_ADDR    5
    #define BOOTLOADER_FLAG_ENABLE  0xac
    #define BOOTLOADER_FLAG_DISABLE 0xff

    /* TIMER2_DIVIDER: supported: 8 and 128 */
    /* 8 = TCNT2 overflows 16 times per second; good for checking ASIC more often in case that INT1 fails, but it uses a little bit more power */
    /* 128 = TCNT2 overflows once per second; this should be the normal one. however I had a lot of problems with the custmers because INT1 failed */
    #define T2DIV 8
    #define CYCLES_PER_SECOND (32768/T2DIV)
    #define OVERFLOWS_PER_SECOND (CYCLES_PER_SECOND/256)

    #if (OVERFLOWS_PER_SECOND * 256 * T2DIV != 32768)
        #error cannot use this divider. please change T2DIV to use a lower value and divides properly
    #endif

    #define DDR_LED     DDRC    /* DDR where the LEDs are located */
    #define PORT_LED    PORTC   /* PORT where the LEDs are located */

#ifdef PCB_1LED
    #define LED_GREEN   5       /* the pin of the GREEN led */
    #define LED_RED     5       /* the pin of the RED led */
    #define PORTC_VALUE 0       /* there is no reset control on the first PCB */
#elif defined V11
    #define LED_GREEN   0       /* the pin of the GREEN led */
    #define LED_RED     1       /* the pin of the RED led */
    #define PORTC_VALUE 0       /* there is no reset control on V11 */
#else
    #define LED_GREEN   0       /* the pin of the GREEN led */
    #define LED_RED     1       /* the pin of the RED led */
    #define AVR_RESET   5       /* the pin where we control the reset */
    #define PORTC_VALUE (1<<AVR_RESET) /* AVR_RESET has to be a pull up resistor. If this ever goes down, the AVR resets */
#endif

/* define this if LEDS need VCC to be turned on - applicable on new boards */
#ifdef VCC_LEDS
    #define LED_ON(a)  {PORT_LED |=  (1<<(a));}
    #define LED_OFF(a) {PORT_LED &= ~(1<<(a));}
    #define LED_SWITCH(a) {PORT_LED ^= (1<<(a));}
#elif defined LEDS_OFF
    #define LED_ON(a)   
    #define LED_OFF(a) 
    #define LED_SWITCH(a) 
#else
    #define LED_ON(a)  {PORT_LED &= ~(1<<(a));}
    #define LED_OFF(a) {PORT_LED |=  (1<<(a));}
    #define LED_SWITCH(a) {PORT_LED ^= (1<<(a));}
#endif


    #define PIN_CONNECTOR  2

#if defined WT41
    #define serialConnected() ((PIND & (1<<PIN_CONNECTOR)) != 0)
#else
    /* while off, our internal pullup resistor will force the pin to 1
     * when USB is connected, the device will force this pin to GND    */
    #define serialConnected() ((PIND & (1<<PIN_CONNECTOR)) == 0)
#endif

/* ------------------- start: macros ------------------- */
    #define sbi(port,zbit) { port |=  (1<<(zbit)); }   //set bit in port
    #define cbi(port,zbit) { port &= ~(1<<(zbit)); }  // bit in port
    #define swb(port,zbit) { port ^=  (1<<(zbit));}
    #define cli()          { #asm("cli") }
    #define sei()          { #asm("sei") }
    #define wdt_reset()    { #asm("wdr") }

#ifdef DEBUG
    #define assert(__condition__, __v__)    \
    {                                       \
        if (!(__condition__))               \
        {                                   \
            printf("assert fail: " __FILE__ "@%i (" #__condition__ ")\n", __LINE__); \
            printf( #__v__ " = %i\n", __v__); \
            while(1)                        \
            {                               \
                LED_SWITCH(LED_RED);        \
                delay_ms(200);              \
                wdt_reset();                \
            }                               \
        }                                   \
    }
 #else
    #define assert(a)
#endif

#ifdef DEBUG_ASYNC_CALLS
    #define vChar(a) putchar(a)
#else
    #define vChar(a)
#endif

    #define BUF_SIZE        70          /* has to accomodate 32 bytes passwords => at least 64+3 (67) bytes are required. */

    typedef char bool;
    typedef unsigned long   uint32_t;
    typedef unsigned char   uint8_t;
    typedef unsigned int    uint16_t;
    typedef signed long     int32_t;
    typedef signed char     int8_t;
    typedef signed int      int16_t;

    enum
    {
        false = 0,
        true
    };

    typedef enum
    {
       Failure,
       Success
    }  Error;

#ifndef max
    #define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
    #define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#define wdt_start()                                     \
{                                                       \
    /* Watchdog Timer initialization        */          \
    /* Watchdog Timer Prescaler: OSC/2048k  */          \
    WDTCSR = (1<<WDCE | 1<<WDE);                        \
    WDTCSR = (1<<WDE | 1<< WDP2 | 1<<WDP1 | 1<<WDP0);   \
}

#define wdt_stop()                                      \
{                                                       \
    MCUSR &= ~(1<<WDRF);                                \
    /* Watchdog Timer initialization       */           \
    /* Watchdog Timer Prescaler: OSC/2048k */           \
    WDTCSR = (1<<WDCE | 1<<WDE);                        \
    WDTCSR = 0;                                         \
}

//#define DBUG(a) printf a
#define DBUG(a)

//#define DBUGR(a) printf a
//#define DBUGR(a) DBUG(a)
//#define TRACE() DBUGR((__FILE__ ":%i\n", __LINE__))

#ifdef DEBUG_OSCCAL
    #define oLog(a) printf a
#else
    #define oLog(a)
#endif

#endif
