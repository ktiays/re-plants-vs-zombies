#pragma once

#include "pvz/engine/Audio.h"
#include "pvz/engine/Resources.h"
#include "pvz/engine/Xml.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace pvz::engine::core
{

enum class SoundManifestError : std::uint8_t
{
    None,
    SourceDocument,
    InvalidRoot,
    InvalidSection,
    MissingAttribute,
    DuplicateResource,
    ResourcesAreLoaded,
};

class SoundResourceManager final : public ISoundResources
{
public:
    SoundResourceManager(
        const IResourceStore& theResourceStore,
        const IXmlDocumentLoader& theDocuments,
        const IAudioDecoder& theDecoder,
        IAudioDevice& theAudioDevice);
    ~SoundResourceManager() override;

    SoundResourceManager(const SoundResourceManager&) = delete;
    SoundResourceManager& operator=(const SoundResourceManager&) = delete;

    [[nodiscard]] bool LoadManifest(std::string_view thePath);
    [[nodiscard]] bool IsManifestLoaded() const;
    [[nodiscard]] std::size_t GetDefinitionCount() const;
    [[nodiscard]] std::vector<std::string> GetDefinitionIds() const;
    [[nodiscard]] SoundManifestError GetManifestError() const;
    [[nodiscard]] XmlDocumentDiagnostic
        GetManifestDocumentDiagnostic() const;
    [[nodiscard]] std::uint32_t GetManifestErrorLine() const;

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

private:
    struct Definition
    {
        std::string mPath;
    };

    struct LoadedResource
    {
        SoundResource mResource;
        std::uint32_t mReferenceCount{};
    };

    void ResetManifestError();
    [[nodiscard]] bool ParseManifest(
        const XmlNode& theRoot,
        std::unordered_map<std::string, Definition>& theDefinitions);
    void FailManifest(
        SoundManifestError theError,
        std::uint32_t theLine);
    void ReleaseAll();

    const IResourceStore& mResourceStore;
    const IXmlDocumentLoader& mDocuments;
    const IAudioDecoder& mDecoder;
    IAudioDevice& mAudioDevice;
    std::unordered_map<std::string, Definition> mDefinitions;
    std::unordered_map<std::string, LoadedResource> mLoadedResources;
    std::unordered_map<std::uint64_t, std::string> mIdsByHandle;
    SoundManifestError mManifestError{SoundManifestError::None};
    XmlDocumentDiagnostic mManifestDocumentDiagnostic;
    std::uint32_t mManifestErrorLine{};
    bool mManifestLoaded{};
};

[[nodiscard]] const char* GetSoundManifestErrorMessage(
    SoundManifestError theError);
[[nodiscard]] const char* GetSoundResourceErrorMessage(
    SoundResourceError theError);
[[nodiscard]] const char* GetSoundDecodeErrorMessage(
    SoundDecodeError theError);

} // namespace pvz::engine::core
