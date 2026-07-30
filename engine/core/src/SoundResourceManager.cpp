#include "pvz/engine/core/SoundResourceManager.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pvz::engine::core
{
namespace
{

inline constexpr std::array<std::string_view, 2> kSoundExtensions{
    ".ogg",
    ".wav",
};

enum class ResourceReadResult : std::uint8_t
{
    Found,
    NotFound,
    ReadFailed,
};

[[nodiscard]] std::string_view TrimTrailingSlashes(
    std::string_view thePath)
{
    while (!thePath.empty() &&
           (thePath.back() == '/' || thePath.back() == '\\'))
    {
        thePath.remove_suffix(1);
    }
    return thePath;
}

[[nodiscard]] std::string JoinPath(
    std::string_view theDirectory,
    std::string_view thePath)
{
    const auto aDirectory = TrimTrailingSlashes(theDirectory);
    if (aDirectory.empty())
        return std::string(thePath);

    std::string aResult(aDirectory);
    aResult.push_back('/');
    aResult.append(thePath);
    return aResult;
}

[[nodiscard]] std::string GetFileStem(std::string_view thePath)
{
    const auto aSlash = thePath.find_last_of("/\\");
    const auto aStart =
        aSlash == std::string_view::npos ? 0 : aSlash + 1;
    const auto aDot = thePath.find_last_of('.');
    const auto anEnd =
        aDot == std::string_view::npos || aDot < aStart
            ? thePath.size()
            : aDot;
    return std::string(thePath.substr(aStart, anEnd - aStart));
}

[[nodiscard]] bool HasExtension(std::string_view thePath)
{
    const auto aSlash = thePath.find_last_of("/\\");
    const auto aDot = thePath.find_last_of('.');
    return aDot != std::string_view::npos &&
           (aSlash == std::string_view::npos || aDot > aSlash);
}

[[nodiscard]] ResourceReadResult ReadSoundResource(
    const IResourceStore& theResources,
    std::string_view theLogicalPath,
    std::string& theResolvedPath,
    std::vector<std::byte>& theBytes)
{
    const auto aTryPath =
        [&](std::string_view theCandidate) -> ResourceReadResult
    {
        if (!theResources.Contains(theCandidate))
            return ResourceReadResult::NotFound;
        if (!theResources.ReadAll(theCandidate, theBytes))
            return ResourceReadResult::ReadFailed;
        theResolvedPath = theCandidate;
        return ResourceReadResult::Found;
    };

    if (HasExtension(theLogicalPath))
        return aTryPath(theLogicalPath);

    for (const auto anExtension : kSoundExtensions)
    {
        std::string aCandidate(theLogicalPath);
        aCandidate.append(anExtension);
        const auto aResult = aTryPath(aCandidate);
        if (aResult != ResourceReadResult::NotFound)
            return aResult;
    }
    return ResourceReadResult::NotFound;
}

[[nodiscard]] std::uint64_t MakeHandleKey(SoundHandle theSound)
{
    return (static_cast<std::uint64_t>(theSound.mGeneration) << 32) |
           theSound.mIndex;
}

[[nodiscard]] bool ValidateDecodedSound(const DecodedSound& theSound)
{
    const auto& aDescriptor = theSound.mDescriptor;
    if (aDescriptor.mSampleRate == 0 ||
        (aDescriptor.mChannelCount != 1 &&
         aDescriptor.mChannelCount != 2) ||
        aDescriptor.mFrameCount == 0 ||
        aDescriptor.mFrameCount >
            std::numeric_limits<std::uint64_t>::max() /
                aDescriptor.mChannelCount)
    {
        return false;
    }
    const auto aSampleCount =
        aDescriptor.mFrameCount * aDescriptor.mChannelCount;
    return aSampleCount == theSound.mInterleavedSamples.size();
}

} // namespace

SoundResourceManager::SoundResourceManager(
    const IResourceStore& theResourceStore,
    const IXmlDocumentLoader& theDocuments,
    const IAudioDecoder& theDecoder,
    IAudioDevice& theAudioDevice)
    : mResourceStore(theResourceStore),
      mDocuments(theDocuments),
      mDecoder(theDecoder),
      mAudioDevice(theAudioDevice)
{
}

SoundResourceManager::~SoundResourceManager()
{
    ReleaseAll();
}

bool SoundResourceManager::LoadManifest(std::string_view thePath)
{
    ResetManifestError();
    if (!mLoadedResources.empty())
    {
        FailManifest(SoundManifestError::ResourcesAreLoaded, 0);
        return false;
    }
    mDefinitions.clear();
    mManifestLoaded = false;

    std::vector<XmlNode> aRoots;
    if (!mDocuments.Load(
            thePath,
            XmlDocumentMode::SingleRoot,
            aRoots,
            mManifestDocumentDiagnostic))
    {
        FailManifest(
            SoundManifestError::SourceDocument,
            mManifestDocumentDiagnostic.mLine);
        return false;
    }
    if (aRoots.size() != 1 ||
        aRoots.front().mName != "ResourceManifest")
    {
        FailManifest(
            SoundManifestError::InvalidRoot,
            aRoots.empty() ? 0 : aRoots.front().mLine);
        return false;
    }

    std::unordered_map<std::string, Definition> aDefinitions;
    if (!ParseManifest(aRoots.front(), aDefinitions))
        return false;

    mDefinitions = std::move(aDefinitions);
    mManifestLoaded = true;
    return true;
}

bool SoundResourceManager::IsManifestLoaded() const
{
    return mManifestLoaded;
}

std::size_t SoundResourceManager::GetDefinitionCount() const
{
    return mDefinitions.size();
}

std::vector<std::string>
SoundResourceManager::GetDefinitionIds() const
{
    std::vector<std::string> anIds;
    anIds.reserve(mDefinitions.size());
    for (const auto& [anId, aDefinition] : mDefinitions)
    {
        static_cast<void>(aDefinition);
        anIds.push_back(anId);
    }
    std::ranges::sort(anIds);
    return anIds;
}

SoundManifestError SoundResourceManager::GetManifestError() const
{
    return mManifestError;
}

XmlDocumentDiagnostic
SoundResourceManager::GetManifestDocumentDiagnostic() const
{
    return mManifestDocumentDiagnostic;
}

std::uint32_t SoundResourceManager::GetManifestErrorLine() const
{
    return mManifestErrorLine;
}

bool SoundResourceManager::Load(
    std::string_view theResourceId,
    SoundResource& theResource,
    SoundResourceDiagnostic& theDiagnostic)
{
    theResource = {};
    theDiagnostic = {};
    if (!mManifestLoaded)
    {
        theDiagnostic.mError =
            SoundResourceError::ManifestNotLoaded;
        return false;
    }

    const auto aDefinition =
        mDefinitions.find(std::string(theResourceId));
    if (aDefinition == mDefinitions.end())
    {
        theDiagnostic.mError =
            SoundResourceError::ResourceNotFound;
        return false;
    }

    const auto aLoaded =
        mLoadedResources.find(std::string(theResourceId));
    if (aLoaded != mLoadedResources.end())
    {
        if (aLoaded->second.mReferenceCount ==
            std::numeric_limits<std::uint32_t>::max())
        {
            theDiagnostic.mError =
                SoundResourceError::ReferenceCountOverflow;
            return false;
        }
        ++aLoaded->second.mReferenceCount;
        theResource = aLoaded->second.mResource;
        return true;
    }

    std::string aPath;
    std::vector<std::byte> aBytes;
    const auto aReadResult = ReadSoundResource(
        mResourceStore,
        aDefinition->second.mPath,
        aPath,
        aBytes);
    if (aReadResult != ResourceReadResult::Found)
    {
        theDiagnostic = {
            .mError =
                aReadResult == ResourceReadResult::ReadFailed
                    ? SoundResourceError::SourceReadFailed
                    : SoundResourceError::SourceNotFound,
            .mPath = aDefinition->second.mPath,
        };
        return false;
    }

    DecodedSound aDecodedSound;
    SoundDecodeError aDecodeError{};
    if (!mDecoder.Decode(aBytes, aDecodedSound, aDecodeError) ||
        !ValidateDecodedSound(aDecodedSound))
    {
        theDiagnostic = {
            .mError = SoundResourceError::DecodeFailed,
            .mDecodeError =
                aDecodeError == SoundDecodeError::None
                    ? SoundDecodeError::InvalidData
                    : aDecodeError,
            .mPath = aPath,
        };
        return false;
    }

    SoundHandle aSound;
    if (!mAudioDevice.CreateSound(
            aDecodedSound.mDescriptor,
            aDecodedSound.mInterleavedSamples,
            aSound))
    {
        theDiagnostic = {
            .mError = SoundResourceError::DeviceUploadFailed,
            .mPath = aPath,
        };
        return false;
    }

    const SoundResource aResource{
        .mSound = aSound,
        .mDescriptor = aDecodedSound.mDescriptor,
    };
    const std::string anId(theResourceId);
    mLoadedResources.emplace(
        anId,
        LoadedResource{
            .mResource = aResource,
            .mReferenceCount = 1,
        });
    mIdsByHandle.emplace(MakeHandleKey(aSound), anId);
    theResource = aResource;
    return true;
}

void SoundResourceManager::Release(SoundHandle theSound)
{
    const auto anId = mIdsByHandle.find(MakeHandleKey(theSound));
    if (anId == mIdsByHandle.end())
        return;
    const auto aLoaded = mLoadedResources.find(anId->second);
    if (aLoaded == mLoadedResources.end())
        return;
    if (aLoaded->second.mReferenceCount > 1)
    {
        --aLoaded->second.mReferenceCount;
        return;
    }

    mAudioDevice.DestroySound(aLoaded->second.mResource.mSound);
    mLoadedResources.erase(aLoaded);
    mIdsByHandle.erase(anId);
}

bool SoundResourceManager::Play(
    SoundHandle theSound,
    const SoundPlayback& thePlayback,
    VoiceHandle& theVoice)
{
    return mAudioDevice.Play(theSound, thePlayback, theVoice);
}

void SoundResourceManager::Stop(VoiceHandle theVoice)
{
    mAudioDevice.Stop(theVoice);
}

void SoundResourceManager::StopAll()
{
    mAudioDevice.StopAll();
}

bool SoundResourceManager::IsPlaying(VoiceHandle theVoice) const
{
    return mAudioDevice.IsPlaying(theVoice);
}

void SoundResourceManager::SetMasterVolume(float theVolume)
{
    mAudioDevice.SetMasterVolume(theVolume);
}

void SoundResourceManager::ResetManifestError()
{
    mManifestError = SoundManifestError::None;
    mManifestDocumentDiagnostic = {};
    mManifestErrorLine = 0;
}

bool SoundResourceManager::ParseManifest(
    const XmlNode& theRoot,
    std::unordered_map<std::string, Definition>& theDefinitions)
{
    std::string aDefaultPath;
    std::string aDefaultIdPrefix;
    for (const auto& aGroup : theRoot.mChildren)
    {
        if (aGroup.mName != "Resources")
        {
            FailManifest(
                SoundManifestError::InvalidSection,
                aGroup.mLine);
            return false;
        }

        for (const auto& aNode : aGroup.mChildren)
        {
            if (aNode.mName == "SetDefaults")
            {
                if (const auto* aPath =
                        aNode.FindAttribute("path"))
                {
                    aDefaultPath = TrimTrailingSlashes(*aPath);
                }
                if (const auto* anIdPrefix =
                        aNode.FindAttribute("idprefix"))
                {
                    aDefaultIdPrefix =
                        TrimTrailingSlashes(*anIdPrefix);
                }
                continue;
            }
            if (aNode.mName == "Image" ||
                aNode.mName == "Font")
            {
                continue;
            }
            if (aNode.mName != "Sound")
            {
                FailManifest(
                    SoundManifestError::InvalidSection,
                    aNode.mLine);
                return false;
            }

            const auto* aPath = aNode.FindAttribute("path");
            if (aPath == nullptr || aPath->empty())
            {
                FailManifest(
                    SoundManifestError::MissingAttribute,
                    aNode.mLine);
                return false;
            }

            std::string anId = aDefaultIdPrefix;
            if (const auto* anExplicitId =
                    aNode.FindAttribute("id"))
            {
                anId.append(*anExplicitId);
            }
            else
            {
                anId.append(GetFileStem(*aPath));
            }
            if (anId.empty())
            {
                FailManifest(
                    SoundManifestError::MissingAttribute,
                    aNode.mLine);
                return false;
            }

            Definition aDefinition{
                .mPath = JoinPath(aDefaultPath, *aPath),
            };
            if (!theDefinitions.emplace(
                    std::move(anId),
                    std::move(aDefinition)).second)
            {
                FailManifest(
                    SoundManifestError::DuplicateResource,
                    aNode.mLine);
                return false;
            }
        }
    }
    return true;
}

void SoundResourceManager::FailManifest(
    SoundManifestError theError,
    std::uint32_t theLine)
{
    mManifestError = theError;
    mManifestErrorLine = theLine;
    mManifestLoaded = false;
}

void SoundResourceManager::ReleaseAll()
{
    for (const auto& [anId, aLoaded] : mLoadedResources)
    {
        static_cast<void>(anId);
        mAudioDevice.DestroySound(aLoaded.mResource.mSound);
    }
    mIdsByHandle.clear();
    mLoadedResources.clear();
}

const char* GetSoundManifestErrorMessage(SoundManifestError theError)
{
    switch (theError)
    {
    case SoundManifestError::None:
        return "no error";
    case SoundManifestError::SourceDocument:
        return "sound manifest could not be read";
    case SoundManifestError::InvalidRoot:
        return "sound manifest root is invalid";
    case SoundManifestError::InvalidSection:
        return "sound manifest contains an invalid section";
    case SoundManifestError::MissingAttribute:
        return "sound manifest is missing a required attribute";
    case SoundManifestError::DuplicateResource:
        return "sound manifest contains a duplicate resource";
    case SoundManifestError::ResourcesAreLoaded:
        return "sound manifest cannot change while sounds are loaded";
    }
    return "unknown sound manifest error";
}

const char* GetSoundResourceErrorMessage(SoundResourceError theError)
{
    switch (theError)
    {
    case SoundResourceError::None:
        return "no error";
    case SoundResourceError::ManifestNotLoaded:
        return "sound manifest is not loaded";
    case SoundResourceError::ResourceNotFound:
        return "sound resource identifier is not defined";
    case SoundResourceError::SourceNotFound:
        return "sound source is missing";
    case SoundResourceError::SourceReadFailed:
        return "sound source could not be read";
    case SoundResourceError::DecodeFailed:
        return "sound source could not be decoded";
    case SoundResourceError::DeviceUploadFailed:
        return "sound could not be uploaded";
    case SoundResourceError::ReferenceCountOverflow:
        return "sound reference count overflowed";
    }
    return "unknown sound resource error";
}

const char* GetSoundDecodeErrorMessage(SoundDecodeError theError)
{
    switch (theError)
    {
    case SoundDecodeError::None:
        return "no error";
    case SoundDecodeError::EmptyInput:
        return "audio input is empty";
    case SoundDecodeError::UnsupportedFormat:
        return "audio format is unsupported";
    case SoundDecodeError::InvalidData:
        return "audio data is invalid";
    case SoundDecodeError::ChannelsUnsupported:
        return "audio channel count is unsupported";
    case SoundDecodeError::SampleRateUnsupported:
        return "audio sample rate is unsupported";
    case SoundDecodeError::SizeOverflow:
        return "audio size overflows";
    case SoundDecodeError::AllocationFailed:
        return "audio allocation failed";
    }
    return "unknown audio decode error";
}

} // namespace pvz::engine::core
