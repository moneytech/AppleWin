a65 -b -l HDDRVR.A65 >hddrvr.lst
@del HDDRVR.BIN
rename 6502.bin HDDRVR.BIN
copy HDDRVR.BIN ..\..\resource
Echo Do not forget to embed HDDRVR.BIN into offset 0700 in AppleHDD_IN.rom and Apple_EX.rom