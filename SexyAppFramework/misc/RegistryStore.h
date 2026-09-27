#ifndef __SEXY_REGISTRYSTORE_H__
#define __SEXY_REGISTRYSTORE_H__

#include "Common.h"
#include "CritSect.h"

namespace Sexy
{

// Value types, numerically identical to the Win32 registry types so existing
// code and persisted data agree.
enum RegistryValueType
{
	REGISTRY_TYPE_STRING	= 1,	// REG_SZ
	REGISTRY_TYPE_BINARY	= 3,	// REG_BINARY
	REGISTRY_TYPE_DWORD		= 4		// REG_DWORD
};

// A small portable replacement for the per-user Windows registry key the
// framework keeps its settings in. Keys are backslash-separated and compared
// case-insensitively, like registry keys. The store is saved to a text file
// after every modification.
class RegistryStore
{
public:
	struct Value
	{
		std::uint32_t					mType = 0;
		std::vector<unsigned char>		mData;
	};

private:
	typedef std::map<std::string, Value, StringLessNoCase> ValueMap;
	typedef std::map<std::string, ValueMap, StringLessNoCase> KeyMap;

	CritSect					mCritSect;
	KeyMap						mKeys;
	std::string					mFileName;

	static std::string			NormalizeKey(const std::string& theKey);
	bool						Save();

public:
	void						Load(const std::string& theFileName);

	bool						Read(const std::string& theKey, const std::string& theValueName, Value& theValue);
	bool						Write(const std::string& theKey, const std::string& theValueName, std::uint32_t theType, const unsigned char* theData, size_t theLength);
	bool						EraseKey(const std::string& theKey);
	bool						EraseValue(const std::string& theKey, const std::string& theValueName);
	bool						GetSubKeys(const std::string& theKey, std::vector<std::string>& theSubKeys);
};

}

#endif //__SEXY_REGISTRYSTORE_H__
