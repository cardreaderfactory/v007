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

#ifndef UNITTEST_H
#define UNITTEST_H

//#define ENABLE_UNITTEST

//    #define DEBUG_MEM     // menu to fill and test the memory
//    #define DEBUG_PORTS   // menu to show the status of all ports
//    #define INCLUDE_VIEWMEM
//    #define INCLUDE_SHUTDOWN
//    #define DEBUG_SETKEY
//    #define DEBUG_PARSEPASS
//    #define DEBUG_AES
//    #define INCLUDE_DECRYPTION // testing purposes; needed by the unittest


#define MEMGUARD 0xbadf00d

#ifdef DEBUG_SWITCH_CLOCK
    void checkClock(void);
#else
    #define checkClock()
#endif

#ifdef INCLUDE_VIEWMEM
  char viewMem(char *);
#endif
#ifdef DEBUG_MEM
  char testMem(char *st);
  //void testMem_manual(void);
#else
  #define testMem_manual()
#endif

#if defined (DEBUG_AES) && defined (INCLUDE_ENCRYPTION)
  char test_aes(char *);
#endif

#ifdef DEBUG_PORTS
  char portStats(char *);
#endif

void putBin(char data);

#ifdef ENABLE_UNITTEST
    void unitTests();
#else
    #define unitTests()
#endif

#endif
