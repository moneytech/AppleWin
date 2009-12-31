#pragma once

#include "DiskImage.h"

enum Drive_e
{
	DRIVE_1 = 0,
	DRIVE_2,
	NUM_DRIVES
};

const bool IMAGE_USE_FILES_WRITE_PROTECT_STATUS = false;
const bool IMAGE_FORCE_WRITE_PROTECTED = true;
const bool IMAGE_DONT_CREATE = false;
const bool IMAGE_CREATE = true;

extern BOOL enhancedisk;
extern string DiskPathFilename[NUM_DRIVES];

void    DiskInitialize (); // DiskIIManagerStartup()
void    DiskDestroy (); // no, doesn't "destroy" the disk image.  DiskIIManagerShutdown()

void    DiskBoot ();
void    DiskEject( const int iDrive );
LPCTSTR DiskGetFullName(const int iDrive);


enum Disk_Status_e
{
	DISK_STATUS_OFF  ,
	DISK_STATUS_READ ,
	DISK_STATUS_WRITE,
	DISK_STATUS_PROT ,
	NUM_DISK_STATUS
};
void    DiskGetLightStatus (int *pDisk1Status_,int *pDisk2Status_);

LPCTSTR DiskGetName(const int iDrive);
ImageError_e DiskInsert(const int iDrive, LPCTSTR pszImageFilename, const bool bForceWriteProtected, const bool bCreateIfNecessary);
BOOL    DiskIsSpinning ();
void    DiskNotifyInvalidImage(LPCTSTR pszImageFilename, const ImageError_e Error);
void    DiskReset ();
bool    DiskGetProtect( const int iDrive );
void    DiskSetProtect( const int iDrive, const bool bWriteProtect );
void    DiskSelect (int);
void    DiskUpdatePosition (DWORD);
bool    DiskDriveSwap();
void    DiskLoadRom(LPBYTE pCxRomPeripheral, UINT uSlot);
DWORD   DiskGetSnapshot(SS_CARD_DISK2* pSS, DWORD dwSlot);
DWORD   DiskSetSnapshot(SS_CARD_DISK2* pSS, DWORD dwSlot);

void Disk_LoadLastDiskImage( int iDrive );
void Disk_SaveLastDiskImage( int iDrive );

bool Disk_ImageIsWriteProtected(const int iDrive);
bool Disk_IsDriveEmpty(const int iDrive);
