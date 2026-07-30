#pragma once

#include "pvz/engine/Types.h"

#include <cstddef>
#include <cstdint>
#include <span>

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

enum class BlendMode : std::uint8_t
{
    Normal,
    Additive,
};

enum class FilterMode : std::uint8_t
{
    Nearest,
    Linear,
};

enum class AddressMode : std::uint8_t
{
    Clamp,
    Repeat,
};

enum class MirrorMode : std::uint8_t
{
    None,
    Horizontal,
    Vertical,
    Both,
};

enum class ClipMode : std::uint8_t
{
    Disabled,
    Enabled,
};

struct SpriteDraw
{
    ImageHandle mImage{};
    RectI mSource{};
    RectF mDestination{};
    RectI mClip{};
    ColorRgba8 mColor{255, 255, 255, 255};
    PointF mRotationCenter{};
    float mRotationRadians{};
    BlendMode mBlendMode{BlendMode::Normal};
    FilterMode mFilterMode{FilterMode::Nearest};
    AddressMode mAddressMode{AddressMode::Clamp};
    MirrorMode mMirrorMode{MirrorMode::None};
    ClipMode mClipMode{ClipMode::Disabled};
};

class IRenderFrame
{
public:
    virtual ~IRenderFrame() = default;

    [[nodiscard]] virtual SizeI GetLogicalSize() const = 0;
    virtual void Clear(ColorRgba8 theColor) = 0;
    virtual void SubmitSprites(std::span<const SpriteDraw> theDraws) = 0;
};

static_assert(sizeof(ImageHandle) == 8);
static_assert(sizeof(ImagePixelFormat) == 1);

} // namespace pvz::engine
