#include "PakInterface.h"
#include "Common.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>

typedef unsigned char uchar;

enum
{
	FILEFLAGS_END = 0x80
};

PakInterface* gPakInterface = new PakInterface();

static std::string PakStringToUpper(const std::string& theString)
{
	std::string aString;
	aString.reserve(theString.length());

	for (unsigned i = 0; i < theString.length(); i++)
		aString += (char)toupper((unsigned char)theString[i]);

	return aString;
}

// Converts a file name to the key used in the record map: upper case,
// backslash separated, with "a\..\b" collapsed and a leading "./" removed.
static std::string FixFileName(const char* theFileName)
{
	std::string aCurDir = Sexy::GetCurDir();
	std::string aName = theFileName;

	// Absolute paths inside the working directory become relative.
	if (!aCurDir.empty())
	{
		std::string aPrefix = aCurDir;
		if (aPrefix.back() != '/' && aPrefix.back() != '\\')
			aPrefix += '/';

		std::string aNormName = aName;
		std::string aNormPrefix = aPrefix;
		std::replace(aNormName.begin(), aNormName.end(), '\\', '/');
		std::replace(aNormPrefix.begin(), aNormPrefix.end(), '\\', '/');
		if (aNormName.size() > aNormPrefix.size() && strncasecmp(aNormName.c_str(), aNormPrefix.c_str(), aNormPrefix.size()) == 0)
			aName = aName.substr(aNormPrefix.size());
	}

	std::string aResult;
	aResult.reserve(aName.size());
	bool lastSlash = false;
	for (size_t i = 0; i < aName.size(); i++)
	{
		char c = aName[i];
		if ((c == '\\') || (c == '/'))
		{
			// 统一转为反斜杠，且多个斜杠的情况下只保留一个
			if (!lastSlash)
				aResult += '\\';
			lastSlash = true;
		}
		else if ((c == '.') && (lastSlash) && (i + 1 < aName.size()) && (aName[i + 1] == '.'))
		{
			// 此处将形如“a\b\..\c”的路径简化为“a\c”
			if (!aResult.empty())
				aResult.pop_back();
			while (!aResult.empty() && aResult.back() != '\\')
				aResult.pop_back();
			i++;
			// Skip the separator that follows ".."
			if (i + 1 < aName.size() && (aName[i + 1] == '\\' || aName[i + 1] == '/'))
				i++;
			lastSlash = true;
		}
		else
		{
			aResult += (char)toupper((unsigned char)c);
			lastSlash = false;
		}
	}

	// Remove a leading ".\"
	while (aResult.size() >= 2 && aResult[0] == '.' && aResult[1] == '\\')
		aResult.erase(0, 2);

	return aResult;
}

PakInterface::PakInterface()
{
}

PakInterface::~PakInterface()
{
}

bool PakInterface::AddPakFile(const std::string& theFileName)
{
	FILE* aFP = fopen(Sexy::FixPath(theFileName).c_str(), "rb");
	if (aFP == NULL)
		return false;

	fseek(aFP, 0, SEEK_END);
	long aFileSize = ftell(aFP);
	fseek(aFP, 0, SEEK_SET);
	if (aFileSize < 8)
	{
		fclose(aFP);
		return false;
	}

	mPakCollectionList.push_back(PakCollection());
	PakCollection* aPakCollection = &mPakCollectionList.back();
	aPakCollection->mData.resize((size_t)aFileSize);
	size_t aRead = fread(aPakCollection->mData.data(), 1, (size_t)aFileSize, aFP);
	fclose(aFP);
	if (aRead != (size_t)aFileSize)
	{
		mPakCollectionList.pop_back();
		return false;
	}

	// Parse the (encrypted) directory from a temporary record spanning the
	// whole pack.
	PakRecord aPackRecord;
	aPackRecord.mCollection = aPakCollection;
	aPackRecord.mFileName = theFileName;
	aPackRecord.mFileTime = 0;
	aPackRecord.mStartPos = 0;
	aPackRecord.mSize = (int)aFileSize;

	PFILE aDirFile;
	aDirFile.mRecord = &aPackRecord;
	aDirFile.mPos = 0;
	aDirFile.mFP = NULL;

	std::uint32_t aMagic = 0;
	FRead(&aMagic, sizeof(aMagic), 1, &aDirFile);
	if (aMagic != 0xBAC04AC0)
	{
		mPakCollectionList.pop_back();
		return false;
	}

	std::uint32_t aVersion = 0;
	FRead(&aVersion, sizeof(aVersion), 1, &aDirFile);
	if (aVersion > 0)
	{
		mPakCollectionList.pop_back();
		return false;
	}

	struct PendingRecord
	{
		std::string		mName;
		int				mSize;
		std::uint64_t	mFileTime;
		int				mPos;
	};
	std::vector<PendingRecord> aRecords;

	int aPos = 0;
	for (;;)
	{
		uchar aFlags = 0;
		size_t aCount = FRead(&aFlags, 1, 1, &aDirFile);
		if ((aFlags & FILEFLAGS_END) || (aCount == 0))
			break;

		uchar aNameWidth = 0;
		char aName[256];
		FRead(&aNameWidth, 1, 1, &aDirFile);
		FRead(aName, 1, aNameWidth, &aDirFile);
		aName[aNameWidth] = 0;

		std::int32_t aSrcSize = 0;
		FRead(&aSrcSize, sizeof(aSrcSize), 1, &aDirFile);
		std::uint64_t aFileTime = 0;
		FRead(&aFileTime, sizeof(aFileTime), 1, &aDirFile);

		if (aSrcSize < 0)
		{
			mPakCollectionList.pop_back();
			return false;
		}

		aRecords.push_back({ aName, aSrcSize, aFileTime, aPos });
		aPos += aSrcSize;
	}

	int anOffset = aDirFile.mPos;
	if ((std::int64_t)anOffset + aPos > aFileSize)
	{
		mPakCollectionList.pop_back();
		return false;
	}

	for (const PendingRecord& aPending : aRecords)
	{
		PakRecord& aPakRecord = mPakRecordMap[FixFileName(aPending.mName.c_str())];
		aPakRecord.mCollection = aPakCollection;
		aPakRecord.mFileName = aPending.mName;
		aPakRecord.mStartPos = anOffset + aPending.mPos;
		aPakRecord.mSize = aPending.mSize;
		aPakRecord.mFileTime = aPending.mFileTime;
	}

	return true;
}

PFILE* PakInterface::FOpen(const char* theFileName, const char* anAccess)
{
	if ((strcasecmp(anAccess, "r") == 0) || (strcasecmp(anAccess, "rb") == 0) || (strcasecmp(anAccess, "rt") == 0))
	{
		PakRecordMap::iterator anItr = mPakRecordMap.find(FixFileName(theFileName));
		if (anItr != mPakRecordMap.end())
		{
			PFILE* aPFP = new PFILE;
			aPFP->mRecord = &anItr->second;
			aPFP->mPos = 0;
			aPFP->mFP = NULL;
			return aPFP;
		}
	}

	FILE* aFP = fopen(Sexy::FixPath(theFileName).c_str(), anAccess);
	if (aFP == NULL)
		return NULL;
	PFILE* aPFP = new PFILE;
	aPFP->mRecord = NULL;
	aPFP->mPos = 0;
	aPFP->mFP = aFP;
	return aPFP;
}

int PakInterface::FClose(PFILE* theFile)
{
	if (theFile->mRecord == NULL)
		fclose(theFile->mFP);
	delete theFile;
	return 0;
}

int PakInterface::FSeek(PFILE* theFile, long theOffset, int theOrigin)
{
	if (theFile->mRecord != NULL)
	{
		if (theOrigin == SEEK_SET)
			theFile->mPos = (int)theOffset;
		else if (theOrigin == SEEK_END)
			theFile->mPos = theFile->mRecord->mSize + (int)theOffset;
		else if (theOrigin == SEEK_CUR)
			theFile->mPos += (int)theOffset;

		// 当前指针位置不能超过整个文件的大小，且不能小于 0
		theFile->mPos = std::max(std::min(theFile->mPos, theFile->mRecord->mSize), 0);
		return 0;
	}
	else
		return fseek(theFile->mFP, theOffset, theOrigin);
}

int PakInterface::FTell(PFILE* theFile)
{
	if (theFile->mRecord != NULL)
		return theFile->mPos;
	else
		return (int)ftell(theFile->mFP);
}

size_t PakInterface::FRead(void* thePtr, int theElemSize, int theCount, PFILE* theFile)
{
	if (theFile->mRecord != NULL)
	{
		if (theElemSize <= 0 || theCount <= 0)
			return 0;

		// 实际读取的字节数不能超过当前资源文件剩余可读取的字节数
		int aSizeBytes = std::min(theElemSize*theCount, theFile->mRecord->mSize - theFile->mPos);
		if (aSizeBytes <= 0)
			return 0;

		const uchar* src = theFile->mRecord->mCollection->mData.data() + theFile->mRecord->mStartPos + theFile->mPos;
		uchar* dest = (uchar*) thePtr;
		for (int i = 0; i < aSizeBytes; i++)
			*(dest++) = (*src++) ^ 0xF7; // 'Decrypt'
		theFile->mPos += aSizeBytes;
		return aSizeBytes / theElemSize;
	}

	return fread(thePtr, theElemSize, theCount, theFile->mFP);
}

int PakInterface::FGetC(PFILE* theFile)
{
	if (theFile->mRecord != NULL)
	{
		for (;;)
		{
			if (theFile->mPos >= theFile->mRecord->mSize)
				return EOF;
			char aChar = (char)(theFile->mRecord->mCollection->mData[theFile->mRecord->mStartPos + theFile->mPos++] ^ 0xF7);
			if (aChar != '\r')
				return (uchar) aChar;
		}
	}

	return fgetc(theFile->mFP);
}

int PakInterface::UnGetC(int theChar, PFILE* theFile)
{
	if (theFile->mRecord != NULL)
	{
		// This won't work if we're not pushing the same chars back in the stream
		theFile->mPos = std::max(theFile->mPos - 1, 0);
		return theChar;
	}

	return ungetc(theChar, theFile->mFP);
}

char* PakInterface::FGetS(char* thePtr, int theSize, PFILE* theFile)
{
	if (theFile->mRecord != NULL)
	{
		int anIdx = 0;
		while (anIdx < theSize - 1)
		{
			if (theFile->mPos >= theFile->mRecord->mSize)
			{
				if (anIdx == 0)
					return NULL;
				break;
			}
			char aChar = (char)(theFile->mRecord->mCollection->mData[theFile->mRecord->mStartPos + theFile->mPos++] ^ 0xF7);
			if (aChar != '\r')
				thePtr[anIdx++] = aChar;
			if (aChar == '\n')
				break;
		}
		thePtr[anIdx] = 0;
		return thePtr;
	}

	return fgets(thePtr, theSize, theFile->mFP);
}

int PakInterface::FEof(PFILE* theFile)
{
	if (theFile->mRecord != NULL)
		return theFile->mPos >= theFile->mRecord->mSize;
	else
		return feof(theFile->mFP);
}

///////////////////////////////////////////////////////////////////////////////

PFILE* p_fopen(const char* theFileName, const char* theAccess)
{
	return gPakInterface->FOpen(theFileName, theAccess);
}

int p_fclose(PFILE* theFile)
{
	return gPakInterface->FClose(theFile);
}

int p_fseek(PFILE* theFile, long theOffset, int theOrigin)
{
	return gPakInterface->FSeek(theFile, theOffset, theOrigin);
}

int p_ftell(PFILE* theFile)
{
	return gPakInterface->FTell(theFile);
}

size_t p_fread(void* thePtr, int theSize, int theCount, PFILE* theFile)
{
	return gPakInterface->FRead(thePtr, theSize, theCount, theFile);
}

size_t p_fwrite(const void* thePtr, int theSize, int theCount, PFILE* theFile)
{
	if (theFile->mFP == NULL)
		return 0;
	return fwrite(thePtr, theSize, theCount, theFile->mFP);
}

int p_fgetc(PFILE* theFile)
{
	return gPakInterface->FGetC(theFile);
}

int p_ungetc(int theChar, PFILE* theFile)
{
	return gPakInterface->UnGetC(theChar, theFile);
}

char* p_fgets(char* thePtr, int theSize, PFILE* theFile)
{
	return gPakInterface->FGetS(thePtr, theSize, theFile);
}

int p_feof(PFILE* theFile)
{
	return gPakInterface->FEof(theFile);
}

bool PakReadFile(const std::string& theFileName, std::vector<unsigned char>& theData)
{
	PFILE* aFP = p_fopen(theFileName.c_str(), "rb");
	if (aFP == NULL)
		return false;

	p_fseek(aFP, 0, SEEK_END);
	int aSize = p_ftell(aFP);
	p_fseek(aFP, 0, SEEK_SET);
	if (aSize < 0)
	{
		p_fclose(aFP);
		return false;
	}

	theData.resize((size_t)aSize);
	size_t aRead = aSize > 0 ? p_fread(theData.data(), 1, aSize, aFP) : 0;
	p_fclose(aFP);
	return aRead == (size_t)aSize;
}
