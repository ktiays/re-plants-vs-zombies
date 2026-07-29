#include "pvz/engine/core/ApplicationRunner.h"
#include "pvz/engine/core/PakResourceStore.h"
#include "pvz/engine/core/ResourceXmlDocumentLoader.h"
#include "pvz/game/GameModule.h"
#include "pvz/platform/macos/MetalRenderDevice.h"
#include "pvz/platform/macos/SdlPlatform.h"

#import <Foundation/Foundation.h>

#include <filesystem>
#include <iostream>
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
        pvz::engine::IXmlDocumentLoader& theDocuments)
        : mLogger(theLogger),
          mResources(theResources),
          mDocuments(theDocuments)
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

private:
    pvz::engine::ILogger& mLogger;
    pvz::engine::IResourceStore& mResources;
    pvz::engine::IXmlDocumentLoader& mDocuments;
};

} // namespace

int main(int theArgumentCount, char** theArguments)
{
    @autoreleasepool
    {
        pvz::engine::core::PakResourceStore aResources;
        if (theArgumentCount > 2)
        {
            std::cerr
                << "usage: PlantsVsZombies [path-to-main.pak]\n";
            return 2;
        }
        if (theArgumentCount == 2 &&
            !aResources.LoadFromFile(
                std::filesystem::path(theArguments[1])))
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

        pvz::platform::macos::MetalRenderDevice aRenderer;
        if (!aRenderer.Initialize(aPlatform.GetMetalLayer()))
        {
            std::cerr << aRenderer.GetLastError() << '\n';
            return 1;
        }

        MacLogger aLogger;
        pvz::engine::core::ResourceXmlDocumentLoader aDocuments(
            aResources);
        MacEngineServices aServices(
            aLogger,
            aResources,
            aDocuments);
        pvz::game::GameModule aGame;
        const pvz::engine::core::ApplicationRunner aRunner;
        const auto aResult = aRunner.Run(
            aGame,
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
        return 0;
    }
}
