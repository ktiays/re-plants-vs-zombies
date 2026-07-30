#include "pvz/engine/core/NullSoundResources.h"

namespace pvz::engine::core
{

bool NullSoundResources::Load(
    std::string_view theResourceId,
    SoundResource& theResource,
    SoundResourceDiagnostic& theDiagnostic)
{
    static_cast<void>(theResourceId);
    theResource = {};
    theDiagnostic = {
        .mError = SoundResourceError::ManifestNotLoaded,
    };
    return false;
}

void NullSoundResources::Release(SoundHandle theSound)
{
    static_cast<void>(theSound);
}

bool NullSoundResources::Play(
    SoundHandle theSound,
    const SoundPlayback& thePlayback,
    VoiceHandle& theVoice)
{
    static_cast<void>(theSound);
    static_cast<void>(thePlayback);
    theVoice = {};
    return false;
}

void NullSoundResources::Stop(VoiceHandle theVoice)
{
    static_cast<void>(theVoice);
}

void NullSoundResources::StopAll()
{
}

bool NullSoundResources::IsPlaying(VoiceHandle theVoice) const
{
    static_cast<void>(theVoice);
    return false;
}

void NullSoundResources::SetMasterVolume(float theVolume)
{
    static_cast<void>(theVolume);
}

} // namespace pvz::engine::core
