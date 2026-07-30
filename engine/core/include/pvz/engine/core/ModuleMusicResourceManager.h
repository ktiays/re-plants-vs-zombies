#pragma once

#include "pvz/engine/Music.h"
#include "pvz/engine/Resources.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace pvz::engine::core
{

class ModuleMusicResourceManager final : public IMusicResources
{
public:
    ModuleMusicResourceManager(
        const IResourceStore& theResourceStore,
        IModuleMusicDevice& theMusicDevice);
    ~ModuleMusicResourceManager() override;

    ModuleMusicResourceManager(
        const ModuleMusicResourceManager&) = delete;
    ModuleMusicResourceManager& operator=(
        const ModuleMusicResourceManager&) = delete;

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

private:
    struct LoadedResource
    {
        MusicResource mResource;
        std::uint32_t mReferenceCount{};
    };

    void ReleaseAll();

    const IResourceStore& mResourceStore;
    IModuleMusicDevice& mMusicDevice;
    std::unordered_map<std::string, LoadedResource> mLoadedResources;
    std::unordered_map<std::uint64_t, std::string> mPathsByHandle;
};

[[nodiscard]] const char* GetMusicResourceErrorMessage(
    MusicResourceError theError);

} // namespace pvz::engine::core
