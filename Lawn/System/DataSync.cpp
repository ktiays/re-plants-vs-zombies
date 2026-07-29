#include "DataSync.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <limits>
#include <stdexcept>

DataReader::~DataReader()
{
    Close();
}

bool DataReader::OpenFile(const std::string& theFileName)
{
    Close();
    mFile = std::fopen(theFileName.c_str(), "rb");
    return mFile != nullptr;
}

void DataReader::OpenMemory(
    const void* theData,
    std::size_t theDataLen,
    bool theTakeOwnership)
{
    Close();
    mData = static_cast<char*>(const_cast<void*>(theData));
    mDataLen = theDataLen;
    mDataPos = 0;
    mOwnData = theTakeOwnership;
}

void DataReader::Close()
{
    if (mFile)
    {
        std::fclose(mFile);
        mFile = nullptr;
    }

    if (mOwnData)
        delete[] mData;

    mData = nullptr;
    mDataLen = 0;
    mDataPos = 0;
    mOwnData = false;
}

void DataReader::ReadBytes(void* theMemory, std::size_t theByteCount)
{
    if (mData)
    {
        if (mDataPos > mDataLen || theByteCount > mDataLen - mDataPos)
            throw DataReaderException();

        std::memcpy(theMemory, mData + mDataPos, theByteCount);
        mDataPos += theByteCount;
        return;
    }

    if (!mFile ||
        std::fread(theMemory, sizeof(std::uint8_t), theByteCount, mFile) !=
            theByteCount)
    {
        throw DataReaderException();
    }
}

void DataReader::Rewind(std::size_t theByteCount)
{
    theByteCount = std::min(theByteCount, mDataPos);
    mDataPos -= theByteCount;
}

std::uint16_t DataReader::ReadShort()
{
    std::array<std::uint8_t, 2> aBytes{};
    ReadBytes(aBytes.data(), aBytes.size());
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(aBytes[0]) |
        static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(aBytes[1]) << 8));
}

std::uint64_t DataReader::ReadUInt64()
{
    const auto aLow = static_cast<std::uint64_t>(ReadLong());
    const auto aHigh = static_cast<std::uint64_t>(ReadLong());
    return aLow | (aHigh << 32);
}

std::uint32_t DataReader::ReadLong()
{
    std::array<std::uint8_t, 4> aBytes{};
    ReadBytes(aBytes.data(), aBytes.size());
    return static_cast<std::uint32_t>(aBytes[0]) |
           (static_cast<std::uint32_t>(aBytes[1]) << 8) |
           (static_cast<std::uint32_t>(aBytes[2]) << 16) |
           (static_cast<std::uint32_t>(aBytes[3]) << 24);
}

std::uint8_t DataReader::ReadByte()
{
    std::uint8_t aValue{};
    ReadBytes(&aValue, sizeof(aValue));
    return aValue;
}

bool DataReader::ReadBool()
{
    return ReadByte() != 0;
}

float DataReader::ReadFloat()
{
    return std::bit_cast<float>(ReadLong());
}

double DataReader::ReadDouble()
{
    return std::bit_cast<double>(ReadUInt64());
}

void DataReader::ReadString(std::string& theString)
{
    const auto aLength = ReadShort();
    theString.resize(aLength);
    ReadBytes(theString.data(), aLength);
}

DataSync::DataSync(DataReader& theReader)
{
    Reset();
    mReader = &theReader;
}

DataSync::DataSync(DataWriter& theWriter)
{
    Reset();
    mWriter = &theWriter;
}

void DataSync::ResetPointerTable()
{
    mIntToPointerMap.clear();
    mPointerToIntMap.clear();
    mPointerSyncList.clear();
    mCurPointerIndex = 1;
    mPointerToIntMap[nullptr] = 0;
    mIntToPointerMap[0] = nullptr;
}

void DataSync::Reset()
{
    mReader = nullptr;
    mWriter = nullptr;
    mVersion = 0;
    ResetPointerTable();
}

void DataSync::SyncBytes(void* theData, std::size_t theDataLen)
{
    if (mReader)
        mReader->ReadBytes(theData, theDataLen);
    else
        mWriter->WriteBytes(theData, theDataLen);
}

void DataSync::SyncBool(bool& theValue)
{
    if (mReader)
        theValue = mReader->ReadBool();
    else
        mWriter->WriteBool(theValue);
}

void DataSync::SyncUInt64(std::uint64_t& theValue)
{
    if (mReader)
        theValue = mReader->ReadUInt64();
    else
        mWriter->WriteUInt64(theValue);
}

void DataSync::SyncInt64(std::int64_t& theValue)
{
    if (mReader)
    {
        theValue = std::bit_cast<std::int64_t>(mReader->ReadUInt64());
    }
    else
    {
        mWriter->WriteUInt64(std::bit_cast<std::uint64_t>(theValue));
    }
}

void DataSync::SyncFloat(float& theValue)
{
    if (mReader)
        theValue = mReader->ReadFloat();
    else
        mWriter->WriteFloat(theValue);
}

void DataSync::SyncDouble(double& theValue)
{
    if (mReader)
        theValue = mReader->ReadDouble();
    else
        mWriter->WriteDouble(theValue);
}

void DataSync::SyncString(std::string& theString)
{
    if (mReader)
        mReader->ReadString(theString);
    else
        mWriter->WriteString(theString);
}

DataWriter::~DataWriter()
{
    Close();
    delete[] mData;
}

bool DataWriter::OpenFile(const std::string& theFileName)
{
    Close();
    delete[] mData;
    mData = nullptr;
    mDataLen = 0;
    mCapacity = 0;
    mFile = std::fopen(theFileName.c_str(), "wb");
    return mFile != nullptr;
}

void DataWriter::Close()
{
    if (mFile)
    {
        std::fclose(mFile);
        mFile = nullptr;
    }
}

void DataWriter::EnsureCapacity(std::size_t theRequiredCapacity)
{
    if (mCapacity >= theRequiredCapacity)
        return;

    std::size_t aNewCapacity = std::max<std::size_t>(mCapacity, 32);
    while (aNewCapacity < theRequiredCapacity)
    {
        if (aNewCapacity > std::numeric_limits<std::size_t>::max() / 2)
        {
            aNewCapacity = theRequiredCapacity;
            break;
        }
        aNewCapacity *= 2;
    }

    char* aData = new char[aNewCapacity];
    std::memcpy(aData, mData, mDataLen);
    delete[] mData;
    mData = aData;
    mCapacity = aNewCapacity;
}

void DataWriter::OpenMemory(std::size_t theReserveAmount)
{
    Close();
    delete[] mData;
    mData = nullptr;
    mDataLen = 0;
    mCapacity = 0;

    theReserveAmount = std::max<std::size_t>(theReserveAmount, 32);
    mData = new char[theReserveAmount];
    mCapacity = theReserveAmount;
}

bool DataWriter::WriteToFile(const std::string& theFileName) const
{
    auto* aFile = std::fopen(theFileName.c_str(), "wb");
    if (!aFile)
        return false;

    const auto aWritten =
        std::fwrite(mData, sizeof(std::uint8_t), mDataLen, aFile);
    const auto aCloseResult = std::fclose(aFile);
    return aWritten == mDataLen && aCloseResult == 0;
}

void DataWriter::WriteBytes(const void* theData, std::size_t theDataLen)
{
    if (mData)
    {
        if (theDataLen >
            std::numeric_limits<std::size_t>::max() - mDataLen)
        {
            throw std::length_error("serialized data is too large");
        }

        EnsureCapacity(mDataLen + theDataLen);
        std::memcpy(mData + mDataLen, theData, theDataLen);
        mDataLen += theDataLen;
    }
    else if (mFile)
    {
        if (std::fwrite(
                theData,
                sizeof(std::uint8_t),
                theDataLen,
                mFile) != theDataLen)
        {
            throw std::runtime_error("could not write serialized data");
        }
    }
}

void DataWriter::WriteLong(std::uint32_t theValue)
{
    const std::array<std::uint8_t, 4> aBytes{
        static_cast<std::uint8_t>(theValue),
        static_cast<std::uint8_t>(theValue >> 8),
        static_cast<std::uint8_t>(theValue >> 16),
        static_cast<std::uint8_t>(theValue >> 24),
    };
    WriteBytes(aBytes.data(), aBytes.size());
}

void DataWriter::WriteUInt64(std::uint64_t theValue)
{
    WriteLong(static_cast<std::uint32_t>(theValue));
    WriteLong(static_cast<std::uint32_t>(theValue >> 32));
}

void DataWriter::WriteShort(std::uint16_t theValue)
{
    const std::array<std::uint8_t, 2> aBytes{
        static_cast<std::uint8_t>(theValue),
        static_cast<std::uint8_t>(theValue >> 8),
    };
    WriteBytes(aBytes.data(), aBytes.size());
}

void DataWriter::WriteByte(std::uint8_t theValue)
{
    WriteBytes(&theValue, sizeof(theValue));
}

void DataWriter::WriteBool(bool theValue)
{
    WriteByte(theValue ? std::uint8_t{1} : std::uint8_t{0});
}

void DataWriter::WriteFloat(float theValue)
{
    WriteLong(std::bit_cast<std::uint32_t>(theValue));
}

void DataWriter::WriteDouble(double theValue)
{
    WriteUInt64(std::bit_cast<std::uint64_t>(theValue));
}

void DataWriter::WriteString(const std::string& theString)
{
    if (theString.size() > std::numeric_limits<std::uint16_t>::max())
        throw std::length_error("serialized string is too large");

    WriteShort(static_cast<std::uint16_t>(theString.size()));
    WriteBytes(theString.data(), theString.size());
}

std::size_t DataWriter::GetPos() const
{
    return mDataLen;
}

void DataWriter::SetLong(
    std::uint32_t theValue,
    std::size_t thePosition)
{
    if (!mData || thePosition > mDataLen || 4 > mDataLen - thePosition)
        throw std::out_of_range("serialized position is outside the buffer");

    const auto aSavedPosition = mDataLen;
    mDataLen = thePosition;
    WriteLong(theValue);
    mDataLen = aSavedPosition;
}

void DataWriter::SetShort(
    std::uint16_t theValue,
    std::size_t thePosition)
{
    if (!mData || thePosition > mDataLen || 2 > mDataLen - thePosition)
        throw std::out_of_range("serialized position is outside the buffer");

    const auto aSavedPosition = mDataLen;
    mDataLen = thePosition;
    WriteShort(theValue);
    mDataLen = aSavedPosition;
}

void DataWriter::SetByte(
    std::uint8_t theValue,
    std::size_t thePosition)
{
    if (!mData || thePosition >= mDataLen)
        throw std::out_of_range("serialized position is outside the buffer");
    mData[thePosition] = static_cast<char>(theValue);
}
