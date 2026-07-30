#include "pvz/engine/core/ImageResourceManager.h"
#include "pvz/engine/core/ResourceXmlDocumentLoader.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

void Expect(bool theCondition, const char* theMessage);

namespace
{

class MemoryResourceStore final : public pvz::engine::IResourceStore
{
public:
    void AddText(std::string thePath, std::string_view theText)
    {
        std::vector<std::byte> aBytes;
        aBytes.reserve(theText.size());
        for (const char aCharacter : theText)
        {
            aBytes.push_back(static_cast<std::byte>(
                static_cast<unsigned char>(aCharacter)));
        }
        mResources.emplace(std::move(thePath), std::move(aBytes));
    }

    void AddBytes(
        std::string thePath,
        std::vector<std::byte> theBytes)
    {
        mResources.emplace(std::move(thePath), std::move(theBytes));
    }

    [[nodiscard]] bool Contains(
        std::string_view thePath) const override
    {
        return mResources.contains(std::string(thePath));
    }

    [[nodiscard]] bool GetSize(
        std::string_view thePath,
        std::uint64_t& theSize) const override
    {
        const auto aResource =
            mResources.find(std::string(thePath));
        if (aResource == mResources.end())
            return false;
        theSize = aResource->second.size();
        return true;
    }

    [[nodiscard]] bool ReadAll(
        std::string_view thePath,
        std::vector<std::byte>& theBytes) const override
    {
        const auto aResource =
            mResources.find(std::string(thePath));
        if (aResource == mResources.end())
            return false;
        theBytes = aResource->second;
        return true;
    }

private:
    std::unordered_map<std::string, std::vector<std::byte>>
        mResources;
};

class TaggedImageDecoder final : public pvz::engine::IImageDecoder
{
public:
    [[nodiscard]] bool Decode(
        std::span<const std::byte> theEncodedBytes,
        pvz::engine::DecodedImage& theImage,
        pvz::engine::ImageDecodeError& theError) const override
    {
        if (theEncodedBytes.size() != 1)
        {
            theError = pvz::engine::ImageDecodeError::InvalidData;
            return false;
        }

        const auto aTag =
            std::to_integer<std::uint8_t>(theEncodedBytes.front());
        if (aTag == 1)
        {
            theImage = {
                .mDescriptor =
                    {
                        .mSize = {2, 1},
                        .mPixelFormat =
                            pvz::engine::ImagePixelFormat::
                                Bgra8Unorm,
                    },
                .mBytesPerRow = 8,
                .mPixels =
                    {
                        std::byte{10},
                        std::byte{20},
                        std::byte{30},
                        std::byte{255},
                        std::byte{40},
                        std::byte{50},
                        std::byte{60},
                        std::byte{255},
                    },
            };
        }
        else
        {
            theImage = {
                .mDescriptor =
                    {
                        .mSize = {2, 1},
                        .mPixelFormat =
                            pvz::engine::ImagePixelFormat::
                                Bgra8Unorm,
                    },
                .mBytesPerRow = 8,
                .mPixels =
                    {
                        std::byte{7},
                        std::byte{0},
                        std::byte{0},
                        std::byte{255},
                        std::byte{200},
                        std::byte{0},
                        std::byte{0},
                        std::byte{255},
                    },
            };
        }
        theError = pvz::engine::ImageDecodeError::None;
        return true;
    }
};

class CapturingImageStore final : public pvz::engine::IImageStore
{
public:
    [[nodiscard]] bool CreateImage(
        const pvz::engine::ImageDescriptor& theDescriptor,
        std::span<const std::byte> theInitialPixels,
        std::uint32_t theSourceBytesPerRow,
        pvz::engine::ImageHandle& theImage) override
    {
        mDescriptors.push_back(theDescriptor);
        mRows.push_back(theSourceBytesPerRow);
        mUploads.emplace_back(
            theInitialPixels.begin(),
            theInitialPixels.end());
        theImage = {
            .mIndex = static_cast<std::uint32_t>(
                mDescriptors.size() - 1),
            .mGeneration = 1,
        };
        return true;
    }

    [[nodiscard]] bool UpdateImage(
        pvz::engine::ImageHandle theImage,
        const pvz::engine::ImageUpdate& theUpdate) override
    {
        static_cast<void>(theImage);
        static_cast<void>(theUpdate);
        return false;
    }

    void DestroyImage(
        pvz::engine::ImageHandle theImage) override
    {
        static_cast<void>(theImage);
        ++mDestroyCount;
    }

    [[nodiscard]] bool GetImageSize(
        pvz::engine::ImageHandle theImage,
        pvz::engine::SizeI& theSize) const override
    {
        if (!theImage.IsValid() ||
            theImage.mIndex >= mDescriptors.size())
        {
            theSize = {};
            return false;
        }
        theSize = mDescriptors[theImage.mIndex].mSize;
        return true;
    }

    std::vector<pvz::engine::ImageDescriptor> mDescriptors;
    std::vector<std::uint32_t> mRows;
    std::vector<std::vector<std::byte>> mUploads;
    std::uint32_t mDestroyCount{};
};

void TestManifestMappingAndAlphaComposition()
{
    MemoryResourceStore aResources;
    aResources.AddText(
        "properties/resources.xml",
        "<ResourceManifest>"
        "<Resources id=\"test\">"
        "<SetDefaults path=\"images\" idprefix=\"IMAGE_\"/>"
        "<Image id=\"TEST\" path=\"base\" cols=\"2\"/>"
        "<Image id=\"ALPHA_ONLY\" path=\"mask\" "
        "alphacolor=\"112233\"/>"
        "</Resources>"
        "</ResourceManifest>");
    aResources.AddBytes("images/base.jpg", {std::byte{1}});
    aResources.AddBytes("images/_base.png", {std::byte{2}});
    aResources.AddBytes("images/_mask.png", {std::byte{2}});

    pvz::engine::core::ResourceXmlDocumentLoader aDocuments(
        aResources);
    TaggedImageDecoder aDecoder;
    CapturingImageStore anImages;
    pvz::engine::core::ImageResourceManager aManager(
        aResources,
        aDocuments,
        aDecoder,
        anImages);

    Expect(
        aManager.LoadManifest("properties/resources.xml"),
        "image manifest loads");
    Expect(aManager.IsManifestLoaded(), "image manifest state");
    Expect(
        aManager.GetDefinitionCount() == 2,
        "image manifest definition count");

    pvz::engine::ImageResource aResource;
    pvz::engine::ImageResourceDiagnostic aDiagnostic;
    Expect(
        aManager.Load("IMAGE_TEST", aResource, aDiagnostic),
        "mapped image resource loads");
    Expect(aResource.mSize.mWidth == 2, "mapped image width");
    Expect(aResource.mColumns == 2, "mapped image columns");
    Expect(anImages.mUploads.size() == 1, "mapped image uploads once");
    if (!anImages.mUploads.empty())
    {
        const auto& anUpload = anImages.mUploads.front();
        Expect(
            anUpload[3] == std::byte{7} &&
                anUpload[7] == std::byte{200},
            "automatic companion alpha is composed");
    }

    const auto aFirstHandle = aResource.mImage;
    Expect(
        aManager.Load("IMAGE_TEST", aResource, aDiagnostic),
        "mapped image resource is cached");
    Expect(
        aResource.mImage.mIndex == aFirstHandle.mIndex &&
            aResource.mImage.mGeneration ==
                aFirstHandle.mGeneration,
        "cached image handle is stable");
    Expect(anImages.mUploads.size() == 1, "cache avoids duplicate upload");
    aManager.Release(aResource.mImage);
    Expect(anImages.mDestroyCount == 0, "first release retains cached image");
    aManager.Release(aResource.mImage);
    Expect(anImages.mDestroyCount == 1, "last release destroys cached image");

    Expect(
        aManager.Load(
            "IMAGE_ALPHA_ONLY",
            aResource,
            aDiagnostic),
        "alpha-only image resource loads");
    Expect(anImages.mUploads.size() == 2, "alpha-only image uploads");
    if (anImages.mUploads.size() == 2)
    {
        const auto& anUpload = anImages.mUploads[1];
        Expect(
            anUpload[0] == std::byte{0x33} &&
                anUpload[1] == std::byte{0x22} &&
                anUpload[2] == std::byte{0x11} &&
                anUpload[3] == std::byte{7},
            "alpha-only image uses manifest compose color");
    }
    aManager.Release(aResource.mImage);
}

void TestManifestValidation()
{
    MemoryResourceStore aResources;
    aResources.AddText(
        "bad.xml",
        "<ResourceManifest><Resources>"
        "<Image id=\"A\" path=\"one\"/>"
        "<Image id=\"A\" path=\"two\"/>"
        "</Resources></ResourceManifest>");
    pvz::engine::core::ResourceXmlDocumentLoader aDocuments(
        aResources);
    TaggedImageDecoder aDecoder;
    CapturingImageStore anImages;
    pvz::engine::core::ImageResourceManager aManager(
        aResources,
        aDocuments,
        aDecoder,
        anImages);

    Expect(!aManager.LoadManifest("bad.xml"), "duplicate image id fails");
    Expect(
        aManager.GetManifestError() ==
            pvz::engine::core::ImageManifestError::
                DuplicateResource,
        "duplicate image id error");
}

} // namespace

void RunImageResourceManagerTests()
{
    TestManifestMappingAndAlphaComposition();
    TestManifestValidation();
}
