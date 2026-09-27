#ifndef __PAKINTERFACE_H__
#define __PAKINTERFACE_H__

#include <cstdint>
#include <cstdio>
#include <list>
#include <map>
#include <string>
#include <vector>

class PakCollection;

// [定义]资源包文件：包含了若干游戏资源的 .pak 文件。例如：main.pak
// [定义]资源文件：资源包文件中的一项具体资源的文件。例如：zombie_falling_1.ogg

// ====================================================================================================
// ★ 一个 PakRecord 实例对应资源包内的一个资源文件的数据，包括文件名，地址，大小等信息
// ====================================================================================================
class PakRecord
{
public:
	PakCollection*			mCollection;			// 指向该资源文件所在的资源包的 PakCollection
	std::string				mFileName;				// 资源文件的名称及路径（路径从 .pak 开始），例如 sounds\zombie_falling_1.ogg
	std::uint64_t			mFileTime;				// Windows FILETIME of the file
	int						mStartPos;				// 该资源文件在资源包中的位置（即在 mCollection->mData 中的偏移量）
	int						mSize;					// 资源文件的大小，单位为 Byte（字节数）
};

typedef std::map<std::string, PakRecord> PakRecordMap;

// ====================================================================================================
// ★ 一个 PakCollection 实例对应一个 pak 资源包在内存中的数据
// ====================================================================================================
class PakCollection
{
public:
	std::vector<unsigned char>	mData;				// 资源包中的所有数据（仍为加密状态）
};

typedef std::list<PakCollection> PakCollectionList;

struct PFILE
{
	PakRecord*				mRecord;
	int						mPos;
	FILE*					mFP;
};

class PakInterfaceBase
{
public:
	virtual ~PakInterfaceBase() {}
	virtual PFILE*			FOpen(const char* theFileName, const char* theAccess) = 0;
	virtual int				FClose(PFILE* theFile) = 0;
	virtual int				FSeek(PFILE* theFile, long theOffset, int theOrigin) = 0;
	virtual int				FTell(PFILE* theFile) = 0;
	virtual size_t			FRead(void* thePtr, int theElemSize, int theCount, PFILE* theFile) = 0;
	virtual int				FGetC(PFILE* theFile) = 0;
	virtual int				UnGetC(int theChar, PFILE* theFile) = 0;
	virtual char*			FGetS(char* thePtr, int theSize, PFILE* theFile) = 0;
	virtual int				FEof(PFILE* theFile) = 0;
};

class PakInterface : public PakInterfaceBase
{
public:
	PakCollectionList		mPakCollectionList;		// 通过 AddPakFile() 添加的各个资源包的数据的链表
	PakRecordMap			mPakRecordMap;			// 所有已添加的资源包中的所有资源文件的、从文件名到文件数据的映射容器

public:
	PakInterface();
	virtual ~PakInterface();

	bool					AddPakFile(const std::string& theFileName);
	PFILE*					FOpen(const char* theFileName, const char* theAccess);
	int						FClose(PFILE* theFile);
	int						FSeek(PFILE* theFile, long theOffset, int theOrigin);
	int						FTell(PFILE* theFile);
	size_t					FRead(void* thePtr, int theElemSize, int theCount, PFILE* theFile);
	int						FGetC(PFILE* theFile);
	int						UnGetC(int theChar, PFILE* theFile);
	char*					FGetS(char* thePtr, int theSize, PFILE* theFile);
	int						FEof(PFILE* theFile);
};

extern PakInterface* gPakInterface;

PFILE*						p_fopen(const char* theFileName, const char* theAccess);
int							p_fclose(PFILE* theFile);
int							p_fseek(PFILE* theFile, long theOffset, int theOrigin);
int							p_ftell(PFILE* theFile);
size_t						p_fread(void* thePtr, int theSize, int theCount, PFILE* theFile);
size_t						p_fwrite(const void* thePtr, int theSize, int theCount, PFILE* theFile);
int							p_fgetc(PFILE* theFile);
int							p_ungetc(int theChar, PFILE* theFile);
char*						p_fgets(char* thePtr, int theSize, PFILE* theFile);
int							p_feof(PFILE* theFile);

// Reads a whole file from the mounted packs, or from disk if it is not packed.
bool						PakReadFile(const std::string& theFileName, std::vector<unsigned char>& theData);

#endif //__PAKINTERFACE_H__
