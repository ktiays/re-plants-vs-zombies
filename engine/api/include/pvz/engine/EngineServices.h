#pragma once

#include "pvz/engine/Audio.h"
#include "pvz/engine/Font.h"
#include "pvz/engine/Music.h"
#include "pvz/engine/Render.h"
#include "pvz/engine/Resources.h"
#include "pvz/engine/Xml.h"

#include <cstdint>
#include <string_view>

namespace pvz::engine
{

enum class LogLevel : std::uint8_t
{
    Debug,
    Information,
    Warning,
    Error,
};

class ILogger
{
public:
    virtual ~ILogger() = default;

    virtual void Log(LogLevel theLevel, std::string_view theMessage) = 0;
};

class IEngineServices
{
public:
    virtual ~IEngineServices() = default;

    [[nodiscard]] virtual ILogger& GetLogger() = 0;
    [[nodiscard]] virtual IResourceStore& GetResources() = 0;
    [[nodiscard]] virtual IXmlDocumentLoader& GetXmlDocuments() = 0;
    [[nodiscard]] virtual IImageStore& GetImages() = 0;
    [[nodiscard]] virtual IImageResources& GetImageResources() = 0;
    [[nodiscard]] virtual IFontResources& GetFontResources() = 0;
    [[nodiscard]] virtual ISoundResources& GetSoundResources() = 0;
    [[nodiscard]] virtual IMusicResources& GetMusicResources() = 0;
};

} // namespace pvz::engine
