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

#ifndef SMTH_H
#define SMTH_H
// smth.c

#define PASS_BUFSIZE     33         /* buffer for the password */

enum
{
    Led_StandBy     = 0x01,
    Led_CardRead    = 0x01,
    Led_PowerUp     = 0x02,
    Led_PowerDown   = 0x05,
    Led_Demo        = 0x05,

    /* note: if 0xff is exceeded, you need to enlarge hardwareFailure to int32_t */
    Fail_Clock      = 0x01,
    Fail_Asic       = 0x02,
    Fail_Asic2      = 0x04,
    Fail_Memory     = 0x08,
    Fail_Bluetooth  = 0x10,
    Fail_Eeprom     = 0x20,

    Int_Rtc         = 0,
    Int_0,
    Int_1,

    ECB             = 0,
    CBC             = 1,

    Fuse_65ms       = 0xe2,       /* slow boot; same fuse value for atmega168v and atmega328p */
    Fuse_0ms        = 0xc2        /* fast boot; same fuse value for atmega168v and atmega328p */

};

enum
{
    Blink_Startup = 1<<0,
    Blink_Swipe = 1<<1,
};

enum AccessLevel
{
    Access_None = 0,
    Access_User,
    Access_Root,

    None = (1 << Access_None),
    User = (1 << Access_User),
    Root = (1 << Access_Root),

    Power_Idle = 0,
    Power_Save,
    Power_Down
};

enum
{
    AES_BLOCKSIZE = 16
};


extern uint16_t           hardwareFailure;

extern flash uint8_t      demoSwipes;
extern flash uint16_t     demoSeconds;

extern eeprom uint8_t     key[AES_BLOCKSIZE];
extern eeprom char        cypherMode;

extern bool               isDemo;
extern uint32_t           totalSwipes;      /* total swipes since erased */
extern uint32_t           lifeSwipes;       /* all swipes read by this device */
extern uint8_t            useLed;

extern bool               intRtc;           /* set true by RTC interrupt */
extern bool               flush_pending;    /* set true when the temporary memory flush is requested (save to dataflash) */
extern bool               slowWakeUp;       /* set true if the controler is not waking up in 0ms from sleep */
extern bool               stayAwake;        /* keeps the controller awake */

extern char               buf[BUF_SIZE];


// ------------------------ end menu  ------------------------

/* Initializes the peripherials (asic, clock, ports, dataflash etc) */
void init(void);

/** string compare between ram string and eeprom string
 *
 * @param s1  pointer to the 1st zero terminated string
 * @param s2  pointer to the 2nd zero terminated string
 *
 * @return bool     true if strings match
 *                  false if strings do not match
 *                  warning: does not return -1, 0, 1 !!!
 **/
bool strcmp_re(const char *s1, eeprom const char *s2);

/**
 *
 * @param s1  pointer to the 1st zero terminated string
 * @param s2  pointer to the 2nd zero terminated string
 *
 * @return bool     true if strings match
 *                  false if strings do not match
 *                  warning: does not return -1, 0, 1 !!!
 **/
bool strcmp_ef(eeprom char *s1, flash const char *s2);

/* copies a string from ram to eeprom */
void strcpy_er(eeprom char *s1, const char *s2);

/* copies a string from flash to eeprom */
void strcpy_ef(eeprom char *s1, flash char *s2);

/* copies a string from eeprom to ram */
void strcpy_re(char *s1, eeprom char *s2);

/* copies data from flash to eeprom */
void memcpy_ef(eeprom uint8_t *s1, flash uint8_t *s2, uint8_t size);

/* prints a string stored in eeprom */
void print_eeprom_string(eeprom char* st);

// utils
/**
 * shows the current menu
 */
void showMenu(void);

/**
 *  Reads from serial port a string with a specified maximum
 *  length. supports backspace and enter;
 *
 *  note: as this routine is blocking, times out in 3 to 4
 *  seconds if no input is detected and returns whatever it was
 *  entered until then.
 *
 *
 * @param st    pointer to the buffer where to write the result
 * @param len   maximum length allowed (sugestion:
 *               sizeof(buffer) )
 */
void myscanf(char *st, char len);

#ifdef DEBUG_PORTS
void putBin(char data);
void putState(char port, char ddr);
#endif

/**
 * Turns on, off our blinks the blue led.
 *
 *             number      - [01 .. 0xf0] - blinks 'number'
 *                           times.
 */
void ledOk(char code);

/**
 * Blinks the red led and updates hardwareFailure variable with
 * the specified error.
 *
 * @param code Error code. Bitwise codes should be used.
 *             (example: 1<<0, 1<<1, 1<<2, etc).
 */
void ledFail(char code);

/**
 * Generates CRC of the data. Zmodem compatible.
 *
 * @param crc               old crc
 * @param serialData        data
 *
 * @return unsigned int     new crc
 */
unsigned int crcUpdate(unsigned int crc, unsigned char serialData);

/**
 * Ensures that the input string complies to the requirements
 * and replaces the first '#' encountered with '\0'
 *
 * @param st        input string (note: this will be modified if
 *                  the return is not null)
 *
 * @return char*    returns pointer to st if the length of the
 *                  password is correct
 */
char *parsePassword(char *st);

/* starts a callback to shut down the led */
void scheduleCallback_reader(uint16_t ms);

/* starts a callback to flush the dataflash and eeprom */
void scheduleCallback_flush(uint16_t ms);

/**
 * Returns the timestamp in miliseconds * 3.90625
 *  32 units =  120ms
 *  64 units =  250ms
 * 128 units =  500ms
 * 256 units = 1000ms
 */
unsigned long timestamp2(void);

/**
 * Blinks on the required led for a specified number of times
 * and ms
 *
 * @param led   specifies the led [0..7]
 * @param count the number of times to blink
 * @param delay the ms to keep the led on and off
 */
void blink(char led, char count, char delay);

/**
 * Shows the revision, configuration and status of the device
 *
 * @param st        NULL will show reduced information
 *
 * @return bool     true if successful
 */
bool revision(char *st);

/**
 * Starts sending the used or whole memory to usart using our
 * own download protocol (which is very simple)
 *
 * @param st  'A' for the whole memory
 *            anything else will download the whole memory
 *
 * @return bool     true if successful
 */
bool download(char *st);

/**
 * Erases the used or the whole memory.
 *
 * @param st  'Y' for the whole memory
 *            '' will erase the used memory
 *            anything else will fail
 *
 * @return bool   true if successful
 */
bool eraseMem(char *st);

/**
 * Changes from demo to full mode or from full mode to demo
 * mode.
 *
 * Note: in order to change from demo mode from full mode, root
 * access is required
 *
 * @param st
 *
 *      demo upgrade password, terminated with '\0' or '#'
 *      (# is supported for CRFSuite compatibility purposes)
 *
 * @return bool   true if successful
 */
bool setDemo(char *st);

/**
 * Changes the current user password
 *
 * @param st
 *
 *        st is the new user password, terminated by '\0' or '#'
 *        (# is supported for CRFSuite compatibility purposes)
 *
 * @return bool
 */
bool setPass(char *st);

/**
 * Changes the encryption key, no questions asked. This function
 * should be accessible only by the root.
 *
 * @param st
 *
 *     st[0..31] is the key in literal form
 *     st[32] '1' for CBC mode
 *            '0' for ECB mode
 *     (example: ab21c49412029f73f26510b52d9bbfb60)
 *
 * @return bool     true - if successful
 *                  fail - when the key is incorrect (null,
 *                         incorrect lenght or contains invalid
 *                         characters)
 *
 */
bool setKey(char *);

/**
 * Sets the current time.
 *
 * @param st current time (text, base10, in seconds)
 *
 * @return bool     true - if successful
 */
bool setTime(char *st);

/**
 * Logs in the device
 *
 * @param st     user's password
 *
 * @return bool  true - if successful
 */
bool logIn(char *st);

/**
 * Logs out
 *
 * @param st    ignored
 *
 * @return bool true
 */
bool logOut(char *st);

/**
 * Reboots the device. For security purposes, this function is
 * available for non demo devices only.
 *
 * @param st    NULL - reboots even if in demo mode. the reason
 *              is that we need this function to reboot for
 *              different reasons, but the user commands menu
 *              will never pass a NULL
 *
 *
 * @return bool false if unsuccessful
 *              if successful, it never returns :-)
 */
bool reboot(char *st);

/**
 * Switches the Magtek chip read mode from new to old or
 * viceversa
 *
 * @param st        ignored
 *
 * @return bool     true
 */
bool changeReaderMode(char *st);

/**
 * Enables the bootloader mode.
 *
 * @param st    "yk" is required to enable the bootloader mode;
 *              anything else will fail
 *
 * @return bool     true - if successful
 */
bool enableBootLoader(char *st);


bool switchLed(char *st);


/**
 * Reads the value of the programmed fuse / lockbit
 *
 * fuse == 0 => fuse low
 * fuse == 1 => lock bits
 * fuse == 2 => fuse extended (EFB)
 * fuse == 3 => fuse high
 *
 * @return uint8_t fuse value
 */
uint8_t getFuse(char fuse);

#endif /* SMTH_H */
