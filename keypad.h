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

#ifndef _KEYPAD_H
#define _KEYPAD_H

#ifdef HAS_KEYPAD

void keypad_init(void);
void keypad_main(void);
extern uint32_t kb_timestamp;

extern eeprom uint32_t eTotalKeys;
extern uint32_t totalKeys;


#else

#   define keypad_init()
#   define keypad_main()

#endif /* HAS_KEYPAD */

#endif /* _KEYPAD_H */
