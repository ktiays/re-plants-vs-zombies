#include "pvz/engine/core/NullMusicResources.h"

namespace pvz::engine::core
{

bool NullMusicResources::Load(
    std::string_view thePath,
    MusicResource& theResource,
    MusicResourceDiagnostic& theDiagnostic)
{
    theResource = {};
    theDiagnostic = {
        .mError = MusicResourceError::ResourceNotFound,
        .mPath = std::string(thePath),
    };
    return false;
}

void NullMusicResources::Release(ModuleHandle theModule)
{
    static_cast<void>(theModule);
}

bool NullMusicResources::Play(
    ModuleHandle theModule,
    const MusicPlayback& thePlayback)
{
    static_cast<void>(theModule);
    static_cast<void>(thePlayback);
    return false;
}

void NullMusicResources::Stop(ModuleHandle theModule)
{
    static_cast<void>(theModule);
}

void NullMusicResources::Pause(
    ModuleHandle theModule,
    bool thePaused)
{
    static_cast<void>(theModule);
    static_cast<void>(thePaused);
}

bool NullMusicResources::IsPlaying(ModuleHandle theModule) const
{
    static_cast<void>(theModule);
    return false;
}

bool NullMusicResources::SetPosition(
    ModuleHandle theModule,
    MusicPosition thePosition)
{
    static_cast<void>(theModule);
    static_cast<void>(thePosition);
    return false;
}

bool NullMusicResources::GetPosition(
    ModuleHandle theModule,
    MusicPosition& thePosition) const
{
    static_cast<void>(theModule);
    thePosition = {};
    return false;
}

bool NullMusicResources::SetChannelEnabled(
    ModuleHandle theModule,
    std::uint32_t theChannel,
    bool theEnabled)
{
    static_cast<void>(theModule);
    static_cast<void>(theChannel);
    static_cast<void>(theEnabled);
    return false;
}

bool NullMusicResources::SetVolume(
    ModuleHandle theModule,
    float theVolume)
{
    static_cast<void>(theModule);
    static_cast<void>(theVolume);
    return false;
}

bool NullMusicResources::SetTempoFactor(
    ModuleHandle theModule,
    float theFactor)
{
    static_cast<void>(theModule);
    static_cast<void>(theFactor);
    return false;
}

void NullMusicResources::SetMasterVolume(float theVolume)
{
    static_cast<void>(theVolume);
}

} // namespace pvz::engine::core
