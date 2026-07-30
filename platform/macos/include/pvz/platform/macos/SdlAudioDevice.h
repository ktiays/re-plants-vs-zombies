#pragma once

#include "pvz/engine/Audio.h"
#include "pvz/engine/Music.h"

#include <memory>
#include <string_view>

namespace pvz::platform::macos
{

class SdlAudioDevice final
    : public engine::IAudioDevice,
      public engine::IModuleMusicDevice
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

    [[nodiscard]] bool CreateModule(
        std::span<const std::byte> theEncodedBytes,
        engine::ModuleHandle& theModule,
        engine::ModuleDescriptor& theDescriptor) override;
    void DestroyModule(engine::ModuleHandle theModule) override;
    [[nodiscard]] bool PlayModule(
        engine::ModuleHandle theModule,
        const engine::MusicPlayback& thePlayback) override;
    void StopModule(engine::ModuleHandle theModule) override;
    void PauseModule(
        engine::ModuleHandle theModule,
        bool thePaused) override;
    [[nodiscard]] bool IsModulePlaying(
        engine::ModuleHandle theModule) const override;
    [[nodiscard]] bool SetModulePosition(
        engine::ModuleHandle theModule,
        engine::MusicPosition thePosition) override;
    [[nodiscard]] bool GetModulePosition(
        engine::ModuleHandle theModule,
        engine::MusicPosition& thePosition) const override;
    [[nodiscard]] bool SetModuleChannelEnabled(
        engine::ModuleHandle theModule,
        std::uint32_t theChannel,
        bool theEnabled) override;
    [[nodiscard]] bool SetModuleVolume(
        engine::ModuleHandle theModule,
        float theVolume) override;
    [[nodiscard]] bool SetModuleTempoFactor(
        engine::ModuleHandle theModule,
        float theFactor) override;
    void SetMusicMasterVolume(float theVolume) override;

private:
    struct Implementation;
    std::unique_ptr<Implementation> mImplementation;
};

} // namespace pvz::platform::macos
