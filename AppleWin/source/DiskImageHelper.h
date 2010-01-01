#pragma once

struct ImageInfo;

enum eImageType { eImageUNKNOWN, eImageAPL, eImageDO,  eImageIIE,  eImageNIB1,  eImageNIB2,  eImagePO,  eImagePRG };
enum eDetectResult {eMismatch, ePossibleMatch, eMatch};

class CImageBase;
enum FileType_e { eFileNormal, eFileGZip, eFileZip };

struct ImageInfo
{
	TCHAR			szFilename[MAX_PATH];
	CImageBase*		pImageType;
	FileType_e		FileType;
	HANDLE			hFile;
	DWORD			uOffset;
	bool			bWriteProtected;
	BYTE			ValidTrack[TRACKS_MAX];
	UINT			uNumTracks;
	BYTE*			pImageBuffer;
	UINT			uImageSize;
	char			szFilenameInZip[MAX_PATH];
	zip_fileinfo	zipFileInfo;
};

//-------------------------------------

class CImageBase
{
public:
	CImageBase(void) { }
	virtual ~CImageBase(void) {}

	virtual bool Boot(ImageInfo* pImageInfo) { return false; }
	virtual eDetectResult Detect(LPBYTE pImage, DWORD dwImageSize) = 0;
	virtual void Read(ImageInfo* pImageInfo, int nTrack, int nQuarterTrack, LPBYTE pTrackImageBuffer, int* pNibbles) { }
	virtual void Write(ImageInfo* pImageInfo, int nTrack, int nQuarterTrack, LPBYTE pTrackImage, int nNibbles) { }

	virtual bool AllowBoot(void) { return false; }		// Only:    APL and PRG
	virtual bool AllowRW(void) { return true; }			// All but: APL and PRG
	virtual bool AllowCreate(void) { return false; }	// WE CREATE ONLY DOS ORDER (DO) OR 6656-NIBBLE (NIB) FORMAT FILES
	virtual UINT GetTrackSizeForCreate(void) { return 0; }

	virtual eImageType GetType(void) = 0;
	virtual char* GetCreateExtensions(void) = 0;
	virtual char* GetRejectExtensions(void) = 0;

	enum SectorOrder_e {eProDOSOrder, eDOSOrder, eSIMSYSTEMOrder, NUM_SECTOR_ORDERS};

protected:
	bool ReadTrack(ImageInfo* pImageInfo, const int nTrack, LPBYTE pTrackBuffer, const UINT uTrackSize);
	bool WriteTrack(ImageInfo* pImageInfo, const int nTrack, LPBYTE pTrackBuffer, const UINT uTrackSize);

	LPBYTE Code62(int sector);
	void Decode62(LPBYTE imageptr);
	void DenibblizeTrack (LPBYTE trackimage, SectorOrder_e SectorOrder, int nibbles);
	DWORD NibblizeTrack (LPBYTE trackimagebuffer, SectorOrder_e SectorOrder, int track);
	void SkewTrack (int track, int nibbles, LPBYTE trackimagebuffer);
	bool IsValidImageSize(DWORD uImageSize);

public:
	static UINT ms_uNumTracksInImage;	// Init'd by ImageOpen() & possibly updated by IsValidImageSize()
	static LPBYTE ms_pWorkBuffer;

protected:
	static BYTE ms_DiskByte[0x40];
	static BYTE ms_SectorNumber[NUM_SECTOR_ORDERS][0x10];
};

//-------------------------------------

class CDiskImageHelper
{
public:
	CDiskImageHelper(void);
	virtual ~CDiskImageHelper(void);

	CImageBase* Detect(LPBYTE pImage, DWORD dwSize, const TCHAR* pszExt, DWORD& dwOffset);
	CImageBase* GetImageForCreation(const TCHAR* pszExt);

	UINT GetNumTracksInImage(void) { return CImageBase::ms_uNumTracksInImage; }
	void SetNumTracksInImage(UINT uNumTracks) { CImageBase::ms_uNumTracksInImage = uNumTracks; }

	LPBYTE GetWorkBuffer(void) { return CImageBase::ms_pWorkBuffer; }
	void SetWorkBuffer(LPBYTE pBuffer) { CImageBase::ms_pWorkBuffer = pBuffer; }

private:
	UINT GetNumImages(void) { return m_vecImageTypes.size(); };
	CImageBase* GetImage(UINT uIndex) { _ASSERT(uIndex<GetNumImages()); return m_vecImageTypes[uIndex]; }
	CImageBase* GetImage(eImageType Type)
	{
		if (Type == eImageUNKNOWN)
			return NULL;
		for (UINT i=0; i<GetNumImages(); i++)
		{
			if (m_vecImageTypes[i]->GetType() == Type)
				return m_vecImageTypes[i];
		}
		_ASSERT(0);
		return NULL;
	}

	void SkipMacBinaryHdr(LPBYTE& pImage, DWORD& dwSize, DWORD& dwOffset);

private:
	typedef std::vector<CImageBase*> VECIMAGETYPE;
	VECIMAGETYPE m_vecImageTypes;
};
