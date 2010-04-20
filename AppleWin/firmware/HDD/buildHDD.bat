ca65 -v -t apple2 --cpu 6502 -l HDDRVR.s
ld65 -t apple2 HDDRVR.o -o AppleHDD_IN.rom
copy AppleHDD_IN.rom ..\..\resource
copy AppleHDD_IN.rom ..\..\resource\AppleHDD_EX.rom
Echo These files will replace AppleHDD_IN.rom and AppleHDD_EX.rom