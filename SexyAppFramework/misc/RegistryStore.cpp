#include "RegistryStore.h"
#include "AutoCrit.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace Sexy;

// File format (UTF-8 text):
//   [Key\SubKey]
//   ValueName=<type>:<hex bytes>
// Value names and key names are percent-encoded for '%', '=', '[', ']' and
// control characters.

static std::string Escape(const std::string& theString)
{
	static const char* kHex = "0123456789ABCDEF";
	std::string aResult;
	for (unsigned char c : theString)
	{
		if (c == '%' || c == '=' || c == '[' || c == ']' || c < 0x20 || c == 0x7F)
		{
			aResult += '%';
			aResult += kHex[c >> 4];
			aResult += kHex[c & 0xF];
		}
		else
			aResult += (char)c;
	}
	return aResult;
}

static int HexDigit(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

static std::string Unescape(const std::string& theString)
{
	std::string aResult;
	for (size_t i = 0; i < theString.size(); i++)
	{
		if (theString[i] == '%' && i + 2 < theString.size() && HexDigit(theString[i + 1]) >= 0 && HexDigit(theString[i + 2]) >= 0)
		{
			aResult += (char)(HexDigit(theString[i + 1]) * 16 + HexDigit(theString[i + 2]));
			i += 2;
		}
		else
			aResult += theString[i];
	}
	return aResult;
}

std::string RegistryStore::NormalizeKey(const std::string& theKey)
{
	std::string aKey;
	for (char c : theKey)
	{
		if (c == '/')
			c = '\\';
		if (c == '\\' && (aKey.empty() || aKey.back() == '\\'))
			continue;
		aKey += c;
	}
	while (!aKey.empty() && aKey.back() == '\\')
		aKey.pop_back();
	return aKey;
}

void RegistryStore::Load(const std::string& theFileName)
{
	AutoCrit anAutoCrit(mCritSect);

	mFileName = theFileName;
	mKeys.clear();

	std::ifstream aStream(theFileName, std::ios::in | std::ios::binary);
	if (!aStream.is_open())
		return;

	std::string aLine;
	std::string aCurKey;
	while (std::getline(aStream, aLine))
	{
		if (!aLine.empty() && aLine.back() == '\r')
			aLine.pop_back();
		if (aLine.empty() || aLine[0] == ';')
			continue;

		if (aLine[0] == '[' && aLine.back() == ']')
		{
			aCurKey = NormalizeKey(Unescape(aLine.substr(1, aLine.size() - 2)));
			mKeys[aCurKey];
			continue;
		}

		size_t anEquals = aLine.find('=');
		size_t aColon = aLine.find(':', anEquals);
		if (anEquals == std::string::npos || aColon == std::string::npos)
			continue;

		Value aValue;
		aValue.mType = (std::uint32_t)strtoul(aLine.substr(anEquals + 1, aColon - anEquals - 1).c_str(), nullptr, 10);
		std::string aHex = aLine.substr(aColon + 1);
		for (size_t i = 0; i + 1 < aHex.size(); i += 2)
		{
			int aHigh = HexDigit(aHex[i]);
			int aLow = HexDigit(aHex[i + 1]);
			if (aHigh < 0 || aLow < 0)
				break;
			aValue.mData.push_back((unsigned char)(aHigh * 16 + aLow));
		}

		mKeys[aCurKey][Unescape(aLine.substr(0, anEquals))] = aValue;
	}
}

bool RegistryStore::Save()
{
	if (mFileName.empty())
		return false;

	static const char* kHex = "0123456789ABCDEF";
	std::ostringstream aStream;
	aStream << "; Plants vs. Zombies settings\n";
	for (const auto& aKey : mKeys)
	{
		aStream << '[' << Escape(aKey.first) << "]\n";
		for (const auto& aValue : aKey.second)
		{
			aStream << Escape(aValue.first) << '=' << aValue.second.mType << ':';
			for (unsigned char c : aValue.second.mData)
				aStream << kHex[c >> 4] << kHex[c & 0xF];
			aStream << '\n';
		}
	}

	// Write to a temporary file first so a crash never leaves a torn file.
	std::string aTempName = mFileName + ".tmp";
	{
		std::ofstream anOut(aTempName, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!anOut.is_open())
			return false;
		anOut << aStream.str();
		if (!anOut.good())
			return false;
	}

	std::error_code anError;
	std::filesystem::rename(aTempName, mFileName, anError);
	return !anError;
}

bool RegistryStore::Read(const std::string& theKey, const std::string& theValueName, Value& theValue)
{
	AutoCrit anAutoCrit(mCritSect);

	auto aKeyItr = mKeys.find(NormalizeKey(theKey));
	if (aKeyItr == mKeys.end())
		return false;

	auto aValueItr = aKeyItr->second.find(theValueName);
	if (aValueItr == aKeyItr->second.end())
		return false;

	theValue = aValueItr->second;
	return true;
}

bool RegistryStore::Write(const std::string& theKey, const std::string& theValueName, std::uint32_t theType, const unsigned char* theData, size_t theLength)
{
	AutoCrit anAutoCrit(mCritSect);

	std::string aKey = NormalizeKey(theKey);

	// Creating a key implicitly creates its parents, as in the registry.
	for (size_t aPos = aKey.find('\\'); aPos != std::string::npos; aPos = aKey.find('\\', aPos + 1))
		mKeys[aKey.substr(0, aPos)];

	Value& aValue = mKeys[aKey][theValueName];
	aValue.mType = theType;
	aValue.mData.assign(theData, theData + theLength);
	return Save();
}

bool RegistryStore::EraseKey(const std::string& theKey)
{
	AutoCrit anAutoCrit(mCritSect);

	std::string aKey = NormalizeKey(theKey);
	auto anItr = mKeys.find(aKey);
	if (anItr == mKeys.end())
		return false;

	// RegDeleteKey only deletes keys without subkeys; the framework only
	// erases leaf keys, so remove descendants too for robustness.
	std::string aPrefix = aKey + "\\";
	for (auto aDescendant = mKeys.begin(); aDescendant != mKeys.end(); )
	{
		if (strncasecmp(aDescendant->first.c_str(), aPrefix.c_str(), aPrefix.size()) == 0)
			aDescendant = mKeys.erase(aDescendant);
		else
			++aDescendant;
	}

	mKeys.erase(aKey);
	Save();
	return true;
}

bool RegistryStore::EraseValue(const std::string& theKey, const std::string& theValueName)
{
	AutoCrit anAutoCrit(mCritSect);

	auto aKeyItr = mKeys.find(NormalizeKey(theKey));
	if (aKeyItr == mKeys.end())
		return false;

	if (aKeyItr->second.erase(theValueName) == 0)
		return false;

	Save();
	return true;
}

bool RegistryStore::GetSubKeys(const std::string& theKey, std::vector<std::string>& theSubKeys)
{
	AutoCrit anAutoCrit(mCritSect);

	theSubKeys.clear();
	std::string aKey = NormalizeKey(theKey);
	if (!aKey.empty() && mKeys.find(aKey) == mKeys.end())
		return false;

	std::string aPrefix = aKey.empty() ? std::string() : aKey + "\\";
	for (const auto& anEntry : mKeys)
	{
		if (anEntry.first.size() <= aPrefix.size() || strncasecmp(anEntry.first.c_str(), aPrefix.c_str(), aPrefix.size()) != 0)
			continue;

		std::string aRest = anEntry.first.substr(aPrefix.size());
		if (aRest.find('\\') == std::string::npos)
			theSubKeys.push_back(aRest);
	}

	return true;
}
