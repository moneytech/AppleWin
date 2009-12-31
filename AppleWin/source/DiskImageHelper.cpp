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

/* Description: Disk Image Helper
 *
 * Author: Tom
 */


#include "stdafx.h"
#include "DiskImageHelper.h"
#include "DiskImage.h"


/* DO logical order  0 1 2 3 4 5 6 7 8 9 A B C D E F */
/*    physical order 0 D B 9 7 5 3 1 E C A 8 6 4 2 F */

/* PO logical order  0 E D C B A 9 8 7 6 5 4 3 2 1 F */
/*    physical order 0 2 4 6 8 A C E 1 3 5 7 9 B D F */

BYTE CImageBase::ms_DiskByte[0x40] =
{
	0x96,0x97,0x9A,0x9B,0x9D,0x9E,0x9F,0xA6,
	0xA7,0xAB,0xAC,0xAD,0xAE,0xAF,0xB2,0xB3,
	0xB4,0xB5,0xB6,0xB7,0xB9,0xBA,0xBB,0xBC,
	0xBD,0xBE,0xBF,0xCB,0xCD,0xCE,0xCF,0xD3,
	0xD6,0xD7,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,
	0xDF,0xE5,0xE6,0xE7,0xE9,0xEA,0xEB,0xEC,
	0xED,0xEE,0xEF,0xF2,0xF3,0xF4,0xF5,0xF6,
	0xF7,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF
};

BYTE CImageBase::ms_SectorNumber[NUM_SECTOR_ORDERS][0x10] =
{
	{0x00,0x08,0x01,0x09,0x02,0x0A,0x03,0x0B, 0x04,0x0C,0x05,0x0D,0x06,0x0E,0x07,0x0F},
	{0x00,0x07,0x0E,0x06,0x0D,0x05,0x0C,0x04, 0x0B,0x03,0x0A,0x02,0x09,0x01,0x08,0x0F},
	{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}
};

UINT CImageBase::ms_uNumTracksInImage = 0;
LPBYTE CImageBase::ms_pWorkBuffer = NULL;

//-------------------------------------

bool CImageBase::ReadTrack(ImageInfo* pImageInfo, const int nTrack, LPBYTE pTrackBuffer, const UINT uTrackSize)
{
	const long Offset = pImageInfo->uOffset + nTrack * uTrackSize;
	memcpy(pTrackBuffer, &pImageInfo->pImageBuffer[Offset], uTrackSize);

	return true;
}

//-------------------------------------

bool CImageBase::WriteTrack(ImageInfo* pImageInfo, const int nTrack, LPBYTE pTrackBuffer, const UINT uTrackSize)
{
	const long Offset = pImageInfo->uOffset + nTrack * uTrackSize;
	memcpy(&pImageInfo->pImageBuffer[Offset], pTrackBuffer, uTrackSize);

	if (pImageInfo->FileType == eFileNormal && pImageInfo->hFile != INVALID_HANDLE_VALUE)
	{
		SetFilePointer(pImageInfo->hFile, Offset, NULL, FILE_BEGIN);

		DWORD dwBytesWritten;
		WriteFile(pImageInfo->hFile, pTrackBuffer, uTrackSize, &dwBytesWritten, NULL);
		_ASSERT(dwBytesWritten == uTrackSize);
		if (dwBytesWritten != uTrackSize)
			return false;
	}
	else if (pImageInfo->FileType == eFileGZip)
	{
		// Write entire compressed image each time (dirty track change or dirty disk removal)
		gzFile hGZFile = gzopen(pImageInfo->szFilename, "wb");
		if (hGZFile == NULL)
			return false;

		int nLen = gzwrite(hGZFile, pImageInfo->pImageBuffer, pImageInfo->uImageSize);
		if (nLen != pImageInfo->uImageSize)
			return false;

		int nRes = gzclose(hGZFile);
		hGZFile = NULL;
		if (nRes != Z_OK)
			return false;
	}
	else if (pImageInfo->FileType == eFileZip)
	{
		_ASSERT(0);	// TODO
		return false;
	}
	else
	{
		_ASSERT(0);
		return false;
	}

	return true;
}

//-------------------------------------

LPBYTE CImageBase::Code62(int sector)
{
	// CONVERT THE 256 8-BIT BYTES INTO 342 6-BIT BYTES, WHICH WE STORE
	// STARTING AT 4K INTO THE WORK BUFFER.
	{
		LPBYTE sectorbase = ms_pWorkBuffer+(sector << 8);
		LPBYTE resultptr  = ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE;
		BYTE   offset     = 0xAC;
		while (offset != 0x02)
		{
			BYTE value = 0;
#define ADDVALUE(a) value = (value << 2) |        \
							(((a) & 0x01) << 1) | \
							(((a) & 0x02) >> 1)
			ADDVALUE(*(sectorbase+offset));  offset -= 0x56;
			ADDVALUE(*(sectorbase+offset));  offset -= 0x56;
			ADDVALUE(*(sectorbase+offset));  offset -= 0x53;
#undef ADDVALUE
			*(resultptr++) = value << 2;
		}
		*(resultptr-2) &= 0x3F;
		*(resultptr-1) &= 0x3F;
		int loop = 0;
		while (loop < 0x100)
			*(resultptr++) = *(sectorbase+(loop++));
	}

	// EXCLUSIVE-OR THE ENTIRE DATA BLOCK WITH ITSELF OFFSET BY ONE BYTE,
	// CREATING A 343RD BYTE WHICH IS USED AS A CHECKSUM.  STORE THE NEW
	// BLOCK OF 343 BYTES STARTING AT 5K INTO THE WORK BUFFER.
	{
		BYTE   savedval  = 0;
		LPBYTE sourceptr = ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE;
		LPBYTE resultptr = ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE+0x400;
		int    loop      = 342;
		while (loop--)
		{
			*(resultptr++) = savedval ^ *sourceptr;
			savedval = *(sourceptr++);
		}
		*resultptr = savedval;
	}

	// USING A LOOKUP TABLE, CONVERT THE 6-BIT BYTES INTO DISK BYTES.  A VALID
	// DISK BYTE IS A BYTE THAT HAS THE HIGH BIT SET, AT LEAST TWO ADJACENT
	// BITS SET (EXCLUDING THE HIGH BIT), AND AT MOST ONE PAIR OF CONSECUTIVE
	// ZERO BITS.  THE CONVERTED BLOCK OF 343 BYTES IS STORED STARTING AT 4K
	// INTO THE WORK BUFFER.
	{
		LPBYTE sourceptr = ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE+0x400;
		LPBYTE resultptr = ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE;
		int    loop      = 343;
		while (loop--)
			*(resultptr++) = ms_DiskByte[(*(sourceptr++)) >> 2];
	}

	return ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE;
}

//-------------------------------------

void CImageBase::Decode62(LPBYTE imageptr)
{
	// IF WE HAVEN'T ALREADY DONE SO, GENERATE A TABLE FOR CONVERTING
	// DISK BYTES BACK INTO 6-BIT BYTES
	static BOOL tablegenerated = 0;
	static BYTE sixbitbyte[0x80];
	if (!tablegenerated)
	{
		ZeroMemory(sixbitbyte,0x80);
		int loop = 0;
		while (loop < 0x40) {
			sixbitbyte[ms_DiskByte[loop]-0x80] = loop << 2;
			loop++;
		}
		tablegenerated = 1;
	}

	// USING OUR TABLE, CONVERT THE DISK BYTES BACK INTO 6-BIT BYTES
	{
		LPBYTE sourceptr = ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE;
		LPBYTE resultptr = ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE+0x400;
		int    loop      = 343;
		while (loop--)
			*(resultptr++) = sixbitbyte[*(sourceptr++) & 0x7F];
	}

	// EXCLUSIVE-OR THE ENTIRE DATA BLOCK WITH ITSELF OFFSET BY ONE BYTE
	// TO UNDO THE EFFECTS OF THE CHECKSUMMING PROCESS
	{
		BYTE   savedval  = 0;
		LPBYTE sourceptr = ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE+0x400;
		LPBYTE resultptr = ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE;
		int    loop      = 342;
		while (loop--)
		{
			*resultptr = savedval ^ *(sourceptr++);
			savedval = *(resultptr++);
		}
	}

	// CONVERT THE 342 6-BIT BYTES INTO 256 8-BIT BYTES
	{
		LPBYTE lowbitsptr = ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE;
		LPBYTE sectorbase = ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE+0x56;
		BYTE   offset     = 0xAC;
		while (offset != 0x02)
		{
			if (offset >= 0xAC)
			{
				*(imageptr+offset) = (*(sectorbase+offset) & 0xFC)
										| (((*lowbitsptr) & 0x80) >> 7)
										| (((*lowbitsptr) & 0x40) >> 5);
			}

			offset -= 0x56;
			*(imageptr+offset) = (*(sectorbase+offset) & 0xFC)
										| (((*lowbitsptr) & 0x20) >> 5)
										| (((*lowbitsptr) & 0x10) >> 3);

			offset -= 0x56;
			*(imageptr+offset) = (*(sectorbase+offset) & 0xFC)
										| (((*lowbitsptr) & 0x08) >> 3)
										| (((*lowbitsptr) & 0x04) >> 1);

			offset -= 0x53;
			lowbitsptr++;
		}
	}
}

//-------------------------------------

void CImageBase::DenibblizeTrack(LPBYTE trackimage, SectorOrder_e SectorOrder, int nibbles)
{
	ZeroMemory(ms_pWorkBuffer, TRACK_DENIBBLIZED_SIZE);

	// SEARCH THROUGH THE TRACK IMAGE FOR EACH SECTOR.  FOR EVERY SECTOR
	// WE FIND, COPY THE NIBBLIZED DATA FOR THAT SECTOR INTO THE WORK
	// BUFFER AT OFFSET 4K.  THEN CALL DECODE62() TO DENIBBLIZE THE DATA
	// IN THE BUFFER AND WRITE IT INTO THE FIRST PART OF THE WORK BUFFER
	// OFFSET BY THE SECTOR NUMBER.

	int offset    = 0;
	int partsleft = 33;
	int sector    = 0;
	while (partsleft--)
	{
		BYTE byteval[3] = {0,0,0};
		int  bytenum    = 0;
		int  loop       = nibbles;
		while ((loop--) && (bytenum < 3))
		{
			if (bytenum)
				byteval[bytenum++] = *(trackimage+offset++);
			else if (*(trackimage+offset++) == 0xD5)
				bytenum = 1;

			if (offset >= nibbles)
				offset = 0;
		}

		if ((bytenum == 3) && (byteval[1] = 0xAA))
		{
			int loop       = 0;
			int tempoffset = offset;
			while (loop < 384)
			{
				*(ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE+loop++) = *(trackimage+tempoffset++);
				if (tempoffset >= nibbles)
					tempoffset = 0;
			}
			
			if (byteval[2] == 0x96)
			{
				sector = ((*(ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE+4) & 0x55) << 1)
						| (*(ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE+5) & 0x55);
			}
			else if (byteval[2] == 0xAD)
			{
				Decode62(ms_pWorkBuffer+(ms_SectorNumber[SectorOrder][sector] << 8));
				sector = 0;
			}
		}
	}
}

//-------------------------------------

DWORD CImageBase::NibblizeTrack(LPBYTE trackimagebuffer, SectorOrder_e SectorOrder, int track)
{
	ZeroMemory(ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE, TRACK_DENIBBLIZED_SIZE);
	LPBYTE imageptr = trackimagebuffer;
	BYTE   sector   = 0;

	// WRITE GAP ONE, WHICH CONTAINS 48 SELF-SYNC BYTES
	int loop;
	for (loop = 0; loop < 48; loop++)
		*(imageptr++) = 0xFF;

	while (sector < 16)
	{
		// WRITE THE ADDRESS FIELD, WHICH CONTAINS:
		//   - PROLOGUE (D5AA96)
		//   - VOLUME NUMBER ("4 AND 4" ENCODED)
		//   - TRACK NUMBER ("4 AND 4" ENCODED)
		//   - SECTOR NUMBER ("4 AND 4" ENCODED)
		//   - CHECKSUM ("4 AND 4" ENCODED)
		//   - EPILOGUE (DEAAEB)
		*(imageptr++) = 0xD5;
		*(imageptr++) = 0xAA;
		*(imageptr++) = 0x96;
#define VOLUME 0xFE
#define CODE44A(a) ((((a) >> 1) & 0x55) | 0xAA)
#define CODE44B(a) (((a) & 0x55) | 0xAA)
		*(imageptr++) = CODE44A(VOLUME);
		*(imageptr++) = CODE44B(VOLUME);
		*(imageptr++) = CODE44A((BYTE)track);
		*(imageptr++) = CODE44B((BYTE)track);
		*(imageptr++) = CODE44A(sector);
		*(imageptr++) = CODE44B(sector);
		*(imageptr++) = CODE44A(VOLUME ^ ((BYTE)track) ^ sector);
		*(imageptr++) = CODE44B(VOLUME ^ ((BYTE)track) ^ sector);
#undef CODE44A
#undef CODE44B
		*(imageptr++) = 0xDE;
		*(imageptr++) = 0xAA;
		*(imageptr++) = 0xEB;

		// WRITE GAP TWO, WHICH CONTAINS SIX SELF-SYNC BYTES
		for (loop = 0; loop < 6; loop++)
			*(imageptr++) = 0xFF;

		// WRITE THE DATA FIELD, WHICH CONTAINS:
		//   - PROLOGUE (D5AAAD)
		//   - 343 6-BIT BYTES OF NIBBLIZED DATA, INCLUDING A 6-BIT CHECKSUM
		//   - EPILOGUE (DEAAEB)
		*(imageptr++) = 0xD5;
		*(imageptr++) = 0xAA;
		*(imageptr++) = 0xAD;
		CopyMemory(imageptr, Code62(ms_SectorNumber[SectorOrder][sector]), 343);
		imageptr += 343;
		*(imageptr++) = 0xDE;
		*(imageptr++) = 0xAA;
		*(imageptr++) = 0xEB;

		// WRITE GAP THREE, WHICH CONTAINS 27 SELF-SYNC BYTES
		for (loop = 0; loop < 27; loop++)
			*(imageptr++) = 0xFF;

		sector++;
	}

	return imageptr-trackimagebuffer;
}

//-------------------------------------

void CImageBase::SkewTrack(int track, int nibbles, LPBYTE trackimagebuffer)
{
	int skewbytes = (track*768) % nibbles;
	CopyMemory(ms_pWorkBuffer,trackimagebuffer,nibbles);
	CopyMemory(trackimagebuffer,ms_pWorkBuffer+skewbytes,nibbles-skewbytes);
	CopyMemory(trackimagebuffer+nibbles-skewbytes,ms_pWorkBuffer,skewbytes);
}

//-------------------------------------

bool CImageBase::IsValidImageSize(DWORD uImageSize)
{
	ms_uNumTracksInImage = 0;

	if ((TRACKS_MAX>TRACKS_STANDARD) && (uImageSize > TRACKS_MAX*TRACK_DENIBBLIZED_SIZE))
		return false;	// >160KB

	//

	bool bValidSize = false;

	if (uImageSize >= (TRACKS_STANDARD+1)*TRACK_DENIBBLIZED_SIZE)
	{
		// Is uImageSize == 140KB + n*4K?	(where n>=1)
		bValidSize = (((uImageSize - TRACKS_STANDARD*TRACK_DENIBBLIZED_SIZE) % TRACK_DENIBBLIZED_SIZE) == 0);
	}
	else
	{
		bValidSize = (  ((uImageSize >= 143105) && (uImageSize <= 143364)) ||
						 (uImageSize == 143403) ||
						 (uImageSize == 143488) );
	}

	if (bValidSize)
		ms_uNumTracksInImage = uImageSize / TRACK_DENIBBLIZED_SIZE;

	return bValidSize;
}

//===========================================================================

// RAW PROGRAM IMAGE (APL) FORMAT IMPLEMENTATION
class CAplImage : public CImageBase
{
public:
	CAplImage(void) {}
	virtual ~CAplImage(void) {}

	virtual bool Boot(ImageInfo* ptr)
	{
		SetFilePointer(ptr->hFile, 0, NULL, FILE_BEGIN);
		WORD address = 0;
		WORD length  = 0;
		DWORD bytesread;
		ReadFile(ptr->hFile, &address, sizeof(WORD), &bytesread, NULL);
		ReadFile(ptr->hFile, &length , sizeof(WORD), &bytesread, NULL);
		if ((((WORD)(address+length)) <= address) ||
			(address >= 0xC000) ||
			(address+length-1 >= 0xC000))
		{
			return false;
		}

		ReadFile(ptr->hFile, mem+address, length, &bytesread, NULL);
		int loop = 192;
		while (loop--)
			*(memdirty+loop) = 0xFF;

		regs.pc = address;
		return true;
	}

	virtual eDetectResult Detect(LPBYTE pImage, DWORD dwImageSize)
	{
		DWORD dwLength = *(LPWORD)(pImage+2);
		bool bRes = (((dwLength+4) == dwImageSize) ||
					((dwLength+4+((256-((dwLength+4) & 255)) & 255)) == dwImageSize));

		return !bRes ? eMismatch : ePossibleMatch;
	}

	virtual bool AllowBoot(void) { return true; }
	virtual bool AllowRW(void) { return false; }

	virtual eImageType GetType(void) { return eImageAPL; }
	virtual char* GetCreateExtensions(void) { return ".apl"; }
	virtual char* GetRejectExtensions(void) { return ".do;.dsk;.iie;.nib;.po"; }
};

//-------------------------------------

// DOS ORDER (DO) FORMAT IMPLEMENTATION
class CDoImage : public CImageBase
{
public:
	CDoImage(void) {}
	virtual ~CDoImage(void) {}

	virtual eDetectResult Detect(LPBYTE pImage, DWORD dwImageSize)
	{
		if (!IsValidImageSize(dwImageSize))
			return eMismatch;

		// CHECK FOR A DOS ORDER IMAGE OF A DOS DISKETTE
		{
			int  loop      = 0;
			bool bMismatch = false;
			while ((loop++ < 15) && !bMismatch)
			{
				if (*(pImage+0x11002+(loop << 8)) != loop-1)
					bMismatch = true;
			}
			if (!bMismatch)
				return eMatch;
		}

		// CHECK FOR A DOS ORDER IMAGE OF A PRODOS DISKETTE
		{
			int  loop      = 1;
			bool bMismatch = false;
			while ((loop++ < 5) && !bMismatch)
			{
				if ((*(LPWORD)(pImage+(loop << 9)+0x100) != ((loop == 5) ? 0 : 6-loop)) ||
					(*(LPWORD)(pImage+(loop << 9)+0x102) != ((loop == 2) ? 0 : 8-loop)))
					bMismatch = 1;
			}
			if (!bMismatch)
				return eMatch;
		}

		return ePossibleMatch;
	}

	virtual void Read(ImageInfo* pImageInfo, int nTrack, int nQuarterTrack, LPBYTE pTrackImageBuffer, int* pNibbles)
	{
		ReadTrack(pImageInfo, nTrack, ms_pWorkBuffer, TRACK_DENIBBLIZED_SIZE);
		*pNibbles = NibblizeTrack(pTrackImageBuffer, eDOSOrder, nTrack);
		if (!enhancedisk)
			SkewTrack(nTrack, *pNibbles, pTrackImageBuffer);
	}

	virtual void Write(ImageInfo* pImageInfo, int nTrack, int nQuarterTrack, LPBYTE pTrackImage, int nNibbles)
	{
		ZeroMemory(ms_pWorkBuffer, TRACK_DENIBBLIZED_SIZE);
		DenibblizeTrack(pTrackImage, eDOSOrder, nNibbles);
		WriteTrack(pImageInfo, nTrack, ms_pWorkBuffer, TRACK_DENIBBLIZED_SIZE);
	}

	virtual bool AllowCreate(void) { return true; }
	virtual UINT GetTrackSizeForCreate(void) { return TRACK_DENIBBLIZED_SIZE; }

	virtual eImageType GetType(void) { return eImageDO; }
	virtual char* GetCreateExtensions(void) { return ".do;.dsk"; }
	virtual char* GetRejectExtensions(void) { return ".nib;.iie;.po;.prg"; }
};

//-------------------------------------

// SIMSYSTEM IIE (IIE) FORMAT IMPLEMENTATION
class CIIeImage : public CImageBase
{
public:
	CIIeImage(void) : m_pHeader(NULL) {}
	virtual ~CIIeImage(void) { delete [] m_pHeader; }

	virtual eDetectResult Detect(LPBYTE pImage, DWORD dwImageSize)
	{
		if (strncmp((const char *)pImage, "SIMSYSTEM_IIE", 13) || (*(pImage+13) > 3))
			return eMismatch;

		ms_uNumTracksInImage = TRACKS_STANDARD;	// Assume default # tracks
		return eMatch;
	}

	virtual void Read(ImageInfo* pImageInfo, int nTrack, int nQuarterTrack, LPBYTE pTrackImageBuffer, int* pNibbles)
	{
		// IF WE HAVEN'T ALREADY DONE SO, READ THE IMAGE FILE HEADER
		if (!m_pHeader)
		{
			m_pHeader = (LPBYTE) VirtualAlloc(NULL, 88, MEM_COMMIT, PAGE_READWRITE);
			if (!m_pHeader)
			{
				*pNibbles = 0;
				return;
			}
			ZeroMemory(m_pHeader, 88);
			DWORD dwBytesRead;
			SetFilePointer(pImageInfo->hFile, 0, NULL,FILE_BEGIN);
			ReadFile(pImageInfo->hFile, m_pHeader, 88, &dwBytesRead, NULL);
		}

		// IF THIS IMAGE CONTAINS USER DATA, READ THE TRACK AND NIBBLIZE IT
		if (*(m_pHeader+13) <= 2)
		{
			ConvertSectorOrder(m_pHeader+14);
			SetFilePointer(pImageInfo->hFile, nTrack*TRACK_DENIBBLIZED_SIZE+30, NULL, FILE_BEGIN);
			ZeroMemory(ms_pWorkBuffer, TRACK_DENIBBLIZED_SIZE);
			DWORD bytesread;
			ReadFile(pImageInfo->hFile, ms_pWorkBuffer, TRACK_DENIBBLIZED_SIZE, &bytesread, NULL);
			*pNibbles = NibblizeTrack(pTrackImageBuffer, eSIMSYSTEMOrder, nTrack);
		}
		// OTHERWISE, IF THIS IMAGE CONTAINS NIBBLE INFORMATION, READ IT DIRECTLY INTO THE TRACK BUFFER
		else 
		{
			*pNibbles = *(LPWORD)(m_pHeader+nTrack*2+14);
			LONG Offset = 88;
			while (nTrack--)
				Offset += *(LPWORD)(m_pHeader+nTrack*2+14);
			SetFilePointer(pImageInfo->hFile, Offset, NULL,FILE_BEGIN);
			ZeroMemory(pTrackImageBuffer, *pNibbles);
			DWORD dwBytesRead;
			ReadFile(pImageInfo->hFile, pTrackImageBuffer, *pNibbles, &dwBytesRead, NULL);
		}
	}

	virtual void Write(ImageInfo* pImageInfo, int nTrack, int nQuarterTrack, LPBYTE pTrackImage, int nNibbles)
	{
		// note: unimplemented
	}

	virtual eImageType GetType(void) { return eImageIIE; }
	virtual char* GetCreateExtensions(void) { return ".iie"; }
	virtual char* GetRejectExtensions(void) { return ".do.;.nib;.po;.prg"; }

private:
	void ConvertSectorOrder(LPBYTE sourceorder)
	{
		int loop = 16;
		while (loop--)
		{
			BYTE found = 0xFF;
			int  loop2 = 16;
			while (loop2-- && (found == 0xFF))
			{
				if (*(sourceorder+loop2) == loop)
					found = loop2;
			}

			if (found == 0xFF)
				found = 0;

			ms_SectorNumber[2][loop] = found;
		}
	}

private:
	LPBYTE m_pHeader;
};

//-------------------------------------

// NIBBLIZED 6656-NIBBLE (NIB) FORMAT IMPLEMENTATION
class CNib1Image : public CImageBase
{
public:
	CNib1Image(void) {}
	virtual ~CNib1Image(void) {}

	static const UINT NIB1_TRACK_SIZE = NIBBLES;

	virtual eDetectResult Detect(LPBYTE pImage, DWORD dwImageSize)
	{
		if (dwImageSize != NIB1_TRACK_SIZE*TRACKS_STANDARD)
			return eMismatch;

		ms_uNumTracksInImage = TRACKS_STANDARD;
		return eMatch;
	}

	virtual void Read(ImageInfo* pImageInfo, int nTrack, int nQuarterTrack, LPBYTE pTrackImageBuffer, int* pNibbles)
	{
		ReadTrack(pImageInfo, nTrack, pTrackImageBuffer, NIB1_TRACK_SIZE);
		*pNibbles = NIB1_TRACK_SIZE;
	}

	virtual void Write(ImageInfo* pImageInfo, int nTrack, int nQuarterTrack, LPBYTE pTrackImage, int nNibbles)
	{
		_ASSERT(nNibbles == NIB1_TRACK_SIZE);	// Must be true - as nNibbles gets init'd by ImageReadTrace()
		WriteTrack(pImageInfo, nTrack, pTrackImage, nNibbles);
	}

	virtual bool AllowCreate(void) { return true; }
	virtual UINT GetTrackSizeForCreate(void) { return NIB1_TRACK_SIZE; }

	virtual eImageType GetType(void) { return eImageNIB1; }
	virtual char* GetCreateExtensions(void) { return ".nib"; }
	virtual char* GetRejectExtensions(void) { return ".do;.iie;.po;.prg"; }
};

//-------------------------------------

// NIBBLIZED 6384-NIBBLE (NB2) FORMAT IMPLEMENTATION
class CNib2Image : public CImageBase
{
public:
	CNib2Image(void) {}
	virtual ~CNib2Image(void) {}

	static const UINT NIB2_TRACK_SIZE = 6384;

	virtual eDetectResult Detect(LPBYTE pImage, DWORD dwImageSize)
	{
		if (dwImageSize != NIB2_TRACK_SIZE*TRACKS_STANDARD)
			return eMismatch;

		ms_uNumTracksInImage = TRACKS_STANDARD;
		return eMatch;
	}

	virtual void Read(ImageInfo* pImageInfo, int nTrack, int nQuarterTrack, LPBYTE pTrackImageBuffer, int* pNibbles)
	{
		ReadTrack(pImageInfo, nTrack, pTrackImageBuffer, NIB2_TRACK_SIZE);
		*pNibbles = NIB2_TRACK_SIZE;
	}

	virtual void Write(ImageInfo* pImageInfo, int nTrack, int nQuarterTrack, LPBYTE pTrackImage, int nNibbles)
	{
		_ASSERT(nNibbles == NIB2_TRACK_SIZE);	// Must be true - as nNibbles gets init'd by ImageReadTrace()
		WriteTrack(pImageInfo, nTrack, pTrackImage, nNibbles);
	}

	virtual eImageType GetType(void) { return eImageNIB2; }
	virtual char* GetCreateExtensions(void) { return ".nb2"; }
	virtual char* GetRejectExtensions(void) { return ".do;.iie;.po;.prg"; }
};

//-------------------------------------

// PRODOS ORDER (PO) FORMAT IMPLEMENTATION
class CPoImage : public CImageBase
{
public:
	CPoImage(void) {}
	virtual ~CPoImage(void) {}

	virtual eDetectResult Detect(LPBYTE pImage, DWORD dwImageSize)
	{
		if (!IsValidImageSize(dwImageSize))
			return eMismatch;

		// CHECK FOR A PRODOS ORDER IMAGE OF A DOS DISKETTE
		{
			int  loop      = 4;
			bool bMismatch = false;
			while ((loop++ < 13) && !bMismatch)
				if (*(pImage+0x11002+(loop << 8)) != 14-loop)
					bMismatch = true;
			if (!bMismatch)
				return eMatch;
		}

		// CHECK FOR A PRODOS ORDER IMAGE OF A PRODOS DISKETTE
		{
			int  loop      = 1;
			bool bMismatch = false;
			while ((loop++ < 5) && !bMismatch)
			{
				if ((*(LPWORD)(pImage+(loop << 9)  ) != ((loop == 2) ? 0 : loop-1)) ||
					(*(LPWORD)(pImage+(loop << 9)+2) != ((loop == 5) ? 0 : loop+1)))
					bMismatch = true;
			}
			if (!bMismatch)
				return eMatch;
		}

		return ePossibleMatch;
	}

	virtual void Read(ImageInfo* pImageInfo, int nTrack, int nQuarterTrack, LPBYTE pTrackImageBuffer, int* pNibbles)
	{
		ReadTrack(pImageInfo, nTrack, ms_pWorkBuffer, TRACK_DENIBBLIZED_SIZE);
		*pNibbles = NibblizeTrack(pTrackImageBuffer, eProDOSOrder, nTrack);
		if (!enhancedisk)
			SkewTrack(nTrack, *pNibbles, pTrackImageBuffer);
	}

	virtual void Write(ImageInfo* pImageInfo, int nTrack, int nQuarterTrack, LPBYTE pTrackImage, int nNibbles)
	{
		ZeroMemory(ms_pWorkBuffer, TRACK_DENIBBLIZED_SIZE);
		DenibblizeTrack(pTrackImage, eProDOSOrder, nNibbles);
		WriteTrack(pImageInfo, nTrack, ms_pWorkBuffer, TRACK_DENIBBLIZED_SIZE);
	}

	virtual eImageType GetType(void) { return eImagePO; }
	virtual char* GetCreateExtensions(void) { return ".po"; }
	virtual char* GetRejectExtensions(void) { return ".do;.iie;.nib;.prg"; }
};

//-------------------------------------

class CPrgImage : public CImageBase
{
public:
	CPrgImage(void) {}
	virtual ~CPrgImage(void) {}

	virtual bool Boot(ImageInfo* pImageInfo)
	{
		SetFilePointer(pImageInfo->hFile, 5, NULL, FILE_BEGIN);

		WORD address = 0;
		WORD length  = 0;
		DWORD bytesread;

		ReadFile(pImageInfo->hFile, &address, sizeof(WORD), &bytesread, NULL);
		ReadFile(pImageInfo->hFile, &length , sizeof(WORD), &bytesread, NULL);

		length <<= 1;
		if ((((WORD)(address+length)) <= address) ||
			(address >= 0xC000) ||
			(address+length-1 >= 0xC000))
		{
			return false;
		}

		SetFilePointer(pImageInfo->hFile,128,NULL,FILE_BEGIN);
		ReadFile(pImageInfo->hFile, mem+address, length, &bytesread, NULL);

		int loop = 192;
		while (loop--)
			*(memdirty+loop) = 0xFF;

		regs.pc = address;
		return true;
	}

	virtual eDetectResult Detect(LPBYTE pImage, DWORD dwImageSize)
	{
		return (*(LPDWORD)pImage == 0x214C470A) ? eMatch : eMismatch;
	}

	virtual bool AllowBoot(void) { return true; }
	virtual bool AllowRW(void) { return false; }

	virtual eImageType GetType(void) { return eImagePRG; }
	virtual char* GetCreateExtensions(void) { return ".prg"; }
	virtual char* GetRejectExtensions(void) { return ".do;.dsk;.iie;.nib;.po"; }
};

//-----------------------------------------------------------------------------

CDiskImageHelper::CDiskImageHelper(void)
{
	m_vecImageTypes.push_back( new CAplImage );
	m_vecImageTypes.push_back( new CDoImage );
	m_vecImageTypes.push_back( new CIIeImage );
	m_vecImageTypes.push_back( new CNib1Image );
	m_vecImageTypes.push_back( new CNib2Image );
	m_vecImageTypes.push_back( new CPoImage );
	m_vecImageTypes.push_back( new CPrgImage );
}

CDiskImageHelper::~CDiskImageHelper(void)
{
	for (UINT i=0; i<m_vecImageTypes.size(); i++)
		delete m_vecImageTypes[i];
}

void CDiskImageHelper::SkipMacBinaryHdr(LPBYTE& pImage, DWORD& dwSize, DWORD& dwOffset)
{
	dwOffset = 0;

	// DETERMINE WHETHER THE FILE HAS A 128-BYTE MACBINARY HEADER
	const UINT uMacBinHdrSize = 128;
	if ((dwSize > uMacBinHdrSize) &&
		(!*pImage) &&
		(*(pImage+1) < 120) &&
		(!*(pImage+*(pImage+1)+2)) &&
		(*(pImage+0x7A) == 0x81) &&
		(*(pImage+0x7B) == 0x81))
	{
		pImage += uMacBinHdrSize;
		dwSize -= uMacBinHdrSize;
		dwOffset = uMacBinHdrSize;
	}
}

CImageBase* CDiskImageHelper::Detect(LPBYTE pImage, DWORD dwSize, const TCHAR* pszExt, DWORD& dwOffset)
{
	SkipMacBinaryHdr(pImage, dwSize, dwOffset);

	// CALL THE DETECTION FUNCTIONS IN ORDER, LOOKING FOR A MATCH
	eImageType ImageType = eImageUNKNOWN;
	eImageType PossibleType = eImageUNKNOWN;

	UINT uLoop = 0;
	while ((uLoop < GetNumImages()) && (ImageType == eImageUNKNOWN))
	{
		if (*pszExt && _tcsstr(GetImage(uLoop)->GetRejectExtensions(), pszExt))
		{
			uLoop++;
		}
		else
		{
			eDetectResult Result = GetImage(uLoop)->Detect(pImage, dwSize);
			if (Result == eMatch)
				ImageType = GetImage(uLoop)->GetType();
			else if ((Result == ePossibleMatch) && (PossibleType == eImageUNKNOWN))
				PossibleType = GetImage(uLoop)->GetType();

			uLoop++;
		}
	}

	if (ImageType == eImageUNKNOWN)
		ImageType = PossibleType;

	return GetImage(ImageType);
}

CImageBase* CDiskImageHelper::GetImageForCreation(const TCHAR* pszExt)
{
	// WE CREATE ONLY DOS ORDER (DO) OR 6656-NIBBLE (NIB) FORMAT FILES
	for (UINT uLoop = 0; uLoop <= GetNumImages(); uLoop++)
	{
		if (!GetImage(uLoop)->AllowCreate())
			continue;

		if (*pszExt && _tcsstr(GetImage(uLoop)->GetCreateExtensions(), pszExt))
			return GetImage(uLoop);
	}

	return NULL;
}
