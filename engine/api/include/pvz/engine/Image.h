#pragma once

#include "pvz/engine/Types.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace pvz::engine
{

struct ImageHandle
{
    std::uint32_t mIndex{};
    std::uint32_t mGeneration{};

    [[nodiscard]] constexpr bool IsValid() const
    {
        return mGeneration != 0;
    }
};

enum class ImagePixelFormat : std::uint8_t
{
    Bgra8Unorm,
};

struct ImageDescriptor
{
    SizeI mSize{};
    ImagePixelFormat mPixelFormat{ImagePixelFormat::Bgra8Unorm};
};

struct ImageUpdate
{
    RectI mDestination{};
    std::uint32_t mSourceBytesPerRow{};
    std::span<const std::byte> mPixels{};
};

class IImageStore
{
public:
    virtual ~IImageStore() = default;

    [[nodiscard]] virtual bool CreateImage(
        const ImageDescriptor& theDescriptor,
        std::span<const std::byte> theInitialPixels,
        std::uint32_t theSourceBytesPerRow,
        ImageHandle& theImage) = 0;
    [[nodiscard]] virtual bool UpdateImage(
        ImageHandle theImage,
        const ImageUpdate& theUpdate) = 0;
    virtual void DestroyImage(ImageHandle theImage) = 0;
    [[nodiscard]] virtual bool GetImageSize(
        ImageHandle theImage,
        SizeI& theSize) const = 0;
};

enum class ImageDecodeError : std::uint8_t
{
    None,
    EmptyInput,
    UnsupportedFormat,
    InvalidData,
    DimensionsUnsupported,
    SizeOverflow,
    AllocationFailed,
};

struct DecodedImage
{
    ImageDescriptor mDescriptor{};
    std::uint32_t mBytesPerRow{};
    std::vector<std::byte> mPixels;
};

class IImageDecoder
{
public:
    virtual ~IImageDecoder() = default;

    [[nodiscard]] virtual bool Decode(
        std::span<const std::byte> theEncodedBytes,
        DecodedImage& theImage,
        ImageDecodeError& theError) const = 0;
};

static_assert(sizeof(ImageHandle) == 8);
static_assert(sizeof(ImagePixelFormat) == 1);

} // namespace pvz::engine
