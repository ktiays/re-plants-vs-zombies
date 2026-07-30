#include "pvz/engine/core/NullImageStore.h"

namespace pvz::engine::core
{

bool NullImageStore::CreateImage(
    const ImageDescriptor& theDescriptor,
    std::span<const std::byte> theInitialPixels,
    std::uint32_t theSourceBytesPerRow,
    ImageHandle& theImage)
{
    static_cast<void>(theDescriptor);
    static_cast<void>(theInitialPixels);
    static_cast<void>(theSourceBytesPerRow);
    theImage = {};
    return false;
}

bool NullImageStore::UpdateImage(
    ImageHandle theImage,
    const ImageUpdate& theUpdate)
{
    static_cast<void>(theImage);
    static_cast<void>(theUpdate);
    return false;
}

void NullImageStore::DestroyImage(ImageHandle theImage)
{
    static_cast<void>(theImage);
}

bool NullImageStore::GetImageSize(
    ImageHandle theImage,
    SizeI& theSize) const
{
    static_cast<void>(theImage);
    theSize = {};
    return false;
}

bool NullImageResources::Load(
    std::string_view theResourceId,
    ImageResource& theResource,
    ImageResourceDiagnostic& theDiagnostic)
{
    static_cast<void>(theResourceId);
    theResource = {};
    theDiagnostic = {
        .mError = ImageResourceError::ManifestNotLoaded,
    };
    return false;
}

void NullImageResources::Release(ImageHandle theImage)
{
    static_cast<void>(theImage);
}

} // namespace pvz::engine::core
