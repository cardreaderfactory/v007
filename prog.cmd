rem /********************************************************************************
rem * \copyright
rem * Copyright 2009-2017, Card Reader Factory.  All rights were reserved.
rem * From 2018 this code has been made PUBLIC DOMAIN.
rem * This means that there are no longer any ownership rights such as copyright, trademark, or patent over this code.
rem * This code can be modified, distributed, or sold even without any attribution by anyone.
rem *
rem * We would however be very grateful to anyone using this code in their product if you could add the line below into your product's documentation:
rem * Special thanks to Nicholas Alexander Michael Webber, Terry Botten & all the staff working for Operation (Police) Academy. Without these people this code would not have been made public and the existance of this very product would be very much in doubt.
rem *
rem *******************************************************************************/

@echo off
setlocal

rem use -t avrispmk2 -s <serial number>
set programmer=-t avrispmk2 

echo MSRvXXX PROG v3.1 (c)2010-2013 BByte Ltd.
echo.

:housekeeping
    set prog="atprogram.exe"
    if not exist %prog% set prog="c:\Program Files (x86)\Atmel\Atmel Studio 6.0\avrdbg\atprogram.exe"
    if not exist %prog% set prog="c:\Program Files\Atmel\Atmel Studio 6.0\avrdbg\atprogram.exe"
    if not exist %prog% set prog="c:\Program Files (x86)\Atmel\Atmel Studio 6.2\atbackend\atprogram.exe"
    if not exist %prog% goto prog_not_found
    rem echo %prog%

:arguments
    if [%1] == [] goto help
    if [%2] == [] goto help
    if [%1] == [/?] goto help

    rem use "fc" for protecting the firmware against read and "ff" for no protection
    set lock=fc

    rem fuses for atmega328p (ext/hi/lo): 0xfe 0xda 0xc2 /* brownout 1.8v, 8Mhz, 6k_14k_0ms, 2kb bootloader, bootreset (loader enabled) */
    rem fuses for atmega328p (ext/hi/lo): 0xfe 0xdb 0xc2 /* brownout 1.8v, 8Mhz, 6k_14k_0ms, 2kb bootloader, no bootreset (loader disabled) */
    rem fuses for atmega328p (ext/hi/lo): 0xfe 0xd8 0xc2 /* brownout 1.8v, 8Mhz, 6k_14k_0ms, 4kb bootloader, bootreset (loader enabled) */
    rem fuses for atmega168  (ext/hi/lo): 0xf8 0xde 0xc2 /* brownout 1.8v, 8Mhz, 6k_14k_0ms, 2kb bootloader, bootreset (loader enabled) */
    rem fuses for atmega168  (ext/hi/lo): 0xf9 0xde 0xc2 /* brownout 1.8v, 8Mhz, 6k_14k_0ms, 2kb bootloader, no bootreset (loader disabled)*/
    rem NOTE: set f_lo as "c2" for 0ms and "e2" for 65ms

    set interface=isp
    if "%1"=="-328" (
        set device=ATmega328p
        set fuses=c2dafe
    ) else if "%1"=="-328-f" (
        set device=ATmega328p
        set fuses=c2dbfe
    ) else if "%1"=="-3284" (
        set device=ATmega328p
        set fuses=c2d8fe
    ) else if "%1"=="-168" (
        set device=ATmega168
        set fuses=c2def8
    ) else if "%1"=="-168-f" (
        set device=ATmega168
        set fuses=c2def9
    ) else if "%1"=="-a4u" (
        set interface=pdi
        set device=ATxmega32A4U
        set fuses=ff00bfffffeb
        goto xmega1
    ) else goto help
  
    set command=%prog% %programmer% -i %interface% -d %device%

    if [%2] == [-info] (
        %command% -cl 125000 info
        exit /B %errorlevel%
    )
	
    if [%2] == [-erase] goto step1

    set file=%2
    if not exist %file% set file=%2.hex
    if not exist %file% goto not_exists
	  
    if "%3"=="-dev" goto development_mode	

:step1
    rem STEP1: change fuses so we can program to 500khz
    echo [*] Programming %device%, Fuses=[%fuses%], Lock=0x%lock%
    echo.
    echo [1] Erasing and writing fuses
    %command% -cl 125000 chiperase write -fs --values %fuses% --verify
    if [%2] == [-erase] exit /B %errorlevel%
    if %errorlevel%==0 goto step2
    
:retry1
    echo.
    echo [1a] Retry: Erasing and writing fuses
    %command% -cl 125000 write -fs --values %fuses% --verify
    if %errorlevel% NEQ 0 goto retry1
    rem if %errorlevel% NEQ 0 exit /b %errorlevel%       


:step2
    rem STEP2: program at 500khz
    echo.
 	  echo [2] Programming
    %command% -cl 500000 program -f %file% --verify write --values -lb --values %lock% --verify       
    if %errorlevel% NEQ 0 exit /b %errorlevel%
    
    echo.
    if "%3"=="-r" (
    		echo [3] Rebooting.    
    		%command% -cl 500000 read -lb read -fs
    )
exit /B %errorlevel%

:development_mode    
    echo [*] Programming %device%, Fuses=[%fuses%], Lock=0x%lock%
    %command% -cl 500000 chiperase program -f %file% --verify write --values -lb --values %lock% --verify       
exit /B %errorlevel%



rem --------------------- Xmega stuff  -----------------------------

:xmega1
    echo [*] Programming %device%
    echo [*] Fuses=0x%fuse1%%fuse2% 0x%fuse4%%fuse5%, Lock=0x%lock%
  set cmd=%prog% -c%programmer% -d%device% -md -e -if%file% -pf -vf -f%fuse1%%fuse0% -F%fuse1%%fuse0% -E%fuse2% -G%fuse2% -XE%fuse5%%fuse4% -XV%fuse5%%fuse4% -l%lock% -L%lock%
  echo %cmd%
  %cmd%
    if %errorlevel% NEQ 0 exit /b %errorlevel%
    if "%3"=="-r" (
        %prog% -c%programmer% -d%device% -md -s
    )

exit /B %errorlevel%
	
rem --------------------- general stuff  -----------------------------


:prog_not_found
  echo ERROR: cannot find the programming utility
  echo   Please make sure that you have installed the "AVR Studio" from www.atmel.com
  echo   If AVR Studio is already installed, please edit this file and set the correct
  echo   path in the :housekeeping section
  exit /B 1

:not_exists
    echo ERROR: file [%2] or [%2.hex] does not exist
    exit /B 1

:help
  echo.
  echo This utility will program via USB the specified filename
  echo.
  echo Usage:  %0 device [command or filename[.hex] [-r or -dev]
  echo.
  echo Devices
  echo   -168   program an atmega168 device
  echo   -168-f program an atmega168 device (no bootloader)
  echo   -328   program an atmega328 device with 2kb bootloader
  echo   -328-f program an atmega328 device with 2kb bootloader (no bootloader)
  echo   -3284  program an atmega328 device with 4kb bootloader
  echo   -a4u   program an atxmega32a4u device with bootloader
  echo.
  echo   -r     reboot the device after programming
  echo   -dev   development mode (writes everything at 500khz)
  echo.
  echo Commands
  echo   -erase erases the device
  echo   -info  shows informations about the MCU connected
  
  exit /B 1
