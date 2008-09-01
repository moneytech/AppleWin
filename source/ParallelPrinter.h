#pragma once

void    PrintDestroy();
void    PrintLoadRom(LPBYTE pCxRomPeripheral, UINT uSlot);
void    PrintReset();
void    PrintUpdate(DWORD);
void    Printer_SetFilename(char* pszFilename);
char*   Printer_GetFilename();
extern bool       g_bDumpToPrinter;
extern bool       g_bConvertEncoding;
extern bool       g_bFilterUnprintable;
extern bool       g_bPrinterAppend;

