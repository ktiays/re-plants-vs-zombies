#pragma once

#include "pvz/engine/Font.h"
#include "pvz/engine/Xml.h"
#include "pvz/engine/core/BitmapFontDescriptor.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace pvz::engine::core
{

enum class FontManifestError : std::uint8_t
{
    None,
    SourceDocument,
    InvalidRoot,
    InvalidSection,
    MissingAttribute,
    DuplicateResource,
    ResourcesAreLoaded,
};

class BitmapFontResourceManager final : public IFontResources
{
public:
    BitmapFontResourceManager(
        const IResourceStore& theResourceStore,
        const IXmlDocumentLoader& theDocuments,
        IImageResources& theImages);
    ~BitmapFontResourceManager() override;

    BitmapFontResourceManager(
        const BitmapFontResourceManager&) = delete;
    BitmapFontResourceManager& operator=(
        const BitmapFontResourceManager&) = delete;

    [[nodiscard]] bool LoadManifest(std::string_view thePath);
    [[nodiscard]] bool IsManifestLoaded() const;
    [[nodiscard]] std::size_t GetDefinitionCount() const;
    [[nodiscard]] std::vector<std::string> GetDefinitionIds() const;
    [[nodiscard]] FontManifestError GetManifestError() const;
    [[nodiscard]] XmlDocumentDiagnostic
        GetManifestDocumentDiagnostic() const;
    [[nodiscard]] std::uint32_t GetManifestErrorLine() const;

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

private:
    struct Definition
    {
        std::string mPath;
    };

    struct LoadedLayer
    {
        BitmapFontLayerDescriptor mDescriptor;
        ImageResource mAtlas;
    };

    struct LoadedResource
    {
        FontResource mResource;
        std::vector<LoadedLayer> mLayers;
        std::uint32_t mReferenceCount{};
    };

    [[nodiscard]] bool ParseManifest(
        const XmlNode& theRoot,
        std::unordered_map<std::string, Definition>& theDefinitions);
    [[nodiscard]] LoadedResource* FindLoaded(FontHandle theFont);
    [[nodiscard]] const LoadedResource* FindLoaded(
        FontHandle theFont) const;
    void FailManifest(
        FontManifestError theError,
        std::uint32_t theLine);
    void ReleaseLayers(std::vector<LoadedLayer>& theLayers);
    void ReleaseAll();

    const IResourceStore& mResourceStore;
    const IXmlDocumentLoader& mDocuments;
    IImageResources& mImages;
    std::unordered_map<std::string, Definition> mDefinitions;
    std::unordered_map<std::string, LoadedResource> mLoadedResources;
    std::unordered_map<std::uint64_t, std::string> mIdsByHandle;
    FontManifestError mManifestError{FontManifestError::None};
    XmlDocumentDiagnostic mManifestDocumentDiagnostic;
    std::uint32_t mManifestErrorLine{};
    std::uint32_t mNextHandleIndex{1};
    bool mManifestLoaded{};
};

[[nodiscard]] const char* GetFontManifestErrorMessage(
    FontManifestError theError);
[[nodiscard]] const char* GetFontResourceErrorMessage(
    FontResourceError theError);

} // namespace pvz::engine::core
