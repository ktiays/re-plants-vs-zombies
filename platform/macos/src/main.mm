#include "pvz/engine/core/ApplicationRunner.h"
#include "pvz/engine/core/BinaryStateIO.h"
#include "pvz/engine/core/BitmapFontResourceManager.h"
#include "pvz/engine/core/ImageResourceManager.h"
#include "pvz/engine/core/ModuleMusicResourceManager.h"
#include "pvz/engine/core/PakResourceStore.h"
#include "pvz/engine/core/ResourceXmlDocumentLoader.h"
#include "pvz/engine/core/ReplaySession.h"
#include "pvz/engine/core/SoundResourceManager.h"
#include "pvz/engine/audio/PortableAudioDecoder.h"
#include "pvz/engine/image/PortableImageDecoder.h"
#include "pvz/game/GameModule.h"
#include "pvz/platform/macos/MetalRenderDevice.h"
#include "pvz/platform/macos/RendererSmokeGame.h"
#include "pvz/platform/macos/SdlAudioDevice.h"
#include "pvz/platform/macos/SdlPlatform.h"

#import <Foundation/Foundation.h>

#include <filesystem>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace
{

class MacLogger final : public pvz::engine::ILogger
{
public:
    void Log(
        pvz::engine::LogLevel theLevel,
        std::string_view theMessage) override
    {
        const char* aPrefix = "info";
        switch (theLevel)
        {
        case pvz::engine::LogLevel::Debug:
            aPrefix = "debug";
            break;
        case pvz::engine::LogLevel::Information:
            break;
        case pvz::engine::LogLevel::Warning:
            aPrefix = "warning";
            break;
        case pvz::engine::LogLevel::Error:
            aPrefix = "error";
            break;
        }
        std::cerr << '[' << aPrefix << "] " << theMessage << '\n';
    }
};

class MacEngineServices final : public pvz::engine::IEngineServices
{
public:
    MacEngineServices(
        pvz::engine::ILogger& theLogger,
        pvz::engine::IResourceStore& theResources,
        pvz::engine::IXmlDocumentLoader& theDocuments,
        pvz::engine::IImageStore& theImages,
        pvz::engine::IImageResources& theImageResources,
        pvz::engine::IFontResources& theFontResources,
        pvz::engine::ISoundResources& theSoundResources,
        pvz::engine::IMusicResources& theMusicResources)
        : mLogger(theLogger),
          mResources(theResources),
          mDocuments(theDocuments),
          mImages(theImages),
          mImageResources(theImageResources),
          mFontResources(theFontResources),
          mSoundResources(theSoundResources),
          mMusicResources(theMusicResources)
    {
    }

    [[nodiscard]] pvz::engine::ILogger& GetLogger() override
    {
        return mLogger;
    }

    [[nodiscard]] pvz::engine::IResourceStore& GetResources() override
    {
        return mResources;
    }

    [[nodiscard]] pvz::engine::IXmlDocumentLoader&
    GetXmlDocuments() override
    {
        return mDocuments;
    }

    [[nodiscard]] pvz::engine::IImageStore& GetImages() override
    {
        return mImages;
    }

    [[nodiscard]] pvz::engine::IImageResources&
    GetImageResources() override
    {
        return mImageResources;
    }

    [[nodiscard]] pvz::engine::IFontResources&
    GetFontResources() override
    {
        return mFontResources;
    }

    [[nodiscard]] pvz::engine::ISoundResources&
    GetSoundResources() override
    {
        return mSoundResources;
    }

    [[nodiscard]] pvz::engine::IMusicResources&
    GetMusicResources() override
    {
        return mMusicResources;
    }

private:
    pvz::engine::ILogger& mLogger;
    pvz::engine::IResourceStore& mResources;
    pvz::engine::IXmlDocumentLoader& mDocuments;
    pvz::engine::IImageStore& mImages;
    pvz::engine::IImageResources& mImageResources;
    pvz::engine::IFontResources& mFontResources;
    pvz::engine::ISoundResources& mSoundResources;
    pvz::engine::IMusicResources& mMusicResources;
};

void PrintUsage()
{
    std::cerr
        << "usage: PlantsVsZombies "
           "[--renderer-smoke] "
           "[--record-session path-to-capture.pvzc] "
           "[path-to-main.pak]\n";
}

[[nodiscard]] bool WriteBytesAtomically(
    const std::filesystem::path& thePath,
    std::span<const std::byte> theBytes,
    std::string& theError)
{
    const auto aPath = thePath.string();
    NSString* aPathString =
        [NSString stringWithUTF8String:aPath.c_str()];
    if (aPathString == nil)
    {
        theError = "capture path is not valid UTF-8";
        return false;
    }
    NSData* aData = [NSData
        dataWithBytes:theBytes.data()
               length:static_cast<NSUInteger>(theBytes.size())];
    NSError* anError = nil;
    if (![aData writeToFile:aPathString
                    options:NSDataWritingAtomic
                      error:&anError])
    {
        if (anError != nil)
        {
            const char* aDescription =
                [[anError localizedDescription] UTF8String];
            if (aDescription != nullptr)
                theError = aDescription;
        }
        if (theError.empty())
            theError = "atomic capture write failed";
        return false;
    }
    return true;
}

} // namespace

int main(int theArgumentCount, char** theArguments)
{
    @autoreleasepool
    {
        bool aUseRendererSmokeGame = false;
        std::optional<std::filesystem::path> aPakPath;
        std::optional<std::filesystem::path>
            aSessionCapturePath;
        for (int anArgumentIndex = 1;
             anArgumentIndex < theArgumentCount;
             ++anArgumentIndex)
        {
            const std::string_view anArgument(
                theArguments[anArgumentIndex]);
            if (anArgument == "--renderer-smoke")
            {
                aUseRendererSmokeGame = true;
            }
            else if (anArgument == "--record-session")
            {
                if (aSessionCapturePath.has_value() ||
                    anArgumentIndex + 1 >= theArgumentCount)
                {
                    PrintUsage();
                    return 2;
                }
                ++anArgumentIndex;
                aSessionCapturePath =
                    std::filesystem::path(
                        theArguments[anArgumentIndex]);
            }
            else if (anArgument.starts_with("--"))
            {
                PrintUsage();
                return 2;
            }
            else if (!aPakPath.has_value())
            {
                aPakPath = std::filesystem::path(anArgument);
            }
            else
            {
                PrintUsage();
                return 2;
            }
        }
        if (aUseRendererSmokeGame &&
            aSessionCapturePath.has_value())
        {
            std::cerr
                << "--record-session captures gameplay and cannot "
                   "be combined with --renderer-smoke\n";
            return 2;
        }

        pvz::engine::core::PakResourceStore aResources;
        if (aPakPath.has_value() &&
            !aResources.LoadFromFile(
                *aPakPath))
        {
            std::cerr
                << pvz::engine::core::GetPakErrorMessage(
                       aResources.GetError())
                << '\n';
            return 1;
        }

        pvz::platform::macos::SdlPlatform aPlatform;
        if (!aPlatform.Initialize("Plants vs. Zombies", 1'066, 800))
        {
            std::cerr << aPlatform.GetLastError() << '\n';
            return 1;
        }

        pvz::platform::macos::SdlAudioDevice anAudioDevice;
        if (!anAudioDevice.Initialize())
        {
            std::cerr << anAudioDevice.GetLastError() << '\n';
            return 1;
        }

        pvz::platform::macos::MetalRenderDevice aRenderer;
        if (!aRenderer.Initialize(aPlatform.GetMetalLayer()))
        {
            std::cerr << aRenderer.GetLastError() << '\n';
            return 1;
        }

        MacLogger aLogger;
        pvz::engine::core::ResourceXmlDocumentLoader aDocuments(
            aResources);
        pvz::engine::image::PortableImageDecoder anImageDecoder;
        pvz::engine::audio::PortableAudioDecoder anAudioDecoder;
        pvz::engine::core::ImageResourceManager anImageResources(
            aResources,
            aDocuments,
            anImageDecoder,
            aRenderer);
        if (aPakPath.has_value() &&
            !anImageResources.LoadManifest(
                "properties/resources.xml"))
        {
            std::cerr
                << pvz::engine::core::GetImageManifestErrorMessage(
                       anImageResources.GetManifestError())
                << " at line "
                << anImageResources.GetManifestErrorLine()
                << '\n';
            return 1;
        }
        pvz::engine::core::BitmapFontResourceManager
            aFontResources(
                aResources,
                aDocuments,
                anImageResources);
        if (aPakPath.has_value() &&
            !aFontResources.LoadManifest(
                "properties/resources.xml"))
        {
            std::cerr
                << pvz::engine::core::GetFontManifestErrorMessage(
                       aFontResources.GetManifestError())
                << " at line "
                << aFontResources.GetManifestErrorLine()
                << '\n';
            return 1;
        }
        pvz::engine::core::SoundResourceManager
            aSoundResources(
                aResources,
                aDocuments,
                anAudioDecoder,
                anAudioDevice);
        if (aPakPath.has_value() &&
            !aSoundResources.LoadManifest(
                "properties/resources.xml"))
        {
            std::cerr
                << pvz::engine::core::GetSoundManifestErrorMessage(
                       aSoundResources.GetManifestError())
                << " at line "
                << aSoundResources.GetManifestErrorLine()
                << '\n';
            return 1;
        }
        pvz::engine::core::ModuleMusicResourceManager
            aMusicResources(
                aResources,
                anAudioDevice);
        MacEngineServices aServices(
            aLogger,
            aResources,
            aDocuments,
            aRenderer,
            anImageResources,
            aFontResources,
            aSoundResources,
            aMusicResources);
        pvz::game::GameModule aGame;
        pvz::platform::macos::RendererSmokeGame
            aRendererSmokeGame;
        pvz::engine::IGame* aSelectedGame =
            aUseRendererSmokeGame
                ? static_cast<pvz::engine::IGame*>(
                      &aRendererSmokeGame)
                : static_cast<pvz::engine::IGame*>(&aGame);
        std::optional<
            pvz::engine::core::ReplayRecordingGame>
            aRecordingGame;
        if (aSessionCapturePath.has_value())
        {
            aRecordingGame.emplace(*aSelectedGame);
            aSelectedGame = &*aRecordingGame;
        }
        const pvz::engine::core::ApplicationRunner aRunner;
        const auto aResult = aRunner.Run(
            *aSelectedGame,
            aServices,
            aPlatform,
            aPlatform,
            aPlatform,
            aRenderer);
        if (aResult !=
            pvz::engine::core::ApplicationRunResult::Success)
        {
            std::cerr << aRenderer.GetLastError() << '\n';
            return 1;
        }
        if (aRecordingGame.has_value())
        {
            const auto aRecordingError =
                aRecordingGame->GetRecordingError();
            if (aRecordingError !=
                pvz::engine::core::ReplayRecordingError::None)
            {
                std::cerr
                    << pvz::engine::core::
                           GetReplayRecordingErrorMessage(
                               aRecordingError);
                if (aRecordingError ==
                    pvz::engine::core::ReplayRecordingError::
                        InputCaptureFailed)
                {
                    std::cerr
                        << ": "
                        << pvz::engine::core::
                               GetInputReplayErrorMessage(
                                   aRecordingGame->
                                       GetInputReplayError());
                }
                std::cerr << '\n';
                return 1;
            }

            pvz::engine::core::BinaryStateWriter aCaptureWriter;
            pvz::engine::core::ReplaySessionError
                aSessionError{};
            const auto& aSession =
                aRecordingGame->GetSession();
            if (!aSession.Save(
                    aCaptureWriter,
                    aSessionError))
            {
                std::cerr
                    << pvz::engine::core::
                           GetReplaySessionErrorMessage(
                               aSessionError)
                    << '\n';
                return 1;
            }

            std::string aWriteError;
            if (!WriteBytesAtomically(
                    *aSessionCapturePath,
                    aCaptureWriter.GetBytes(),
                    aWriteError))
            {
                std::cerr
                    << "could not write replay capture: "
                    << aWriteError
                    << '\n';
                return 1;
            }
            std::cout
                << "replay-session="
                << aSessionCapturePath->string()
                << " frames="
                << aSession.GetStateHashes().size()
                << " final-state-fnv1a="
                << aSession.GetFinalStateHash()
                << " transcript-fnv1a="
                << aSession.GetTranscriptHash()
                << '\n';
        }
        return 0;
    }
}
