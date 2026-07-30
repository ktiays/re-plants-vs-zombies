#pragma once

#include "pvz/engine/Font.h"

namespace pvz::engine::core
{

class NullFontResources final : public IFontResources
{
public:
    [[nodiscard]] bool Load(
        std::string_view theResourceId,
        FontResource& theResource,
        FontResourceDiagnostic& theDiagnostic) override;
    void Release(FontHandle theFont) override;
    [[nodiscard]] bool MeasureText(
        FontHandle theFont,
        std::u32string_view theText,
        TextMetrics& theMetrics) const override;
    [[nodiscard]] bool AppendTextSprites(
        FontHandle theFont,
        std::u32string_view theText,
        PointF theBaseline,
        ColorRgba8 theColor,
        std::vector<SpriteDraw>& theSprites) const override;
};

} // namespace pvz::engine::core
