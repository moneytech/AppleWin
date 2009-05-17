#pragma once

bool    APLSPI_CardIsEnabled();
void    APLSPI_SetEnabled(bool bEnabled);
//LPCTSTR HD_GetFullName (int drive);
VOID    APLSPI_Load_Rom(LPBYTE pCxRomPeripheral, UINT uSlot);
VOID	APLSPI_Cleanup();
BYTE __stdcall APLSPI_Update_Rom(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCyclesLeft);
//BOOL    HD_InsertDisk2(int nDrive, LPCTSTR pszFilename);
//BOOL    HD_InsertDisk(int nDrive, LPCTSTR imagefilename);
//void    HD_Select(int nDrive);

