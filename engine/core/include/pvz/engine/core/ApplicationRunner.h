#pragma once

#include "pvz/engine/Game.h"
#include "pvz/engine/Runtime.h"

#include <cstdint>

namespace pvz::engine::core
{

struct FixedStepConfiguration
{
    std::uint32_t mMaximumUpdatesPerFrame{8};
    MonotonicTimeMicroseconds mMaximumFrameDelta{250'000};
};

class FixedStepScheduler
{
public:
    explicit FixedStepScheduler(
        FixedStepConfiguration theConfiguration = {});

    void Reset(MonotonicTimeMicroseconds theTime);
    [[nodiscard]] std::uint32_t Advance(
        MonotonicTimeMicroseconds theTime);

    [[nodiscard]] MonotonicTimeMicroseconds
        GetNextUpdateDeadline() const;
    [[nodiscard]] std::uint64_t GetDroppedUpdateCount() const;

private:
    FixedStepConfiguration mConfiguration;
    MonotonicTimeMicroseconds mLastTime{};
    MonotonicTimeMicroseconds mAccumulator{};
    MonotonicTimeMicroseconds mNextUpdateDeadline{};
    std::uint64_t mDroppedUpdateCount{};
    bool mInitialized{};
};

enum class ApplicationRunResult : std::uint8_t
{
    Success,
    GameInitializationFailed,
    RenderDeviceFailed,
};

class ApplicationRunner
{
public:
    explicit ApplicationRunner(
        FixedStepConfiguration theConfiguration = {});

    [[nodiscard]] ApplicationRunResult Run(
        IGame& theGame,
        IEngineServices& theServices,
        IPlatformEventLoop& theEvents,
        const IMonotonicClock& theClock,
        IInputSystem& theInput,
        IRenderDevice& theRenderer) const;

private:
    FixedStepConfiguration mConfiguration;
};

} // namespace pvz::engine::core
