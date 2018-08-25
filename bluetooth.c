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


#ifdef HAS_BLUETOOTH

bool changeBtConfig;
eeprom char btName[PASS_BUFSIZE] @ 0x7c;
eeprom char btPeer[PASS_BUFSIZE] @ 0x9d;
eeprom char btName[PASS_BUFSIZE] = DEVICE_NAME " Build " BUILD;
eeprom char btPeer[PASS_BUFSIZE] = DEFAULT_PEERING;

char strelen(const eeprom char *str)
{
  const eeprom char *s;
  for (s = str; *s; ++s);
  return(s - str);
}

bool setBluetooth(char *st)
{
    char    *name = NULL;
    uint8_t  len = 0;

    if (st == NULL)
        return false;

    for (len = 0; st[len] != 0; len++)
    {
        if (st[len] == ',')
        {
            if (name == NULL)
            {
                name = st+len+1;
                st[len] = 0;
            }
            else
            {
                return false; /* more than 1 comma */
            }
        }
        else if (st[len] == '=')
        {
            return false; /* '=' is not accepted as it screws up CRFSuite while parsing revision() */
        }
    }

    if ( st[0] == 0 ||
        name[0] == 0 ||
        (name - st) > PASS_BUFSIZE || /* name - st - 1 == strlen(st) */
        len - (name - st)  > PASS_BUFSIZE /* len - ( name - st ) - 1 == strlen(name) */)
        return false;

//  printf("len = %i\n", len);  nn
    strcpy_er(btPeer, st);
    strcpy_er(btName, name);
//    printf("peer = %s, name = %s\n", st, name);
    changeBtConfig = true;  /* configure on next bt disconnect or reboot */
    //updateBlueTooth();
    return true;
}
#endif




