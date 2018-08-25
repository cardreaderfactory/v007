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

#ifndef _AES_H
#define _AES_H

#ifdef INCLUDE_ENCRYPTION
/**
 *
 * Initialises the Aes routines.
 *
 * This function initializes the key but does not flushes the
 * data or initializes the CBC. The reason is that this function
 * is designed to initialize the whole aes system. If it would
 * flush first, it might use uninitialized variables or access
 * uninitialized memory.
 *
 * You should not change the key when there's data in memory as
 * the whole hell will break loose.
 *
 * On startup, call this function and then initialize the
 * dataflash.
 *
 * If you want to change the key, flush, erase memory, call this
 * function and then initChainBlock()
 *
 * @return bool
 */
void aes_init();

/**
 * Flushes the encryted buffer.
 *
 */
void aes_flush(void);

/**
 * Writes into the encrypted dataflash the data pointed by the
 * buffer.
 *
 * @param buf
 * @param length
 *
 * @return uint16_t
 */
uint8_t aes_write( const uint8_t *buf, uint8_t length);

/**
 * Encrypts a buffer
 *
 * @param aes_raw
 * @param aes_crypted
 */
void aes_encrypt( uint8_t *aes_raw, uint8_t *aes_crypted );

/**
 * Initializes the chain encryption buffer for CBC mode
 */
void initChainBlock();

#ifdef INCLUDE_DECRYPTION
/**
 * Decrypts a buffer
 *
 * @param aes_crypted
 * @param aes_raw
 */
void aes_decrypt( uint8_t *aes_crypted,uint8_t *aes_raw );

#endif /* INCLUDE_DECRYPTION */

#else /* INCLUDE_ENCRYPTION */

#define aes_init()
#define aes_flush()
#define aes_encrypt()
#define initChainBlock()

#endif /* INCLUDE_ENCRYPTION */

#endif /* _AES_H */
