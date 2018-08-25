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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "global.h"
#include "unittest.h"
#include "smth.h"
#include "dataflash.h"
#include "aes.h"
#include "usart.h"
#include "profiler.h"
#include "rtc.h"


#if defined DEBUG_AES && defined INCLUDE_ENCRYPTION
    char current_char=0;
    flash char activity[]="/-\\|";
#endif

extern char     userLevel;
extern uint32_t timestamp;
extern eeprom uint8_t  failedDemoTries;

enum
{
    TestCharStart   =   47,
    TestCharStop    =   57,
    AesTestTime     =   2,
    BufferSize      =   51           // used by testmem and viewmem. should be less than ZMAX_BUF

};


#if defined DEBUG_AES && defined INCLUDE_ENCRYPTION
void show_activity(void)
{
  if (current_char<3)
      current_char++;
  else
      current_char=0;

  putchar(activity[current_char]);
  putchar(8);
}

void benchmark_aes(void)
{
    char        clear[17];
    char        crypted[17];
    uint32_t    startTime;
    uint32_t    i;

    printf("Start: benchmarkAes()\n");

    memset(clear,0,sizeof(clear));
    memset(crypted,0,sizeof(crypted));

    printf("Testing AES routines ... \n");

//    sprintf(clear,"{bla tst}");
//    printf("    clear text: [%s]\n",clear);
//    Aes_encrypt(clear,crypted);
//    printf("    encrypted text: [%s]\n",crypted);
//    sprintf(clear,"zbla tst muahahz");
//    clear[0]='z';
//    printf("    clear text: [%s]\n",clear);
//    Aes_encrypt(clear,crypted);
//    printf("    encrypted text: [%s]\n",crypted);

    printf("    benchmarking encryption: ");
    i=0;
    startTime = timestamp;
    while ( timestamp - startTime < AesTestTime )
    {
        aes_encrypt(clear,crypted);
        i++;
        wdt_reset(); /* reset watchdog */
    }
    printf("%lub/s\n",(unsigned long)(16 * i / AesTestTime));

#ifdef INCLUDE_DECRYPTION
    printf("    benchmarking decryption: ");
    i=0;
    startTime = timestamp;
    while ( timestamp - startTime < AesTestTime )
    {
        aes_decrypt(crypted,clear);
        i++;
        wdt_reset(); /* reset watchdog */
    }
    printf("%lub/s\n",(unsigned long)(16 * i / AesTestTime));
    //total_blocks=0;
}

char test_aes(void)
{
#ifdef INCLUDE_DECRYPTION
    char        testok      = true;
    char        j;
    char        should_exit = false;
    char        ch;
#endif
    char        clear[17];
    uint32_t    guard1 = MEMGUARD;
    char        clear2[17];
    uint32_t    guard2 = MEMGUARD;
    char        crypted[17];
    uint32_t    guard3 = MEMGUARD;
    uint32_t    startTime;
    uint32_t    i;

    memset(clear,0,sizeof(clear));
    memset(crypted,0,sizeof(crypted));

    printf("Start: testAes()\n");

    printf("Testing AES routines ... \n");

    i=0;
    printf("    data integrity tests. press 'q' to abort, 's' to show stats\n    testing ... ");
    srand(timestamp);

    startTime = timestamp;
    while (!should_exit)
    {
        wdt_reset(); /* reset watchdog */
        i++;
//        if (i==0xffff) { total_blocks++; i=0;}
        if (i%500==0) show_activity();
        for (j=0;j<16;j++)
        {
            clear[j] = rand();
            if (i<100)
                key[j] = rand();
        }

        if (i<100)
            aes_init();

//        sprintf(clear,"{bla tst}");
//        printf("   clear text: [%i][%s]\n",strlen(clear),clear);
        profiler_breakPoint("nothing");
        aes_encrypt(clear,crypted);
        memcpy(clear2, clear, 16);
        aes_encrypt(clear2,clear2);
        if (guard1 != MEMGUARD || guard2 != MEMGUARD || guard3 != MEMGUARD)
        {
            printf("encrypt: memory corrupted\n");
            testok=false;
            break;
        }
        if (strcmp(clear,crypted)==0)
        {
            putchar('1');
            testok=false;
            break;
        }
        if (strcmp(clear2,crypted)!=0)
        {
            printf("\nclear   = %s"   \
                   "\nclear2  = %s"   \
                   "\ncrypted = %s\n", clear, clear2, crypted);
            putchar('3');
            testok=false;
            break;
        }
        profiler_breakPoint("aes_encrypt");
        aes_decrypt(crypted,crypted);
        if (guard1 != MEMGUARD || guard2 != MEMGUARD || guard3 != MEMGUARD)
        {
            printf("decrypt: memory corrupted\n");
            testok=false;
            break;
        }
        profiler_breakPoint("aes_decrypt");
//        aes_encrypt(crypted,crypted);
//        aes_decrypt(crypted,crypted);
//           printf("   decrypted text: [%i][%s]\n",strlen(crypted),crypted);
        if (usart_gotChar())
        {
            ch=getchar();
            if (ch=='q') { should_exit=true; break;}
            if (ch=='s') { printf("passed\n    encrypted & decrypted %lukb in %lu seconds\n    testing ... ",(unsigned long)(i>>6), (unsigned long)(timestamp - startTime));}
        };
        if (strcmp(clear,crypted)!=0)
        {
            testok=false;
            break;
        }
    } // while
    if (testok) printf("passed\n"); else printf("failed\n");
//    printf("start = %lu, stop = %lu, i = %lu", startTime, timestamp, i);
    printf("    encrypted & decrypted %lukb in %lusec\n    ",(unsigned long)(i>>6),(unsigned long)(timestamp - startTime));
 #endif
    printf("Test complete\n");


    return 1;
}
#else
    #define benchmark_aes()
    #define test_aes()
#endif /* #if defined DEBUG_AES && defined INCLUDE_ENCRYPTION */

#ifdef DEBUG_MEM

void showStats( unsigned long bytesdone, unsigned long tofill, unsigned long clock)
{
    static unsigned long lastShow = 0;
    unsigned long        kbps     = 0;
    int                  i;

    if (clock <= lastShow && bytesdone != tofill)
        return;

    if (clock > 2)
        kbps=(unsigned long)(bytesdone/1024/clock);

    if (lastShow != 0)
        for (i = 0; i < 27; i++)
            putchar('\b');

    printf(" %3i%% (%7lukb) ", bytesdone*100/tofill, bytesdone/1024);

    if (kbps > 0)
        printf("%4lukBps ", kbps);
    else
        printf("         ", kbps);

    if (bytesdone != tofill)
        lastShow = clock;
    else
        lastShow = 0;
}

uint32_t AesRoundUp(uint32_t bytes)
{
#ifdef INCLUDE_ENCRYPTION
    if (bytes % AES_BLOCKSIZE != 0)
        bytes += (AES_BLOCKSIZE - bytes % AES_BLOCKSIZE );
#endif
    return bytes;
}

bool testMem(char *st)
{
    uint32_t len;
    uint32_t i;
    uint32_t stime;
    uint32_t ltime;
    uint8_t block;

    len = atoi(st);
    len *= 1024;
    if (len <= 0)
        return 0;
    printf("filling %lu of memory\n",len);

    stime = ltime = timestamp;
    i = len;
    sprintf(buf, "[1234567890abcdef][1234567890abcdef]");
    block = strlen(buf);
    while (i > 0)
    {
        uint8_t toWrite;
        if (i > block)
            toWrite = block;
        else
            toWrite = i;

        write(buf,toWrite);
        i -= toWrite;
        if  (ltime != timestamp)
        {
            printf("left: %lu (%lu bytes/sec)    \r", i, (len-i)/(timestamp-stime));
            ltime = timestamp;
        }
    }
    flush();
}


bool writeJunk(uint32_t start, uint32_t stop, uint16_t *crc)
{
    uint16_t    toWrite;
    uint16_t    wrote;
    int         i;
    uint32_t    j;
    uint32_t    startTime;
    signed char content;

    start = AesRoundUp(start);
    stop = AesRoundUp(stop);
    printf(" * writing memory from %lu to %lu ... ", start, stop);
    startTime = timestamp;
    *crc = 0;
    content = start & 0xff;
//    srand(timestamp);
    df_seek(start);
    for (j = start; j < stop; )
    {
        if (stop - j > BUF_SIZE)
            toWrite = BUF_SIZE;
        else
            toWrite = stop - j;

        for (i = 0; i < toWrite; i++, content++)
        {
            buf[i] = content;
            *crc = crcUpdate(*crc, buf[i]);
        }

        wrote = write(buf,toWrite);

        if (wrote != toWrite)
        {
           printf("failed (wrote = %u, towrite = %u)\n", wrote, toWrite);
           return false;
        }

        j += toWrite;
        showStats(j - start, stop - start, timestamp - startTime);
    }

    printf("done\n");
    flush();
    return true;
}
#if 0
bool checkMem(uint32_t start, uint32_t stop, uint16_t expectedCrc, bool checkByte)
{
    int             i;
    unsigned char   expected;
    uint16_t        crc        = 0;
    uint16_t        toRead;
    uint16_t        bytesRead;
    uint32_t        j;
    uint32_t        startTime;

    startTime = timestamp;
    start = AesRoundUp(start);
    stop = AesRoundUp(stop);

    printf(" * verifying memory from %lu to %lu ... ", start, stop);

    Df_seek(start);
    expected = start & 0xff;
    for (j = start; j < stop; )
    {
        if (stop - j > BUF_SIZE)
            toRead = BUF_SIZE;
        else
            toRead = stop - j;

        bytesRead = df.read(buf,AesRoundUp(toRead));
        if (bytesRead != AesRoundUp(toRead))
        {
            printf("failed @%lu (toRead = %u, bytesRead = %u)\n", j, AesRoundUp(toRead), bytesRead);
            return false;
        }

        for (i = 0; i < toRead; i++, expected++)
        {
            crc = crcUpdate(crc, buf[i]);
            if (checkByte && buf[i] != expected )
            {
                printf("failed (at %lu read %i, expected %i)\n", j+i, buf[i], (j+i) & 0xff);
                return false;
            }
        }

        j += toRead;
        showStats(j - start, stop - start, timestamp - startTime);
    }

    if (crc != expectedCrc)
    {
        printf("crc failed (expected = %u, result = %u)\n", expectedCrc, crc);
        return false;
    }

    printf("done\n");
    return true;
}

void testMem_manual(void)
{
    int i,k,f;

    printf("testMem_manual ... ");
    sprintf(buf, "[Lorem Ipsum is simply du");
    write(buf, strlen(buf));
    sprintf(buf, "mmy text of the printing and typesetting industry.]\n");
    write(buf, strlen(buf));
    flush();
    sprintf(buf, "\n");
    write(buf, strlen(buf));

    for (k = 0; k < 16; k++)
    {
        for (i = '0'; i <= '9'; i++)
        {
            buf[0] = i;
            for (f = 0; f < k; f++)
              buf[f+1] = ' ';
            buf [f+1] = 0;
            write(buf, k);
        }
        buf[0] = '\n';
        buf[1] = 0;
        write(buf, 1);
    }

    for (i = 0; i < 999; i++)
    {
        sprintf(buf, "%03i. [Lorem Ipsum is simply dummy text of the printing",i);
        write(buf, strlen(buf));
        sprintf(buf, " and typesetting industry.]\n");
        write(buf, strlen(buf));
    }

    printf("done\n");

    flush();
}

#define STARTEMPTY  (0)
#define STOPEMPTY   (10*1024)
#define START1      (0)
#define STOP1       (10*1024+53)
#define START2      (188)
#define STOP2       (150*1024+197)

char testMem1(char *st)
{
    unsigned int    crc;
    uint32_t        temp;
    bool            decryptState;
    char            thorough = false;

    printf("testmem:\n  'mm' - normal tests\n  'mmt' - thorough tests\n  'mmm' - manual test (for debugging)\n   st=[%s]\n",st);

    if (st[0] == 'm')
    {
       testMem_manual();
       return true;
    }
    else if (st[0] == 't')
    {
       thorough = true;
    }


    decryptState = df_decrypt;
#if defined INCLUDE_DECRYPTION && !defined INCLUDE_ENCRYPTION
    printf("cannot test when decryption is is not compiled");
    return false;
#endif

    printf(" 1. totalMemory ... \n");
    if (df_totalMemory == 0)
        goto failure;

#if 1
    printf(" 2. eraseMemory ... \n * ");
    Df_erase(false);
    printf("\n");
    if (df_usedMemory != 0 || df_currentByte != 0 || df_currentPage != 0 || df_dirty)
        goto failure;
    for (crc = 0, temp = STARTEMPTY; temp < STOPEMPTY; temp++)
        crc = crcUpdate(crc, 0xff);
    Df_decrypt(false);
    if (!checkMem(STARTEMPTY, STOPEMPTY, crc, false))
        goto failure;

    /**
     * test 3
     */
    printf(" 3. Writing & reading... \n");
    Df_decrypt(true);
    if (!writeJunk(START1, STOP1, &crc))
        goto failure;
    temp = AesRoundUp(STOP1);
    if (df_usedMemory != temp)
    {
        printf(" ! usedMemory = %lu, expected = %lu\n", df_usedMemory, temp);
        goto failure;
    }

    if (!checkMem(START1, STOP1, crc, true))
        goto failure;

    printf(" * findUsedMem ... ");
    temp = Df_findUsedMemory() ;
    if (temp != df_usedMemory)
    {
        printf("failed\n ! findUsedMemory() = %lu, expected = %lu\n", temp, df_usedMemory);
        goto failure;
    }
    printf("done\n");

    if (!writeJunk(START2, STOP2, &crc))
        goto failure;
    temp = AesRoundUp(STOP2);
    if (df_usedMemory != temp)
    {
        printf("\n ! usedMemory = %lu, expected = %lu\n", df_usedMemory, temp);
        goto failure;
    }

    printf(" * findUsedMem ... ");
    temp = Df_findUsedMemory() ;
    if (temp != df_usedMemory)
    {
        printf("failed\n ! findUsedMemory() = %lu, expected = %lu\n", temp, df_usedMemory);
        goto failure;
    }
    printf("done\n");

    if (!checkMem(START2, STOP2, crc, true))
        goto failure;

    /* lenghty tests from now on */
    if (!thorough)
        goto success;
#endif

    printf(" 4. eraseMemory ...");
    Df_erase(true);
    putchar('\n');

    printf(" 5. write all memory ...\n");
    Df_decrypt(true);
    if (!writeJunk(0, df_totalMemory, &crc) || df_usedMemory != df_totalMemory)
        goto failure;

    if (!checkMem(0, df_totalMemory, crc, true))
        goto failure;

    printf(" 6. eraseMemory ... ");
    Df_erase(true);
    printf("\n * calculating checksum ... ");

    wdt_stop();
    for (crc = 0, temp = 0; temp < df_totalMemory; temp++)
        crc = crcUpdate(crc, 0xff);
    wdt_start();
    printf("\n * calculated memory crc = %u\n", crc);

    Df_decrypt(false);
    if (!checkMem(0, df_totalMemory, crc, false))
        goto failure;


success:
    putchar('\n');
    Df_decrypt(decryptState);
    return true;

failure:
    df_decrypt = decryptState;
    printf("failed\n");
    return false;
}
#endif
#endif
#ifdef INCLUDE_VIEWMEM
void ViewMem_Show(char c){
  if ((c<32 && c!=10 && c!=13) || (c>128 && c!=255)) putchar('_'); else
  if (c==255) putchar('?'); else
  putchar(c);
}

char viewMem (char *st)
{
//    #define maxlines 24
//    #define wrap 80
    #define MAXLINES 10
    #define WRAP 200
    #define WRAP_hex 8
    unsigned long pstart,max_read=0;//,kbdone;
    unsigned char lines=0,i,ch,cnt=0,toread=1;
    char hex=false,chars_per_line=WRAP;

    if (userLevel>Access_None)
    {
        printf(" > Show [All/Hex/Kb]: ");
        myscanf(buf,sizeof(buf)); // buf, show chars, block
        if (buf[0]=='h') { hex=true;chars_per_line = WRAP_hex;max_read = atoi(buf+1);} else max_read = atoi(buf);
        if (max_read<=0 || max_read > df_totalMemory/1024) max_read=df_usedMemory; else max_read*=1024;
    } else {
        max_read = df_usedMemory;
    }

//    printf("Viewing memory ... ",tofill);
    flush();
    pstart=Df_tell();
    df_seek(0);
    printf(" * viewing ");
    #ifdef INCLUDE_DECRYPTION
        if (df_decrypt) printf(" decrypted memory ... \n");  else
    #endif
    printf("raw memory ... \n");
//    printf("To read: %lu bytes, (from @%li) ...\n",max_read,Df_tell());
    printf(" * To read: %lu bytes...\n",max_read);
    while (Df_tell()<max_read && toread!=0) {
       while (lines < MAXLINES && Df_tell()<max_read && toread!=0) {
            if (Df_tell() + BufferSize > max_read) toread = max_read-Df_tell(); else toread = BufferSize;
            toread = df.read(buf,toread);
            wdt_reset();
            for (i=0;i<toread;i++)
              {
                    if (lines < MAXLINES)
                        {
                            if (hex){ printf("%x ",buf[i]);if ((cnt+1)%8==0) putchar('  ');} else ViewMem_Show(buf[i]);
                            cnt++;

                        }
                    else break;
                    if (buf[i]=='\n' && !hex) { lines++;cnt=0;}
                    if (cnt>=chars_per_line) { putchar(13); cnt=0;lines++;}
              }
       }
       if (Df_tell()<max_read) {
         printf(" > 'c' continue / 'q' quit ... ");
         if (userLevel == Access_Root) printf("@%lu",Df_tell());
         while (!usart_gotChar()) { wdt_reset(); }
         ch=getchar();
         printf("\n");
         if (ch=='q') break;
       }
       lines=0;
       for (i=i;i<toread;i++) {
         if (hex){ printf("%x ",buf[i]);if ((cnt+1)%8==0) putchar('  ');} else ViewMem_Show(buf[i]);
         cnt++;
         if (buf[i]=='\n' && !hex) { lines++;cnt=0;}
         if (cnt>=chars_per_line) { putchar(13); cnt=0;lines++;}
       }
    }
//    printf("\nreadcursor: %li\n",Df_tell());
    df_seek(pstart);
    printf("\n");
    hex=false;

    return 1;
}
#endif


void putBin(char data)
{
  char i;
  for (i=0;i<8;i++)
    {
        if (data & 1<<(7-i)) putchar('1'); else putchar('0');
    };
  putchar(' ');
}

#ifdef DEBUG_PORTS

void putState(char port, char ddr)
{
  char i;
  for (i=7;i+1>0;i--) {
     if ((ddr & 1<<i) == 0 && (port & 1<<i)!=0 ) { putchar('R');} else
     if ((ddr & 1<<i) != 0 && (port & 1<<i)==0 ) { putchar('G');} else
     if ((ddr & 1<<i) != 0 && (port & 1<<i)!=0 ) { putchar('V');} else
     putchar('-');
  }
  putchar(' ');
}

char portStats(void)
{
    char j;
    signed int i;
    printf("\n      GATE B   GATE C   GATE D\n      ");
    for (j=0;j<3;j++){
      for (i=7;i>=0;i--) {putchar(i+48);};
      putchar(' ');
    };

    printf("\nddr   "); putBin(DDRB); putBin(DDRC); putBin(DDRD);
    printf("\nport  "); putBin(PORTB); putBin(PORTC); putBin(PORTD);
    printf("\nstate "); putState(PORTB,DDRB);  putState(PORTC,DDRC);  putState(PORTD,DDRD);
    printf("\npin   "); putBin(PINB); putBin(PINC); putBin(PIND);

    printf("\n\n      PCMSK0   PCMSK1   EIMSK    PRR      TIMKS0   TIMSK1   TIMSK2 \n      ");

    for (j=0;j<7;j++){
      for (i=7;i>=0;i--) {putchar(i+48);};
      putchar(' ');
    };
    printf("\n      ");putBin(PCMSK0);putBin(PCMSK1);putBin(EIMSK);putBin(PRR);putBin(TIMSK0);putBin(TIMSK1);putBin(TIMSK2);
    printf("\n\nResistive loads on: [ ");
    for (i=0;i<8;i++)
     {
       if ((DDRB & 1<<i) == 0 && (PORTB & 1<<i)!=0 ) printf("B.%i ",i);
       if ((DDRC & 1<<i) == 0 && (PORTC & 1<<i)!=0 ) printf("C.%i ",i);
       if ((DDRD & 1<<i) == 0 && (PORTD & 1<<i)!=0 ) printf("D.%i ",i);
     }
    printf("]\n");
//  printf("OCR1AH=%i OCRIAL=%i\n",OCR1AH,OCR1AL);
//  printf("X = %i, Y=%i, Z=%i\n",azar,azar,azar);
   return 1;
}

#endif

#ifdef DEBUG_PARSEPASS
void testParsePassword()
{
    int i,j;
    printf("testing parsePassword()\n");
    if (parsePassword(NULL) != NULL)
    {
        printf("fail @ %i\n", __LINE__);
        return;
    }
    for (i = 0; i < 70; i++)
    {
        for (j = 0; j < i; j++)
            buf[j] = 'a';

        printf("buf = [%s], parsed = %s\n", buf, parsePassword(buf));

        buf[j/2] = '#';

        printf("buf = [%s], parsed = %s\n", buf, parsePassword(buf));
    }

}
#else
    #define testParsePassword()
#endif

#ifdef DEBUG_SETKEY

void testSetKey()
{
    int i, j;
    char key2[16];

    printf("testing setKey()\n");
    if (df_currentByte != 0)
    {
        printf("cannot test as memory is not empty");
        return;
    }
    printf(" * valid test cases\n");
    for (j = 0; j < 10; j++)
    {
        for (i = 0; i < sizeof(key2); i++)
        {
          key2[i] = rand();
          sprintf(&buf[i*2], "%02x", key2[i]);
        }
        buf[AES_BLOCKSIZE*2] = '1';
        buf[AES_BLOCKSIZE*2+1] = 0;

/*
        printf("size = %i\nbuf = [%s]\nkey2 = [", sizeof(key2), buf);
        for (i = 0; i < sizeof(key2); i++)
            printf("%02x", key2[i]);
        printf("]\n");
*/

        if(!setKey(buf))
        {
            printf("fail @ %i\n", __LINE__);
            return;
        }

        for (i = 0; i < sizeof(key2); i++)
            if (key[i] != key2[i])
            {
                printf("fail @ %i\n", __LINE__);
                return;
            }

    }

    printf(" * null string\n");
    if (setKey(NULL)) /* null string */
    {
        printf("fail @ %i\n", __LINE__);
        return;
    }

    printf(" * invalid test cases\n");
    for (i = 0; i < 64; i++)
    {
        buf[i] = 0;
        if (i > 0)
        {
            buf[i-1] = 'k';

            //printf(" * invalid chars, length = %i, buf = [%s]\n", i, buf);
            if (setKey(buf))
            {
                printf("fail @ %i\n", __LINE__);
                return;
            }

            buf[i-1] = '0';
            if (i != AES_BLOCKSIZE*2+1)
            {
                //printf(" * valid chars, invalid length: [%s]\n", buf);
                if (setKey(buf))
                {
                    printf("fail @ %i\n", __LINE__);
                    return;
                }
            }
        }

    }
    printf("done");
}
#else
    #define testSetKey()
#endif

#ifdef DEBUG_SWITCH_CLOCK

uint16_t t0_overflows;
uint16_t t0_seconds;
uint16_t calcErr = 6450;

interrupt [TIM0_OVF] void timer0_ovf(void)
{
    t0_overflows++;
    if (t0_overflows == 8000000/256)
    {
        t0_seconds++;
        t0_overflows=0;
    }
}

void checkClock()
{
    uint32_t lt = 1;
    uint16_t cnt = 0;
    bool loop = false;


do {
        //putchar('.');
        switchT2toT1();
        printf("testing with calcErr = %u\n", calcErr);
        PRR   &= ~(1<<PRTIM0); // start timer 1
        TCCR0B = 0; // stop clock
        TIMSK0 = 0;             // delete any interrupt sources
        TIFR0 = 0xFF;           // delete all timer1 flags, including (1<<TOV1) overflow interrupt flag
        TCNT0 = 0;              // clear timer1 counter (need to write high byte first)
        TIFR2 = 0xff;

        if (rtcState==OnT2)
        {
            switchT2toT1();
            printf("sw to T1\n");
        }

        t0_overflows = t0_seconds = timestamp = 0;
        clicksRemaining = 0;
        t1_overflows = 0;

        TCNT1H = 0;
        TCNT1L = 0;
        TCCR0B = (1<<CS00);     // start timer0 with 256 divider
        TIMSK0 = (1<<TOIE0);    // activate compare interrupt

        do
        {
            if (usart_gotChar())
            {
                char ch = getchar();
                printf("got ch = '%c'\n", ch);
                if (ch == '+')
                {
                    calcErr++;
                    loop = true;
                }
                else if (ch == '-')
                {
                    calcErr--;
                    loop = true;
                }
                else
                {
                    loop = false;
                    break;
                }
            }
            cnt++;
            if (lt != t0_seconds)
            {
                printf("timestamp[calc/real]: %lu.%03lu / %u.%03lu t1_ovf: (%u) cnt: %u, calcErr = %u\n", timestamp, (uint32_t)t1_overflows*1000/256, t0_seconds, (uint32_t)t0_overflows*1000/31250, t1_overflows, cnt, calcErr);
                lt = t0_seconds;
                cnt = 0;
            }
    //        delay_ms(100);
            switchT1toT2();
    //        if (lt != t0_seconds)
    //        {
    //            printf("timestamp[calc/real]: %lu.%03lu / %u.%03lu t1_ovf: (%u), clicks remaining: %lu, cnt: %u\n", timestamp, (uint32_t)t1_overflows*1000/256, t0_seconds, (uint32_t)t0_overflows*1000/31250, t1_overflows, clicksRemaining, cnt);
    //            lt = t0_seconds;
    //            cnt = 0;
    //        }
    //        delay_ms(100);

    //        scheduleT2toT1();
    //        while (rtcState != OkToSwitchToT1);
            switchT2toT1();

            //{
            //   uint16_t j;
            //   for (j = 0; j < 65535; j++)
            //    SPDR = j;                          // put byte 'output' in SPI data register
            //}

            wdt_reset(); /* reset watchdog */

        } while (1);
    } while (loop);

}
#endif

#ifdef ENABLE_UNITTEST
void unitTests()
{
    benchmark_aes();
    test_aes();
    testSetKey();
    testParsePassword();
}
#endif