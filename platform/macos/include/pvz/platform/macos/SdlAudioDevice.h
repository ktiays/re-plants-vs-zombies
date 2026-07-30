#pragma once

#include "pvz/engine/Audio.h"

#include <memory>
#include <string_view>

namespace pvz::platform::macos
{

class SdlAudioDevice final : public engine::IAudioDevice
{
public:
    SdlAudioDevice();
    ~SdlAudioDevice() override;

    SdlAudioDevice(const SdlAudioDevice&) = delete;
    SdlAudioDevice& operator=(const SdlAudioDevice&) = delete;

    [[nodiscard]] bool Initialize();
    [[nodiscard]] std::string_view GetLastError() const;

    [[nodiscard]] bool CreateSound(
        const engine::SoundDescriptor& theDescriptor,
        std::span<const std::int16_t> theInterleavedSamples,
        engine::SoundHandle& theSound) override;
    void DestroySound(engine::SoundHandle theSound) override;
    [[nodiscard]] bool Play(
        engine::SoundHandle theSound,
        const engine::SoundPlayback& thePlayback,
        engine::VoiceHandle& theVoice) override;
    void Stop(engine::VoiceHandle theVoice) override;
    void StopAll() override;
    [[nodiscard]] bool IsPlaying(
        engine::VoiceHandle theVoice) const override;
    void SetMasterVolume(float theVolume) override;

private:
    struct Implementation;
    std::unique_ptr<Implementation> mImplementation;
};

} // namespace pvz::platform::macos
