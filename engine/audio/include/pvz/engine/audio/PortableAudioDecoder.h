#pragma once

#include "pvz/engine/Audio.h"

namespace pvz::engine::audio
{

class PortableAudioDecoder final : public IAudioDecoder
{
public:
    [[nodiscard]] bool Decode(
        std::span<const std::byte> theEncodedBytes,
        DecodedSound& theSound,
        SoundDecodeError& theError) const override;
};

} // namespace pvz::engine::audio
