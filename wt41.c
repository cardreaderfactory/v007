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
#include "global.h"
#include "usart.h"
#include "smth.h"
#include "delay.h"
#include "bluetooth.h"

#ifdef WT41

/********************************** WT41 DRIVER *********************************/

char ok[] = "OK\r\n";

bool waitReply(char *st)
{
    int8_t i = 0;
    char c;
    int len = strlen(st);

    for (i = 0; i < len; i++)
    {
        if (!getchar_withTimeout(&c, 1))
            return false;
        if (c != st[i])
            i = -1;
    }

    return true;
}

void updateBlueTooth()
{
    bool success = true;

    if (serialConnected() || !changeBtConfig)
        return;

    if (hardwareFailure & Fail_Bluetooth)
        return;

    printf("\nat\n"); success = waitReply(ok);

    if (success)
    {
		// note: bluetooth module must be preconfigured with this command:
        // set control init set bt pagemode 4 2000 1
		// set control baud 250000,8n1
		
		// optionally, we can move these to preconfigure the bluetooth:		
		// set control echo 4
		// set bt class 5a020c
		// set control cd 4 2 20		
		
		/* there should be no reply from bluetooth for the following commands */

        /*
         * SET CONTROL ECHO {echo_mask}
         *    Hexadecimal bit mask for controlling the display of echo and events
         *    Bit 0  If this bit is set, the start-up banner is visible.
         *    Bit 1  If this bit is set, characters are echoed back to client in command mode.
         *    Bit 2  This bit indicates if set events are displayed in command mode.
         *    Bit 3  If this bit is set, SYNTAX ERROR messages are disabled.         *
         */
        printf("set control echo 4\n");

        /*
         * SET BT CLASS {class_of_device}
         * Service Class        : Telephony (Cordless telephony, Modem, Headset service)
         * Major Device Class   : Smartphone
         */
        printf("set bt class 5a020c\n");

        /* configure PIO5 for data mode, and PIO6 (which is ignored by our software) for bluetooth link active
         *
         * SET CONTROL CD {cd_mask} {mode} [datamode_mask]
         * CD signal is driven high when in datamoode
         *
         * for mode 2:
         * cd_mask       = indicating the exitence of a Bluetooth connection
         * datamode_mask = indicating that the module is in data mode
         *
         * value 0x04 (    b100) must be used for PIO2
         * value 0x20 ( b100000) must be used for PIO5
         * value 0x40 (b1000000) must be used for PIO6
         */
        printf("set control cd 4 2 20\n");

        /* delete all init commands */
//        printf("set control init\n");

        /* should not delete all pairings */
//        printf("set bt pair *\n");

        printf("set bt auth * "); print_eeprom_string(btPeer); putchar('\n');
        printf("set bt name "); print_eeprom_string(btName); putchar('\n');

         /* SET BT PAGEMODE {page_mode} {page_timeout} {page_scan_mode}  [{alt_page_mode}  {conn_count}]
          *  page_mode 4
          *  if there are NO connections: iWRAP is visible in the inquiry and answers calls.
          *  If there are connections: is NOT visible in the inquiry and does NOT answers calls
          */
        printf("set control init set bt pagemode 4 2000 1\n");

        /* SET CONTROL INIT [command]
         * DELAY {id} [delay] [command]
         * after 5 minutes, set to pagemode 2:
         * page_mode 2: iWRAP is NOT visible in the inquiry but answers calls
         */
        printf("delay 0 300000 set bt pagemode 2\n");

        /* bt should answer ok to AT */
        printf("at\n");
        success = waitReply(ok);

        printf("set\n");
    }

    if (!success)
    {
        ledFail(Fail_Bluetooth);        
    }
    else
    {
#if 0        
        blink(LED_GREEN, 1, 25);
        blink(LED_RED, 1, 25);
        blink(LED_GREEN, 1, 25);
#endif        
    }
    
    changeBtConfig = false; /* only one try for every disconnect */    
}

bool checkBlueTooth(void)
{
    if (serialConnected())
        return true;
        
    printf("\nat\n");
    return waitReply(ok);
}

#endif
