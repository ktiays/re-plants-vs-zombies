#include "pvz/engine/core/ApplicationRunner.h"
#include "pvz/engine/core/NullFontResources.h"
#include "pvz/engine/core/NullImageStore.h"

#include "pvz/engine/core/BinaryStateIO.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

void Expect(bool theCondition, const char* theMessage);

namespace
{

class TestClock final : public pvz::engine::IMonotonicClock
{
public:
    [[nodiscard]] pvz::engine::MonotonicTimeMicroseconds
    GetTime() const override
    {
        return mTime;
    }

    void SetTime(pvz::engine::MonotonicTimeMicroseconds theTime)
    {
        mTime = theTime;
    }

private:
    pvz::engine::MonotonicTimeMicroseconds mTime{};
};

class TestEventLoop final : public pvz::engine::IPlatformEventLoop
{
public:
    explicit TestEventLoop(TestClock& theClock)
        : mClock(theClock)
    {
    }

    void PumpEvents() override
    {
        ++mPumpCount;
        if (mPumpCount >= 5)
            mQuitRequested = true;
    }

    [[nodiscard]] bool IsQuitRequested() const override
    {
        return mQuitRequested;
    }

    [[nodiscard]] bool IsSuspended() const override
    {
        return false;
    }

    void WaitUntil(
        pvz::engine::MonotonicTimeMicroseconds theDeadline) override
    {
        if (theDeadline > mClock.GetTime())
            mClock.SetTime(theDeadline);
    }

private:
    TestClock& mClock;
    std::uint32_t mPumpCount{};
    bool mQuitRequested{};
};

class TestInputFrame final : public pvz::engine::IInputFrame
{
public:
    [[nodiscard]] bool IsKeyDown(
        pvz::engine::KeyCode theKey) const override
    {
        static_cast<void>(theKey);
        return false;
    }

    [[nodiscard]] bool WasKeyPressed(
        pvz::engine::KeyCode theKey) const override
    {
        static_cast<void>(theKey);
        return false;
    }

    [[nodiscard]] bool IsPointerButtonDown(
        pvz::engine::PointerButton theButton) const override
    {
        static_cast<void>(theButton);
        return false;
    }

    [[nodiscard]] bool WasPointerButtonPressed(
        pvz::engine::PointerButton theButton) const override
    {
        static_cast<void>(theButton);
        return false;
    }

    [[nodiscard]] pvz::engine::PointerState GetPointerState()
        const override
    {
        return {};
    }

    [[nodiscard]] std::span<const char32_t> GetTextInput() const override
    {
        return {};
    }
};

class TestInputSystem final : public pvz::engine::IInputSystem
{
public:
    [[nodiscard]] const pvz::engine::IInputFrame&
    GetFrame() const override
    {
        return mFrame;
    }

    void ConsumeTransientEvents() override
    {
        ++mConsumeCount;
    }

    [[nodiscard]] std::uint32_t GetConsumeCount() const
    {
        return mConsumeCount;
    }

private:
    TestInputFrame mFrame;
    std::uint32_t mConsumeCount{};
};

class TestRenderFrame final : public pvz::engine::IRenderFrame
{
public:
    [[nodiscard]] pvz::engine::SizeI GetLogicalSize() const override
    {
        return {800, 600};
    }

    void Clear(pvz::engine::ColorRgba8 theColor) override
    {
        static_cast<void>(theColor);
        ++mClearCount;
    }

    void SubmitSprites(
        std::span<const pvz::engine::SpriteDraw> theDraws) override
    {
        static_cast<void>(theDraws);
    }

    [[nodiscard]] std::uint32_t GetClearCount() const
    {
        return mClearCount;
    }

private:
    std::uint32_t mClearCount{};
};

class TestRenderDevice final : public pvz::engine::IRenderDevice
{
public:
    [[nodiscard]] pvz::engine::RenderFrameResult BeginFrame(
        pvz::engine::IRenderFrame*& theFrame) override
    {
        ++mBeginCount;
        theFrame = &mFrame;
        return pvz::engine::RenderFrameResult::Ready;
    }

    [[nodiscard]] bool EndFrame() override
    {
        ++mEndCount;
        return true;
    }

    [[nodiscard]] std::uint32_t GetBeginCount() const
    {
        return mBeginCount;
    }

    [[nodiscard]] std::uint32_t GetEndCount() const
    {
        return mEndCount;
    }

    [[nodiscard]] const TestRenderFrame& GetFrame() const
    {
        return mFrame;
    }

private:
    TestRenderFrame mFrame;
    std::uint32_t mBeginCount{};
    std::uint32_t mEndCount{};
};

class TestLogger final : public pvz::engine::ILogger
{
public:
    void Log(
        pvz::engine::LogLevel theLevel,
        std::string_view theMessage) override
    {
        static_cast<void>(theLevel);
        static_cast<void>(theMessage);
        ++mMessageCount;
    }

    [[nodiscard]] std::uint32_t GetMessageCount() const
    {
        return mMessageCount;
    }

private:
    std::uint32_t mMessageCount{};
};

class EmptyResourceStore final : public pvz::engine::IResourceStore
{
public:
    [[nodiscard]] bool Contains(
        std::string_view thePath) const override
    {
        static_cast<void>(thePath);
        return false;
    }

    [[nodiscard]] bool GetSize(
        std::string_view thePath,
        std::uint64_t& theSize) const override
    {
        static_cast<void>(thePath);
        static_cast<void>(theSize);
        return false;
    }

    [[nodiscard]] bool ReadAll(
        std::string_view thePath,
        std::vector<std::byte>& theBytes) const override
    {
        static_cast<void>(thePath);
        static_cast<void>(theBytes);
        return false;
    }
};

class EmptyXmlDocumentLoader final
    : public pvz::engine::IXmlDocumentLoader
{
public:
    [[nodiscard]] bool Load(
        std::string_view thePath,
        pvz::engine::XmlDocumentMode theMode,
        std::vector<pvz::engine::XmlNode>& theRoots,
        pvz::engine::XmlDocumentDiagnostic& theDiagnostic)
        const override
    {
        static_cast<void>(thePath);
        static_cast<void>(theMode);
        static_cast<void>(theRoots);
        theDiagnostic.mError =
            pvz::engine::XmlDocumentError::ResourceReadFailed;
        return false;
    }
};

class TestServices final : public pvz::engine::IEngineServices
{
public:
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

    [[nodiscard]] const TestLogger& GetTestLogger() const
    {
        return mLogger;
    }

private:
    TestLogger mLogger;
    EmptyResourceStore mResources;
    EmptyXmlDocumentLoader mDocuments;
    pvz::engine::core::NullImageStore mImages;
    pvz::engine::core::NullImageResources mImageResources;
    pvz::engine::core::NullFontResources mFontResources;
};

class TestGame final : public pvz::engine::IGame
{
public:
    [[nodiscard]] pvz::engine::LifecycleResult Initialize(
        pvz::engine::IEngineServices& theServices) override
    {
        mServices = &theServices;
        mServices->GetLogger().Log(
            pvz::engine::LogLevel::Information,
            "initialized");
        return pvz::engine::LifecycleResult::Success;
    }

    void Update(
        const pvz::engine::GameTick& theTick,
        const pvz::engine::IInputFrame& theInput) override
    {
        static_cast<void>(theInput);
        mLastTick = theTick.mIndex;
        ++mUpdateCount;
    }

    void Render(pvz::engine::IRenderFrame& theFrame) const override
    {
        theFrame.Clear({1, 2, 3, 255});
    }

    [[nodiscard]] bool LoadState(
        pvz::engine::IStateReader& theReader) override
    {
        static_cast<void>(theReader);
        return true;
    }

    [[nodiscard]] bool SaveState(
        pvz::engine::IStateWriter& theWriter) const override
    {
        static_cast<void>(theWriter);
        return true;
    }

    void Suspend() override
    {
    }

    void Resume() override
    {
    }

    void Shutdown() override
    {
        mServices->GetLogger().Log(
            pvz::engine::LogLevel::Information,
            "shutdown");
        mServices = nullptr;
    }

    [[nodiscard]] std::uint64_t GetUpdateCount() const
    {
        return mUpdateCount;
    }

    [[nodiscard]] pvz::engine::TickIndex GetLastTick() const
    {
        return mLastTick;
    }

private:
    pvz::engine::IEngineServices* mServices{};
    pvz::engine::TickIndex mLastTick{};
    std::uint64_t mUpdateCount{};
};

void TestFixedStepScheduler()
{
    pvz::engine::core::FixedStepScheduler aScheduler;
    aScheduler.Reset(0);
    Expect(
        aScheduler.GetNextUpdateDeadline() == 10'000,
        "fixed-step initial deadline");
    Expect(aScheduler.Advance(9'999) == 0, "partial tick waits");
    Expect(aScheduler.Advance(10'000) == 1, "complete tick updates");
    Expect(aScheduler.Advance(35'000) == 2, "scheduler catches up");

    pvz::engine::core::FixedStepScheduler anOverloadedScheduler({
        .mMaximumUpdatesPerFrame = 3,
        .mMaximumFrameDelta = 1'000'000,
    });
    anOverloadedScheduler.Reset(0);
    Expect(
        anOverloadedScheduler.Advance(100'000) == 3,
        "scheduler caps catch-up work");
    Expect(
        anOverloadedScheduler.GetDroppedUpdateCount() == 7,
        "scheduler records dropped catch-up ticks");
    Expect(
        anOverloadedScheduler.GetNextUpdateDeadline() == 110'000,
        "overloaded scheduler discards stale backlog");
}

void TestApplicationRunner()
{
    TestClock aClock;
    TestEventLoop anEvents(aClock);
    TestInputSystem anInput;
    TestRenderDevice aRenderer;
    TestServices aServices;
    TestGame aGame;

    const pvz::engine::core::ApplicationRunner aRunner;
    Expect(
        aRunner.Run(
            aGame,
            aServices,
            anEvents,
            aClock,
            anInput,
            aRenderer) ==
            pvz::engine::core::ApplicationRunResult::Success,
        "application runner completes");
    Expect(aGame.GetUpdateCount() == 3, "runner advances fixed ticks");
    Expect(aGame.GetLastTick() == 2, "runner uses stable tick indices");
    Expect(
        anInput.GetConsumeCount() == 3,
        "runner consumes transient input once per tick");
    Expect(
        aRenderer.GetBeginCount() == 4 &&
            aRenderer.GetEndCount() == 4 &&
            aRenderer.GetFrame().GetClearCount() == 4,
        "runner renders and presents each frame");
    Expect(
        aServices.GetTestLogger().GetMessageCount() == 2,
        "runner owns game initialization and shutdown");
}

} // namespace

void RunApplicationRunnerTests()
{
    TestFixedStepScheduler();
    TestApplicationRunner();
}
