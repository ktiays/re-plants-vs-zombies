#pragma once

#include "pvz/engine/Music.h"

namespace pvz::engine::core
{

class NullMusicResources final : public IMusicResources
{
public:
    [[nodiscard]] bool Load(
        std::string_view thePath,
        MusicResource& theResource,
        MusicResourceDiagnostic& theDiagnostic) override;
    void Release(ModuleHandle theModule) override;
    [[nodiscard]] bool Play(
        ModuleHandle theModule,
        const MusicPlayback& thePlayback) override;
    void Stop(ModuleHandle theModule) override;
    void Pause(ModuleHandle theModule, bool thePaused) override;
    [[nodiscard]] bool IsPlaying(
        ModuleHandle theModule) const override;
    [[nodiscard]] bool SetPosition(
        ModuleHandle theModule,
        MusicPosition thePosition) override;
    [[nodiscard]] bool GetPosition(
        ModuleHandle theModule,
        MusicPosition& thePosition) const override;
    [[nodiscard]] bool SetChannelEnabled(
        ModuleHandle theModule,
        std::uint32_t theChannel,
        bool theEnabled) override;
    [[nodiscard]] bool SetVolume(
        ModuleHandle theModule,
        float theVolume) override;
    [[nodiscard]] bool SetTempoFactor(
        ModuleHandle theModule,
        float theFactor) override;
    void SetMasterVolume(float theVolume) override;
};

} // namespace pvz::engine::core
