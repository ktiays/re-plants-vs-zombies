#include "fixtures/LegacyBoardGeometryFixture.h"

#include "pvz/engine/EngineServices.h"
#include "pvz/engine/core/NullFontResources.h"
#include "pvz/engine/core/NullMusicResources.h"
#include "pvz/engine/core/NullSoundResources.h"
#include "pvz/game/BoardGeometry.h"
#include "pvz/game/GameFlow.h"
#include "pvz/game/GameModule.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace
{

namespace legacy = pvz::game::test::legacy_reference;

std::uint32_t gFailureCount{};

void Expect(bool theCondition, std::string_view theMessage)
{
    if (theCondition)
        return;
    std::cerr << "FAIL: " << theMessage << '\n';
    ++gFailureCount;
}

[[nodiscard]] pvz::game::BoardStageLayout ToPortableStage(
    legacy::Stage theStage)
{
    switch (theStage)
    {
    case legacy::Stage::Day:
        return pvz::game::BoardStageLayout::Day;
    case legacy::Stage::Pool:
        return pvz::game::BoardStageLayout::Pool;
    case legacy::Stage::Roof:
        return pvz::game::BoardStageLayout::Roof;
    }
    return pvz::game::BoardStageLayout::Count;
}

[[nodiscard]] std::int32_t GetExpectedPlantY(
    const legacy::StageFixture& theFixture,
    std::uint8_t theColumn,
    std::uint8_t theRow)
{
    if (theFixture.mStage != legacy::Stage::Roof)
        return theFixture.mFlatPlantOriginsY[theRow];
    return
        theFixture.mRoofPlantBaseY[theColumn] +
        static_cast<std::int32_t>(theRow) * 85;
}

[[nodiscard]] std::int32_t GetExpectedHitY(
    const legacy::StageFixture& theFixture,
    std::uint8_t theColumn,
    std::uint8_t theRow)
{
    if (theFixture.mStage != legacy::Stage::Roof)
        return theFixture.mFlatPlantOriginsY[theRow];
    return
        theFixture.mRoofHitBaseY[theColumn] +
        static_cast<std::int32_t>(theRow) * 85;
}

class TestInputFrame final : public pvz::engine::IInputFrame
{
public:
    void Clear()
    {
        mPressedKeys.fill(false);
        mPressedButtons.fill(false);
    }

    void PressKey(pvz::engine::KeyCode theKey)
    {
        mPressedKeys[static_cast<std::size_t>(theKey)] = true;
    }

    void PressPointer(pvz::engine::PointI thePosition)
    {
        mPointer.mPosition = thePosition;
        mPressedButtons[static_cast<std::size_t>(
            pvz::engine::PointerButton::Primary)] = true;
    }

    [[nodiscard]] bool IsKeyDown(
        pvz::engine::KeyCode theKey) const override
    {
        static_cast<void>(theKey);
        return false;
    }

    [[nodiscard]] bool WasKeyPressed(
        pvz::engine::KeyCode theKey) const override
    {
        return mPressedKeys[static_cast<std::size_t>(theKey)];
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
        return mPressedButtons[static_cast<std::size_t>(theButton)];
    }

    [[nodiscard]] pvz::engine::PointerState GetPointerState()
        const override
    {
        return mPointer;
    }

    [[nodiscard]] std::span<const char32_t> GetTextInput()
        const override
    {
        return {};
    }

private:
    std::array<
        bool,
        static_cast<std::size_t>(pvz::engine::KeyCode::Count)>
        mPressedKeys{};
    std::array<
        bool,
        static_cast<std::size_t>(
            pvz::engine::PointerButton::Count)>
        mPressedButtons{};
    pvz::engine::PointerState mPointer{};
};

void EnterAdventureDay(
    pvz::game::GameFlow& theFlow,
    TestInputFrame& theInput)
{
    theInput.PressKey(pvz::engine::KeyCode::Enter);
    theFlow.Update(theInput);
    theInput.Clear();
    theInput.PressKey(pvz::engine::KeyCode::Enter);
    theFlow.Update(theInput);
    theInput.Clear();
    for (std::uint16_t aTick = 0; aTick < 60; ++aTick)
        theFlow.Update(theInput);
}

void TestBoardGeometryMatchesLegacyReference()
{
    for (const auto& aFixture : legacy::kStages)
    {
        const auto aStage = ToPortableStage(aFixture.mStage);
        const auto aDimensions =
            pvz::game::BoardGeometry::GetDimensions(aStage);
        Expect(
            aDimensions.mColumnCount ==
                    legacy::kColumnOriginsX.size() &&
                aDimensions.mRowCount == aFixture.mRowCount,
            "stage dimensions match the legacy reference");

        for (std::uint8_t aColumn = 0;
             aColumn < aDimensions.mColumnCount;
             ++aColumn)
        {
            for (std::uint8_t aRow = 0;
                 aRow < aDimensions.mRowCount;
                 ++aRow)
            {
                const auto anExpectedX =
                    legacy::kColumnOriginsX[aColumn];
                const auto anExpectedPlantY =
                    GetExpectedPlantY(
                        aFixture,
                        aColumn,
                        aRow);
                const auto anExpectedHitY =
                    GetExpectedHitY(
                        aFixture,
                        aColumn,
                        aRow);
                const auto anOrigin =
                    pvz::game::BoardGeometry::GridToPixel(
                        aStage,
                        aColumn,
                        aRow);
                Expect(
                    anOrigin.mX == anExpectedX &&
                        anOrigin.mY == anExpectedPlantY,
                    "plant origin matches the independent legacy fixture");

                const auto aRect =
                    pvz::game::BoardGeometry::GetCellRect(
                        aStage,
                        aColumn,
                        aRow);
                Expect(
                    aRect.mOrigin.mX == anExpectedX &&
                        aRect.mOrigin.mY == anExpectedHitY &&
                        aRect.mSize.mWidth ==
                            legacy::kCellWidth &&
                        aRect.mSize.mHeight ==
                            aFixture.mCellHeight,
                    "cell rectangle matches the independent legacy fixture");

                pvz::game::GridCoordinate aCoordinate;
                const pvz::engine::PointI aCenter{
                    aRect.mOrigin.mX +
                        static_cast<std::int32_t>(
                            aRect.mSize.mWidth / 2),
                    aRect.mOrigin.mY +
                        static_cast<std::int32_t>(
                            aRect.mSize.mHeight / 2),
                };
                Expect(
                    pvz::game::BoardGeometry::TryPixelToGrid(
                        aStage,
                        aCenter,
                        aCoordinate) &&
                        aCoordinate.mColumn == aColumn &&
                        aCoordinate.mRow == aRow,
                    "cell center maps back to the legacy grid coordinate");
            }
        }
    }

    pvz::game::GridCoordinate aCoordinate{8, 4};
    Expect(
        !pvz::game::BoardGeometry::TryPixelToGrid(
            pvz::game::BoardStageLayout::Day,
            {39, 80},
            aCoordinate),
        "point before the lawn is rejected");
    Expect(
        !pvz::game::BoardGeometry::TryPixelToGrid(
            pvz::game::BoardStageLayout::Day,
            {760, 579},
            aCoordinate),
        "point after the final column is rejected");
    Expect(
        !pvz::game::BoardGeometry::TryPixelToGrid(
            pvz::game::BoardStageLayout::Day,
            {759, 580},
            aCoordinate),
        "point after the final day row is rejected");
    Expect(
        pvz::game::BoardGeometry::GetCellRect(
            pvz::game::BoardStageLayout::Day,
            9,
            0).mSize.mWidth == 0,
        "out-of-range grid coordinate has no rectangle");
}

void TestAdventureInputMatchesLegacyReference()
{
    pvz::game::GameFlow aFlow;
    TestInputFrame anInput;
    EnterAdventureDay(aFlow, anInput);
    const auto& aDay = legacy::kStages[0];

    for (std::uint8_t aRow = 0;
         aRow < aDay.mRowCount;
         ++aRow)
    {
        for (std::uint8_t aColumn = 0;
             aColumn < legacy::kColumnOriginsX.size();
             ++aColumn)
        {
            anInput.PressPointer(
                {
                    legacy::kColumnOriginsX[aColumn] + 40,
                    aDay.mFlatPlantOriginsY[aRow] + 50,
                });
            aFlow.Update(anInput);
            anInput.Clear();
            Expect(
                aFlow.GetGridColumn() == aColumn &&
                    aFlow.GetGridRow() == aRow &&
                    aFlow.IsGridCellOccupied(aColumn, aRow),
                "Adventure input maps a legacy fixture point correctly");
        }
    }
}

class SilentLogger final : public pvz::engine::ILogger
{
public:
    void Log(
        pvz::engine::LogLevel theLevel,
        std::string_view theMessage) override
    {
        static_cast<void>(theLevel);
        static_cast<void>(theMessage);
    }
};

class EmptyResourceStore final
    : public pvz::engine::IResourceStore
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

class ParityImageStore final : public pvz::engine::IImageStore
{
public:
    [[nodiscard]] bool CreateImage(
        const pvz::engine::ImageDescriptor& theDescriptor,
        std::span<const std::byte> theInitialPixels,
        std::uint32_t theSourceBytesPerRow,
        pvz::engine::ImageHandle& theImage) override
    {
        static_cast<void>(theInitialPixels);
        static_cast<void>(theSourceBytesPerRow);
        if (theDescriptor.mSize.mWidth == 0 ||
            theDescriptor.mSize.mHeight == 0)
        {
            return false;
        }
        theImage = {100, 1};
        return true;
    }

    [[nodiscard]] bool UpdateImage(
        pvz::engine::ImageHandle theImage,
        const pvz::engine::ImageUpdate& theUpdate) override
    {
        static_cast<void>(theImage);
        static_cast<void>(theUpdate);
        return true;
    }

    void DestroyImage(pvz::engine::ImageHandle theImage) override
    {
        static_cast<void>(theImage);
    }

    [[nodiscard]] bool GetImageSize(
        pvz::engine::ImageHandle theImage,
        pvz::engine::SizeI& theSize) const override
    {
        if (theImage.mIndex != 100 ||
            theImage.mGeneration != 1)
        {
            return false;
        }
        theSize = {1, 1};
        return true;
    }
};

class ParityImageResources final
    : public pvz::engine::IImageResources
{
public:
    [[nodiscard]] bool Load(
        std::string_view theResourceId,
        pvz::engine::ImageResource& theResource,
        pvz::engine::ImageResourceDiagnostic& theDiagnostic)
        override
    {
        theDiagnostic = {};
        if (theResourceId == "IMAGE_BACKGROUND1")
        {
            theResource = {
                .mImage = {10, 1},
                .mSize = {1020, 600},
            };
            return true;
        }
        if (theResourceId == "IMAGE_SEEDBANK")
        {
            theResource = {
                .mImage = {11, 1},
                .mSize = {446, 87},
            };
            return true;
        }
        return false;
    }

    [[nodiscard]] bool LoadSource(
        std::string_view theLogicalPath,
        pvz::engine::ImageResource& theResource,
        pvz::engine::ImageResourceDiagnostic& theDiagnostic)
        override
    {
        static_cast<void>(theLogicalPath);
        static_cast<void>(theResource);
        static_cast<void>(theDiagnostic);
        return false;
    }

    void Release(pvz::engine::ImageHandle theImage) override
    {
        static_cast<void>(theImage);
    }
};

class ParityServices final : public pvz::engine::IEngineServices
{
public:
    [[nodiscard]] pvz::engine::ILogger& GetLogger() override
    {
        return mLogger;
    }

    [[nodiscard]] pvz::engine::IResourceStore&
    GetResources() override
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
    SilentLogger mLogger;
    EmptyResourceStore mResources;
    EmptyXmlDocumentLoader mXmlDocuments;
    ParityImageStore mImages;
    ParityImageResources mImageResources;
    pvz::engine::core::NullFontResources mFontResources;
    pvz::engine::core::NullSoundResources mSoundResources;
    pvz::engine::core::NullMusicResources mMusicResources;
};

class CaptureRenderFrame final : public pvz::engine::IRenderFrame
{
public:
    [[nodiscard]] pvz::engine::SizeI GetLogicalSize()
        const override
    {
        return {
            legacy::kBoardWidth,
            legacy::kBoardHeight,
        };
    }

    void Clear(pvz::engine::ColorRgba8 theColor) override
    {
        mClearColor = theColor;
    }

    void SubmitSprites(
        std::span<const pvz::engine::SpriteDraw> theDraws)
        override
    {
        mDraws.insert(
            mDraws.end(),
            theDraws.begin(),
            theDraws.end());
    }

    pvz::engine::ColorRgba8 mClearColor{};
    std::vector<pvz::engine::SpriteDraw> mDraws;
};

[[nodiscard]] bool IsDestination(
    const pvz::engine::SpriteDraw& theDraw,
    float theX,
    float theY,
    float theWidth,
    float theHeight)
{
    return
        theDraw.mDestination.mOrigin.mX == theX &&
        theDraw.mDestination.mOrigin.mY == theY &&
        theDraw.mDestination.mSize.mWidth == theWidth &&
        theDraw.mDestination.mSize.mHeight == theHeight;
}

void TestAdventureRenderCommandsMatchLegacyReference()
{
    ParityServices aServices;
    pvz::game::GameModule aGame;
    TestInputFrame anInput;
    Expect(
        aGame.Initialize(aServices) ==
            pvz::engine::LifecycleResult::Success,
        "parity-render game initializes");

    anInput.PressKey(pvz::engine::KeyCode::Enter);
    aGame.Update({0}, anInput);
    anInput.Clear();
    anInput.PressKey(pvz::engine::KeyCode::Enter);
    aGame.Update({1}, anInput);
    anInput.Clear();
    for (pvz::engine::TickIndex aTick = 2;
         aTick < 62;
         ++aTick)
    {
        aGame.Update({aTick}, anInput);
    }

    constexpr std::uint8_t kColumn = 3;
    constexpr std::uint8_t kRow = 4;
    const auto& aDay = legacy::kStages[0];
    const auto anExpectedX =
        legacy::kColumnOriginsX[kColumn];
    const auto anExpectedY =
        aDay.mFlatPlantOriginsY[kRow];
    anInput.PressPointer(
        {
            anExpectedX + 40,
            anExpectedY + 50,
        });
    aGame.Update({62}, anInput);
    anInput.Clear();

    CaptureRenderFrame aFrame;
    aGame.Render(aFrame);

    bool hasExpectedBackground{};
    std::uint32_t aSelectionDrawCount{};
    bool hasTop{};
    bool hasBottom{};
    bool hasLeft{};
    bool hasRight{};
    for (const auto& aDraw : aFrame.mDraws)
    {
        if (aDraw.mImage.mIndex == 10)
        {
            hasExpectedBackground =
                aDraw.mSource.mOrigin.mX ==
                    legacy::kBoardBackgroundSourceX &&
                aDraw.mSource.mOrigin.mY == 0 &&
                aDraw.mSource.mSize.mWidth ==
                    legacy::kBoardWidth &&
                aDraw.mSource.mSize.mHeight ==
                    legacy::kBoardHeight &&
                IsDestination(
                    aDraw,
                    0.0F,
                    0.0F,
                    800.0F,
                    600.0F);
        }
        const bool isSelection =
            aDraw.mColor.mRed == 255 &&
            aDraw.mColor.mGreen == 225 &&
            aDraw.mColor.mBlue == 45 &&
            aDraw.mColor.mAlpha == 230;
        if (!isSelection)
            continue;
        ++aSelectionDrawCount;
        hasTop =
            hasTop ||
            IsDestination(
                aDraw,
                static_cast<float>(anExpectedX),
                static_cast<float>(anExpectedY),
                80.0F,
                4.0F);
        hasBottom =
            hasBottom ||
            IsDestination(
                aDraw,
                static_cast<float>(anExpectedX),
                static_cast<float>(anExpectedY + 96),
                80.0F,
                4.0F);
        hasLeft =
            hasLeft ||
            IsDestination(
                aDraw,
                static_cast<float>(anExpectedX),
                static_cast<float>(anExpectedY),
                4.0F,
                100.0F);
        hasRight =
            hasRight ||
            IsDestination(
                aDraw,
                static_cast<float>(anExpectedX + 76),
                static_cast<float>(anExpectedY),
                4.0F,
                100.0F);
    }

    Expect(
        hasExpectedBackground,
        "day background crop matches BOARD_OFFSET reference");
    Expect(
        aSelectionDrawCount == 4 &&
            hasTop &&
            hasBottom &&
            hasLeft &&
            hasRight,
        "selection render commands match the legacy day cell");
    aGame.Shutdown();
}

} // namespace

int main()
{
    TestBoardGeometryMatchesLegacyReference();
    TestAdventureInputMatchesLegacyReference();
    TestAdventureRenderCommandsMatchLegacyReference();

    if (gFailureCount != 0)
    {
        std::cerr
            << gFailureCount
            << " legacy parity assertion(s) failed\n";
        return 1;
    }

    std::cout
        << "Legacy parity tests passed against reference "
        << legacy::kLegacySourceRevision
        << '\n';
    return 0;
}
