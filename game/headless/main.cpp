#include "pvz/engine/core/BinaryStateIO.h"
#include "pvz/engine/core/DeterministicHash.h"
#include "pvz/engine/core/InputReplay.h"
#include "pvz/engine/core/NullFontResources.h"
#include "pvz/engine/core/NullImageStore.h"
#include "pvz/engine/core/NullMusicResources.h"
#include "pvz/engine/core/NullSoundResources.h"
#include "pvz/game/GameModule.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

class HeadlessLogger final : public pvz::engine::ILogger
{
public:
    void Log(
        pvz::engine::LogLevel theLevel,
        std::string_view theMessage) override
    {
        static_cast<void>(theLevel);
        std::cout << theMessage << '\n';
    }
};

class HeadlessServices final : public pvz::engine::IEngineServices
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
        return mXmlDocuments;
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
            theDiagnostic.mLine = 0;
            return false;
        }
    };

    HeadlessLogger mLogger;
    EmptyResourceStore mResources;
    EmptyXmlDocumentLoader mXmlDocuments;
    pvz::engine::core::NullImageStore mImages;
    pvz::engine::core::NullImageResources mImageResources;
    pvz::engine::core::NullFontResources mFontResources;
    pvz::engine::core::NullSoundResources mSoundResources;
    pvz::engine::core::NullMusicResources mMusicResources;
};

[[nodiscard]] bool AppendReplayFrame(
    pvz::engine::core::InputReplay& theReplay,
    pvz::engine::core::RecordedInputFrame theFrame)
{
    pvz::engine::core::InputReplayError anError{};
    return theReplay.AppendFrame(
        std::move(theFrame),
        anError);
}

[[nodiscard]] bool BuildAdventureReplay(
    pvz::engine::core::InputReplay& theReplay)
{
    constexpr pvz::engine::TickIndex kFrameCount = 65;
    for (pvz::engine::TickIndex aTick = 0;
         aTick < kFrameCount;
         ++aTick)
    {
        pvz::engine::core::RecordedInputFrame aFrame;
        aFrame.mTick = aTick;
        if (aTick == 0 || aTick == 1)
        {
            aFrame.SetKeyDown(
                pvz::engine::KeyCode::Enter,
                true);
            aFrame.SetKeyPressed(
                pvz::engine::KeyCode::Enter,
                true);
        }
        else if (aTick == 62)
        {
            aFrame.SetPointerButtonDown(
                pvz::engine::PointerButton::Primary,
                true);
            aFrame.SetPointerButtonPressed(
                pvz::engine::PointerButton::Primary,
                true);
            aFrame.mPointer.mPosition = {320, 530};
        }
        else if (aTick == 63)
        {
            aFrame.SetKeyDown(
                pvz::engine::KeyCode::ArrowLeft,
                true);
            aFrame.SetKeyPressed(
                pvz::engine::KeyCode::ArrowLeft,
                true);
        }
        else if (aTick == 64)
        {
            aFrame.SetKeyDown(
                pvz::engine::KeyCode::Space,
                true);
            aFrame.SetKeyPressed(
                pvz::engine::KeyCode::Space,
                true);
        }
        if (!AppendReplayFrame(theReplay, std::move(aFrame)))
            return false;
    }
    return true;
}

class HeadlessRenderFrame final : public pvz::engine::IRenderFrame
{
public:
    [[nodiscard]] pvz::engine::SizeI GetLogicalSize() const override
    {
        return {800, 600};
    }

    void Clear(pvz::engine::ColorRgba8 theColor) override
    {
        mClearColor = theColor;
    }

    void SubmitSprites(
        std::span<const pvz::engine::SpriteDraw> theDraws) override
    {
        mSpriteCount += static_cast<std::uint64_t>(theDraws.size());
    }

private:
    pvz::engine::ColorRgba8 mClearColor{};
    std::uint64_t mSpriteCount{};
};

} // namespace

int main()
{
    HeadlessServices aServices;
    HeadlessRenderFrame aFrame;
    pvz::game::GameModule aGame;
    pvz::engine::core::InputReplay aReplay;

    if (!BuildAdventureReplay(aReplay))
        return 1;

    pvz::engine::core::BinaryStateWriter aReplayWriter;
    pvz::engine::core::InputReplayError aReplayError{};
    if (!aReplay.Save(aReplayWriter, aReplayError))
        return 1;
    pvz::engine::core::BinaryStateReader aReplayReader(
        aReplayWriter.GetBytes());
    pvz::engine::core::InputReplay aLoadedReplay;
    if (!aLoadedReplay.Load(aReplayReader, aReplayError))
        return 1;

    if (aGame.Initialize(aServices) !=
        pvz::engine::LifecycleResult::Success)
    {
        return 1;
    }

    auto aTranscriptHash =
        pvz::engine::core::kFnv1a64Offset;
    for (const auto& aRecordedFrame :
         aLoadedReplay.GetFrames())
    {
        const pvz::engine::core::ReplayInputFrame anInput(
            aRecordedFrame);
        aGame.Update(
            pvz::engine::GameTick{aRecordedFrame.mTick},
            anInput);

        pvz::engine::core::BinaryStateWriter aTickWriter;
        if (!aGame.SaveState(aTickWriter))
            return 1;
        aTranscriptHash =
            pvz::engine::core::CalculateFnv1a64(
                aTickWriter.GetBytes(),
                aTranscriptHash);
    }

    aGame.Render(aFrame);

    pvz::engine::core::BinaryStateWriter aWriter;
    if (!aGame.SaveState(aWriter))
        return 1;

    const auto aFinalHash =
        pvz::engine::core::CalculateFnv1a64(
            aWriter.GetBytes());
    const auto aFlowState = aGame.GetFlowState();
    const bool hasExpectedState =
        aGame.GetScene() ==
            pvz::game::GameScene::AdventureDay &&
        aGame.GetLastTick() == 64 &&
        aGame.GetUpdateCount() == 65 &&
        aFlowState.mGridColumn == 2 &&
        aFlowState.mGridRow == 4 &&
        aFlowState.mOccupiedCells ==
            ((std::uint64_t{1} << 38U) |
             (std::uint64_t{1} << 39U)) &&
        aFinalHash == 14'239'196'991'121'916'159ULL &&
        aTranscriptHash == 5'816'442'757'865'445'562ULL;

    std::cout << "replay-frames="
              << aLoadedReplay.GetFrames().size()
              << " replay-bytes="
              << aReplayWriter.GetBytesWritten()
              << " state-bytes=" << aWriter.GetBytesWritten()
              << " state-fnv1a=" << aFinalHash
              << " transcript-fnv1a=" << aTranscriptHash
              << '\n';
    aGame.Shutdown();
    return hasExpectedState ? 0 : 1;
}
