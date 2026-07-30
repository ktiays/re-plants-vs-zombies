#pragma once

#include "pvz/engine/Audio.h"

namespace pvz::engine::core
{

class NullSoundResources final : public ISoundResources
{
public:
    [[nodiscard]] bool Load(
        std::string_view theResourceId,
        SoundResource& theResource,
        SoundResourceDiagnostic& theDiagnostic) override;
    void Release(SoundHandle theSound) override;
    [[nodiscard]] bool Play(
        SoundHandle theSound,
        const SoundPlayback& thePlayback,
        VoiceHandle& theVoice) override;
    void Stop(VoiceHandle theVoice) override;
    void StopAll() override;
    [[nodiscard]] bool IsPlaying(
        VoiceHandle theVoice) const override;
    void SetMasterVolume(float theVolume) override;
};

} // namespace pvz::engine::core
