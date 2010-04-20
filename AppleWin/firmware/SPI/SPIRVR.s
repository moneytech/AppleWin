;AppleWin : An Apple //e emulator for Windows
;
;Copyright (C) 1994-1996, Michael O'Brien
;Copyright (C) 1999-2001, Oliver Schmidt
;Copyright (C) 2002-2005, Tom Charlesworth
;Copyright (C) 2006-2007, Tom Charlesworth, Michael Pohoreski
;
;AppleWin is free software; you can redistribute it and/or modify
;it under the terms of the GNU General Public License as published by
;the Free Software Foundation; either version 2 of the License, or
;(at your option) any later version.
;
;AppleWin is distributed in the hope that it will be useful,
;but WITHOUT ANY WARRANTY; without even the implied warranty of
;MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;GNU General Public License for more details.
;
;You should have received a copy of the GNU General Public License
;along with AppleWin; if not, write to the Free Software
;Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
;

; Description: Firmware for Apple SPI card - Applewin version
;
; Author: Copyright (c) 2010, R. Glenn Jones
;

;
; Created by Glenn Jones
; Based on Rich Drehers CFFA driver architecture
; Uses ca65 and ld65 tool chain
;


.segment "EXEHDR"            ; just to keep the linker happy
.segment "STARTUP"           ; just to keep the linker happy



; AppleSPI with SPI and CS8900A
;  
;  // Uthernet
;	C0F0	(r/w)  address of 'receive/transmit data' port on Uthernet
;	C0F1	(r/w)  address of 'receive/transmit data' port on Uthernet 
;   // SPI
;	C0F2	(r)   SPI Data In
;	C0F2	(w)   SPI Data Out 
;	C0F3	(r)   SPI Status
;	C0F3	(w)   SPI Control
;  // Uthernet
;	C0F4	(r/w)  address of 'transmit command' port on Uthernet
;	C0F5	(r/w)  address of 'transmit command' port on Uthernet
;	C0F6	(r/w)  address of 'transmission length' port on Uthernet
;	C0F7	(r/w)  address of 'transmission length' port on Uthernet
;  // EEPROM
;	C0F8	(r)   Write Protect status
;	C0F8	(w)   00 - Disable EEPROM Write Protect
;	C0F8	(w)   01 - Enable EEPROM Write Protect
;	C0F9	(r/w)  EEPROM Bank select
;  // Uthernet
;	C0FA	(r/w)  address of 'packet page' port on Uthernet
;	C0FB	(r/w)  address of 'packet page' port on Uthernet
;	C0FC	(r/w)  address of 'packet data' port on Uthernet
;	C0FD	(r/w)  address of 'packet data' port on Uthernet
;  // SPI
;	C0FE	(r/w)  SCLK Divisor
;	C0FF	(r/w)  Slave Select



; constants
IOBase 				= $C080         ; indexed with X = $n0
spi_in		 		= IOBase+2
spi_out		 		= IOBase+2
spi_status 		= IOBase+3
spi_controlk 	= IOBase+3
spi_clock_div = IOBase+$e
spi_slave_sel = IOBase+$f



; The Autoboot rom will call this.
; This is also the entry point for such things as IN#7 and PR#7

CR              = $0D
FIRMWARE_VER    = $10        ; Version 1.0 (Version of this code)


;-------------------------------------------------------------------------
; BEGIN ROM CONTENT
;-------------------------------------------------------------------------

;-------------------------------------------------------------------------
; $C000
;
; The base address for this 32K of code is $C000.  The first page ($C0xx) is
; not available in the Apple II address space, so we'll just fill it with
; something you might want to see if you have a copy of the firmware on
; disk.
;-------------------------------------------------------------------------
   .ORG $C000
   .byte "AppleSPI Firmware",CR,CR,CR
   .byte "See <http://a2retrosystems.com/AppleSPI.htm>."
   .byte CR,CR,CR,CR,CR,CR,CR,CR,CR,CR
   .byte "Version "
   .byte $30+(FIRMWARE_VER/16), ".", $30+(FIRMWARE_VER & $F)
   .byte " for "
   .byte "6502"
   .byte " or later.",CR,CR
   .RES $C100-*, $FF            ; fill the rest of the $C0xx area (unused)

;------------------------------------------------------------------------------
; Macros
;------------------------------------------------------------------------------

.macro ASSERT condition, string
   .if condition
   .else
      .error string
   .endif
.endmacro

;------------------------------------------------------------------------------
; $C100
;
; Start of Peripheral Card ROM Space $Cn00 to $CnFF
;
; A macro (CnXX) is used here so that this code can be easily generated for
; each slot, with minor variations.
;
; This is done because the hardware does not overlay the C1xx address space at
; C2xx, C3xx, etc. automatically. Instead, the base address for the ROM is
; $C000, and a read from $Cnxx is mapped to $0nxx in our ROM.
;
; ROM offsets $0000 to $00FF are not used.  $0nxx is used for slot n.  From
; $0800 to $0FFE is the first 2K expansion ROM page. Since we are emulating a 
; 32K pageable ROm there are 14 more pages after that.
;
;------------------------------------------------------------------------------

.macro CnXX SLOT, SIGNATURE_BYTE_4

   .local start
      
start:
 ; Print string @ $c800
 rts

  .RES   $C000+(SLOT*$100)+$FF-*,$77  ; skip to $CnFF
	.byte $77
	
.endmacro


.macro C800 bank, fill

	.org $c800

   ASSERT *=$C800, "User-config section must start at $C800"
   
	.byte "BANK", bank
	      
	.local ALLOW_OVERFLOWING_ROM

ALLOW_OVERFLOWING_ROM = 0

.if ALLOW_OVERFLOWING_ROM
   .warning "Allowing ROM overflow, just to see how big the code is."
   .byte $FF ; for $CFFE
   .byte $FF ; for $CFFF
;   .out .concat("Section Counter = ", .string(*))
   
.else
;   .out .concat("Free bytes in EEPROM = ", .string($CFFE-*))
   .RES $CFFF-*, fill    ; $7FE bytes avail EEPROM from $C800 to $CFFE.
   .byte $FF           ; touching $CFEF turns off the AUX ROM
   ASSERT *=$D000, "User-config section must end at $D000"
.endif



.endmacro

;-------------------------------------------------------------------------
; For $C1xx through $C7xx, expand the CnXX macro 7 times to generate the
; seven variants of the slot-ROM code.
;
; Parameters for following macro invocations:
;     CnXX SLOT, SIGNATURE_BYTE_4
;
; A signature byte of $3C is same as Disk ][, and turns SmartPort support off,
; and allows autoboot on ][+, ][e.
;
; A signature byte of $00 signals SmartPort support, allow 4 drives in slot 5,
; but disables autoboot on ][+, ][e.
;-------------------------------------------------------------------------
   CnXX 1, $3C         ; Slot PROM code for slot 1. SmartPort disabled. Autoboot.
   CnXX 2, $3C         ; Slot PROM code for slot 2. SmartPort disabled. Autoboot.
   CnXX 3, $3C         ; Slot PROM code for slot 3. SmartPort disabled. Autoboot.
   CnXX 4, $3C         ; Slot PROM code for slot 4. SmartPort disabled. Autoboot.
   CnXX 5, $3C         ; Slot PROM code for slot 5. SmartPort enabled. No Autoboot.
   CnXX 6, $3C         ; Slot PROM code for slot 6. SmartPort disabled. Autoboot.
.listbytes unlimited   ; Show all of the code bytes for the 7th slot
   CnXX 7, $3C         ; Slot PROM code for slot 7. SmartPort disabled. Autoboot.

.listbytes      12              ; revert back to normal listing mode for the rest
;-------------------- End of Peripheral Card ROM Space ----------------------

;----------------- Start of I/O expansion ROM (AUX ROM) Space ---------------
; $C800
;
; This code area is absolute and can use absolute addressing.  
;
; In this implmementation we are emulating a 32K onboard EEPROM so we have 15 
; sections of 2KB to fill
;
;---------------------------------------------------------------------------


;------------------------------------------------------------------------------

; Not happy with this but it's only eye candy for now

	C800 "01", $01
	C800 "02", $02
	C800 "03", $03
	C800 "04", $04
	C800 "05", $05
	C800 "06", $06
	C800 "07", $07
	C800 "08", $08
	C800 "09", $09
	C800 "10", $0A
	C800 "11", $0B
	C800 "12", $0C
	C800 "13", $0D
	C800 "14", $0E
	C800 "15", $0F	