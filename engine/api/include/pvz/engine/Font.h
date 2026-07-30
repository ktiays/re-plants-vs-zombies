#pragma once

#include "pvz/engine/Render.h"
#include "pvz/engine/Resources.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace pvz::engine
{

struct FontHandle
{
    std::uint32_t mIndex{};
    std::uint32_t mGeneration{};

    [[nodiscard]] constexpr bool IsValid() const
    {
        return mGeneration != 0;
    }
};

struct FontMetrics
{
    std::int32_t mAscent{};
    std::int32_t mAscentPadding{};
    std::int32_t mHeight{};
    std::int32_t mLineSpacingOffset{};
    std::uint32_t mDefaultPointSize{};
};

struct TextMetrics
{
    std::int32_t mAdvance{};
};

enum class FontResourceError : std::uint8_t
{
    None,
    ManifestNotLoaded,
    ResourceNotFound,
    SourceNotFound,
    SourceReadFailed,
    DescriptorMalformed,
    UnsupportedDescriptorCommand,
    AtlasLoadFailed,
    AtlasBoundsInvalid,
    HandleInvalid,
    NumericOverflow,
};

struct FontResourceDiagnostic
{
    FontResourceError mError{FontResourceError::None};
    std::uint32_t mLine{};
    std::string mPath;
    std::string mCommand;
    ImageResourceDiagnostic mImageDiagnostic;
};

struct FontResource
{
    FontHandle mFont{};
    FontMetrics mMetrics{};
    std::uint32_t mLayerCount{};
    std::uint32_t mGlyphCount{};
    std::uint32_t mKerningPairCount{};
};

class IFontResources
{
public:
    virtual ~IFontResources() = default;

    [[nodiscard]] virtual bool Load(
        std::string_view theResourceId,
        FontResource& theResource,
        FontResourceDiagnostic& theDiagnostic) = 0;
    virtual void Release(FontHandle theFont) = 0;
    [[nodiscard]] virtual bool MeasureText(
        FontHandle theFont,
        std::u32string_view theText,
        TextMetrics& theMetrics) const = 0;
    [[nodiscard]] virtual bool AppendTextSprites(
        FontHandle theFont,
        std::u32string_view theText,
        PointF theBaseline,
        ColorRgba8 theColor,
        std::vector<SpriteDraw>& theSprites) const = 0;
};

static_assert(sizeof(FontHandle) == 8);
static_assert(sizeof(FontMetrics) == 20);
static_assert(sizeof(TextMetrics) == 4);

} // namespace pvz::engine
