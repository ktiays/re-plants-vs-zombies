#pragma once

#include "pvz/engine/Resources.h"

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
};

} // namespace pvz::engine
