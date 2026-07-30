#pragma once

#include "pvz/engine/Render.h"

namespace pvz::engine::core
{

class NullImageStore final : public IImageStore
{
public:
    [[nodiscard]] bool CreateImage(
        const ImageDescriptor& theDescriptor,
        std::span<const std::byte> theInitialPixels,
        std::uint32_t theSourceBytesPerRow,
        ImageHandle& theImage) override;
    [[nodiscard]] bool UpdateImage(
        ImageHandle theImage,
        const ImageUpdate& theUpdate) override;
    void DestroyImage(ImageHandle theImage) override;
    [[nodiscard]] bool GetImageSize(
        ImageHandle theImage,
        SizeI& theSize) const override;
};

} // namespace pvz::engine::core
