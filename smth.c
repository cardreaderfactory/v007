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
#include <sleep.h>
#include <stdlib.h>
#include <delay.h>
#include "global.h"
#include "smth.h"
#include "dataflash.h"
#include "usart.h"
#include "reader.h"
#include "aes.h"
#include "rtc.h"
#include "profiler.h"
#include "unittest.h"
#include "bluetooth.h"
#include "keypad.h"

#ifdef _MEGA328P_INCLUDED_
#   ifdef BOOTLOADER_4K
#       define UC "3284"
#   else
#       define UC "328p"
#   endif
#elif defined _MEGA168_INCLUDED_
#   define UC "168v"
#else
#   error unknown microcontroller
#endif

/* ------------------------ menu functions prototypes  ------------------------ */

#ifdef PROFILER
    #define powerManagement() /* disable the power management */
#else
    void powerManagement(void);
#endif


/* ------------------------ menu functions prototypes  ------------------------ */

void menu(void);

enum
{
    /* Download params */
    DownloadBlock         = 128,        /* Download block size */

    Pass_MinimumLength    = 4,
    Pass_MaximumLength    = (PASS_BUFSIZE-1),
    Pass_TriesUntilLockup = 5,          /* how many wrong user passwords are allowed to be tried until lockup */
    Pass_LockupInterval   = 4,          /* hours */

    Demo_Limit            = 10,         /* how many swipes or minutes is allowed for demo device */
    Demo_LockupInterval   = 4,          /* hours */
    Demo_TriesUntilLockup = 5,          /* how many wrong demo unlock codes are allowed to be tried until lockup */

    Fuses_Mask             = 0x03       /* further programming and verification is disabled */
};

typedef flash struct
{
    unsigned char key;              // key to press and show on the screen
    unsigned char allow;
    unsigned char show;
    char flash *comment;            // description shown to the user
    char (*func)(char *);           // function to execute
} menu_t;

flash menu_t mainMenu[] = {
//  STATE   Allow           Show              Comment         Function
    {'l',    None,           None,            "LogIn",        logIn               },
    {'s',   (User | Root),  (User | Root),    "Statistics",   revision            },
    {'d',   (User | Root),  (User | Root),    "Dwn",          download            },
    {'e',   (User | Root),  (User | Root),    "Erase",        eraseMem            },
    {'p',   (User | Root),  (User | Root),    "Pass",         setPass             },
    {'k',   (       Root),  (       Root),    "setKey",       setKey              },
    {'t',   (User | Root),  (User | Root),    "Time",         setTime             },
    {'i',   (User | Root),  (User | Root),    "blInk",        switchLed           },
#ifdef HAS_BLUETOOTH
    {'b',   (User | Root),  (User | Root),    "Bluetooth",    setBluetooth        },
#endif
    {'$',   (User | Root),  (       Root),    "demo",         setDemo             },
    {'n',   (User | Root),  (User | Root),    "ReadMode",     changeReaderMode    },
    {'f',   (User | Root),  (       Root),    "update Fw",    enableBootLoader    },
#ifdef INCLUDE_VIEWMEM
    {'v',   (       Root),  (       Root),    "View mem",     viewMem             },
#endif
#ifdef DEBUG_MEM
    {'m',   (       Root),  (       Root),    "test Mem",     testMem             },
#endif
#if defined DEBUG_AES && defined INCLUDE_ENCRYPTION
    {'a',   (       Root),  (       Root),    "test Aes",     test_aes             },
#endif
#ifdef DEBUG_PORTS
    {'o',   (       Root),  (       Root),    "pOrts",        portStats           },
#endif
#ifdef PROFILER
    {'1',   (User | Root),  (User | Root),    "showStats",    profiler_showReport },
#endif
    {'r',   (User | Root),  (User | Root),    "Reboot",        reboot              },
    {'x',   (User | Root),  (User | Root),    "eXit",          logOut              },
    {0,     0,              0,                NULL,            NULL}
};



// -------------------- global variables -------------------- //


char     buf[BUF_SIZE];                 /* temporary buffer */
char     userLevel     = Access_None;    /* user access level */
bool     intRtc        = false;          /* set true by RTC interrupt */
bool     intExternal   = false;          /* set true by external interrupts */
bool     slowWakeUp    = false;          /* set true if the controler is not waking up in 0ms from sleep */
bool     flush_pending = false;          /* set true when the temporary memory flush is requested (save to dataflash) */
bool     stayAwake     = false;          /* keeps the microcontroller awake */
bool     isDemo        = true;           /* ram copy of eIsDemo */
uint8_t  useLed        = 0xff;


uint16_t hardwareFailure = 0;

flash  char     deviceName[]           = DEVICE_NAME;
flash  uint8_t  demoSwipes             = Demo_Limit;
flash  uint16_t demoSeconds            = Demo_Limit * 60;

/* address 0x00 is not used as it might get corrupt */
eeprom uint32_t eGuard                      @ 0x01;
eeprom uint8_t  bootLoader                  @ BOOTLOADER_FLAG_ADDR; /* this has to be at this address in order for the bootloader to work! */
eeprom bool     eIsDemo                     @ 0x06;
eeprom bool     readerMode                  @ 0x07;
eeprom uint32_t eLifeSwipes                 @ 0x08;
eeprom uint32_t eTotalSwipes                @ 0x0c;
eeprom uint8_t  failedLogins                @ 0x10;
eeprom uint8_t  failedDemoTries             @ 0x11;
eeprom char     serialNumber[sizeof(BUILD)] @ 0x12;
eeprom char     demoPass[PASS_BUFSIZE]      @ 0x19;
eeprom char     passwd[Access_Root][PASS_BUFSIZE] @ 0x3a;
// note: BLUETOOTH is using 2 * PASS_BUFSIZE: from [0x7c, 0xbe) - see bluetooth.c
eeprom char     cypherMode                  @ 0xbe;
eeprom uint8_t  key[AES_BLOCKSIZE]          @ 0xbf;
eeprom uint16_t eLock                       @ 0xcf;
eeprom uint8_t  eUseLed                     @ 0xd1;
// note: KEYPAD is using 1 byte at 0xd2 - see keypad.c




eeprom uint32_t eGuard                 = EEPROM_GUARD;
eeprom uint8_t  bootLoader             = BOOTLOADER_FLAG_DISABLE;
eeprom bool     eIsDemo                = true;
eeprom bool     readerMode             = true;
eeprom uint32_t eLifeSwipes            = 0;       /* eeprom: all swipes read by this device */
eeprom uint32_t eTotalSwipes           = 0;       /* eeprom: total swipes since erased */
eeprom uint8_t  failedLogins           = 0;
eeprom uint8_t  failedDemoTries        = 0;
eeprom char     serialNumber[sizeof(BUILD)]        = BUILD;
eeprom char     demoPass[PASS_BUFSIZE]             = DEMO_PASS;
eeprom char     passwd[Access_Root][PASS_BUFSIZE]  = { USER_PASS, ROOT_PASS };
eeprom char     cypherMode                         = ECB;
eeprom uint8_t  key[AES_BLOCKSIZE]                 = AES_KEY;
eeprom uint16_t eLock;
eeprom uint8_t  eUseLed;


/** We work with variables in RAM and EEPROM as we try to
 *  preserve the eeprom by writing fewer times in it. We do this
 *  through late updating the eeprom variables on the clock
 *  interrupt */
uint32_t   lifeSwipes = 0;      /* all swipes read by this device */
uint32_t   totalSwipes = 0;     /* total swipes since erased */

interrupt [EXT_INT0] void ext_int0_isr(void)
{
//    int0++;
    intExternal = true;
    EIMSK &= ~(1<<INT0); /* prevent further calls until this is processed */
}

interrupt [EXT_INT1] void ext_int1_isr(void)
{
//    int1++;
    intExternal = true;
    EIMSK &= ~(1<<INT1); /* prevent further calls until this is processed */
}

#pragma warn-
/* atmega168v: does not implement this function */
/* atmega328p: 0x1e, 0x95, 0x0f */
uint8_t getSignature(uint8_t flag)
{
#asm
    in r22, SREG
    cli
    //step1: set Z to the specified param (flag)
    ldi  r31, 0
    ld   r30, y         // load in r30 the last argument of the function (flag)
    //step2: set SPMCSR
    ldi  R23,LOW(33)    // SPMCSR = (1<<SIGRD) | (1<<SPMEN);
    out  0x37,R23       // SPMCSR = (1<<SIGRD) | (1<<SPMEN);
    //step3: read the signature
    LPM  R30,Z          // in CVAVR the functions return their values in R30 for uint8_t
    out SREG, r22
#endasm
}
#pragma warn+

#pragma warn-
/**
 * fuse == 0 => fuse low
 * fuse == 1 => lock bits
 * fuse == 2 => fuse extended (EFB)
 * fuse == 3 => fuse high
 *
 * @return uint8_t fuse value
 */
uint8_t getFuse(uint8_t fuse)
{
#asm
    in r22, SREG
    cli
    //step1: set Z to the specified param (flag)
    ldi  r31, 0
    ld   r30, y         // load in r30 the last argument of the function (fuse)
    //step2: set SPMCSR
    ldi  R23,LOW(9)     // SPMCSR = (1<<BLBSET) | (1<<SELFPRGEN); SPMCSR = ( 1<<3 | 1<<0 );
    out  0x37,R23       // SPMCSR = (1<<BLBSET) | (1<<SELFPRGEN);
    //step3: read the signature
    LPM  R30,Z          // in CVAVR the functions return their values in R30 for uint8_t
    out SREG, r22
#endasm
}
#pragma warn+

void eeprom_flush(void)
{
    /* write these variables later into eeprom to preserve the eeprom as we can only write it 100000 times */
    if (totalSwipes != eTotalSwipes)
        eTotalSwipes = totalSwipes;
    if (lifeSwipes != eLifeSwipes)
        eLifeSwipes = lifeSwipes;
#ifdef HAS_KEYPAD
    if (totalKeys != eTotalKeys)
        eTotalKeys = totalKeys;
#endif /* HAS_KEYPAD */                
}

void wrong_firmware()
{
    bootLoader = BOOTLOADER_FLAG_ENABLE;
    //printf("%04x%04x,%02x,%p\n", (uint16_t)(eGuard>>16), (uint16_t)eGuard, isDemo, UC);
    ledFail(Fail_Eeprom);
    reboot(NULL);
}

#if !(defined VSHOW) && !(defined INTERNET_UPDATE)
void eeprom_init_real()
{
    eIsDemo = true;
    readerMode = true;
    eLifeSwipes = 0;
    eTotalSwipes = 0;
    failedLogins = 0;
    failedDemoTries = 0;
    strcpy_ef(serialNumber, BUILD);
    strcpy_ef(demoPass, DEMO_PASS);
    strcpy_ef(passwd[0],USER_PASS);
    strcpy_ef(passwd[1],ROOT_PASS);
    bootLoader = BOOTLOADER_FLAG_DISABLE;
    eUseLed = 0xff;

#ifdef HAS_BLUETOOTH
    {
        int i;
        i = sprintf(buf, "%p Build ", deviceName);
        strcpy_re(buf+i, serialNumber);
    }

    strcpy_er(btName, buf);
    strcpy_ef(btPeer, DEFAULT_PEERING);
#endif

#ifdef HAS_KEYPAD
    eTotalKeys = 0;
#endif
}
#endif

void eeprom_init()
{
#if defined (VSHOW) /* this will not touch the eeprom. useful for showing what was in the eeprom of the device. */

    if (eGuard != EEPROM_GUARD)
    {
        /* we need to initialize these or we might not be able to log in */
        failedLogins = 0;
        failedDemoTries = 0;
        strcpy_ef(passwd[0],USER_PASS);
        eGuard = EEPROM_GUARD;
    }

#elif defined (VTEST)

    if (eGuard != EEPROM_GUARD)
    {
        eeprom_init_real();
        eGuard = EEPROM_GUARD;
    }

#elif defined (INTERNET_UPDATE)

    /* internet updates do not initialize the eeprom. If the eeprom contains something different we will get back into bootloader mode */
    /** EEPROM GUARD was used to initialize all versions before 7.0.
     *
     * We do not support upgrades from before 7.0 in the generic
     * version therefore eGuard has to be EEPROM_GUARD+1 which is
     * the value used for v7.0+
     *
     */
    if (eGuard != EEPROM_GUARD + 1)
        wrong_firmware();

#else /* build specific firmware */

    switch(eGuard)
    {
    case EEPROM_GUARD_EMPTY:
    case EEPROM_GUARD_TEST: /* test versions are treated as new devices (EMPTY) */
        eeprom_init_real();
        /* we have to fallthrough as we need to initialize the key, cypher mode and eGuard too. */

    case EEPROM_GUARD:
        /* on upgrade, we need to make sure that we're not writing some other key on the wrong build. */
        if (!strcmp_ef(serialNumber,BUILD))
            wrong_firmware();

        /** EEPROM GUARD was used to initialize all versions before 7.0.
         *
         * Because these versions did not include the key and cyphermode
         * in the eeprom, we need to write them now. We will also have
         * to change to EEPROM_GUARD+1 in order not to know that we've
         * upgraded.
         */

        cypherMode = ECB;
        memcpy_ef(key, AES_KEY, AES_BLOCKSIZE);
        eGuard = EEPROM_GUARD + 1;
        break;

    case EEPROM_GUARD + 1:
        /* already initialized; nothing to do */
        break;

    default:
        wrong_firmware();
        break;
    }

#endif /* build specific firmware */
}

bool strcmp_re(const char *s1, eeprom const char *s2)
{
    while (*s1 != '\0' && *s1 == *s2) {
        s1++;
        s2++;
    }

    return (*s1 == *s2);
}

bool strcmp_ef(eeprom char *s1, flash const char *s2)
{
    while (*s1 != '\0' && *s1 == *s2) {
        s1++;
        s2++;
    }

    return(*s1 == *s2);
}


void strcpy_er(eeprom char *s1, const char *s2)
{
    /* Do the copying in a loop.  */
    while ((*s1++ = *s2++) != '\0')
        ;               /* The body of this loop is left empty. */
}

void strcpy_ef(eeprom char *s1, flash char *s2)
{
    /* Do the copying in a loop.  */
    while ((*s1++ = *s2++) != '\0')
        ;               /* The body of this loop is left empty. */
}

void strcpy_re(char *s1, eeprom char *s2)
{
    /* Do the copying in a loop.  */
    while ((*s1++ = *s2++) != '\0')
        ;               /* The body of this loop is left empty. */
}

void memcpy_ef(eeprom uint8_t *s1, flash uint8_t *s2, uint8_t size)
{
    for (; size != 0; size--)
        *s1++ = *s2++;
}

void print_eeprom_string(eeprom char* st)
{
    while (*st != 0)
    {
        putchar(*st);
        st++;
    }
}


void init(void)
{
    int i;

/* not required as CVAVR initializes everything to 0. I have tested this. */
//    PORTC   = 0x00;        DDRC   = 0x00;
//    PORTB   = 0x00;        DDRB   = 0x00;
//    PORTD   = 0x00;        DDRD   = 0x00;
//    memset(buf, 0, sizeof(buf));
    PORTC   = PORTC_VALUE;

    /* setup leds */
    DDR_LED |= (1<<LED_GREEN | 1<<LED_RED);
#ifndef VCC_LEDS
    PORT_LED |= (1<<LED_GREEN | 1<<LED_RED);
#endif
    df_inactive();

    CLKPR = (1<<CLKPCE);  // set Clock Prescaler Change Enable
    CLKPR =  0;           // clock divider 1 => XTAL = 8mhz

    sei();

    // Analog Comparator initialization
    // Analog Comparator: Off
    // Analog Comparator Input Capture by Timer/Counter 1: Off
    ACSR    =   0x80;
    ADCSRB  =   0x00;
    PRR     = (1<<PRTWI | 1<<PRTIM0 | 1<<PRTIM1 | 1<<PRADC ); //shutdown everything except TIM2, SPI and USART0

    rtc_init();
    profiler_startTimer();

    /* setup dataflash reset pin */
    DDRB  |=  (1<<PinDfReset);
    PORTB &= ~(1<<PinDfReset);

    /* serial port works only when INT0 is gnd */
    DDRD   &= ~( 1 << PIN_CONNECTOR );
#ifdef HAS_BLUETOOTH
    changeBtConfig = true;
#else    
    PORTD  |=  ( 1 << PIN_CONNECTOR ); //resistive load on INT0 (usb connect)
#endif

    usart_init(COM_SPEED);

    profiler_breakPoint("usart");

    wdt_start();
    eeprom_init();

    useLed = eUseLed;
    isDemo = eIsDemo;
    totalSwipes = eTotalSwipes;
    lifeSwipes = eLifeSwipes;
#ifdef HAS_KEYPAD    
    totalKeys = eTotalKeys;
#endif    

    profiler_breakPoint("eeprom");

#ifdef INCLUDE_ENCRYPTION
    aes_init();  // initialize the buffers for the current key
#endif

    profiler_breakPoint("aes");

#if defined WT41
    EICRA |=  ( 1<<ISC01 ); /* The rising edge of INT0 generates an interrupt request. */
    EIFR = ( 1<<INTF0 ); /* clear external interrupt flag register. note: in order to clear the flag we need to set the bit */
    EIMSK |= ( 1<<INT0 ); /* activate External interrupt for INT0 */
#else
    /* int0 intialization - connector */
    /* We are using low level interrupt as atmega168v won't wake up from anything else.
     * Note: in this mode, an interrupt is generated every time the corresponding pin is set to 0.
     * This means that if we keep the external interrupt enabled (EIMSK) and the pin
     * is 0 when the UC finishes executing the interrupt code, the interrupt code will
     * be called again, resulting in a continuous loop of the interrupt until the pin
     * is released. For this reason we need to disable the interrupt until it gets back to 1 */
    EICRA &=  ~( 1<<ISC00 | 1<<ISC01 ); /* low level interrupt; atmega168v won't wake up from anything else. */
    EIFR = ( 1<<INTF0 ); /* clear external interrupt flag register. note: in order to clear the flag we need to set the bit */
    EIMSK |= ( 1<<INT0 ); /* activate External interrupt for INT0 */
#endif

    reader_init(readerMode);
    profiler_breakPoint("reader");

    userLevel = Access_None;

    /* as dataflash fiddles with the programming pins, we have to access it as late as possible
     * for this reason we are initializing it at the end of startup.
     * solution 1: 65ms delay provided by the boot fuses is enough.
     * solution 2: I've noticed that reader_init() provides enough delay and we can use 0ms fuse delay
     **/

#if 1
    // initialising memory   
    for (i = 0; i < 3; i++)
    {
        if (df_init())
            break;
        delay_ms(10);
        wdt_reset(); /* reset watchdog */ 

        //        printf("Memory initialisation failed. Retrying...\n");
    }

    if (df_totalMemory == 0)
        ledFail(Fail_Memory);

#endif    
    

    keypad_init();

    profiler_breakPoint("memory");
    
    if (!hardwareFailure && (useLed & Blink_Startup))
        ledOk(Led_PowerUp);

    slowWakeUp = (getFuse(0) != Fuse_0ms);
}

void checkLock(void)
{
    uint16_t i;
    uint16_t lock;
    uint8_t  cnt = 0;

#if !defined VTEST && !defined VSHOW
    if ((getFuse(1) & Fuses_Mask) != 0)
    {
        delay_ms(1000);
        while(1)
        {
            printf("Fuse\n");
            blink(LED_RED, 3, 200); // delay = (3*2-1)*200 = 1000ms
        }
    }
#endif

    if (failedLogins < Pass_TriesUntilLockup &&
        failedDemoTries < Demo_TriesUntilLockup)
        return;

    lock = eLock;
    for (i = 0; i < lock; i++)
    {
//        printf("i = %x, eLock = %x\n", i, eLock);
        while (usart_gotChar())
        {
            if (getchar() == '#')
                cnt++;
            else
                cnt = 0;
            if (cnt == 100)
                revision("");
        }
        printf("Brute force %u/%u\n", i, lock);
        blink(LED_RED, 3, 200); /* delay = (3*2-1)*200 = 1000ms */

        if ( (i & 0xff) == 0xff) /* save the progress every 256 seconds */
        {
            eLock -= 0x100;
//            printf("i = %x, eLock = %x [s]\n", i, eLock);
        }
    }

    /* no need to save eLock as it won't be considered after reset of failedLogins and failedDemoTries */

    if (failedLogins >= Pass_TriesUntilLockup)
        failedLogins = 0;
    if (failedDemoTries >= Demo_TriesUntilLockup)
        failedDemoTries = 0;
}

void main(void)
{
    /* initializing the local variables in the declaration, increases the code size */
    bool     clockChecked = false;
    uint32_t loops = 0;

    init();
#ifdef WT41
    if (serialConnected())
#endif
        revision(NULL);
    unitTests();
//#warning checkLock() is disabled    
    checkLock();
#ifdef DEBUG_PORTS
    portStats(NULL);
#endif

    while (1)
    {
        // reader; the pin change interrupt wakes us up as soon as we need to call this function
        reader_main(); /* read but do not display the card to usart */
#ifdef HAS_KEYPAD        
        keypad_main();
#endif        
        
        if (flush_pending)
        {
#ifdef _MEGA328P_INCLUDED_
            if (rtcState == OnT2)
                switchT2toT1();
#endif
            flush();
            eeprom_flush();
            flush_pending = false;
        }

        menu();
        wdt_reset(); /* reset watchdog */

        if(!clockChecked)
        {
            if (loops < 80000 && timestamp == 0 )
            {
                loops++;
            }
            else
            {
//                printf("timestamp = %lu\n", timestamp);
//                printf("calibrateOsccal = %lu\n", calibrateOsccal());
                if (timestamp == 0 || !calibrateOsccal())
                     ledFail(Fail_Clock);

#ifdef BLUETOOTH
                if (!checkBlueTooth())
                    ledFail(Fail_Bluetooth);
#endif                    

               clockChecked = true;
#ifdef _MEGA328P_INCLUDED_
                switchT2toT1();
                checkClock();
#endif
//                printf("loops = %lu, timestamp = %lu \n", loops, timestamp);
            }
        }
        else
        {
            updateBlueTooth(); /* called here to ensure that at least 1second has passed since boot. */
#ifndef DISABLE_POWER_MANAGEMENT
            powerManagement();
#endif
        }
    }
}


void showMenu(void)
{
    char i;
    for (i = 0; mainMenu[i].key != 0; i++)
    {
        if ( (mainMenu[i].allow & (1<<userLevel)) == 0 ||
             (mainMenu[i].show  & (1<<userLevel)) == 0)
            continue;

        printf("%c) %p\n",mainMenu[i].key, mainMenu[i].comment);
    }
}

void menu(void)
{

    char i, c;

#ifdef WT41
    if (!serialConnected())
    {
        if (usart_gotChar())
            getchar();
        return;
    }
#endif

    if (!usart_gotChar())
        return;

    c = getchar();
    if (c==13)
    {
        showMenu();
        return;
    }
    memset(buf, 0, sizeof(buf));
    myscanf(buf, sizeof(buf));
//    printf("got [%s]\n", buf);
    if (c != 'm')
        return;

    for (i = 0; mainMenu[i].key != 0; i++)
    {
        if ( (mainMenu[i].allow & (1<<userLevel)) == 0 ||
              mainMenu[i].key != buf[0]
           )
            continue;

//        printf("running %p\n",mainMenu[i].comment);

#ifdef _MEGA328P_INCLUDED_
        if (rtcState == OnT2)
            switchT2toT1();
#endif

        c = (*mainMenu[i].func)(&buf[1]);
        memset(buf, 0, sizeof(buf));
        printf("return=%i\n", c);
        break;
    }

}

bool logIn(char *st)
{
    signed char i;

    for (i = Access_Root - 1; i >= 0 ; i--)
    {
        if (strcmp_re(st, passwd[i]))
            userLevel = i + 1;
    }

    if (userLevel == Access_None)
    {
        failedLogins++;

        if (failedLogins >= Pass_TriesUntilLockup)
        {
            eLock = Pass_LockupInterval * 60 * 60;
            eraseMem("Y");
            DBUG(("brute force detected!\n"));
            while(1)
                wdt_reset(); /* lock the device in order to require physical power cut off to restart */
        }
        return false;
    }
    else
    {
        if (failedLogins != 0)
            failedLogins = 0;
        return true;
    }
}

/* pass NULL for force reboot */
bool reboot(char *st)
{
#if 1
#	ifndef VTEST
    if (isDemo &&                            /* prevent reboot on demo devices, as the user was able to circumvent the time limit by rebooting the device */
        st != NULL &&                        /* but still allow reboot if we programatically call this function with (NULL) param */
        bootLoader != BOOTLOADER_FLAG_ENABLE /* and allow reboot if the user requested firmware update */
        )
        return false;
#	endif
#else
#	warning reboot in demo is enabled. this is unsafe for BT.
#endif

    // Watchdog Timer initialization
    // Watchdog Timer Prescaler: OSC/2k
    WDTCSR = (1<<WDCE) | (1<<WDE);
    WDTCSR = (1<<WDE);

    /* wait for watchdog to reboot */
    while (1)
    {
        #asm("nop");
    }

    return true;
}

bool logOut(char *st)
{
    userLevel = Access_None;
    return true;
}

char *parsePassword(char *st)
{
    char i;

    if (NULL == st)
        return NULL;

    for (i = 0; st[i] != 0; i++)
        if (st[i] == '#')
        {
            st[i] = 0;
            break;
        }

    i = strlen(st);
    if (i < Pass_MinimumLength || i > Pass_MaximumLength)
    {
        DBUG(("Error: passwd len=%i, min=%i, max=%i\n", i, Pass_MinimumLength, Pass_MaximumLength));
        return NULL;
    }

    return st;
}

bool setDemo(char *st)
{
    st = parsePassword(st);
    if (NULL == st)
        return false;

    if (isDemo)
    {
        if (strcmp_re(st, demoPass))
        {
            failedDemoTries = 0;
            eIsDemo = false;
            isDemo = false;
            return true;
        }
        else
        {
            failedDemoTries++;
            if (failedDemoTries >= Demo_TriesUntilLockup)
            {
                eLock = Demo_LockupInterval * 60 * 60;
                DBUG(("brute force detected!\n"));
                while(1)
                    wdt_reset(); /* lock the device in order to require physical power cut off to restart */
            }
            return false;
        }
    }
    else if (userLevel == Access_Root)
    {
        if (strcmp_re(st, demoPass))
        {
            failedDemoTries = 0;
            eIsDemo = true;
            isDemo = true;
            return true;
        }
        return false;
    }
    else
    {
        return false;
    }
}

bool changeReaderMode(char *st)
{
    readerMode = (!readerMode);
    reader_setNewMode(readerMode);
    return 1;
}

bool enableBootLoader(char *st)
{
    if (st[0] == 'y' && st[1] == 'k' && st[2] == 0)
    {
        bootLoader = BOOTLOADER_FLAG_ENABLE;
        return 1;
    }
    return 0;
}


bool setPass(char *st)
{
    st = parsePassword(st);
    if (NULL == st)
        return false;

    strcpy_er(passwd[userLevel - 1],st);

    return true;
}

/**
 * converts a char to int.
 * assumes that valid value has been passed in.
 *
 * @param ch
 *
 * @return uint8_t
 */
uint8_t c2i(uint8_t ch)
{
    if(ch >= '0' && ch <= '9')
        return (ch - '0');
    else /* if (st[j] >= 'a' && st[j] <= 'f') */
        return (10 + ch - 'a');
//    else
//        return -1;
}


bool setKey(char *st)
{
    uint8_t len, i;
    char *s;

    if (NULL == st)
        return false;

    len = strlen(st);

    if (len != 1 + AES_BLOCKSIZE * 2)
    {
        DBUG(("Error: Incorrect length. current=%i, expected=%i\n", len, AES_BLOCKSIZE * 2));
        return false;
    }

    for (s = st; *s != 0; s++)
        if( (*s < '0' || *s > '9') && (*s < 'a' || *s > 'f'))
        {
            DBUG(("Error: incorrect char@%i = [%c]\n", s-st, *s));
            return false;
        }

    /* ensure that there is no data pending. clear the aes buffer first. */
    aes_flush();
    if (df_currentByte != 0)
        return false;

    s = st;
    for (i = 0; i < AES_BLOCKSIZE; i++)
        key[i] = (c2i(*s++) << 4) + c2i(*s++);

    if (*s == '1')
        cypherMode = CBC;
    else
        cypherMode = ECB;

//    printf("cypher=%i, key=", cypherMode);
//    for (i = 0; i < AES_BLOCKSIZE; i++)
//        printf("%02x", key[i]);
//    putchar('\n');

    aes_init();
    initChainBlock();
    strcpy_ef(passwd[0],USER_PASS);

    DBUG(("setKey: success\n"));
    return true;
}


bool setTime(char *st)
{
    if (st[0] == 0)
        return false;

    timestamp = atol(st);
    TCNT2=0;
    DBUG(("time set to %lu\n", timestamp));
    return true;
}

bool switchLed(char *st)
{
    eUseLed = (uint8_t)atol(st);
    useLed = eUseLed;
    return true;
}

bool revision(char *st)
{
//    printf("int0 = %lu, int1 = %lu\nEIMSK=", int0, int1);
//    putBin(EIMSK);
//    putchar('\n');
    char i;

    printf("name=%p", deviceName);
    if(isDemo)
        printf("_demo");
    putchar(',');

    if (st == NULL)
    {
        /* boot message */
//        printf("mem=%lu,hw=%u\n", df.total_memory, hardwareFailure);
        printf("hw=%u\n", hardwareFailure);
        return true;
    }

    printf("build=");
    print_eeprom_string(serialNumber);
    printf(",total=%lu,used=%lu,free=%lu,current=%lu,all=%lu,time=%lu,rmode=%u,fw=" VERSION ",hw=%u,led=%u",
           df_totalMemory,
           df_usedMemory,
           df_totalMemory - df_usedMemory,
           totalSwipes,
           lifeSwipes,
           timestamp,
           readerMode,
           hardwareFailure,
           useLed);

#ifdef HAS_BLUETOOTH
    printf(",peer=");
    print_eeprom_string(btPeer);
    printf(",bt=");
    print_eeprom_string(btName);
#endif
#ifdef HAS_KEYPAD
    printf(",keys=%u", totalKeys);
#endif
    printf(",uc=" UC ",fs=");
    for(i = 0; i < 4; i++)
        printf("%02x", getFuse(i));
    putchar('\n');

//    printf("rtcState = %u\n", rtcState);

    //printf("countTimer1(%i) = %u, expected = %u\n", OSCCAL, countTimer1(OSCCAL), T1VALUE);

#if 0
    {
        int i;
        printf("keyValidated=%i,key=",keyValidated);
        for (i = 0; i < Aes_BlockSize; i++)
            printf("%02x", key[i]);
        putchar('\n');
    }
#endif

    return true;
}

// st = string to write in,
// max len allowed
void myscanf(char *st, char len)
{
    char ch, i;

    i = 0;
//    DBUG(("start: len=%i, strlen(st)=%i, st=[%s] strlen(st)=%i\n",len, strlen(st),));
    if (len <= 1)
        return; /* need to fit more than \0 inside st in order for this function to work */

    len--; /* reserve space for \0 */

    do
    {
        uint32_t start;
        start = timestamp;
        while ( !usart_gotChar() )
        {
            if ( !serialConnected() || (timestamp - start) > 4)
            {
                st[i] = 0;
                return;
            }
            wdt_reset(); /* reset watchdog */
        }

        ch = getchar();
        switch (ch) {
        case 8:
            //printf("case 8\n");
                if (i==0)
                    break;
                 i--;
                 st[i]=0;
                 break;

            case 13:
                //printf("case 13\n");
                st[i]=0;
                break;

            default:
                if ( i >= len)
                    break;
                //printf("default\n");
                st[i]=ch;
                i++;
                break;
        }
    }
    while (ch != 13);
//    DBUG(("myscanf: returning [%s]\n",st));
}

bool eraseMem(char *st)
{
    if (st == NULL || (st[0] != 0 && st[0] != 'Y'))
    {
//        printf("memory not erased!\n");
        return false;
    }

    df_erase(st[0] == 'Y');
    putchar('\n');
    totalSwipes=0;
#ifdef HAS_KEYPAD    
    totalKeys=0;
#endif    
    eeprom_flush();
    return true;
}

void blink(char led, char count, char delay)
{
    char i;
    for (i = 0; i < count; i++)
    {
        #asm("wdr"); /* reset watchdog */
        LED_ON(led);
        delay_ms(delay);
        LED_OFF(led);
        if (i + 1 < count)
            delay_ms(delay);
    }

}


void ledFail(char code)
{
    hardwareFailure |= code;
//    printf("Err=%i\n", code);
    blink(LED_RED, code, 100);
}

void ledOk(char code)
{
//  printf("led(%i)\n", code);
    blink(LED_GREEN, code, 25);
}

// CRC computing ##############################################################
/* CRC is computed using the self-made function presented below */
#pragma warn-
//-----------------------------------------------------------------------------
unsigned int crcUpdate(unsigned int crc, unsigned char serialData)
//-----------------------------------------------------------------------------
{   // Please, DO NOT MODIFY ! Any change may cause severe malfunction !
#asm
    ldd  r30, y+2
    ldd  r31, y+1
    ld   r27, y
    eor  r30, r27
    mov  r26, r30
    swap r26
    andi r26, 0x0f
    eor  r30, r26
    mov  r26, r30
    swap r26
    andi r26, 0xf0
    eor  r31, r26
    mov  r26, r30
    swap r26
    andi r26, 0xf0
    lsl  r26
    mov  r27, r30
    lsr  r27
    lsr  r27
    lsr  r27
    eor  r31, r27
    eor  r30, r26
#endasm
}
#pragma warn+

#ifndef PROFILER

void powerManagement(void)
{
    if (serialConnected() || READER_HAS_DATA() || flush_pending || stayAwake || (hardwareFailure & Fail_Clock))
        return;

#ifdef HAS_KEYPAD        
    if (timestamp2() < kb_timestamp)
        return;
#endif        

#ifdef _MEGA328P_INCLUDED_
    if (rtcState == OnT1)
        switchT1toT2();
#endif

//    LED_ON(LED_RED);

    df_powerDown();
    wdt_stop();

    do
    {
        uint8_t flags = SREG;  // save global interrupts

#ifdef _MEGA328P_INCLUDED_
        /* there's no need for 31us delay on ATMEGA328P as we are disabling BOD while sleeping.
         * this causes a delay of 60us when resuming from sleep */
#else
        delay_us(31); /* wait for the next TOSC 1/32768 = 30.5us */
#endif

        intExternal = false;
        intRtc = false;

        cli();       /* need to disable to prevent a call of int0 or int1 which could disable the external interrupts before going to sleep resulting in a permanent sleep */
        /* WT41 and serial are providing interrupts */
        EIMSK |= (1<<INT0 | 1<<INT1); // enable pin change interrupts on INT0 & INT1


#ifdef _MEGA328P_INCLUDED_
        {
            char tmp1;
            /* enable sleep in power save mode;
               currently running: clk asy, timer osc, int0, int1, twi, timer2, wdt */
            SMCR |= (1<<SE) | (1<<SM1) | (1<<SM0);

            /* disable brown out detection in sleep */
            tmp1 = MCUCR | (1<<BODS) | (1<<BODSE);
            MCUCR = tmp1;
            MCUCR = tmp1 & (~(1<<BODSE));
            /* going to sleep */
            #asm
                sei
                sleep
            #endasm
        }
#else
        sleep_enable();
        powersave(); /* currently running: clk asy, timer osc, int0, int1, twi, timer2, wdt */
#endif
        SREG = flags; // restore global interrupts as they were
    }
    while (!intExternal &&
//#ifdef HAS_BLUETOOTH // we don't really need this
//           !changeBtConfig &&
//#endif
           !serialConnected() &&
           !READER_HAS_DATA() &&
#ifdef HAS_KEYPAD
            kb_timestamp == 0 &&
#endif                       
           !flush_pending);

    sleep_disable();
    wdt_start();
    df_powerUp();

//    LED_OFF(LED_RED);
}
#endif

static void printLong(uint32_t value)
{
    putchar(((char *)(&value))[3]);
    putchar(((char *)(&value))[2]);
    putchar(((char *)(&value))[1]);
    putchar(((char *)(&value))[0]);
}

static void printCrc(uint16_t crc)
{
      putchar('c');
      putchar('r');
      putchar( ((char*)(&crc))[1] );
      putchar( ((char*)(&crc))[0] );
//    printf("crc");
//    putchar((char)(crc >> 8));
//    putchar((char)(crc));
//    putchar('\n');
//    printf("crc=%u %u %u\n",crc,(char*)(&crc)[0],((char*)(&crc))[1]);
}

bool download(char *st)
{
    unsigned int    crc;
    uint8_t         byte;
    uint32_t        i;
    uint32_t        toDownload;
    uint8_t         recv;

    recv = 0;

    if ((st != NULL) && (st[0] == 'A'))
        toDownload = df_totalMemory;
    else
        toDownload = df_usedMemory;

    df_enableContinuousRead(0);

//    printf("total=%lu,block=%u\n", usedMemory, DownloadBlock);
    putchar('d');
    putchar('l');
    printLong(toDownload);
    printLong(DownloadBlock);
    crc = 0;
    for ( i = 0 ; i < toDownload; i++)
    {
        byte = rwSpi(0x00);
        crc = crcUpdate(crc, byte);
        putchar(byte);

        if (i % DownloadBlock == DownloadBlock -1)
        {
            #asm("wdr"); /* reset watchdog */
            printCrc(crc);
            crc = 0;

            /* interactive stuff */
            if (usart_gotChar())
            {
                while (usart_gotChar())
                {
                    char ch;
                    ch = getchar();
                    if (ch == 8)
                    {
                        memset(buf, 0, sizeof(buf));
                        recv = 0;
                    }
                    else if (recv < 6)
                    {
                        buf[recv] = ch;
                        recv++;
                    }
                }

                if (recv == 6 && buf[0] == 'r' && buf[1] == 'e')
                {
                    printf("retry");
                    i = *((uint32_t*)&buf[2]);
                    if (i >= df_totalMemory)
                        goto cleanup;
                    i = (i / DownloadBlock) * DownloadBlock;
                    printLong(i);
                    df_enableContinuousRead(i);

                    recv = 0;
                    /* we need to send one byte here because the next
                       instruction in for() is i++ */
                    byte = rwSpi(0x00);
                    crc = crcUpdate(crc, byte);
                    putchar(byte);

                }
                else if (recv >= 2 && buf[0] == 'a' && buf[1] == 'b')
                {
                    goto cleanup;
                }

            }
        }
    }

    if ( i % DownloadBlock != 0 )
        printCrc(crc);

cleanup:
    df_inactive();
    return true;
}

