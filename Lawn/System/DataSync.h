#ifndef __DATASYNC_H__
#define __DATASYNC_H__

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

class DataReader
{
protected:
    std::FILE* mFile{};
    char* mData{};
    std::size_t mDataLen{};
    std::size_t mDataPos{};
    bool mOwnData{};

public:
    DataReader() = default;
    virtual ~DataReader();

    bool OpenFile(const std::string& theFileName);
    void OpenMemory(
        const void* theData,
        std::size_t theDataLen,
        bool theTakeOwnership);
    void Close();
    void ReadBytes(void* theMemory, std::size_t theByteCount);
    void Rewind(std::size_t theByteCount);
    std::uint64_t ReadUInt64();
    std::uint32_t ReadLong();
    std::uint16_t ReadShort();
    std::uint8_t ReadByte();
    bool ReadBool();
    float ReadFloat();
    double ReadDouble();
    void ReadString(std::string& theString);
};

class DataReaderException : public std::exception
{
};

class DataWriter
{
protected:
    std::FILE* mFile{};
    char* mData{};
    std::size_t mDataLen{};
    std::size_t mCapacity{};

protected:
    void EnsureCapacity(std::size_t theRequiredCapacity);

public:
    DataWriter() = default;
    virtual ~DataWriter();

    bool OpenFile(const std::string& theFileName);
    void OpenMemory(std::size_t theReserveAmount = 0x20);
    void Close();
    bool WriteToFile(const std::string& theFileName) const;
    void WriteBytes(const void* theData, std::size_t theDataLen);
    void WriteUInt64(std::uint64_t theValue);
    void WriteLong(std::uint32_t theValue);
    void WriteShort(std::uint16_t theValue);
    void WriteByte(std::uint8_t theValue);
    void WriteBool(bool theValue);
    void WriteFloat(float theValue);
    void WriteDouble(double theValue);
    void WriteString(const std::string& theString);
    [[nodiscard]] std::size_t GetPos() const;
    void SetLong(std::uint32_t theValue, std::size_t thePosition);
    void SetShort(std::uint16_t theValue, std::size_t thePosition);
    void SetByte(std::uint8_t theValue, std::size_t thePosition);
    [[nodiscard]] void* GetDataPtr() const { return mData; }
    [[nodiscard]] std::size_t GetDataLen() const { return mDataLen; }
};

using PointerToIntMap = std::map<void*, int>;
using IntToPointerMap = std::map<int, void*>;

class DataSync
{
protected:
    DataReader* mReader{};
    DataWriter* mWriter{};
    int mVersion{};
    PointerToIntMap mPointerToIntMap;
    IntToPointerMap mIntToPointerMap;
    std::vector<void**> mPointerSyncList;
    int mCurPointerIndex{};

protected:
    void ResetPointerTable();
    void Reset();

public:
    explicit DataSync(DataReader& theReader);
    explicit DataSync(DataWriter& theWriter);
    virtual ~DataSync() = default;

    void SyncPointers() {}
    void SetReader(DataReader* theReader) { mReader = theReader; }
    void SetWriter(DataWriter* theWriter) { mWriter = theWriter; }
    [[nodiscard]] DataReader* GetReader() { return mReader; }
    [[nodiscard]] DataWriter* GetWriter() { return mWriter; }
    void SyncBytes(void* theData, std::size_t theDataLen);

    template<std::integral Integer>
        requires (!std::same_as<std::remove_cv_t<Integer>, bool>)
    void SyncLong(Integer& theValue)
    {
        if (mReader)
        {
            const auto aWireValue = mReader->ReadLong();
            if constexpr (std::is_signed_v<Integer>)
            {
                theValue = static_cast<Integer>(
                    std::bit_cast<std::int32_t>(aWireValue));
            }
            else
            {
                theValue = static_cast<Integer>(aWireValue);
            }
        }
        else
        {
            mWriter->WriteLong(static_cast<std::uint32_t>(theValue));
        }
    }

    template<std::integral Integer>
        requires (!std::same_as<std::remove_cv_t<Integer>, bool>)
    void SyncSLong(Integer& theValue)
    {
        if (mReader)
        {
            const auto aWireValue = std::bit_cast<std::int32_t>(
                mReader->ReadLong());
            theValue = static_cast<Integer>(aWireValue);
        }
        else
        {
            const auto aWireValue = static_cast<std::int32_t>(theValue);
            mWriter->WriteLong(std::bit_cast<std::uint32_t>(aWireValue));
        }
    }

    template<std::integral Integer>
        requires (!std::same_as<std::remove_cv_t<Integer>, bool>)
    void SyncShort(Integer& theValue)
    {
        if (mReader)
        {
            const auto aWireValue = mReader->ReadShort();
            if constexpr (std::is_signed_v<Integer>)
            {
                theValue = static_cast<Integer>(
                    std::bit_cast<std::int16_t>(aWireValue));
            }
            else
            {
                theValue = static_cast<Integer>(aWireValue);
            }
        }
        else
        {
            mWriter->WriteShort(static_cast<std::uint16_t>(theValue));
        }
    }

    template<std::integral Integer>
        requires (!std::same_as<std::remove_cv_t<Integer>, bool>)
    void SyncSShort(Integer& theValue)
    {
        if (mReader)
        {
            const auto aWireValue = std::bit_cast<std::int16_t>(
                mReader->ReadShort());
            theValue = static_cast<Integer>(aWireValue);
        }
        else
        {
            const auto aWireValue = static_cast<std::int16_t>(theValue);
            mWriter->WriteShort(std::bit_cast<std::uint16_t>(aWireValue));
        }
    }

    template<std::integral Integer>
        requires (!std::same_as<std::remove_cv_t<Integer>, bool>)
    void SyncByte(Integer& theValue)
    {
        if (mReader)
        {
            const auto aWireValue = mReader->ReadByte();
            if constexpr (std::is_signed_v<Integer>)
            {
                theValue = static_cast<Integer>(
                    std::bit_cast<std::int8_t>(aWireValue));
            }
            else
            {
                theValue = static_cast<Integer>(aWireValue);
            }
        }
        else
        {
            mWriter->WriteByte(static_cast<std::uint8_t>(theValue));
        }
    }

    template<std::integral Integer>
        requires (!std::same_as<std::remove_cv_t<Integer>, bool>)
    void SyncSByte(Integer& theValue)
    {
        if (mReader)
        {
            const auto aWireValue = std::bit_cast<std::int8_t>(
                mReader->ReadByte());
            theValue = static_cast<Integer>(aWireValue);
        }
        else
        {
            const auto aWireValue = static_cast<std::int8_t>(theValue);
            mWriter->WriteByte(std::bit_cast<std::uint8_t>(aWireValue));
        }
    }

    void SyncBool(bool& theValue);
    void SyncUInt64(std::uint64_t& theValue);
    void SyncInt64(std::int64_t& theValue);
    void SyncFloat(float& theValue);
    void SyncDouble(double& theValue);
    void SyncString(std::string& theString);
    void SyncPointer(void**) {}
    void RegisterPointer(void*) {}
    void SetVersion(int theVersion) { mVersion = theVersion; }
    [[nodiscard]] int GetVersion() const { return mVersion; }
};

#endif
