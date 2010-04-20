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

; Description: Firmware for harddisk card
;
; Author: Copyright (c) 2005, Robert Hoem
;

; Modified by Tom Charlesworth:
; . Fixed so it can be assembled by a65 v1.06
; . Fixed so that ProDOS entrypoint is $c70a
; . TO DO: Make code relocatable
;
;
; Modifed by Glenn Jones
; Changed hd_nextbyte from F8 to FA
; Made driver relocatable based on Rich Drehers CFFA driver
; Converted to ca65 and ld65 tool chain
;


.segment "EXEHDR"            ; just to keep the linker happy
.segment "STARTUP"           ; just to keep the linker happy


; constants
IOBase 				= $C080         ; indexed with X = $n0
hd_execute 		= IOBase+0
hd_error 			= IOBase+1
hd_command 		= IOBase+2
hd_unitnum 		= IOBase+3
hd_memblock 	= IOBase+4
hd_diskblock 	= IOBase+6
hd_nextbyte 	= IOBase+$0a


command = $42
unitnum = $43
memblock = $44
diskblock = $46

slot6   = $c600
OS      = $0801

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
   .byte "AppleWin HDD Firmware",CR,CR,CR
   .byte "See <http://applewin.berlios.de/>."
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
   .local Entrypoint_ProDOS
   .local Entrypoint_SmartPort
   .local Entrypoint
   .local Bootstrap
   .local noerr0
   .local Entrypont_Cx46
   .local hdboot
   .local goload
   .local cmdproc
   .local skipSread
   .local noerr2
   .local sread
   .local loop1
   .local loop2
      
start:

; Autoboot and ProDOS look at the following few opcodes to detect block devices
 lda #$20
 lda #$00
 lda #$03
 lda #$3C
 bne Bootstrap

   ASSERT (* = $C00A+$100*SLOT), "Should be at $Cn0A"
Entrypoint_ProDOS:		; $cx0a - ProDOS entrypoint
 sec
 bcs Entrypoint

Entrypoint_SmartPort:	; $cx0d - SmartPort entrypoint (not supported)
 clc
 
Entrypoint:				; $cx0e - entrypoint?
 bcs cmdproc
 brk

;;

Bootstrap:
; Lets check to see if there's an image ready
 lda #$00
 sta hd_command+SLOT*16

; Slot 7, disk 1
 lda #SLOT*16							;#$70	; Slot# << 4
 sta hd_unitnum+SLOT*16
 lda hd_execute+SLOT*16

; error capturing code.  Applewin is picky
; about code assigning data to registers and
; memory.  The safest method is via I/O port
 pha
 lda hd_error+SLOT*16
 clc
 cmp #1
 bne noerr0
 sec
noerr0:
 pla
 bcc hdboot

; no image ready, boot diskette image instead
 jmp slot6

 ; 24 unused bytes

;*= $cx46	; org $cx46
.RES $C000+(SLOT*$100)+$46-*,$EA  ; fill with NOPs to $Cn46

ASSERT (* = $C046+$100*SLOT), "Should be at $Cn46"
Entrypont_Cx46:	; Old f/w 'cmdproc' entrypoint
				; Keep this for any DOSMaster HDD images created with old AppleWin HDD f/w.
				; DOSMaster hardcodes the entrypoint addr into its bootstrapping code:
				; - So DOSMaster images are tied to the HDD's controller's f/w
 sec
 bcs Entrypoint
 
;
; image ready.  Lets boot from it.
; we want to load block 1 from slotx,d1 to $800 then jump there
hdboot:
 lda #SLOT*16	;#$70	; Slot# << 4
 sta unitnum
 lda #$0
 sta memblock
 sta diskblock
 sta diskblock+1
 lda #$8
 sta memblock+1
 lda #$1
 sta command
 jsr cmdproc
 bcc goload
 jmp slot6
goload:

; X=device
 ldx #SLOT*16	;#$70	; Slot# << 4
 jmp OS

; entry point for ProDOS' block driver
; simple really. Copy the command from $42..$47
; to our I/O ports then execute command
cmdproc:
 clc
 lda $42
 sta hd_command+SLOT*16
 lda $43
 sta hd_unitnum+SLOT*16
 lda $44
 sta hd_memblock+SLOT*16
 lda $45
 sta hd_memblock+SLOT*16+1
 lda $46
 sta hd_diskblock+SLOT*16
 lda $47
 sta hd_diskblock+SLOT*16+1
 lda hd_execute+SLOT*16

; check for error
 pha
 lda command
 cmp #1
 bne skipSread
 jsr sread
skipSread:
 lda hd_error+SLOT*16
 clc
 cmp #1
 bne noerr2
 sec
noerr2:
 pla
 rts


; if there's no error, then lets read the block into memory
; because Applewin is picky about memory management, here's what I did:
; on read, hd_nextbyte = buffer[0], therefore we'll read that byte 256 times (in which
; the emulated code increments the buffer by 1 on each read) to (memblock),y
; increment memblock+1 and read the secod 256 bytes via hd_nextbyte.
;
; if I could figure out how to consistantly get applewin to update it's memory regions all
; this code can be moved into the emulation code (although, this is how I'd build the hardware
; anyway...)

sread:
 tya
 pha
 ldy #0
loop1:
 lda hd_nextbyte+SLOT*16
 sta (memblock),y
 iny
 bne loop1
 inc memblock+1
 ldy #0
loop2:
 lda hd_nextbyte+SLOT*16
 sta (memblock),y
 iny
 bne loop2
 pla
 tay
 rts

;-------------------------------------------------------------------------
; $CnF5 - $CnFF -- Boot ROM signature, version, and capability ID bytes
;
; $CnF5 to $CnFA were defined by CFFA, but should no longer be used.
;     Instead, see the similar bytes in the Aux ROM ($C8xx) area.
;
; $CnFB to $CnFF are defined by ProDOS and SmartPort.
;-------------------------------------------------------------------------
;was *= $c7fc	; org $c7fc

   .RES   $C000+(SLOT*$100)+$F7-*,$77  ; skip to $CnF5

   .byte "HDD", $FF           ; $CnF6..CnFA: Card ID, old 1.x version #

; $CnFB: SmartPort status byte
   .byte $0                    ; Not Extended; not SCSI; not RAM card
                               ; Even if not supporting SmartPort, we need a
                               ; zero at $CnFB so Apple's RAMCard driver
                               ; doesn't mistake us for a "Slinky" memory
                               ; card.

; $CsFE = status bits (BAP p7-14)
;  7 = medium is removable
;  6 = device is interruptable
;  5-4 = number of volumes (0..3 means 1..4)
;  3 = device supports Format call
;  2 = device can be written to
;  1 = device can be read from (must be 1)
;  0 = device status can be read (must be 1)

; $C7 = Removable, Interruptable, #Volumes=1, Supports write/read/status
; $D7 = Removable, Interruptable, #Volumes=2, Supports write/read/status
; $BF = Removable, Interruptable, #Volumes=4, Supports format/write/read/status (KEGS / IIGS)


; datablock.  This starts near the end of the firmware (at offset $FC)
;; data

   ASSERT (* = $C0FC+$100*SLOT), "Should be at $CnFC"

 .word $7fff ; how many blocks are on the device.
 .byte $D7 ; specifics about the device (number of drives, read/write/format capability, etc)
 .byte <Entrypoint_ProDOS ; entry point offset for ProDOS (must be $0a)

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