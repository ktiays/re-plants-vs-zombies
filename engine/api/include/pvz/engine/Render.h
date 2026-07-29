#pragma once

#include "pvz/engine/Types.h"

#include <cstdint>
#include <span>

namespace pvz::engine
{

struct ImageHandle
{
    std::uint32_t mIndex{};
    std::uint32_t mGeneration{};
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

} // namespace pvz::engine
