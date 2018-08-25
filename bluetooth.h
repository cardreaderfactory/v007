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
#ifndef _BLUETOOTH_H
#define _BLUETOOTH_H

#ifdef HAS_BLUETOOTH

extern eeprom char btName[];
extern eeprom char btPeer[];
extern bool changeBtConfig;

/* requests a change in the bluetooth configuration
 * string format:
 *   <peer pass>,<device name>
 *
 * the actual change will be done by updateBlueTooth()
 */
bool setBluetooth(char *);


/* updates the bluetooth module configuration if required and possible
 *
 * Note: as bluetooth does not allow the change of the confguration while
 * connected, we need to reconfigure when it is not connected.
 *
 * it is safe to call this routine anytime
 */
void updateBlueTooth();

char strelen(const eeprom char *str);

bool checkBlueTooth(void);

#else

    #define setBluetooth(st);
    #define updateBlueTooth();
    
#endif /* HAS_BLUETOOTH */

#endif