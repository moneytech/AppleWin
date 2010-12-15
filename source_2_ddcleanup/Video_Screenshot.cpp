/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2009, Tom Charlesworth, Michael Pohoreski, Nick Westgate

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

/* Description: Emulation of video modes
 *
 * Author: Michael Pohoreski
 */

#include "StdAfx.h"
#pragma  hdrstop
#include "..\resource\resource.h"

// Globals __________________________________________________________

	#define SCREENSHOT_BMP 1
	#define SCREENSHOT_TGA 0
	
	// alias for nSuffixScreenShotFileName
	static int  g_nLastScreenShot = 0;
	const  int nMaxScreenShot = 999999999;

	static int g_iScreenshotType;
	static char *g_pLastDiskImageName = NULL;

	//const  int nMaxScreenShot = 2;

// Prototypes _______________________________________________________

	// true  = 280x192
	// false = 560x384
	void Video_SaveScreenShot( const char *pScreenShotFileName );
	void Video_MakeScreenShot( FILE *pFile );

// Implementation ___________________________________________________

//===========================================================================
void Video_ResetScreenshotCounter( char *pImageName )
{
	g_nLastScreenShot = 0;
	g_pLastDiskImageName = pImageName;
}

//===========================================================================
void Util_MakeScreenShotFileName( char *pFinalFileName_ )
{
	char sPrefixScreenShotFileName[ 256 ] = "AppleWin_ScreenShot";
	char *pPrefixFileName = g_pLastDiskImageName ? g_pLastDiskImageName : sPrefixScreenShotFileName;
#if SCREENSHOT_BMP
	sprintf( pFinalFileName_, "%s_%09d.bmp", pPrefixFileName, g_nLastScreenShot );
#endif
#if SCREENSHOT_TGA
	sprintf( pFinalFileName_, "%s%09d.tga", pPrefixFileName, g_nLastScreenShot );
#endif
}

// Returns TRUE if file exists, else FALSE
//===========================================================================
bool Util_TestScreenShotFileName( const char *pFileName )
{
	bool bFileExists = false;
	FILE *pFile = fopen( pFileName, "rt" );
	if (pFile)
	{
		fclose( pFile );
		bFileExists = true;
	}
	return bFileExists;
}

//===========================================================================
void Video_TakeScreenShot( int iScreenShotType )
{
	char sScreenShotFileName[ MAX_PATH ];

	g_iScreenshotType = iScreenShotType;

	// find last screenshot filename so we don't overwrite the existing user ones
	bool bExists = true;
	while( bExists )
	{
		if (g_nLastScreenShot > nMaxScreenShot) // Holy Crap! User has maxed the number of screenshots!?
		{
			sprintf( sScreenShotFileName, "You have more then %d screenshot filenames!  They will no longer be saved.\n\nEither move some of your screenshots or increase the maximum in video.cpp\n", nMaxScreenShot );
			MessageBox( NULL, sScreenShotFileName, "Warning", MB_OK );
			g_nLastScreenShot = 0;
			return;
		}

		Util_MakeScreenShotFileName( sScreenShotFileName );
		bExists = Util_TestScreenShotFileName( sScreenShotFileName );
		if( !bExists )
		{
			break;
		}
		g_nLastScreenShot++;
	}

	Video_SaveScreenShot( sScreenShotFileName );
	g_nLastScreenShot++;
}


typedef char	int8;
typedef short	int16;
typedef int		int32;
typedef unsigned	char	u8;
typedef signed		short	s16;

/// turn off MSVC struct member padding
#pragma pack(push,1)

struct bgra_t
{
	u8 b;
	u8 g;
	u8 r;
	u8 a; // reserved on Win32
};

struct WinBmpHeader_t
{
	// BITMAPFILEHEADER     // Addr Size
	char  nCookie[2]      ; // 0x00 0x02 BM
	int32 nSizeFile       ; // 0x02 0x04 0 = ignore
	int16 nReserved1      ; // 0x06 0x02
	int16 nReserved2      ; // 0x08 0x02
	int32 nOffsetData     ; // 0x0A 0x04
	//                      ==      0x0D (14)

	// BITMAPINFOHEADER
	int32 nStructSize     ; // 0x0E 0x04 biSize
	int32 nWidthPixels    ; // 0x12 0x04 biWidth
	int32 nHeightPixels   ; // 0x16 0x04 biHeight
	int16 nPlanes         ; // 0x1A 0x02 biPlanes
	int16 nBitsPerPixel   ; // 0x1C 0x02 biBitCount
	int32 nCompression    ; // 0x1E 0x04 biCompression 0 = BI_RGB
	int32 nSizeImage      ; // 0x22 0x04 0 = ignore
	int32 nXPelsPerMeter  ; // 0x26 0x04
	int32 nYPelsPerMeter  ; // 0x2A 0x04
	int32 nPaletteColors  ; // 0x2E 0x04
	int32 nImportantColors; // 0x32 0x04
	//                      ==      0x28 (40)

	// RGBQUAD
	// pixelmap
};
#pragma pack(pop)

WinBmpHeader_t g_tBmpHeader;


#if SCREENSHOT_TGA
	enum TargaImageType_e
	{
		TARGA_RGB	= 2
	};

	struct TargaHeader_t
	{										// Addr Bytes
		u8		nIdBytes					; // 00 01 size of ID field that follows 18 byte header (0 usually)
		u8		bHasPalette				; // 01 01
		u8		iImageType				; // 02 01 type of image 0=none,1=indexed,2=rgb,3=grey,+8=rle packed

		s16	iPaletteFirstColor	; // 03 02
		s16	nPaletteColors			; // 05 02
		u8		nPaletteBitsPerEntry	; // 07 01 number of bits per palette entry 15,16,24,32

		s16	nOriginX					; // 08 02 image x origin
		s16	nOriginY					; // 0A 02 image y origin
		s16	nWidthPixels			; // 0C 02
		s16	nHeightPixels			; // 0E 02
		u8		nBitsPerPixel			; // 10 01 image bits per pixel 8,16,24,32
		u8		iDescriptor				; // 11 01 image descriptor bits (vh flip bits)
	    
		// pixel data...
		u8		aPixelData[1]		; // rgb
	};

	TargaHeader_t g_tTargaHeader;
#endif // SCREENSHOT_TGA

//===========================================================================
void Video_MakeScreenShot(FILE *pFile)
{
#if SCREENSHOT_BMP
	g_tBmpHeader.nCookie[ 0 ] = 'B'; // 0x42
	g_tBmpHeader.nCookie[ 1 ] = 'M'; // 0x4d
	g_tBmpHeader.nSizeFile  = 0;
	g_tBmpHeader.nReserved1 = 0;
	g_tBmpHeader.nReserved2 = 0;
	g_tBmpHeader.nOffsetData = sizeof(WinBmpHeader_t) + (256 * sizeof(bgra_t));
	g_tBmpHeader.nStructSize = 0x28; // sizeof( WinBmpHeader_t );
	g_tBmpHeader.nWidthPixels = g_iScreenshotType ? FRAMEBUFFER_W/2 :FRAMEBUFFER_W;
	g_tBmpHeader.nHeightPixels = g_iScreenshotType ?  FRAMEBUFFER_H/2 : FRAMEBUFFER_H;
	g_tBmpHeader.nPlanes = 1;
	g_tBmpHeader.nBitsPerPixel = 8;
	g_tBmpHeader.nCompression = BI_RGB;
	g_tBmpHeader.nSizeImage = 0;
	g_tBmpHeader.nXPelsPerMeter = 0;
	g_tBmpHeader.nYPelsPerMeter = 0;
	g_tBmpHeader.nPaletteColors = 256;
	g_tBmpHeader.nImportantColors = 0;

//	char sText[256];
//	sprintf( sText, "sizeof: BITMAPFILEHEADER = %d\n", sizeof(BITMAPFILEHEADER) ); // = 14
//	MessageBox( NULL, sText, "Info 1", MB_OK );
//	sprintf( sText, "sizeof: BITMAPINFOHEADER = %d\n", sizeof(BITMAPINFOHEADER) ); // = 40
//	MessageBox( NULL, sText, "Info 2", MB_OK );

	char sIfSizeZeroOrUnknown_BadWinBmpHeaderPackingSize[ sizeof( WinBmpHeader_t ) == (14 + 40) ];
	sIfSizeZeroOrUnknown_BadWinBmpHeaderPackingSize;

	// Write Header
	int nLen;
	fwrite( &g_tBmpHeader, sizeof( g_tBmpHeader ), 1, pFile );

	// Write Palette Data
	u8 *pSrc = ((u8*)g_pFramebufferinfo) + sizeof(BITMAPINFOHEADER);
	nLen = g_tBmpHeader.nPaletteColors * sizeof(bgra_t); // RGBQUAD
	fwrite( pSrc, nLen, 1, pFile );
	pSrc += nLen;

	// Write Pixel Data
	// No need to use GetDibBits() since we already have http://msdn.microsoft.com/en-us/library/ms532334.aspx
	// @reference: "Storing an Image" http://msdn.microsoft.com/en-us/library/ms532340(VS.85).aspx
	pSrc = ((u8*)g_pFramebufferbits);
	nLen = g_tBmpHeader.nWidthPixels * g_tBmpHeader.nHeightPixels * g_tBmpHeader.nBitsPerPixel / 8;

#if 0
	if( g_iScreenshotType == SCREENSHOT_280x192 )
	{
		u8 aScanLine[ 280 ];
		u8 *pDst;

		// 50% Half Scan Line clears every odd scanline.
		// Shift-Print Screen saves only the even rows.
		// NOTE: Keep in sync with _Video_RedrawScreen() & Video_MakeScreenShot()

		// Bitmap is upside down...
		for( int y = FRAMEBUFFER_H - 1; y > 0; y -=2 )
		{
			pSrc = pSrc = frameoffsettable[ y ];
			pDst = aScanLine;
			for( int x = 0; x < FRAMEBUFFER_W/2; x++ )
			{
				*pDst++ = *pSrc;
				pSrc += 2; // skip odd pixels
			}
			fwrite( aScanLine, FRAMEBUFFER_W/2, 1, pFile );
		}
	}
#endif
	if( g_iScreenshotType == SCREENSHOT_280x192 )
	{
		u8 aScanLine[ 280 ];
		u8 *pDst;

		// 50% Half Scan Line clears every odd scanline.
		// Shift-Print Screen saves only the even rows.
		// NOTE: Keep in sync with _Video_RedrawScreen() & Video_MakeScreenShot()
		for( int y = 0; y < FRAMEBUFFER_H/2; y++ )
		{
			pDst = aScanLine;
			for( int x = 0; x < FRAMEBUFFER_W/2; x++ )
			{
				*pDst++ = *pSrc;
				pSrc += 2; // skip odd pixels
			}
			fwrite( aScanLine, FRAMEBUFFER_W/2, 1, pFile );
			pSrc += FRAMEBUFFER_W; // scan lines doubled - skip odd ones
		}
	}
	else
	{
		fwrite( pSrc, nLen, 1, pFile );
	}
#endif // SCREENSHOT_BMP

#if SCREENSHOT_TGA
	TargaHeader_t *pHeader = &g_tTargaHeader;
	memset( (void*)pHeader, 0, sizeof( TargaHeader_t ) );

	pHeader->iImageType = TARGA_RGB;
	pHeader->nWidthPixels  = FRAMEBUFFER_W;
	pHeader->nHeightPixels = FRAMEBUFFER_H;
	pHeader->nBitsPerPixel =  24;
#endif // SCREENSHOT_TGA

}

//===========================================================================
void Video_SaveScreenShot( const char *pScreenShotFileName )
{
	FILE *pFile = fopen( pScreenShotFileName, "wb" );
	if( pFile )
	{
		Video_MakeScreenShot( pFile );
		fclose( pFile );
	}

	if( g_bDisplayPrintScreenFileName )
	{
		MessageBox( NULL, pScreenShotFileName, "Screen Captured", MB_OK );
	}
}
