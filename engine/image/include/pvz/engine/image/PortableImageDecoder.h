#pragma once

#include "pvz/engine/Image.h"

namespace pvz::engine::image
{

class PortableImageDecoder final : public IImageDecoder
{
public:
    [[nodiscard]] bool Decode(
        std::span<const std::byte> theEncodedBytes,
        DecodedImage& theImage,
        ImageDecodeError& theError) const override;
};

} // namespace pvz::engine::image
