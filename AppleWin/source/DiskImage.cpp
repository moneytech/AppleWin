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

BOOL ImageBoot(const HIMAGE hDiskImage)
{
	ImageInfo* ptr = (ImageInfo*) hDiskImage;
	BOOL result = 0;

	if (ptr->pImageType->AllowBoot())
		result = ptr->pImageType->Boot(ptr);

	if (result)
		ptr->bWriteProtected = 1;

	return result;
}

//===========================================================================

void ImageClose(const HIMAGE hDiskImage)
{
	ImageInfo* ptr = (ImageInfo*) hDiskImage;

	if (ptr->file != INVALID_HANDLE_VALUE)
		CloseHandle(ptr->file);
	if (ptr->hGZFile != NULL)
		gzclose(ptr->hGZFile);

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

void ImageDestroy(void)
{
	VirtualFree(sg_DiskImageHelper.GetWorkBuffer(), 0, MEM_RELEASE);
	sg_DiskImageHelper.SetWorkBuffer(NULL);
}

//===========================================================================

void ImageInitialize(void)
{
	LPBYTE pBuffer = (LPBYTE) VirtualAlloc(NULL, TRACK_DENIBBLIZED_SIZE*2, MEM_COMMIT, PAGE_READWRITE);
	sg_DiskImageHelper.SetWorkBuffer(pBuffer);
}

//===========================================================================

static ImageError_e CheckGZipFile(	LPCTSTR imagefilename,
									CImageBase*& pImageType,
									DWORD& dwOffset,
									gzFile& hGZFile,
									DWORD& dwSize,
									bool* pWriteProtected_)
{
	hGZFile = gzopen(imagefilename, "rb");
	if (hGZFile == NULL)
		return eIMAGE_ERROR_UNABLE_TO_OPEN_GZ;

	const UINT MAX_UNCOMPRESSED_SIZE = 512*1024;
	BYTE* pFileBuffer = new BYTE[MAX_UNCOMPRESSED_SIZE];
	if (!pFileBuffer)
	{
		gzclose(hGZFile);
		return eIMAGE_ERROR_BAD_POINTER;
	}

	int nLen = gzread(hGZFile, pFileBuffer, MAX_UNCOMPRESSED_SIZE);
	if (nLen < 0 || nLen == MAX_UNCOMPRESSED_SIZE)
	{
		gzclose(hGZFile);
		delete [] pFileBuffer;
		return eIMAGE_ERROR_BAD_SIZE;
	}

	dwSize = nLen;

	sg_DiskImageHelper.SetNumTracksInImage( (dwSize > 0) ? TRACKS_STANDARD : 0 );	// Assume default # tracks

	pImageType = sg_DiskImageHelper.Detect(pFileBuffer, nLen, "", dwOffset);
	delete [] pFileBuffer;

	if (pImageType && pImageType->GetType() == eImageIIE)
		return eIMAGE_ERROR_UNSUPPORTED;

	*pWriteProtected_ = 1;	// GZip files are read-only (for now)

	return eIMAGE_ERROR_NONE;
}

//-------------------------------------

static ImageError_e CheckNormalFile(	LPCTSTR imagefilename,
									CImageBase*& pImageType,
									DWORD& dwOffset,
									HANDLE& hFile,
									DWORD& dwSize,
									bool* pWriteProtected_,
									const bool bCreateIfNecessary)
{
	// TRY TO OPEN THE IMAGE FILE

	if (! *pWriteProtected_)
		hFile = CreateFile(imagefilename,
                      GENERIC_READ | GENERIC_WRITE,
                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                      (LPSECURITY_ATTRIBUTES)NULL,
                      OPEN_EXISTING,
                      FILE_ATTRIBUTE_NORMAL,
                      NULL);

	// File may have read-only attribute set, so try to open as read-only.
	if (hFile == INVALID_HANDLE_VALUE)
	{
		hFile = CreateFile(
			imagefilename,
			GENERIC_READ,
			FILE_SHARE_READ,
			(LPSECURITY_ATTRIBUTES)NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL );
		
		if (hFile != INVALID_HANDLE_VALUE)
			*pWriteProtected_ = 1;
	}

	if ((hFile == INVALID_HANDLE_VALUE) && bCreateIfNecessary)
		hFile = CreateFile(
			imagefilename,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			(LPSECURITY_ATTRIBUTES)NULL,
			CREATE_NEW,
			FILE_ATTRIBUTE_NORMAL,
			NULL );

	// IF WE AREN'T ABLE TO OPEN THE FILE, RETURN
	if (hFile == INVALID_HANDLE_VALUE)
		return eIMAGE_ERROR_UNABLE_TO_OPEN;

	// DETERMINE THE FILE'S EXTENSION AND CONVERT IT TO LOWERCASE
	LPCTSTR imagefileext = imagefilename;
	if (_tcsrchr(imagefileext,TEXT('\\')))
	imagefileext = _tcsrchr(imagefileext,TEXT('\\'))+1;
	if (_tcsrchr(imagefileext,TEXT('.')))
	imagefileext = _tcsrchr(imagefileext,TEXT('.'));
	TCHAR szExt[_MAX_EXT];
	_tcsncpy(szExt,imagefileext,_MAX_EXT);
	CharLowerBuff(szExt,_tcslen(szExt));

	dwSize = GetFileSize(hFile, NULL);

	sg_DiskImageHelper.SetNumTracksInImage( (dwSize > 0) ? TRACKS_STANDARD : 0 );	// Assume default # tracks

	if (dwSize > 0)
	{
		// MAP THE FILE INTO MEMORY FOR USE BY THE DETECTION FUNCTIONS
		HANDLE mapping = CreateFileMapping(
			hFile,
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

	return eIMAGE_ERROR_NONE;
}

//=====================================

ImageError_e ImageOpen(	LPCTSTR imagefilename,
						HIMAGE* hDiskImage_,
						bool* pWriteProtected_,
						const bool bCreateIfNecessary)
{
	if (! (imagefilename && hDiskImage_ && pWriteProtected_ && sg_DiskImageHelper.GetWorkBuffer()))
		return eIMAGE_ERROR_BAD_POINTER;

	CImageBase* pImageType = NULL;
	DWORD dwOffset = 0;
	gzFile hGZFile = NULL;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	DWORD dwSize = 0;

    const size_t uStrLen = strlen(imagefilename);
    if (uStrLen > SUFFIX_LEN && strcmp(imagefilename+uStrLen-SUFFIX_LEN, GZ_SUFFIX) == 0)
	{
		ImageError_e Err = CheckGZipFile(imagefilename, pImageType, dwOffset, hGZFile, dwSize, pWriteProtected_);
		if (Err != eIMAGE_ERROR_NONE)
			return Err;
	}
	else
	{
		ImageError_e Err = CheckNormalFile(imagefilename, pImageType, dwOffset, hFile, dwSize, pWriteProtected_, bCreateIfNecessary);
		if (Err != eIMAGE_ERROR_NONE)
			return Err;
	}

	//

	// IF THE FILE MATCHES A KNOWN FORMAT...
	if (pImageType)
	{
		// CREATE A RECORD FOR THE FILE, AND RETURN AN IMAGE HANDLE
		*hDiskImage_ = (HIMAGE) VirtualAlloc(NULL, sizeof(ImageInfo), MEM_COMMIT, PAGE_READWRITE);
		if (*hDiskImage_)
		{
			ZeroMemory(*hDiskImage_, sizeof(ImageInfo));

			const UINT uNumTracksInImage = sg_DiskImageHelper.GetNumTracksInImage();
			ImageInfo* pImageInfo = (ImageInfo*) *hDiskImage_;

			_tcsncpy(pImageInfo->filename, imagefilename, MAX_PATH);
			pImageInfo->pImageType		= pImageType;
			pImageInfo->file			= hFile;
			pImageInfo->hGZFile			= hGZFile;
			pImageInfo->offset			= dwOffset;
			pImageInfo->bWriteProtected	= *pWriteProtected_;
			pImageInfo->uNumTracks		= uNumTracksInImage;

			for (UINT uTrack = 0; uTrack < uNumTracksInImage; uTrack++)
		        pImageInfo->validtrack[uTrack] = (dwSize > 0);

			return eIMAGE_ERROR_NONE;
		}
	}

	CloseHandle(hFile);

	if (dwSize == 0)
		DeleteFile(imagefilename);

	return eIMAGE_ERROR_BAD_SIZE;
}

//===========================================================================

void ImageReadTrack(	const HIMAGE hDiskImage,
						const int nTrack,
						const int nQuarterTrack,
						LPBYTE pTrackImageBuffer,
						int* pNibbles)
{
	_ASSERT(nTrack >= 0);
	if (nTrack < 0)
		return;

	ImageInfo* ptr = (ImageInfo*) hDiskImage;
	if (ptr->pImageType->AllowRW() && ptr->validtrack[nTrack])
	{
		ptr->pImageType->Read(ptr, nTrack, nQuarterTrack, pTrackImageBuffer, pNibbles);
	}
	else
	{
		for (*pNibbles = 0; *pNibbles < NIBBLES; (*pNibbles)++)
			pTrackImageBuffer[*pNibbles] = (BYTE)(rand() & 0xFF);
	}
}

//===========================================================================

void ImageWriteTrack(	const HIMAGE hDiskImage,
						const int nTrack,
						const int nQuarterTrack,
						LPBYTE pTrackImage,
						const int nNibbles)
{
	_ASSERT(nTrack >= 0);
	if (nTrack < 0)
		return;

	ImageInfo* ptr = (ImageInfo*) hDiskImage;
	if (ptr->pImageType->AllowRW() && !ptr->bWriteProtected)
	{
		ptr->pImageType->Write(ptr, nTrack, nQuarterTrack, pTrackImage, nNibbles);
		ptr->validtrack[nTrack] = 1;
	}
}

//===========================================================================

int ImageGetNumTracks(const HIMAGE hDiskImage)
{
	ImageInfo* ptr = (ImageInfo*) hDiskImage;
	return ptr ? ptr->uNumTracks : 0;
}

//===========================================================================

bool ImageIsWriteProtected(const HIMAGE hDiskImage)
{
	ImageInfo* ptr = (ImageInfo*) hDiskImage;
	return ptr ? ptr->bWriteProtected : true;
}
