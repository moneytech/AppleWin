#pragma once

void    PrintDestroy();
void    PrintLoadRom(LPBYTE pCxRomPeripheral, UINT uSlot);
void    PrintReset();
void    PrintUpdate(DWORD);
extern bool       g_bDumpToPrinter;
extern bool       g_bConvertEncoding;
