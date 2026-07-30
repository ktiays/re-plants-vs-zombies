#include "pvz/engine/core/NullFontResources.h"

namespace pvz::engine::core
{

bool NullFontResources::Load(
    std::string_view theResourceId,
    FontResource& theResource,
    FontResourceDiagnostic& theDiagnostic)
{
    static_cast<void>(theResourceId);
    theResource = {};
    theDiagnostic = {
        .mError = FontResourceError::ManifestNotLoaded,
    };
    return false;
}

void NullFontResources::Release(FontHandle theFont)
{
    static_cast<void>(theFont);
}

bool NullFontResources::MeasureText(
    FontHandle theFont,
    std::u32string_view theText,
    TextMetrics& theMetrics) const
{
    static_cast<void>(theFont);
    static_cast<void>(theText);
    theMetrics = {};
    return false;
}

bool NullFontResources::AppendTextSprites(
    FontHandle theFont,
    std::u32string_view theText,
    PointF theBaseline,
    ColorRgba8 theColor,
    std::vector<SpriteDraw>& theSprites) const
{
    static_cast<void>(theFont);
    static_cast<void>(theText);
    static_cast<void>(theBaseline);
    static_cast<void>(theColor);
    static_cast<void>(theSprites);
    return false;
}

} // namespace pvz::engine::core
