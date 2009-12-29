/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2010, Tom Charlesworth, Michael Pohoreski

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Description: Disk Image
 *
 * Author: Various
 */


#include "StdAfx.h"
#include "DiskImage.h"
#include "DiskImageHelper.h"
#pragma  hdrstop


#ifndef GZ_SUFFIX
#  define GZ_SUFFIX ".gz"
#endif
#define SUFFIX_LEN (sizeof(GZ_SUFFIX)-1)


static CDiskImageHelper sg_DiskImageHelper;

//===========================================================================

BOOL ImageBoot (HIMAGE imagehandle)
{
	ImageInfo* ptr = (ImageInfo*) imagehandle;
	BOOL result = 0;

	if (ptr->pImageType->AllowBoot())
		result = ptr->pImageType->Boot(ptr);

	if (result)
		ptr->writeprotected = 1;

	return result;
}

//===========================================================================

void ImageClose (HIMAGE imagehandle)
{
	ImageInfo* ptr = (ImageInfo*) imagehandle;

	if (ptr->file != INVALID_HANDLE_VALUE)
		CloseHandle(ptr->file);

	for (UINT track = 0; track < ptr->uNumTracks; track++)
	{
		if (!ptr->validtrack[track])
		{
			DeleteFile(ptr->filename);
			break;
		}
	}

	if (ptr->header)
		VirtualFree(ptr->header, 0, MEM_RELEASE);

	VirtualFree(ptr, 0, MEM_RELEASE);
}

//===========================================================================

void ImageDestroy ()
{
	VirtualFree(sg_DiskImageHelper.GetWorkBuffer(), 0, MEM_RELEASE);
	sg_DiskImageHelper.SetWorkBuffer(NULL);
}

//===========================================================================

void ImageInitialize ()
{
	LPBYTE pBuffer = (LPBYTE) VirtualAlloc(NULL, TRACK_DENIBBLIZED_SIZE*2, MEM_COMMIT, PAGE_READWRITE);
	sg_DiskImageHelper.SetWorkBuffer(pBuffer);
}

//===========================================================================

int ImageOpen (LPCTSTR  imagefilename,
               HIMAGE  *hDiskImage_,
               BOOL    *pWriteProtected_,
               BOOL     bCreateIfNecessary)
{
	if (! (imagefilename && hDiskImage_ && pWriteProtected_ && sg_DiskImageHelper.GetWorkBuffer()))
		return IMAGE_ERROR_BAD_POINTER;

    const size_t uStrLen = strlen(imagefilename);
    if (uStrLen > SUFFIX_LEN && strcmp(imagefilename+uStrLen-SUFFIX_LEN, GZ_SUFFIX) == 0)
	{
		return IMAGE_ERROR_UNABLE_TO_OPEN;
	}

	// TRY TO OPEN THE IMAGE FILE
	HANDLE file = INVALID_HANDLE_VALUE;

	if (! *pWriteProtected_)
		file = CreateFile(imagefilename,
                      GENERIC_READ | GENERIC_WRITE,
                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                      (LPSECURITY_ATTRIBUTES)NULL,
                      OPEN_EXISTING,
                      FILE_ATTRIBUTE_NORMAL,
                      NULL);

	// File may have read-only attribute set, so try to open as read-only.
	if (file == INVALID_HANDLE_VALUE)
	{
		file = CreateFile(
			imagefilename,
			GENERIC_READ,
			FILE_SHARE_READ,
			(LPSECURITY_ATTRIBUTES)NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL );
		
		if (file != INVALID_HANDLE_VALUE)
			*pWriteProtected_ = 1;
	}

	if ((file == INVALID_HANDLE_VALUE) && bCreateIfNecessary)
		file = CreateFile(
			imagefilename,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			(LPSECURITY_ATTRIBUTES)NULL,
			CREATE_NEW,
			FILE_ATTRIBUTE_NORMAL,
			NULL );

	// IF WE AREN'T ABLE TO OPEN THE FILE, RETURN
	if (file == INVALID_HANDLE_VALUE)
		return IMAGE_ERROR_UNABLE_TO_OPEN;

	// DETERMINE THE FILE'S EXTENSION AND CONVERT IT TO LOWERCASE
	LPCTSTR imagefileext = imagefilename;
	if (_tcsrchr(imagefileext,TEXT('\\')))
	imagefileext = _tcsrchr(imagefileext,TEXT('\\'))+1;
	if (_tcsrchr(imagefileext,TEXT('.')))
	imagefileext = _tcsrchr(imagefileext,TEXT('.'));
	TCHAR szExt[_MAX_EXT];
	_tcsncpy(szExt,imagefileext,_MAX_EXT);
	CharLowerBuff(szExt,_tcslen(szExt));

	DWORD dwSize = GetFileSize(file, NULL);
	DWORD dwOffset = 0;

	CImageBase* pImageType = NULL;
	sg_DiskImageHelper.SetNumTracksInImage( (dwSize > 0) ? TRACKS_STANDARD : 0 );	// Assume default # tracks

	if (dwSize > 0)
	{
		// MAP THE FILE INTO MEMORY FOR USE BY THE DETECTION FUNCTIONS
		HANDLE mapping = CreateFileMapping(
			file,
			(LPSECURITY_ATTRIBUTES)NULL,
			PAGE_READONLY,
			0,0,NULL );

		LPBYTE pImageView = (LPBYTE) MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);

		if (pImageView)
		{
			pImageType = sg_DiskImageHelper.Detect(pImageView, dwSize, szExt, dwOffset);

			// CLOSE THE MEMORY MAPPING
			UnmapViewOfFile(pImageView);
		}

	    CloseHandle(mapping);
	}
	else
	{
		pImageType = sg_DiskImageHelper.GetImageForCreation(szExt);
	}

	// IF THE FILE MATCHES A KNOWN FORMAT...
	if (pImageType != NULL)
	{
		// CREATE A RECORD FOR THE FILE, AND RETURN AN IMAGE HANDLE
		*hDiskImage_ = (HIMAGE) VirtualAlloc(NULL, sizeof(ImageInfo), MEM_COMMIT, PAGE_READWRITE);
		if (*hDiskImage_)
		{
			const UINT uNumTracksInImage = sg_DiskImageHelper.GetNumTracksInImage();

			ZeroMemory(*hDiskImage_, sizeof(ImageInfo));
			_tcsncpy(((ImageInfo*)*hDiskImage_)->filename, imagefilename, MAX_PATH);
			((ImageInfo*)*hDiskImage_)->pImageType		= pImageType;
			((ImageInfo*)*hDiskImage_)->file			= file;
			((ImageInfo*)*hDiskImage_)->offset			= dwOffset;
			((ImageInfo*)*hDiskImage_)->writeprotected	= *pWriteProtected_;
			((ImageInfo*)*hDiskImage_)->uNumTracks		= uNumTracksInImage;

			for (UINT uTrack = 0; uTrack < uNumTracksInImage; uTrack++)
		        ((ImageInfo*)*hDiskImage_)->validtrack[uTrack] = (dwSize > 0);

			return IMAGE_ERROR_NONE;
		}
	}

	CloseHandle(file);

	if (dwSize == 0)
		DeleteFile(imagefilename);

	return IMAGE_ERROR_BAD_SIZE;
}

//===========================================================================

void ImageReadTrack (HIMAGE  imagehandle,
                     int     track,
                     int     quartertrack,
                     LPBYTE  trackimagebuffer,
                     int    *nibbles)
{
	ImageInfo* ptr = (ImageInfo*) imagehandle;
	if (ptr->pImageType->AllowRW() && ptr->validtrack[track])
	{
		ptr->pImageType->Read(ptr, track, quartertrack, trackimagebuffer, nibbles);
	}
	else
	{
		for (*nibbles = 0; *nibbles < NIBBLES; (*nibbles)++)
			trackimagebuffer[*nibbles] = (BYTE)(rand() & 0xFF);
	}
}

//===========================================================================

void ImageWriteTrack (HIMAGE imagehandle,
                      int    track,
                      int    quartertrack,
                      LPBYTE trackimage,
                      int    nibbles)
{
	ImageInfo* ptr = (ImageInfo*) imagehandle;
	if (ptr->pImageType->AllowRW() && !ptr->writeprotected)
	{
		ptr->pImageType->Write(ptr, track, quartertrack, trackimage, nibbles);
		ptr->validtrack[track] = 1;
	}
}

//===========================================================================

int ImageGetNumTracks(HIMAGE imagehandle)
{
	ImageInfo* ptr = (ImageInfo*) imagehandle;
	return ptr ? ptr->uNumTracks : 0;
}
