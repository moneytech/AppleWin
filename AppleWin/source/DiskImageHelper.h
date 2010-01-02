#pragma once

struct ImageInfo;

enum eImageType { eImageUNKNOWN, eImageDO, eImagePO, eImageNIB1, eImageNIB2, eImageHDV, eImageIIE, eImageAPL, eImagePRG };
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
	UINT			uNumEntriesInZip;
};

//-------------------------------------

#define UNIDISK35_800K_SIZE (800*1024)	// UniDisk 3.5"

#define DEFAULT_VOLUME_NUMBER 254

class CImageBase
{
public:
	CImageBase(void) : m_uNumTracksInImage(0), m_uVolumeNumber(DEFAULT_VOLUME_NUMBER) {}
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
	static LPBYTE ms_pWorkBuffer;
	UINT m_uNumTracksInImage;	// Init'd by CDiskImageHelper.Detect()/GetImageForCreation() & possibly updated by IsValidImageSize()
	BYTE m_uVolumeNumber;

protected:
	static BYTE ms_DiskByte[0x40];
	static BYTE ms_SectorNumber[NUM_SECTOR_ORDERS][0x10];
};

//-------------------------------------

class CHdrHelper
{
public:
	virtual eDetectResult DetectHdr(LPBYTE& pImage, DWORD& dwImageSize, DWORD& dwOffset) = 0;
	virtual UINT GetMaxHdrSize(void) = 0;
protected:
	CHdrHelper(void) {}
	virtual ~CHdrHelper(void) {}
};

class CMacBinaryHelper : public CHdrHelper
{
public:
	CMacBinaryHelper(void) {}
	virtual ~CMacBinaryHelper(void) {}
	virtual eDetectResult DetectHdr(LPBYTE& pImage, DWORD& dwImageSize, DWORD& dwOffset);
	virtual UINT GetMaxHdrSize(void) { return uMacBinHdrSize; }

private:
	static const UINT uMacBinHdrSize = 128;
};

#pragma pack(push)
#pragma pack(1)	// Ensure Header2IMG is packed

class C2IMGHelper : public CHdrHelper
{
public:
	C2IMGHelper(void) {}
	virtual ~C2IMGHelper(void) {}
	virtual eDetectResult DetectHdr(LPBYTE& pImage, DWORD& dwImageSize, DWORD& dwOffset);
	virtual UINT GetMaxHdrSize(void) { return sizeof(Header2IMG); }
	BYTE GetVolumeNumber(void);
	bool IsLocked(void);

private:
	static const UINT32 FormatID_2IMG = 'GMI2';			// '2IMG'
	static const UINT32 Creator_2IMG_AppleWin = '1vWA';	// 'AWv1'
	static const USHORT Version_2IMG_AppleWin = 1;

	enum ImageFormat2IMG_e { e2IMGFormatDOS33=0, e2IMGFormatProDOS, e2IMGFormatNIBData };

	struct Flags2IMG
	{
		UINT32 VolumeNumber : 8;				// bits7-0
		UINT32 bDOS33VolumeNumberValid : 1;
		UINT32 Pad : 22;
		UINT32 bDiskImageLocked : 1;			// bit31
	};

	struct Header2IMG
	{
		UINT32	FormatID;		// "2IMG"
		UINT32	CreatorID;
		USHORT	HeaderSize;
		USHORT	Version;
		union
		{
			ImageFormat2IMG_e	ImageFormat;
			UINT32				ImageFormatRaw;
		};
		union
		{
			Flags2IMG			Flags;
			UINT32				FlagsRaw;
		};
		UINT32	NumBlocks;		// The number of 512-byte blocks in the disk image 
		UINT32	DiskDataOffset;
		UINT32	DiskDataLength; 
		UINT32	CommentOffset;	// Optional
		UINT32	CommentLength;	// Optional
		UINT32	CreatorOffset;	// Optional
		UINT32	CreatorLength;	// Optional
		BYTE	Padding[16];
	};

	Header2IMG m_Hdr;
};

#pragma pack(pop)

//-------------------------------------

class CDiskImageHelper
{
public:
	CDiskImageHelper(void);
	virtual ~CDiskImageHelper(void);

	CImageBase* Detect(LPBYTE pImage, DWORD dwSize, const TCHAR* pszExt, DWORD& dwOffset, bool* pWriteProtected_);
	CImageBase* GetImageForCreation(const TCHAR* pszExt);

	UINT GetNumTracksInImage(CImageBase* pImageType) { return pImageType->m_uNumTracksInImage; }
	void SetNumTracksInImage(CImageBase* pImageType, UINT uNumTracks) { pImageType->m_uNumTracksInImage = uNumTracks; }

	LPBYTE GetWorkBuffer(void) { return CImageBase::ms_pWorkBuffer; }
	void SetWorkBuffer(LPBYTE pBuffer) { CImageBase::ms_pWorkBuffer = pBuffer; }

	UINT GetMaxFloppyImageSize(void);

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

	CMacBinaryHelper m_MacBinaryHelper;
	C2IMGHelper m_2IMGHelper;
	eDetectResult m_Result2IMG;
};
