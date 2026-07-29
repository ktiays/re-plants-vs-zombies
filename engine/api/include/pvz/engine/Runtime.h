#pragma once

#include "pvz/engine/Input.h"
#include "pvz/engine/Render.h"

#include <cstdint>

namespace pvz::engine
{

using MonotonicTimeMicroseconds = std::uint64_t;

class IMonotonicClock
{
public:
    virtual ~IMonotonicClock() = default;

    [[nodiscard]] virtual MonotonicTimeMicroseconds GetTime() const = 0;
};

class IPlatformEventLoop
{
public:
    virtual ~IPlatformEventLoop() = default;

    virtual void PumpEvents() = 0;
    [[nodiscard]] virtual bool IsQuitRequested() const = 0;
    [[nodiscard]] virtual bool IsSuspended() const = 0;
    virtual void WaitUntil(MonotonicTimeMicroseconds theDeadline) = 0;
};

class IInputSystem
{
public:
    virtual ~IInputSystem() = default;

    [[nodiscard]] virtual const IInputFrame& GetFrame() const = 0;
    virtual void ConsumeTransientEvents() = 0;
};

enum class RenderFrameResult : std::uint8_t
{
    Ready,
    Unavailable,
    Failure,
};

class IRenderDevice
{
public:
    virtual ~IRenderDevice() = default;

    [[nodiscard]] virtual RenderFrameResult BeginFrame(
        IRenderFrame*& theFrame) = 0;
    [[nodiscard]] virtual bool EndFrame() = 0;
};

} // namespace pvz::engine
