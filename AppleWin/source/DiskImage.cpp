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


#define GZ_SUFFIX ".gz"
#define GZ_SUFFIX_LEN (sizeof(GZ_SUFFIX)-1)

#define ZIP_SUFFIX ".zip"
#define ZIP_SUFFIX_LEN (sizeof(ZIP_SUFFIX)-1)


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

void ImageClose(const HIMAGE hDiskImage, const bool bOpenError /*=false*/)
{
	ImageInfo* ptr = (ImageInfo*) hDiskImage;

	if (ptr->hFile != INVALID_HANDLE_VALUE)
		CloseHandle(ptr->hFile);

	if (!bOpenError)
	{
		for (UINT uTrack = 0; uTrack < ptr->uNumTracks; uTrack++)
		{
			if (!ptr->ValidTrack[uTrack])
			{
				// What's the reason for this?
				DeleteFile(ptr->szFilename);
				break;
			}
		}
	}

	delete [] ptr->pImageBuffer;

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

static ImageError_e CheckGZipFile(LPCTSTR pszImageFilename, ImageInfo* pImageInfo, bool* pWriteProtected_)
{
	gzFile hGZFile = gzopen(pszImageFilename, "rb");
	if (hGZFile == NULL)
		return eIMAGE_ERROR_UNABLE_TO_OPEN_GZ;

	const UINT MAX_UNCOMPRESSED_SIZE = 512*1024;
	pImageInfo->pImageBuffer = new BYTE[MAX_UNCOMPRESSED_SIZE];
	if (!pImageInfo->pImageBuffer)
		return eIMAGE_ERROR_BAD_POINTER;

	int nLen = gzread(hGZFile, pImageInfo->pImageBuffer, MAX_UNCOMPRESSED_SIZE);
	if (nLen < 0 || nLen == MAX_UNCOMPRESSED_SIZE)
		return eIMAGE_ERROR_BAD_SIZE;

	int nRes = gzclose(hGZFile);
	hGZFile = NULL;
	if (nRes != Z_OK)
		return eIMAGE_ERROR_GZ;

	//

	DWORD dwSize = nLen;
	sg_DiskImageHelper.SetNumTracksInImage( (dwSize > 0) ? TRACKS_STANDARD : 0 );	// Assume default # tracks - done before Detect()

	DWORD dwOffset = 0;
	CImageBase* pImageType = sg_DiskImageHelper.Detect(pImageInfo->pImageBuffer, dwSize, "", dwOffset);

	if (!pImageType)
		return eIMAGE_ERROR_UNSUPPORTED;

	const eImageType Type = pImageType->GetType();
	if (Type == eImageAPL || Type == eImageIIE || Type == eImagePRG)
		return eIMAGE_ERROR_UNSUPPORTED;

	pImageInfo->FileType = eFileGZip;
	pImageInfo->uOffset = dwOffset;
	pImageInfo->pImageType = pImageType;
	pImageInfo->uImageSize = dwSize;

	return eIMAGE_ERROR_NONE;
}

//-------------------------------------

static ImageError_e CheckZipFile(LPCTSTR pszImageFilename, ImageInfo* pImageInfo, bool* pWriteProtected_)
{
	zlib_filefunc_def ffunc;
	fill_win32_filefunc(&ffunc);

	unzFile hZipFile = unzOpen2(pszImageFilename, &ffunc);
	if (hZipFile == NULL)
		return eIMAGE_ERROR_UNABLE_TO_OPEN_ZIP;

	unz_global_info global_info;
	int nRes = unzGetGlobalInfo(hZipFile, &global_info);
	if (nRes != UNZ_OK)
		return eIMAGE_ERROR_ZIP;

	nRes = unzGoToFirstFile(hZipFile);	// Only support 1st file in zip archive for now
	if (nRes != UNZ_OK)
		return eIMAGE_ERROR_ZIP;

	unz_file_info file_info;
	char szFilename[MAX_PATH];
	char szExtraField[MAX_PATH];
	char szComment[MAX_PATH];
	nRes = unzGetCurrentFileInfo(hZipFile, &file_info, szFilename, MAX_PATH, szExtraField, MAX_PATH, szComment, MAX_PATH);
	if (nRes != UNZ_OK)
		return eIMAGE_ERROR_ZIP;

	const UINT uFileSize = file_info.uncompressed_size;
	pImageInfo->pImageBuffer = new BYTE[uFileSize];
	if (!pImageInfo->pImageBuffer)
		return eIMAGE_ERROR_BAD_POINTER;

	nRes = unzOpenCurrentFile(hZipFile);
	if (nRes != UNZ_OK)
		return eIMAGE_ERROR_ZIP;

	int nLen = unzReadCurrentFile(hZipFile, pImageInfo->pImageBuffer, uFileSize);
	if (nLen < 0)
	{
		unzCloseCurrentFile(hZipFile);	// Must CloseCurrentFile before Close
		return eIMAGE_ERROR_UNSUPPORTED;
	}

	nRes = unzCloseCurrentFile(hZipFile);
	if (nRes != UNZ_OK)
		return eIMAGE_ERROR_ZIP;

	nRes = unzClose(hZipFile);
	hZipFile = NULL;
	if (nRes != UNZ_OK)
		return eIMAGE_ERROR_ZIP;

	//

	DWORD dwSize = nLen;
	sg_DiskImageHelper.SetNumTracksInImage( (dwSize > 0) ? TRACKS_STANDARD : 0 );	// Assume default # tracks - done before Detect()

	DWORD dwOffset = 0;
	CImageBase* pImageType = sg_DiskImageHelper.Detect(pImageInfo->pImageBuffer, dwSize, "", dwOffset);

	if (!pImageType)
		return eIMAGE_ERROR_UNSUPPORTED;

	const eImageType Type = pImageType->GetType();
	if (Type == eImageAPL || Type == eImageIIE || Type == eImagePRG)
		return eIMAGE_ERROR_UNSUPPORTED;

	*pWriteProtected_ = 1;	// Zip files are read-only (for now)

	pImageInfo->FileType = eFileZip;
	pImageInfo->uOffset = dwOffset;
	pImageInfo->pImageType = pImageType;
	pImageInfo->uImageSize = dwSize;

	return eIMAGE_ERROR_NONE;
}

//-------------------------------------

static ImageError_e CheckNormalFile(LPCTSTR pszImageFilename, ImageInfo* pImageInfo, bool* pWriteProtected_, const bool bCreateIfNecessary)
{
	// TRY TO OPEN THE IMAGE FILE

	HANDLE& hFile = pImageInfo->hFile;
	hFile = INVALID_HANDLE_VALUE;

	if (! *pWriteProtected_)
		hFile = CreateFile(pszImageFilename,
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
			pszImageFilename,
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
			pszImageFilename,
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
	LPCTSTR imagefileext = pszImageFilename;
	if (_tcsrchr(imagefileext,TEXT('\\')))
	imagefileext = _tcsrchr(imagefileext,TEXT('\\'))+1;
	if (_tcsrchr(imagefileext,TEXT('.')))
	imagefileext = _tcsrchr(imagefileext,TEXT('.'));
	TCHAR szExt[_MAX_EXT];
	_tcsncpy(szExt,imagefileext,_MAX_EXT);
	CharLowerBuff(szExt,_tcslen(szExt));

	DWORD dwSize = GetFileSize(hFile, NULL);
	sg_DiskImageHelper.SetNumTracksInImage(TRACKS_STANDARD);	// Assume default # tracks - done before Detect()

	CImageBase* pImageType = NULL;
	DWORD dwOffset = 0;

	if (dwSize > 0)
	{
		pImageInfo->pImageBuffer = new BYTE [dwSize];
		if (!pImageInfo->pImageBuffer)
			return eIMAGE_ERROR_BAD_POINTER;

		DWORD dwBytesRead;
		BOOL bRes = ReadFile(hFile, pImageInfo->pImageBuffer, dwSize, &dwBytesRead, NULL);
		if (!bRes || dwSize != dwBytesRead)
			return eIMAGE_ERROR_BAD_SIZE;

		pImageType = sg_DiskImageHelper.Detect(pImageInfo->pImageBuffer, dwSize, szExt, dwOffset);
	}
	else	// Create (or pre-existing zero-length file)
	{
		pImageType = sg_DiskImageHelper.GetImageForCreation(szExt);

		if (pImageType)
		{
			dwSize = pImageType->GetTrackSizeForCreate() * TRACKS_STANDARD;
			_ASSERT(dwSize);	// Asserts on a code bug
			if (!dwSize)
				return eIMAGE_ERROR_UNSUPPORTED;

			pImageInfo->pImageBuffer = new BYTE [dwSize];
			if (!pImageInfo->pImageBuffer)
				return eIMAGE_ERROR_BAD_POINTER;

			ZeroMemory(pImageInfo->pImageBuffer, dwSize);
		}
	}

	//

	if (!pImageType)
	{
		CloseHandle(hFile);
		hFile = INVALID_HANDLE_VALUE;

		if (dwSize == 0)
			DeleteFile(pszImageFilename);

		return eIMAGE_ERROR_UNSUPPORTED;
	}

	pImageInfo->FileType = eFileNormal;
	pImageInfo->uOffset = dwOffset;
	pImageInfo->pImageType = pImageType;
	pImageInfo->uImageSize = dwSize;

	return eIMAGE_ERROR_NONE;
}

//=====================================

ImageError_e ImageOpen(	LPCTSTR pszImageFilename,
						HIMAGE* hDiskImage_,
						bool* pWriteProtected_,
						const bool bCreateIfNecessary)
{
	if (! (pszImageFilename && hDiskImage_ && pWriteProtected_ && sg_DiskImageHelper.GetWorkBuffer()))
		return eIMAGE_ERROR_BAD_POINTER;

	// CREATE A RECORD FOR THE FILE, AND RETURN AN IMAGE HANDLE
	*hDiskImage_ = (HIMAGE) VirtualAlloc(NULL, sizeof(ImageInfo), MEM_COMMIT, PAGE_READWRITE);
	if (*hDiskImage_ == NULL)
		return eIMAGE_ERROR_BAD_POINTER;

	ZeroMemory(*hDiskImage_, sizeof(ImageInfo));
	ImageInfo* pImageInfo = (ImageInfo*) *hDiskImage_;
	pImageInfo->hFile = INVALID_HANDLE_VALUE;

	//

	ImageError_e Err;
    const size_t uStrLen = strlen(pszImageFilename);

    if (uStrLen > GZ_SUFFIX_LEN && strcmp(pszImageFilename+uStrLen-GZ_SUFFIX_LEN, GZ_SUFFIX) == 0)
	{
		Err = CheckGZipFile(pszImageFilename, pImageInfo, pWriteProtected_);
	}
    else if (uStrLen > ZIP_SUFFIX_LEN && strcmp(pszImageFilename+uStrLen-ZIP_SUFFIX_LEN, ZIP_SUFFIX) == 0)
	{
		Err = CheckZipFile(pszImageFilename, pImageInfo, pWriteProtected_);
	}
	else
	{
		Err = CheckNormalFile(pszImageFilename, pImageInfo, pWriteProtected_, bCreateIfNecessary);
	}

	if (!pImageInfo->pImageType)
		Err = eIMAGE_ERROR_UNSUPPORTED;

	if (Err != eIMAGE_ERROR_NONE)
	{
		ImageClose(*hDiskImage_, true);
		*hDiskImage_ = (HIMAGE)0;
		return Err;
	}

	// THE FILE MATCHES A KNOWN FORMAT
	const UINT uNumTracksInImage = sg_DiskImageHelper.GetNumTracksInImage();

	_tcsncpy(pImageInfo->szFilename, pszImageFilename, MAX_PATH);
	pImageInfo->bWriteProtected	= *pWriteProtected_;
	pImageInfo->uNumTracks		= uNumTracksInImage;

	for (UINT uTrack = 0; uTrack < uNumTracksInImage; uTrack++)
		pImageInfo->ValidTrack[uTrack] = (pImageInfo->uImageSize > 0) ? 1 : 0;

	return eIMAGE_ERROR_NONE;
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
	if (ptr->pImageType->AllowRW() && ptr->ValidTrack[nTrack])
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
		ptr->ValidTrack[nTrack] = 1;
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
