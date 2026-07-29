#include "pvz/engine/core/BinaryStateIO.h"

#include <algorithm>
#include <bit>
#include <cstring>
#include <limits>

namespace pvz::engine::core
{
namespace
{

template<typename Value>
void AppendLittleEndian(std::vector<std::byte>& theBytes, Value theValue)
{
    static_assert(std::is_unsigned_v<Value>);

    for (std::uint32_t aShift = 0; aShift < sizeof(Value) * CHAR_BIT;
         aShift += CHAR_BIT)
    {
        theBytes.push_back(
            static_cast<std::byte>((theValue >> aShift) & Value{0xFF}));
    }
}

template<typename Value>
Value ReadLittleEndian(
    std::span<const std::byte> theBytes,
    std::uint64_t theOffset)
{
    static_assert(std::is_unsigned_v<Value>);

    Value aValue{};
    for (std::uint32_t aIndex = 0; aIndex < sizeof(Value); ++aIndex)
    {
        const auto aByte = static_cast<Value>(
            std::to_integer<std::uint8_t>(
                theBytes[static_cast<std::size_t>(theOffset + aIndex)]));
        aValue |= aByte << (aIndex * CHAR_BIT);
    }
    return aValue;
}

} // namespace

bool BinaryStateWriter::WriteU8(std::uint8_t theValue)
{
    mBytes.push_back(static_cast<std::byte>(theValue));
    return true;
}

bool BinaryStateWriter::WriteU16(std::uint16_t theValue)
{
    AppendLittleEndian(mBytes, theValue);
    return true;
}

bool BinaryStateWriter::WriteU32(std::uint32_t theValue)
{
    AppendLittleEndian(mBytes, theValue);
    return true;
}

bool BinaryStateWriter::WriteU64(std::uint64_t theValue)
{
    AppendLittleEndian(mBytes, theValue);
    return true;
}

bool BinaryStateWriter::WriteI32(std::int32_t theValue)
{
    return WriteU32(std::bit_cast<std::uint32_t>(theValue));
}

bool BinaryStateWriter::WriteI64(std::int64_t theValue)
{
    return WriteU64(std::bit_cast<std::uint64_t>(theValue));
}

bool BinaryStateWriter::WriteBool(bool theValue)
{
    return WriteU8(theValue ? std::uint8_t{1} : std::uint8_t{0});
}

bool BinaryStateWriter::WriteBytes(std::span<const std::byte> theBytes)
{
    mBytes.insert(mBytes.end(), theBytes.begin(), theBytes.end());
    return true;
}

bool BinaryStateWriter::WriteUtf8(std::string_view theValue)
{
    if (theValue.size() > std::numeric_limits<std::uint32_t>::max())
    {
        mError = StateIoError::LengthOverflow;
        return false;
    }

    if (!WriteU32(static_cast<std::uint32_t>(theValue.size())))
        return false;

    return WriteBytes(std::as_bytes(std::span(theValue)));
}

StateIoError BinaryStateWriter::GetError() const
{
    return mError;
}

std::uint64_t BinaryStateWriter::GetBytesWritten() const
{
    return static_cast<std::uint64_t>(mBytes.size());
}

std::span<const std::byte> BinaryStateWriter::GetBytes() const
{
    return mBytes;
}

BinaryStateReader::BinaryStateReader(std::span<const std::byte> theBytes)
    : mBytes(theBytes)
{
}

bool BinaryStateReader::ReadU8(std::uint8_t& theValue)
{
    if (!CanRead(1))
        return false;

    theValue = std::to_integer<std::uint8_t>(
        mBytes[static_cast<std::size_t>(mOffset)]);
    ++mOffset;
    return true;
}

bool BinaryStateReader::ReadU16(std::uint16_t& theValue)
{
    if (!CanRead(sizeof(theValue)))
        return false;

    theValue = ReadLittleEndian<std::uint16_t>(mBytes, mOffset);
    mOffset += sizeof(theValue);
    return true;
}

bool BinaryStateReader::ReadU32(std::uint32_t& theValue)
{
    if (!CanRead(sizeof(theValue)))
        return false;

    theValue = ReadLittleEndian<std::uint32_t>(mBytes, mOffset);
    mOffset += sizeof(theValue);
    return true;
}

bool BinaryStateReader::ReadU64(std::uint64_t& theValue)
{
    if (!CanRead(sizeof(theValue)))
        return false;

    theValue = ReadLittleEndian<std::uint64_t>(mBytes, mOffset);
    mOffset += sizeof(theValue);
    return true;
}

bool BinaryStateReader::ReadI32(std::int32_t& theValue)
{
    std::uint32_t aValue{};
    if (!ReadU32(aValue))
        return false;

    theValue = std::bit_cast<std::int32_t>(aValue);
    return true;
}

bool BinaryStateReader::ReadI64(std::int64_t& theValue)
{
    std::uint64_t aValue{};
    if (!ReadU64(aValue))
        return false;

    theValue = std::bit_cast<std::int64_t>(aValue);
    return true;
}

bool BinaryStateReader::ReadBool(bool& theValue)
{
    std::uint8_t aValue{};
    if (!ReadU8(aValue))
        return false;

    if (aValue > 1)
    {
        mError = StateIoError::InvalidValue;
        return false;
    }

    theValue = aValue == 1;
    return true;
}

bool BinaryStateReader::ReadBytes(std::span<std::byte> theBytes)
{
    if (!CanRead(static_cast<std::uint64_t>(theBytes.size())))
        return false;

    std::memcpy(
        theBytes.data(),
        mBytes.data() + static_cast<std::size_t>(mOffset),
        theBytes.size());
    mOffset += static_cast<std::uint64_t>(theBytes.size());
    return true;
}

bool BinaryStateReader::ReadUtf8(std::string& theValue)
{
    std::uint32_t aLength{};
    if (!ReadU32(aLength))
        return false;

    if (!CanRead(aLength))
        return false;

    const auto* aData = reinterpret_cast<const char*>(
        mBytes.data() + static_cast<std::size_t>(mOffset));
    theValue.assign(aData, aLength);
    mOffset += aLength;
    return true;
}

StateIoError BinaryStateReader::GetError() const
{
    return mError;
}

std::uint64_t BinaryStateReader::GetBytesRemaining() const
{
    return static_cast<std::uint64_t>(mBytes.size()) - mOffset;
}

bool BinaryStateReader::CanRead(std::uint64_t theByteCount)
{
    if (mError != StateIoError::None)
        return false;

    const auto aSize = static_cast<std::uint64_t>(mBytes.size());
    if (mOffset > aSize || theByteCount > aSize - mOffset)
    {
        mError = StateIoError::EndOfInput;
        return false;
    }

    return true;
}

} // namespace pvz::engine::core
