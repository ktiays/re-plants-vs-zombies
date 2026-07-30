#pragma once

#include "pvz/engine/Font.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace pvz::engine::core
{

enum class BitmapFontDescriptorError : std::uint8_t
{
    None,
    Syntax,
    InvalidParameterCount,
    InvalidParameter,
    UndefinedValue,
    DuplicateDefinition,
    DuplicateLayer,
    UndefinedLayer,
    ListSizeMismatch,
    UnsupportedCommand,
    MissingFontData,
};

struct BitmapFontDescriptorDiagnostic
{
    BitmapFontDescriptorError mError{
        BitmapFontDescriptorError::None};
    std::uint32_t mLine{};
    std::string mCommand;
};

struct BitmapFontGlyph
{
    RectI mSource{};
    PointI mOffset{};
    std::int32_t mAdvance{};
    std::unordered_map<char32_t, std::int32_t> mKerning;
};

struct BitmapFontLayerDescriptor
{
    std::string mName;
    std::string mImagePath;
    std::unordered_map<char32_t, BitmapFontGlyph> mGlyphs;
    ColorRgba8 mColorMultiplier{255, 255, 255, 255};
    PointI mOffset{};
    std::int32_t mAscent{};
    std::int32_t mAscentPadding{};
    std::int32_t mLineSpacingOffset{};
    std::uint32_t mPointSize{};
};

struct BitmapFontDescriptor
{
    std::vector<BitmapFontLayerDescriptor> mLayers;
    std::uint32_t mDefaultPointSize{};
};

class BitmapFontDescriptorParser
{
public:
    [[nodiscard]] bool Parse(
        std::span<const std::byte> theBytes,
        BitmapFontDescriptor& theDescriptor,
        BitmapFontDescriptorDiagnostic& theDiagnostic) const;
};

[[nodiscard]] const char* GetBitmapFontDescriptorErrorMessage(
    BitmapFontDescriptorError theError);

} // namespace pvz::engine::core
