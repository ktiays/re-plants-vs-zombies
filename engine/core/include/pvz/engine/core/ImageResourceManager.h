#pragma once

#include "pvz/engine/Resources.h"
#include "pvz/engine/Xml.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace pvz::engine::core
{

enum class ImageManifestError : std::uint8_t
{
    None,
    SourceDocument,
    InvalidRoot,
    InvalidSection,
    MissingAttribute,
    InvalidInteger,
    InvalidColor,
    ConflictingAlphaSources,
    DuplicateResource,
    ResourcesAreLoaded,
};

class ImageResourceManager final : public IImageResources
{
public:
    ImageResourceManager(
        const IResourceStore& theResourceStore,
        const IXmlDocumentLoader& theDocuments,
        const IImageDecoder& theDecoder,
        IImageStore& theImages);
    ~ImageResourceManager() override;

    ImageResourceManager(const ImageResourceManager&) = delete;
    ImageResourceManager& operator=(const ImageResourceManager&) = delete;

    [[nodiscard]] bool LoadManifest(std::string_view thePath);
    [[nodiscard]] bool IsManifestLoaded() const;
    [[nodiscard]] std::size_t GetDefinitionCount() const;
    [[nodiscard]] ImageManifestError GetManifestError() const;
    [[nodiscard]] XmlDocumentDiagnostic
        GetManifestDocumentDiagnostic() const;
    [[nodiscard]] std::uint32_t GetManifestErrorLine() const;

    [[nodiscard]] bool Load(
        std::string_view theResourceId,
        ImageResource& theResource,
        ImageResourceDiagnostic& theDiagnostic) override;
    void Release(ImageHandle theImage) override;

private:
    struct Definition
    {
        std::string mPath;
        std::string mAlphaPath;
        ColorRgba8 mAlphaColor{255, 255, 255, 255};
        std::uint32_t mRows{1};
        std::uint32_t mColumns{1};
        bool mAutoFindAlpha{true};
        bool mAlphaGrid{};
    };

    struct LoadedResource
    {
        ImageResource mResource;
        std::uint32_t mReferenceCount{};
    };

    void ResetManifestError();
    [[nodiscard]] bool ParseManifest(
        const XmlNode& theRoot,
        std::unordered_map<std::string, Definition>& theDefinitions);
    void FailManifest(
        ImageManifestError theError,
        std::uint32_t theLine);
    void ReleaseAll();

    const IResourceStore& mResourceStore;
    const IXmlDocumentLoader& mDocuments;
    const IImageDecoder& mDecoder;
    IImageStore& mImages;
    std::unordered_map<std::string, Definition> mDefinitions;
    std::unordered_map<std::string, LoadedResource> mLoadedResources;
    std::unordered_map<std::uint64_t, std::string> mIdsByHandle;
    ImageManifestError mManifestError{ImageManifestError::None};
    XmlDocumentDiagnostic mManifestDocumentDiagnostic;
    std::uint32_t mManifestErrorLine{};
    bool mManifestLoaded{};
};

[[nodiscard]] const char* GetImageManifestErrorMessage(
    ImageManifestError theError);
[[nodiscard]] const char* GetImageResourceErrorMessage(
    ImageResourceError theError);
[[nodiscard]] const char* GetImageDecodeErrorMessage(
    ImageDecodeError theError);

} // namespace pvz::engine::core
