#pragma once

#include "pvz/engine/Image.h"
#include "pvz/engine/Types.h"

#include <cstdint>
#include <span>

namespace pvz::engine
{

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

enum class SpriteGeometryMode : std::uint8_t
{
    DestinationRect,
    DestinationQuad,
};

struct SpriteQuad
{
    PointF mTopLeft{};
    PointF mTopRight{};
    PointF mBottomLeft{};
    PointF mBottomRight{};
};

struct SpriteDraw
{
    ImageHandle mImage{};
    RectI mSource{};
    RectF mDestination{};
    SpriteQuad mDestinationQuad{};
    SpriteGeometryMode mGeometryMode{
        SpriteGeometryMode::DestinationRect};
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

} // namespace pvz::engine
