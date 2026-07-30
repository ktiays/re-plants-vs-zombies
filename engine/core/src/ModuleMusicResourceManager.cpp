#include "pvz/engine/core/ModuleMusicResourceManager.h"

#include <limits>
#include <vector>

namespace pvz::engine::core
{
namespace
{

[[nodiscard]] std::uint64_t GetHandleKey(ModuleHandle theModule)
{
    return
        (static_cast<std::uint64_t>(theModule.mGeneration) << 32U) |
        theModule.mIndex;
}

} // namespace

ModuleMusicResourceManager::ModuleMusicResourceManager(
    const IResourceStore& theResourceStore,
    IModuleMusicDevice& theMusicDevice)
    : mResourceStore(theResourceStore),
      mMusicDevice(theMusicDevice)
{
}

ModuleMusicResourceManager::~ModuleMusicResourceManager()
{
    ReleaseAll();
}

bool ModuleMusicResourceManager::Load(
    std::string_view thePath,
    MusicResource& theResource,
    MusicResourceDiagnostic& theDiagnostic)
{
    theResource = {};
    theDiagnostic = {
        .mPath = std::string(thePath),
    };

    const auto aLoaded = mLoadedResources.find(theDiagnostic.mPath);
    if (aLoaded != mLoadedResources.end())
    {
        if (aLoaded->second.mReferenceCount ==
            std::numeric_limits<std::uint32_t>::max())
        {
            theDiagnostic.mError =
                MusicResourceError::ReferenceCountOverflow;
            return false;
        }
        ++aLoaded->second.mReferenceCount;
        theResource = aLoaded->second.mResource;
        return true;
    }

    if (!mResourceStore.Contains(thePath))
    {
        theDiagnostic.mError = MusicResourceError::ResourceNotFound;
        return false;
    }

    std::vector<std::byte> aBytes;
    if (!mResourceStore.ReadAll(thePath, aBytes))
    {
        theDiagnostic.mError = MusicResourceError::ResourceReadFailed;
        return false;
    }

    ModuleHandle aModule;
    ModuleDescriptor aDescriptor;
    if (!mMusicDevice.CreateModule(
            aBytes,
            aModule,
            aDescriptor))
    {
        theDiagnostic.mError = MusicResourceError::DeviceCreateFailed;
        return false;
    }

    theResource = {
        .mModule = aModule,
        .mDescriptor = aDescriptor,
    };
    mLoadedResources.emplace(
        theDiagnostic.mPath,
        LoadedResource{
            .mResource = theResource,
            .mReferenceCount = 1,
        });
    mPathsByHandle.emplace(
        GetHandleKey(aModule),
        theDiagnostic.mPath);
    return true;
}

void ModuleMusicResourceManager::Release(ModuleHandle theModule)
{
    const auto aPath = mPathsByHandle.find(GetHandleKey(theModule));
    if (aPath == mPathsByHandle.end())
        return;

    const auto aLoaded = mLoadedResources.find(aPath->second);
    if (aLoaded == mLoadedResources.end())
    {
        mPathsByHandle.erase(aPath);
        return;
    }
    if (aLoaded->second.mReferenceCount > 1)
    {
        --aLoaded->second.mReferenceCount;
        return;
    }

    mMusicDevice.DestroyModule(theModule);
    mLoadedResources.erase(aLoaded);
    mPathsByHandle.erase(aPath);
}

bool ModuleMusicResourceManager::Play(
    ModuleHandle theModule,
    const MusicPlayback& thePlayback)
{
    return mMusicDevice.PlayModule(theModule, thePlayback);
}

void ModuleMusicResourceManager::Stop(ModuleHandle theModule)
{
    mMusicDevice.StopModule(theModule);
}

void ModuleMusicResourceManager::Pause(
    ModuleHandle theModule,
    bool thePaused)
{
    mMusicDevice.PauseModule(theModule, thePaused);
}

bool ModuleMusicResourceManager::IsPlaying(
    ModuleHandle theModule) const
{
    return mMusicDevice.IsModulePlaying(theModule);
}

bool ModuleMusicResourceManager::SetPosition(
    ModuleHandle theModule,
    MusicPosition thePosition)
{
    return mMusicDevice.SetModulePosition(theModule, thePosition);
}

bool ModuleMusicResourceManager::GetPosition(
    ModuleHandle theModule,
    MusicPosition& thePosition) const
{
    return mMusicDevice.GetModulePosition(theModule, thePosition);
}

bool ModuleMusicResourceManager::SetChannelEnabled(
    ModuleHandle theModule,
    std::uint32_t theChannel,
    bool theEnabled)
{
    return mMusicDevice.SetModuleChannelEnabled(
        theModule,
        theChannel,
        theEnabled);
}

bool ModuleMusicResourceManager::SetVolume(
    ModuleHandle theModule,
    float theVolume)
{
    return mMusicDevice.SetModuleVolume(theModule, theVolume);
}

bool ModuleMusicResourceManager::SetTempoFactor(
    ModuleHandle theModule,
    float theFactor)
{
    return mMusicDevice.SetModuleTempoFactor(theModule, theFactor);
}

void ModuleMusicResourceManager::SetMasterVolume(float theVolume)
{
    mMusicDevice.SetMusicMasterVolume(theVolume);
}

void ModuleMusicResourceManager::ReleaseAll()
{
    for (const auto& [aPath, aLoaded] : mLoadedResources)
    {
        static_cast<void>(aPath);
        mMusicDevice.DestroyModule(aLoaded.mResource.mModule);
    }
    mLoadedResources.clear();
    mPathsByHandle.clear();
}

const char* GetMusicResourceErrorMessage(
    MusicResourceError theError)
{
    switch (theError)
    {
    case MusicResourceError::None:
        return "no error";
    case MusicResourceError::ResourceNotFound:
        return "music resource was not found";
    case MusicResourceError::ResourceReadFailed:
        return "music resource could not be read";
    case MusicResourceError::DeviceCreateFailed:
        return "music backend could not load the module";
    case MusicResourceError::ReferenceCountOverflow:
        return "music resource reference count overflow";
    }
    return "unknown music resource error";
}

} // namespace pvz::engine::core
