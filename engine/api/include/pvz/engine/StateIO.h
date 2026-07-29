#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace pvz::engine
{

enum class StateIoError : std::uint8_t
{
    None,
    EndOfInput,
    LengthOverflow,
    InvalidValue,
};

class IStateWriter
{
public:
    virtual ~IStateWriter() = default;

    [[nodiscard]] virtual bool WriteU8(std::uint8_t theValue) = 0;
    [[nodiscard]] virtual bool WriteU16(std::uint16_t theValue) = 0;
    [[nodiscard]] virtual bool WriteU32(std::uint32_t theValue) = 0;
    [[nodiscard]] virtual bool WriteU64(std::uint64_t theValue) = 0;
    [[nodiscard]] virtual bool WriteI32(std::int32_t theValue) = 0;
    [[nodiscard]] virtual bool WriteI64(std::int64_t theValue) = 0;
    [[nodiscard]] virtual bool WriteBool(bool theValue) = 0;
    [[nodiscard]] virtual bool WriteBytes(
        std::span<const std::byte> theBytes) = 0;
    [[nodiscard]] virtual bool WriteUtf8(std::string_view theValue) = 0;

    [[nodiscard]] virtual StateIoError GetError() const = 0;
    [[nodiscard]] virtual std::uint64_t GetBytesWritten() const = 0;
};

class IStateReader
{
public:
    virtual ~IStateReader() = default;

    [[nodiscard]] virtual bool ReadU8(std::uint8_t& theValue) = 0;
    [[nodiscard]] virtual bool ReadU16(std::uint16_t& theValue) = 0;
    [[nodiscard]] virtual bool ReadU32(std::uint32_t& theValue) = 0;
    [[nodiscard]] virtual bool ReadU64(std::uint64_t& theValue) = 0;
    [[nodiscard]] virtual bool ReadI32(std::int32_t& theValue) = 0;
    [[nodiscard]] virtual bool ReadI64(std::int64_t& theValue) = 0;
    [[nodiscard]] virtual bool ReadBool(bool& theValue) = 0;
    [[nodiscard]] virtual bool ReadBytes(std::span<std::byte> theBytes) = 0;
    [[nodiscard]] virtual bool ReadUtf8(std::string& theValue) = 0;

    [[nodiscard]] virtual StateIoError GetError() const = 0;
    [[nodiscard]] virtual std::uint64_t GetBytesRemaining() const = 0;
};

} // namespace pvz::engine
