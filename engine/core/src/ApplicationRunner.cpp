#include "pvz/engine/core/ApplicationRunner.h"

#include <algorithm>
#include <limits>

namespace pvz::engine::core
{
namespace
{

[[nodiscard]] MonotonicTimeMicroseconds AddSaturated(
    MonotonicTimeMicroseconds theLeft,
    MonotonicTimeMicroseconds theRight)
{
    const auto aMaximum =
        std::numeric_limits<MonotonicTimeMicroseconds>::max();
    if (theRight > aMaximum - theLeft)
        return aMaximum;
    return theLeft + theRight;
}

[[nodiscard]] FixedStepConfiguration NormalizeConfiguration(
    FixedStepConfiguration theConfiguration)
{
    if (theConfiguration.mMaximumUpdatesPerFrame == 0)
        theConfiguration.mMaximumUpdatesPerFrame = 1;
    if (theConfiguration.mMaximumFrameDelta <
        kSimulationTickMicroseconds)
    {
        theConfiguration.mMaximumFrameDelta =
            kSimulationTickMicroseconds;
    }
    return theConfiguration;
}

} // namespace

FixedStepScheduler::FixedStepScheduler(
    FixedStepConfiguration theConfiguration)
    : mConfiguration(NormalizeConfiguration(theConfiguration))
{
}

void FixedStepScheduler::Reset(MonotonicTimeMicroseconds theTime)
{
    mLastTime = theTime;
    mAccumulator = 0;
    mNextUpdateDeadline = AddSaturated(
        theTime,
        kSimulationTickMicroseconds);
    mInitialized = true;
}

std::uint32_t FixedStepScheduler::Advance(
    MonotonicTimeMicroseconds theTime)
{
    if (!mInitialized)
    {
        Reset(theTime);
        return 0;
    }

    const auto aRawDelta =
        theTime >= mLastTime ? theTime - mLastTime : 0;
    mLastTime = theTime;
    const auto aDelta = std::min(
        aRawDelta,
        mConfiguration.mMaximumFrameDelta);
    if (aRawDelta > aDelta)
    {
        mDroppedUpdateCount +=
            (aRawDelta - aDelta) /
            kSimulationTickMicroseconds;
    }

    mAccumulator = AddSaturated(mAccumulator, aDelta);
    const auto anAvailableUpdateCount =
        mAccumulator / kSimulationTickMicroseconds;
    const auto anUpdateCount = std::min(
        anAvailableUpdateCount,
        static_cast<MonotonicTimeMicroseconds>(
            mConfiguration.mMaximumUpdatesPerFrame));

    if (anAvailableUpdateCount >
        mConfiguration.mMaximumUpdatesPerFrame)
    {
        mDroppedUpdateCount +=
            anAvailableUpdateCount -
            mConfiguration.mMaximumUpdatesPerFrame;
        mAccumulator %= kSimulationTickMicroseconds;
    }
    else
    {
        mAccumulator -=
            anUpdateCount * kSimulationTickMicroseconds;
    }

    mNextUpdateDeadline = AddSaturated(
        theTime,
        kSimulationTickMicroseconds - mAccumulator);
    return static_cast<std::uint32_t>(anUpdateCount);
}

MonotonicTimeMicroseconds
FixedStepScheduler::GetNextUpdateDeadline() const
{
    return mNextUpdateDeadline;
}

std::uint64_t FixedStepScheduler::GetDroppedUpdateCount() const
{
    return mDroppedUpdateCount;
}

ApplicationRunner::ApplicationRunner(
    FixedStepConfiguration theConfiguration)
    : mConfiguration(NormalizeConfiguration(theConfiguration))
{
}

ApplicationRunResult ApplicationRunner::Run(
    IGame& theGame,
    IEngineServices& theServices,
    IPlatformEventLoop& theEvents,
    const IMonotonicClock& theClock,
    IInputSystem& theInput,
    IRenderDevice& theRenderer) const
{
    if (theGame.Initialize(theServices) != LifecycleResult::Success)
        return ApplicationRunResult::GameInitializationFailed;

    FixedStepScheduler aScheduler(mConfiguration);
    auto aCurrentTime = theClock.GetTime();
    aScheduler.Reset(aCurrentTime);
    TickIndex aTickIndex{};
    bool isSuspended = theEvents.IsSuspended();
    if (isSuspended)
        theGame.Suspend();

    while (!theEvents.IsQuitRequested())
    {
        theEvents.PumpEvents();
        if (theEvents.IsQuitRequested())
            break;

        aCurrentTime = theClock.GetTime();
        const bool shouldSuspend = theEvents.IsSuspended();
        if (shouldSuspend)
        {
            if (!isSuspended)
            {
                theGame.Suspend();
                isSuspended = true;
            }
            aScheduler.Reset(aCurrentTime);
            theEvents.WaitUntil(AddSaturated(
                aCurrentTime,
                kSimulationTickMicroseconds));
            continue;
        }

        if (isSuspended)
        {
            theGame.Resume();
            aScheduler.Reset(aCurrentTime);
            isSuspended = false;
        }

        const auto anUpdateCount = aScheduler.Advance(aCurrentTime);
        for (std::uint32_t anIndex = 0;
             anIndex < anUpdateCount;
             ++anIndex)
        {
            theGame.Update(GameTick{aTickIndex}, theInput.GetFrame());
            theInput.ConsumeTransientEvents();
            ++aTickIndex;
        }

        IRenderFrame* aFrame{};
        const auto aFrameResult = theRenderer.BeginFrame(aFrame);
        if (aFrameResult == RenderFrameResult::Failure ||
            (aFrameResult == RenderFrameResult::Ready &&
             aFrame == nullptr))
        {
            theGame.Shutdown();
            return ApplicationRunResult::RenderDeviceFailed;
        }
        if (aFrameResult == RenderFrameResult::Ready)
        {
            theGame.Render(*aFrame);
            if (!theRenderer.EndFrame())
            {
                theGame.Shutdown();
                return ApplicationRunResult::RenderDeviceFailed;
            }
        }

        theEvents.WaitUntil(aScheduler.GetNextUpdateDeadline());
    }

    theGame.Shutdown();
    return ApplicationRunResult::Success;
}

} // namespace pvz::engine::core
