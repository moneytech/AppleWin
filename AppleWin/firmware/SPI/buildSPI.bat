ca65 -v -t apple2 --cpu 6502 -l SPIRVR.s
ld65 -t apple2 SPIRVR.o -o AppleSPI_IN.rom
copy AppleSPI_IN.rom ..\..\resource
copy AppleSPI_IN.rom ..\..\resource\AppleSPI_EX.rom
Echo These files will replace AppleSPI_IN.rom and AppleSPI_EX.rom