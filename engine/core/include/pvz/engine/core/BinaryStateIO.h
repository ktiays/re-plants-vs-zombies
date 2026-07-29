#pragma once

#include "pvz/engine/StateIO.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pvz::engine::core
{

class BinaryStateWriter final : public IStateWriter
{
public:
    [[nodiscard]] bool WriteU8(std::uint8_t theValue) override;
    [[nodiscard]] bool WriteU16(std::uint16_t theValue) override;
    [[nodiscard]] bool WriteU32(std::uint32_t theValue) override;
    [[nodiscard]] bool WriteU64(std::uint64_t theValue) override;
    [[nodiscard]] bool WriteI32(std::int32_t theValue) override;
    [[nodiscard]] bool WriteI64(std::int64_t theValue) override;
    [[nodiscard]] bool WriteBool(bool theValue) override;
    [[nodiscard]] bool WriteBytes(
        std::span<const std::byte> theBytes) override;
    [[nodiscard]] bool WriteUtf8(std::string_view theValue) override;

    [[nodiscard]] StateIoError GetError() const override;
    [[nodiscard]] std::uint64_t GetBytesWritten() const override;
    [[nodiscard]] std::span<const std::byte> GetBytes() const;

private:
    std::vector<std::byte> mBytes;
    StateIoError mError{StateIoError::None};
};

class BinaryStateReader final : public IStateReader
{
public:
    explicit BinaryStateReader(std::span<const std::byte> theBytes);

    [[nodiscard]] bool ReadU8(std::uint8_t& theValue) override;
    [[nodiscard]] bool ReadU16(std::uint16_t& theValue) override;
    [[nodiscard]] bool ReadU32(std::uint32_t& theValue) override;
    [[nodiscard]] bool ReadU64(std::uint64_t& theValue) override;
    [[nodiscard]] bool ReadI32(std::int32_t& theValue) override;
    [[nodiscard]] bool ReadI64(std::int64_t& theValue) override;
    [[nodiscard]] bool ReadBool(bool& theValue) override;
    [[nodiscard]] bool ReadBytes(std::span<std::byte> theBytes) override;
    [[nodiscard]] bool ReadUtf8(std::string& theValue) override;

    [[nodiscard]] StateIoError GetError() const override;
    [[nodiscard]] std::uint64_t GetBytesRemaining() const override;

private:
    [[nodiscard]] bool CanRead(std::uint64_t theByteCount);

    std::span<const std::byte> mBytes;
    std::uint64_t mOffset{};
    StateIoError mError{StateIoError::None};
};

} // namespace pvz::engine::core
