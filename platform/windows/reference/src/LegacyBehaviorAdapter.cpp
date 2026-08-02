#include "pvz/platform/windows/LegacyBehaviorAdapter.h"

#include "Lawn/Board.h"
#include "Lawn/CursorObject.h"
#include "Lawn/SeedPacket.h"
#include "Lawn/Widget/TitleScreen.h"
#include "LawnApp.h"
#include "pvz/platform/windows/LegacyInputCapture.h"
#include "widget/WidgetManager.h"

#include <cstdint>
#include <limits>

namespace pvz::platform::windows
{
namespace
{

void EnsureBehaviorCaptureEnvironment(LawnApp& theApp)
{
    const bool wasAppFocused = theApp.mHasFocus;
    theApp.mActive = true;
    theApp.mMinimized = false;
    theApp.mHasFocus = true;
    if (theApp.mWidgetManager != nullptr &&
        !theApp.mWidgetManager->mHasFocus)
    {
        theApp.mWidgetManager->GotFocus();
    }
    if (!wasAppFocused)
        theApp.GotFocus();
    if (theApp.mGameScene == GameScenes::SCENE_LEVEL_INTRO &&
        theApp.mBoard != nullptr &&
        theApp.mBoard->mDrawCount == 0)
    {
        theApp.mBoard->mDrawCount = 1;
    }
}

[[nodiscard]] game::BehaviorScene GetScene(const LawnApp& theApp)
{
    if (theApp.mTitleScreen != nullptr)
        return game::BehaviorScene::Title;
    if (theApp.mGameScene == GameScenes::SCENE_LOADING)
        return game::BehaviorScene::Loading;
    if (theApp.mGameSelector != nullptr ||
        theApp.mGameScene == GameScenes::SCENE_MENU)
    {
        return game::BehaviorScene::MainMenu;
    }
    if (theApp.mGameScene == GameScenes::SCENE_LEVEL_INTRO)
        return game::BehaviorScene::AdventureIntro;
    if (theApp.mGameScene == GameScenes::SCENE_PLAYING)
        return game::BehaviorScene::AdventurePlaying;
    return game::BehaviorScene::Other;
}

[[nodiscard]] game::BehaviorBoardStage GetBoardStage(
    const Board* theBoard)
{
    if (theBoard == nullptr)
        return game::BehaviorBoardStage::None;
    switch (theBoard->mBackground)
    {
    case BackgroundType::BACKGROUND_1_DAY:
        return game::BehaviorBoardStage::Day;
    case BackgroundType::BACKGROUND_2_NIGHT:
        return game::BehaviorBoardStage::Night;
    case BackgroundType::BACKGROUND_3_POOL:
        return game::BehaviorBoardStage::Pool;
    case BackgroundType::BACKGROUND_4_FOG:
        return game::BehaviorBoardStage::Fog;
    case BackgroundType::BACKGROUND_5_ROOF:
        return game::BehaviorBoardStage::Roof;
    case BackgroundType::BACKGROUND_6_BOSS:
        return game::BehaviorBoardStage::Boss;
    case BackgroundType::BACKGROUND_MUSHROOM_GARDEN:
    case BackgroundType::BACKGROUND_GREENHOUSE:
    case BackgroundType::BACKGROUND_ZOMBIQUARIUM:
    case BackgroundType::BACKGROUND_TREEOFWISDOM:
        return game::BehaviorBoardStage::Other;
    }
    return game::BehaviorBoardStage::Other;
}

void ObserveGridFocus(
    const Board& theBoard,
    game::BehaviorObservation& theObservation)
{
    if (theBoard.mCursorPreview == nullptr)
        return;
    const int aColumn = theBoard.mCursorPreview->mGridX;
    const int aRow = theBoard.mCursorPreview->mGridY;
    if (aColumn < 0 ||
        aColumn >= static_cast<int>(
            game::kBehaviorBoardColumnCount) ||
        aRow < 0 ||
        aRow >= static_cast<int>(
            game::kBehaviorBoardRowCount))
    {
        return;
    }
    theObservation.mGridColumn =
        static_cast<std::uint8_t>(aColumn);
    theObservation.mGridRow =
        static_cast<std::uint8_t>(aRow);
}

void ObservePlants(
    Board& theBoard,
    game::BehaviorObservation& theObservation)
{
    Plant* aPlant = nullptr;
    while (theBoard.IteratePlants(aPlant))
    {
        if (!aPlant->mIsOnBoard ||
            aPlant->mPlantCol < 0 ||
            aPlant->mPlantCol >= static_cast<int>(
                game::kBehaviorBoardColumnCount) ||
            aPlant->mRow < 0 ||
            aPlant->mRow >= static_cast<int>(
                game::kBehaviorBoardRowCount))
        {
            continue;
        }
        const auto aColumn =
            static_cast<std::uint32_t>(aPlant->mPlantCol);
        const auto aRow =
            static_cast<std::uint32_t>(aPlant->mRow);
        const auto aBit = aRow *
            static_cast<std::uint32_t>(
                game::kBehaviorBoardColumnCount) +
            aColumn;
        theObservation.mOccupiedCells |=
            std::uint64_t{1} << aBit;
        ++theObservation.mPlantCount;
    }
}

[[nodiscard]] std::uint16_t NormalizeU16(int theValue)
{
    if (theValue <= 0)
        return 0;
    constexpr auto aMaximum =
        std::numeric_limits<std::uint16_t>::max();
    if (theValue >= static_cast<int>(aMaximum))
        return aMaximum;
    return static_cast<std::uint16_t>(theValue);
}

[[nodiscard]] game::BehaviorTutorialPhase GetTutorialPhase(
    TutorialState theState)
{
    switch (theState)
    {
    case TutorialState::TUTORIAL_OFF:
        return game::BehaviorTutorialPhase::None;
    case TutorialState::TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER:
        return game::BehaviorTutorialPhase::LevelOnePickUpPeashooter;
    case TutorialState::TUTORIAL_LEVEL_1_PLANT_PEASHOOTER:
        return game::BehaviorTutorialPhase::LevelOnePlantPeashooter;
    case TutorialState::TUTORIAL_LEVEL_1_REFRESH_PEASHOOTER:
        return game::BehaviorTutorialPhase::LevelOneRefreshPeashooter;
    case TutorialState::TUTORIAL_LEVEL_1_COMPLETED:
        return game::BehaviorTutorialPhase::LevelOneCompleted;
    default:
        return game::BehaviorTutorialPhase::Other;
    }
}

void ObserveLevelOneState(
    Board& theBoard,
    game::BehaviorObservation& theObservation)
{
    theObservation.mSun = NormalizeU16(theBoard.mSunMoney);
    theObservation.mTutorialPhase =
        GetTutorialPhase(theBoard.mTutorialState);

    if (theBoard.mSeedBank != nullptr &&
        theBoard.mSeedBank->mNumPackets > 0)
    {
        const auto& aPacket =
            theBoard.mSeedBank->mSeedPackets[0];
        if (aPacket.mRefreshing)
        {
            theObservation.mSeedRefreshing = true;
            theObservation.mSeedRefreshCounter =
                NormalizeU16(aPacket.mRefreshCounter);
            theObservation.mSeedRefreshTime =
                NormalizeU16(aPacket.mRefreshTime);
        }
        if (theBoard.mCursorObject != nullptr &&
            theBoard.mCursorObject->mSeedBankIndex >= 0)
        {
            theObservation.mSeedSelection =
                aPacket.mPacketType ==
                        SeedType::SEED_PEASHOOTER
                ? game::BehaviorSeedSelection::Peashooter
                : game::BehaviorSeedSelection::Other;
        }
    }

    theObservation.mFirstSunSpawned =
        theBoard.mNumSunsFallen > 0;
    if (theObservation.mPlantCount > 0 &&
        !theObservation.mFirstSunSpawned)
    {
        theObservation.mFirstSunCountdown =
            NormalizeU16(theBoard.mSunCountDown);
    }
}

} // namespace

void CaptureLegacyBehaviorTick(LawnApp& theApp)
{
    if (!WasLegacyBehaviorCaptureRequested() ||
        !IsLegacyInputCaptureEnabled())
    {
        return;
    }
    EnsureBehaviorCaptureEnvironment(theApp);
    if (!HasLegacyBehaviorCaptureStarted())
    {
        if (theApp.mTitleScreen != nullptr &&
            theApp.mTitleScreen->mLoadingThreadComplete)
        {
            StartLegacyBehaviorCapture();
        }
        return;
    }
    const auto aFrameCount =
        GetLegacyInputCaptureFrameCount();
    if (aFrameCount == 0)
        return;

    game::BehaviorObservation anObservation;
    anObservation.mTick = aFrameCount - 1U;
    anObservation.mScene = GetScene(theApp);
    anObservation.mBoardStage =
        GetBoardStage(theApp.mBoard);
    if (theApp.mBoard != nullptr)
    {
        if (anObservation.mScene ==
            game::BehaviorScene::AdventurePlaying)
        {
            ObserveGridFocus(*theApp.mBoard, anObservation);
        }
        ObservePlants(*theApp.mBoard, anObservation);
        if (anObservation.mScene ==
                game::BehaviorScene::AdventurePlaying &&
            theApp.mBoard->mLevel == 1)
        {
            ObserveLevelOneState(
                *theApp.mBoard,
                anObservation);
        }
    }
    RecordLegacyBehaviorObservation(anObservation);
}

} // namespace pvz::platform::windows
