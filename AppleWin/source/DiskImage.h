#pragma once

#define NIBBLES 6656
#define TRACK_DENIBBLIZED_SIZE (16 * 256)	// #Sectors x Sector-size

#define	TRACKS_STANDARD	35
#define	TRACKS_EXTRA	5		// Allow up to a 40-track .dsk image (160KB)
#define	TRACKS_MAX		(TRACKS_STANDARD+TRACKS_EXTRA)

enum ImageError_e
{
	eIMAGE_ERROR_NONE,
	eIMAGE_ERROR_BAD_POINTER,
	eIMAGE_ERROR_BAD_SIZE,
	eIMAGE_ERROR_UNABLE_TO_OPEN,
	eIMAGE_ERROR_UNABLE_TO_OPEN_GZ,
	eIMAGE_ERROR_UNABLE_TO_OPEN_ZIP,
};

class CImageBase;

struct ImageInfo
{
	TCHAR		filename[MAX_PATH];
	CImageBase*	pImageType;
	HANDLE		file;
	gzFile		hGZFile;
	DWORD		offset;
	bool		bWriteProtected;
	DWORD		headersize;
	LPBYTE		header;
	BOOL		validtrack[TRACKS_MAX];
	UINT		uNumTracks;
};

BOOL ImageBoot(HIMAGE);
void ImageClose(HIMAGE);
void ImageDestroy(void);
void ImageInitialize(void);

ImageError_e ImageOpen(LPCTSTR imagefilename, HIMAGE* hDiskImage_, bool* pWriteProtected_, const bool bCreateIfNecessary);

void ImageReadTrack(HIMAGE imagehandle, int track, int quartertrack, LPBYTE trackimagebuffer, int* nibbles);
void ImageWriteTrack(HIMAGE imagehandle, int track, int quartertrack, LPBYTE trackimage, int nibbles);

int ImageGetNumTracks(HIMAGE imagehandle);

bool ImageIsWriteProtected(HIMAGE imagehandle);
