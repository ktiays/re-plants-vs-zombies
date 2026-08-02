#include "pvz/game/GameModule.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace pvz::game
{
namespace
{

inline constexpr std::uint32_t kStateMagic = 0x475A5650;
inline constexpr std::uint16_t kLegacyStateVersion = 1;
inline constexpr std::uint16_t kFlowStateVersion = 2;
inline constexpr std::uint16_t kReanimationStateVersion = 3;
inline constexpr std::uint16_t kAdventureIntroStateVersion = 4;
inline constexpr std::uint16_t kLevelOneStateVersion = 5;
inline constexpr std::uint16_t kCombatStateVersion = 6;
inline constexpr std::uint16_t kPointerStateVersion = 7;
inline constexpr std::uint16_t kStateVersion = 8;
inline constexpr std::uint32_t kTitleMusicOrder = 0x98;
inline constexpr std::uint32_t kAdventureMusicOrder = 0;
inline constexpr float kLogicalWidth = 800.0F;
inline constexpr float kLogicalHeight = 600.0F;

[[nodiscard]] std::uint16_t CountSunBeingCollected(
    const LevelOneCombatState& theState)
{
    const auto aCount = std::count_if(
        theState.mSuns.begin(),
        theState.mSuns.end(),
        [](const LevelOneSunState& theSun)
        {
            return theSun.mActive &&
                   theSun.mBeingCollected;
        });
    return static_cast<std::uint16_t>(aCount) *
           LevelOneCombat::kSunValue;
}

inline constexpr std::array<std::string_view, 4> kMenuButtonIds{
    "IMAGE_REANIM_SELECTORSCREEN_STARTADVENTURE_BUTTON",
    "IMAGE_REANIM_SELECTORSCREEN_SURVIVAL_BUTTON",
    "IMAGE_REANIM_SELECTORSCREEN_CHALLENGES_BUTTON",
    "IMAGE_REANIM_SELECTORSCREEN_VASEBREAKER_BUTTON",
};

inline constexpr std::array<std::string_view, 4>
    kMenuButtonHighlightIds{
        "IMAGE_REANIM_SELECTORSCREEN_STARTADVENTURE_HIGHLIGHT",
        "IMAGE_REANIM_SELECTORSCREEN_SURVIVAL_HIGHLIGHT",
        "IMAGE_REANIM_SELECTORSCREEN_CHALLENGES_HIGHLIGHT",
        "IMAGE_REANIM_SELECTORSCREEN_VASEBREAKER_HIGHLIGHT",
    };

[[nodiscard]] engine::RectF ToRectF(const engine::RectI& theRect)
{
    return {
        .mOrigin =
            {
                static_cast<float>(theRect.mOrigin.mX),
                static_cast<float>(theRect.mOrigin.mY),
            },
        .mSize =
            {
                static_cast<float>(theRect.mSize.mWidth),
                static_cast<float>(theRect.mSize.mHeight),
            },
    };
}

[[nodiscard]] engine::SpriteDraw MakeImageDraw(
    const engine::ImageResource& theResource,
    engine::RectF theDestination,
    engine::ColorRgba8 theColor =
        engine::ColorRgba8{255, 255, 255, 255})
{
    return {
        .mImage = theResource.mImage,
        .mSource =
            {
                .mOrigin = {0, 0},
                .mSize = theResource.mSize,
            },
        .mDestination = theDestination,
        .mColor = theColor,
        .mFilterMode = engine::FilterMode::Linear,
    };
}

[[nodiscard]] engine::SpriteDraw MakeSolidDraw(
    engine::ImageHandle theWhitePixel,
    engine::RectF theDestination,
    engine::ColorRgba8 theColor)
{
    return {
        .mImage = theWhitePixel,
        .mSource =
            {
                .mOrigin = {0, 0},
                .mSize = {1, 1},
            },
        .mDestination = theDestination,
        .mColor = theColor,
        .mFilterMode = engine::FilterMode::Nearest,
    };
}

[[nodiscard]] bool Contains(
    const engine::RectI& theRect,
    engine::PointI thePoint)
{
    const auto aRight =
        theRect.mOrigin.mX +
        static_cast<std::int32_t>(theRect.mSize.mWidth);
    const auto aBottom =
        theRect.mOrigin.mY +
        static_cast<std::int32_t>(theRect.mSize.mHeight);
    return
        thePoint.mX >= theRect.mOrigin.mX &&
        thePoint.mX < aRight &&
        thePoint.mY >= theRect.mOrigin.mY &&
        thePoint.mY < aBottom;
}

[[nodiscard]] bool WasKeyboardActivated(
    const engine::IInputFrame& theInput)
{
    return
        theInput.WasKeyPressed(engine::KeyCode::Enter) ||
        theInput.WasKeyPressed(engine::KeyCode::Space);
}

[[nodiscard]] std::u32string ToText(std::uint16_t theValue)
{
    std::u32string aText;
    if (theValue == 0)
        return U"0";

    std::array<char32_t, 5> aDigits{};
    std::size_t aCount{};
    while (theValue > 0)
    {
        aDigits[aCount++] = static_cast<char32_t>(
            U'0' + theValue % 10U);
        theValue = static_cast<std::uint16_t>(
            theValue / 10U);
    }
    while (aCount > 0)
        aText.push_back(aDigits[--aCount]);
    return aText;
}

} // namespace

bool GameModule::SetLevelOneRandomDecisionSource(
    ILevelOneRandomDecisionSource* theSource)
{
    if (mInitialized)
        return false;
    mLevelOneRandomDecisionSource = theSource;
    mLevelOneCombat.SetRandomDecisionSource(theSource);
    return true;
}

engine::LifecycleResult GameModule::Initialize(
    engine::IEngineServices& theServices)
{
    if (mInitialized)
        return engine::LifecycleResult::Failure;

    mServices = &theServices;
    mFlow.Reset();
    mLevelOneBoard.Reset();
    mLevelOneCombat.Reset();
    mLastTick = 0;
    mUpdateCount = 0;
    mInitialized = true;
    mSuspended = false;

    auto LoadImage =
        [this](
            std::string_view theResourceId,
            engine::ImageResource& theResource)
    {
        engine::ImageResourceDiagnostic aDiagnostic;
        static_cast<void>(
            mServices->GetImageResources().Load(
                theResourceId,
                theResource,
                aDiagnostic));
    };

    LoadImage("IMAGE_TITLESCREEN", mTitleScreen);
    LoadImage("IMAGE_PVZ_LOGO", mTitleLogo);
    LoadImage("IMAGE_BACKGROUND1", mDayBackground);
    LoadImage("IMAGE_SEEDBANK", mSeedBank);
    LoadImage("IMAGE_SEEDPACKET_LARGER", mSeedPacket);
    LoadImage("IMAGE_PROJECTILEPEA", mProjectilePea);
    for (std::size_t anIndex = 0;
         anIndex < kMenuButtonIds.size();
         ++anIndex)
    {
        LoadImage(
            kMenuButtonIds[anIndex],
            mMenuButtons[anIndex]);
        LoadImage(
            kMenuButtonHighlightIds[anIndex],
            mMenuButtonHighlights[anIndex]);
    }

    ReanimationClipDiagnostic aReanimationDiagnostic;
    if (mPeashooterClip.Load(
            mServices->GetXmlDocuments(),
            mServices->GetImageResources(),
            "reanim\\PeaShooterSingle.reanim",
            aReanimationDiagnostic))
    {
        if (mPeashooterPlayer.Bind(
                mPeashooterClip,
                "anim_full_idle"))
        {
            constexpr std::size_t kBoardCellCount = 45;
            mReanimationSprites.reserve(
                static_cast<std::size_t>(
                    mPeashooterClip.GetTrackCount()) *
                kBoardCellCount);
            mServices->GetLogger().Log(
                engine::LogLevel::Information,
                "Portable Peashooter reanimation loaded");
        }
        else
        {
            mPeashooterClip.Release(
                mServices->GetImageResources());
        }
    }

    if (mZombieClip.Load(
            mServices->GetXmlDocuments(),
            mServices->GetImageResources(),
            "reanim\\Zombie.reanim",
            aReanimationDiagnostic))
    {
        if (mZombiePlayer.Bind(mZombieClip, "anim_walk"))
        {
            constexpr std::array<std::string_view, 10>
                kNormalZombieHiddenTrackPrefixes{
                    "anim_cone",
                    "anim_bucket",
                    "anim_screendoor",
                    "Zombie_flaghand",
                    "Zombie_duckytube",
                    "anim_tongue",
                    "Zombie_mustache",
                    "Zombie_outerarm_screendoor",
                    "Zombie_innerarm_screendoor",
                    "Zombie_innerarm_screendoor_hand",
                };
            mZombiePlayer.SetHiddenTrackPrefixes(
                kNormalZombieHiddenTrackPrefixes);
            mServices->GetLogger().Log(
                engine::LogLevel::Information,
                "Portable normal Zombie reanimation loaded");
        }
        else
        {
            mZombieClip.Release(
                mServices->GetImageResources());
        }
    }

    if (mSunClip.Load(
            mServices->GetXmlDocuments(),
            mServices->GetImageResources(),
            "reanim\\Sun.reanim",
            aReanimationDiagnostic))
    {
        if (mSunPlayer.Bind(mSunClip) &&
            mSunPlayer.SetFramesPerSecond(6.0F))
        {
            mServices->GetLogger().Log(
                engine::LogLevel::Information,
                "Portable sun reanimation loaded");
        }
        else
        {
            mSunClip.Release(
                mServices->GetImageResources());
        }
    }

    if (mLawnMowerClip.Load(
            mServices->GetXmlDocuments(),
            mServices->GetImageResources(),
            "reanim\\LawnMower.reanim",
            aReanimationDiagnostic))
    {
        if (mLawnMowerPlayer.Bind(
                mLawnMowerClip,
                "anim_normal") &&
            mLawnMowerPlayer.SetFramesPerSecond(70.0F))
        {
            mServices->GetLogger().Log(
                engine::LogLevel::Information,
                "Portable lawn mower reanimation loaded");
        }
        else
        {
            mLawnMowerClip.Release(
                mServices->GetImageResources());
        }
    }

    constexpr std::array<std::byte, 4> kWhitePixel{
        std::byte{255},
        std::byte{255},
        std::byte{255},
        std::byte{255},
    };
    static_cast<void>(
        mServices->GetImages().CreateImage(
            {
                .mSize = {1, 1},
                .mPixelFormat =
                    engine::ImagePixelFormat::Bgra8Unorm,
            },
            kWhitePixel,
            4,
            mWhitePixel));

    engine::FontResourceDiagnostic aFontDiagnostic;
    if (mServices->GetFontResources().Load(
            "FONT_BRIANNETOD16",
            mUiFont,
            aFontDiagnostic))
    {
        RebuildUiText();
        mServices->GetLogger().Log(
            engine::LogLevel::Information,
            "Portable bitmap font and text layout loaded");
    }
    if (mTitleScreen.mImage.IsValid())
    {
        mServices->GetLogger().Log(
            engine::LogLevel::Information,
            mTitleLogo.mImage.IsValid()
                ? "Portable title and alpha-logo resources loaded"
                : "Portable title resource loaded");
    }

    engine::SoundResourceDiagnostic aSoundDiagnostic;
    if (mServices->GetSoundResources().Load(
            "SOUND_LOADINGBAR_FLOWER",
            mLoadingSound,
            aSoundDiagnostic) &&
        mServices->GetSoundResources().Play(
            mLoadingSound.mSound,
            engine::SoundPlayback{},
            mLoadingVoice))
    {
        mServices->GetLogger().Log(
            engine::LogLevel::Information,
            "Portable loading sound decoded and queued");
    }

    engine::MusicResourceDiagnostic aMusicDiagnostic;
    if (mServices->GetMusicResources().Load(
            "sounds/mainmusic.mo3",
            mTitleMusic,
            aMusicDiagnostic))
    {
        if (PlayMusicAt(kTitleMusicOrder))
        {
            mServices->GetLogger().Log(
                engine::LogLevel::Information,
                "Portable title module started at order 0x98");
        }
        else
        {
            mServices->GetMusicResources().Release(
                mTitleMusic.mModule);
            mTitleMusic = {};
        }
    }

    mServices->GetLogger().Log(
        engine::LogLevel::Information,
        "Portable game module initialized");
    return engine::LifecycleResult::Success;
}

void GameModule::Update(
    const engine::GameTick& theTick,
    const engine::IInputFrame& theInput)
{
    if (!mInitialized || mSuspended)
        return;

    const auto aPreviousScene = mFlow.GetScene();
    const auto aPreviousMenuItem = mFlow.GetSelectedMenuItem();
    const auto aPreviousBoardState = mLevelOneBoard.GetState();
    const auto aPreviousCombatState =
        mLevelOneCombat.GetState();
    const bool hadNotice = mFlow.GetNoticeTicks() > 0;
    mFlow.Update(theInput);

    const auto aCurrentScene = mFlow.GetScene();
    if (aPreviousScene != GameScene::StartingAdventure &&
        aCurrentScene == GameScene::StartingAdventure)
    {
        mLevelOneBoard.Reset();
        mLevelOneCombat.Reset();
    }
    if (aPreviousScene != GameScene::AdventureDay &&
        aCurrentScene == GameScene::AdventureDay)
    {
        mFlow.SetOccupiedCells(0);
        mLevelOneCombat.Reset();
    }
    if (aCurrentScene == GameScene::AdventureDay)
    {
        const auto aPointer =
            theInput.GetPointerState().mPosition;
        bool collectedSun = false;
        if (theInput.WasPointerButtonPressed(
                engine::PointerButton::Primary))
        {
            collectedSun =
                mLevelOneCombat.TryCollectSun(aPointer);
        }
        if (theInput.WasPointerButtonPressed(
                engine::PointerButton::Secondary))
        {
            mLevelOneBoard.CancelSelection();
        }
        if (!collectedSun &&
            theInput.WasPointerButtonPressed(
                engine::PointerButton::Primary) &&
            Contains(LevelOneBoard::GetSeedPacketRect(), aPointer))
        {
            static_cast<void>(
                mLevelOneBoard.SelectPeashooter());
        }
        else if (!collectedSun &&
                 mFlow.WasGridActivationRequested())
        {
            if (mLevelOneBoard.IsPeashooterSelected())
            {
                auto anOccupiedCells =
                    mFlow.GetState().mOccupiedCells;
                const auto aColumn =
                    mFlow.GetGridColumn();
                const auto aRow = mFlow.GetGridRow();
                const auto aPlacement =
                    mLevelOneBoard.PlacePeashooter(
                        aColumn,
                        aRow,
                        anOccupiedCells);
                mFlow.SetOccupiedCells(anOccupiedCells);
                if (aPlacement ==
                    LevelOnePlacementResult::Planted)
                {
                    static_cast<void>(
                        mLevelOneCombat.AddPeashooter(
                            aColumn,
                            aRow));
                }
            }
            else if (WasKeyboardActivated(theInput))
            {
                static_cast<void>(
                    mLevelOneBoard.SelectPeashooter());
            }
        }

        mLevelOneCombat.Update();
        const auto aCollectedSun =
            mLevelOneCombat.ConsumeCollectedSun();
        if (aCollectedSun != 0)
            mLevelOneBoard.AddSun(aCollectedSun);
        mLevelOneBoard.Update();
        const auto aDestroyedCells =
            mLevelOneCombat.ConsumeDestroyedCells();
        if (aDestroyedCells != 0)
        {
            mFlow.SetOccupiedCells(
                mFlow.GetState().mOccupiedCells &
                ~aDestroyedCells);
        }
    }

    mLastTick = theTick.mIndex;
    ++mUpdateCount;
    if (aCurrentScene == GameScene::AdventureDay)
    {
        mPeashooterPlayer.Update();
        mZombiePlayer.Update();
        mSunPlayer.Update();
        if (mLevelOneCombat.GetState().mMowerPhase ==
            LevelOneMowerPhase::Triggered)
        {
            mLawnMowerPlayer.Update();
        }
    }

    if (aPreviousScene != aCurrentScene)
        HandleSceneChange(aPreviousScene, aCurrentScene);

    const bool hasNotice = mFlow.GetNoticeTicks() > 0;
    const auto aCurrentBoardState = mLevelOneBoard.GetState();
    const auto aCurrentCombatState =
        mLevelOneCombat.GetState();
    if (aPreviousScene != aCurrentScene ||
        aPreviousMenuItem != mFlow.GetSelectedMenuItem() ||
        hadNotice != hasNotice ||
        aPreviousBoardState.mSun != aCurrentBoardState.mSun ||
        aPreviousBoardState.mSeedRefreshing !=
            aCurrentBoardState.mSeedRefreshing ||
        aPreviousBoardState.mSeedSelection !=
            aCurrentBoardState.mSeedSelection ||
        CountSunBeingCollected(aPreviousCombatState) !=
            CountSunBeingCollected(aCurrentCombatState) ||
        aPreviousCombatState.mPhase !=
            aCurrentCombatState.mPhase)
    {
        RebuildUiText();
    }
}

void GameModule::Render(engine::IRenderFrame& theFrame) const
{
    if (!mInitialized)
        return;

    switch (mFlow.GetScene())
    {
    case GameScene::Title:
        RenderTitle(theFrame);
        break;
    case GameScene::MainMenu:
        RenderMenu(theFrame, false);
        break;
    case GameScene::StartingAdventure:
        RenderMenu(theFrame, true);
        break;
    case GameScene::AdventureIntro:
        RenderAdventureDay(theFrame);
        break;
    case GameScene::AdventureDay:
        RenderAdventureDay(theFrame);
        break;
    case GameScene::Count:
        theFrame.Clear(engine::ColorRgba8{0, 0, 0, 255});
        break;
    }

    if (!mUiTextSprites.empty())
        theFrame.SubmitSprites(mUiTextSprites);
}

bool GameModule::LoadState(engine::IStateReader& theReader)
{
    std::uint32_t aMagic{};
    std::uint16_t aVersion{};
    engine::TickIndex aLastTick{};
    std::uint64_t anUpdateCount{};
    bool aSuspended{};
    std::uint64_t aReanimationTick{};
    LevelOneBoardState aLevelOneBoardState;

    if (!theReader.ReadU32(aMagic) ||
        !theReader.ReadU16(aVersion) ||
        !theReader.ReadU64(aLastTick) ||
        !theReader.ReadU64(anUpdateCount) ||
        !theReader.ReadBool(aSuspended))
    {
        return false;
    }

    if (aMagic != kStateMagic ||
        (aVersion != kLegacyStateVersion &&
         aVersion != kFlowStateVersion &&
         aVersion != kReanimationStateVersion &&
         aVersion != kAdventureIntroStateVersion &&
         aVersion != kLevelOneStateVersion &&
         aVersion != kCombatStateVersion &&
         aVersion != kPointerStateVersion &&
         aVersion != kStateVersion))
    {
        return false;
    }

    GameFlow aFlow;
    if (aVersion == kFlowStateVersion ||
        aVersion == kReanimationStateVersion ||
        aVersion == kAdventureIntroStateVersion ||
        aVersion == kLevelOneStateVersion ||
        aVersion == kCombatStateVersion ||
        aVersion == kPointerStateVersion ||
        aVersion == kStateVersion)
    {
        std::uint8_t aScene{};
        std::uint8_t aMenuItem{};
        GameFlowState aState;
        if (!theReader.ReadU8(aScene) ||
            !theReader.ReadU8(aMenuItem) ||
            !theReader.ReadU16(aState.mTransitionTicks) ||
            !theReader.ReadU16(aState.mNoticeTicks) ||
            !theReader.ReadU8(aState.mGridColumn) ||
            !theReader.ReadU8(aState.mGridRow) ||
            !theReader.ReadU64(aState.mOccupiedCells) ||
            (aVersion >= kPointerStateVersion &&
             (!theReader.ReadBool(aState.mHasPointerPosition) ||
              !theReader.ReadI32(
                  aState.mLastPointerPosition.mX) ||
              !theReader.ReadI32(
                  aState.mLastPointerPosition.mY))))
        {
            return false;
        }
        aState.mScene = static_cast<GameScene>(aScene);
        aState.mMenuItem = static_cast<MainMenuItem>(aMenuItem);
        if (aVersion < kAdventureIntroStateVersion &&
            aState.mScene == GameScene::AdventureIntro)
        {
            return false;
        }
        if (!aFlow.RestoreState(aState))
            return false;
    }
    if ((aVersion == kReanimationStateVersion ||
         aVersion == kAdventureIntroStateVersion ||
         aVersion == kLevelOneStateVersion ||
         aVersion == kCombatStateVersion ||
         aVersion == kPointerStateVersion ||
         aVersion == kStateVersion) &&
        !theReader.ReadU64(aReanimationTick))
    {
        return false;
    }
    LevelOneBoard aLevelOneBoard;
    aLevelOneBoard.Reset();
    if (aVersion == kLevelOneStateVersion ||
        aVersion == kCombatStateVersion ||
        aVersion == kPointerStateVersion ||
        aVersion == kStateVersion)
    {
        std::uint8_t aSeedSelection{};
        if (!theReader.ReadU16(aLevelOneBoardState.mSun) ||
            !theReader.ReadU16(
                aLevelOneBoardState.mSeedRefreshCounter) ||
            !theReader.ReadBool(
                aLevelOneBoardState.mSeedRefreshing) ||
            !theReader.ReadU8(aSeedSelection))
        {
            return false;
        }
        aLevelOneBoardState.mSeedSelection =
            static_cast<LevelOneSeedSelection>(
                aSeedSelection);
        if (!aLevelOneBoard.RestoreState(
                aLevelOneBoardState))
        {
            return false;
        }
    }

    LevelOneCombat aLevelOneCombat;
    aLevelOneCombat.Reset();
    if (aVersion == kCombatStateVersion ||
        aVersion == kPointerStateVersion ||
        aVersion == kStateVersion)
    {
        if (!aLevelOneCombat.LoadState(
                theReader,
                aVersion >= kPointerStateVersion,
                aVersion >= kStateVersion) ||
            aLevelOneCombat.GetOccupiedCells() !=
                aFlow.GetState().mOccupiedCells)
        {
            return false;
        }
    }
    else
    {
        const auto anOccupiedCells =
            aFlow.GetState().mOccupiedCells;
        for (std::uint8_t aRow = 0; aRow < 5; ++aRow)
        {
            for (std::uint8_t aColumn = 0;
                 aColumn < 9;
                 ++aColumn)
            {
                const auto anIndex =
                    static_cast<std::uint32_t>(aRow) * 9U +
                    aColumn;
                if (aRow == LevelOneCombat::kLaneRow &&
                    (anOccupiedCells &
                     (std::uint64_t{1} << anIndex)) != 0 &&
                    !aLevelOneCombat.AddPeashooter(
                        aColumn,
                        aRow))
                {
                    return false;
                }
            }
        }
    }

    mLastTick = aLastTick;
    mUpdateCount = anUpdateCount;
    mSuspended = aSuspended;
    mFlow = aFlow;
    mLevelOneBoard = aLevelOneBoard;
    mLevelOneCombat = aLevelOneCombat;
    mLevelOneCombat.SetRandomDecisionSource(
        mLevelOneRandomDecisionSource);
    mPeashooterPlayer.RestoreTick(aReanimationTick);
    mZombiePlayer.RestoreTick(aReanimationTick);
    mSunPlayer.RestoreTick(aReanimationTick);
    mLawnMowerPlayer.RestoreTick(aReanimationTick);
    RebuildUiText();
    SynchronizeMusic();
    return true;
}

bool GameModule::SaveState(engine::IStateWriter& theWriter) const
{
    if (!mInitialized)
        return false;

    const auto aState = mFlow.GetState();
    const auto aLevelOneBoardState =
        mLevelOneBoard.GetState();
    return
        theWriter.WriteU32(kStateMagic) &&
        theWriter.WriteU16(kStateVersion) &&
        theWriter.WriteU64(mLastTick) &&
        theWriter.WriteU64(mUpdateCount) &&
        theWriter.WriteBool(mSuspended) &&
        theWriter.WriteU8(
            static_cast<std::uint8_t>(aState.mScene)) &&
        theWriter.WriteU8(
            static_cast<std::uint8_t>(aState.mMenuItem)) &&
        theWriter.WriteU16(aState.mTransitionTicks) &&
        theWriter.WriteU16(aState.mNoticeTicks) &&
        theWriter.WriteU8(aState.mGridColumn) &&
        theWriter.WriteU8(aState.mGridRow) &&
        theWriter.WriteU64(aState.mOccupiedCells) &&
        theWriter.WriteBool(aState.mHasPointerPosition) &&
        theWriter.WriteI32(aState.mLastPointerPosition.mX) &&
        theWriter.WriteI32(aState.mLastPointerPosition.mY) &&
        theWriter.WriteU64(mPeashooterPlayer.GetTick()) &&
        theWriter.WriteU16(aLevelOneBoardState.mSun) &&
        theWriter.WriteU16(
            aLevelOneBoardState.mSeedRefreshCounter) &&
        theWriter.WriteBool(
            aLevelOneBoardState.mSeedRefreshing) &&
        theWriter.WriteU8(
            static_cast<std::uint8_t>(
                aLevelOneBoardState.mSeedSelection)) &&
        mLevelOneCombat.SaveState(theWriter);
}

void GameModule::Suspend()
{
    if (mInitialized)
    {
        mSuspended = true;
        if (mTitleMusic.mModule.IsValid())
        {
            mServices->GetMusicResources().Pause(
                mTitleMusic.mModule,
                true);
        }
    }
}

void GameModule::Resume()
{
    if (mInitialized)
    {
        mSuspended = false;
        if (mTitleMusic.mModule.IsValid())
        {
            mServices->GetMusicResources().Pause(
                mTitleMusic.mModule,
                false);
        }
    }
}

void GameModule::Shutdown()
{
    if (!mInitialized)
        return;

    mServices->GetLogger().Log(
        engine::LogLevel::Information,
        "Portable game module shut down");

    mPeashooterPlayer.Reset();
    mPeashooterClip.Release(
        mServices->GetImageResources());
    mZombiePlayer.Reset();
    mZombieClip.Release(
        mServices->GetImageResources());
    mSunPlayer.Reset();
    mSunClip.Release(
        mServices->GetImageResources());
    mLawnMowerPlayer.Reset();
    mLawnMowerClip.Release(
        mServices->GetImageResources());
    for (auto& aResource : mMenuButtonHighlights)
    {
        if (aResource.mImage.IsValid())
            mServices->GetImageResources().Release(aResource.mImage);
        aResource = {};
    }
    for (auto& aResource : mMenuButtons)
    {
        if (aResource.mImage.IsValid())
            mServices->GetImageResources().Release(aResource.mImage);
        aResource = {};
    }
    if (mSeedPacket.mImage.IsValid())
        mServices->GetImageResources().Release(mSeedPacket.mImage);
    if (mProjectilePea.mImage.IsValid())
    {
        mServices->GetImageResources().Release(
            mProjectilePea.mImage);
    }
    if (mSeedBank.mImage.IsValid())
        mServices->GetImageResources().Release(mSeedBank.mImage);
    if (mDayBackground.mImage.IsValid())
        mServices->GetImageResources().Release(mDayBackground.mImage);
    if (mTitleLogo.mImage.IsValid())
        mServices->GetImageResources().Release(mTitleLogo.mImage);
    if (mTitleScreen.mImage.IsValid())
        mServices->GetImageResources().Release(mTitleScreen.mImage);
    if (mWhitePixel.IsValid())
        mServices->GetImages().DestroyImage(mWhitePixel);
    if (mUiFont.mFont.IsValid())
        mServices->GetFontResources().Release(mUiFont.mFont);
    if (mLoadingVoice.IsValid())
        mServices->GetSoundResources().Stop(mLoadingVoice);
    if (mLoadingSound.mSound.IsValid())
    {
        mServices->GetSoundResources().Release(
            mLoadingSound.mSound);
    }
    if (mTitleMusic.mModule.IsValid())
    {
        mServices->GetMusicResources().Stop(
            mTitleMusic.mModule);
        mServices->GetMusicResources().Release(
            mTitleMusic.mModule);
    }

    mUiTextSprites.clear();
    mReanimationSprites.clear();
    mFlow.Reset();
    mLevelOneBoard.Reset();
    mLevelOneCombat.Reset();
    mUiFont = {};
    mLoadingVoice = {};
    mLoadingSound = {};
    mTitleMusic = {};
    mProjectilePea = {};
    mSeedPacket = {};
    mSeedBank = {};
    mDayBackground = {};
    mTitleLogo = {};
    mTitleScreen = {};
    mWhitePixel = {};
    mServices = nullptr;
    mInitialized = false;
    mSuspended = false;
}

bool GameModule::IsInitialized() const
{
    return mInitialized;
}

bool GameModule::IsSuspended() const
{
    return mSuspended;
}

engine::TickIndex GameModule::GetLastTick() const
{
    return mLastTick;
}

std::uint64_t GameModule::GetUpdateCount() const
{
    return mUpdateCount;
}

GameScene GameModule::GetScene() const
{
    return mFlow.GetScene();
}

GameFlowState GameModule::GetFlowState() const
{
    return mFlow.GetState();
}

LevelOneBoardState GameModule::GetLevelOneBoardState() const
{
    return mLevelOneBoard.GetState();
}

LevelOneCombatState GameModule::GetLevelOneCombatState() const
{
    return mLevelOneCombat.GetState();
}

bool GameModule::HasRandomDecisionFailure() const
{
    return mLevelOneCombat.HasRandomDecisionFailure();
}

BehaviorObservation GameModule::GetBehaviorObservation() const
{
    const auto aFlowState = mFlow.GetState();
    BehaviorObservation anObservation;
    anObservation.mTick = mLastTick;
    switch (aFlowState.mScene)
    {
    case GameScene::Title:
        anObservation.mScene = BehaviorScene::Title;
        break;
    case GameScene::MainMenu:
        anObservation.mScene = BehaviorScene::MainMenu;
        break;
    case GameScene::StartingAdventure:
        anObservation.mScene = BehaviorScene::MainMenu;
        break;
    case GameScene::AdventureIntro:
        anObservation.mScene = BehaviorScene::AdventureIntro;
        anObservation.mBoardStage = BehaviorBoardStage::Day;
        break;
    case GameScene::AdventureDay:
    {
        const auto aBoardState = mLevelOneBoard.GetState();
        const auto aCombatState = mLevelOneCombat.GetState();
        anObservation.mScene = BehaviorScene::AdventurePlaying;
        anObservation.mBoardStage = BehaviorBoardStage::Day;
        anObservation.mGridColumn = aFlowState.mGridColumn;
        anObservation.mGridRow = aFlowState.mGridRow;
        anObservation.mOccupiedCells =
            aFlowState.mOccupiedCells;
        anObservation.mPlantCount =
            static_cast<std::uint32_t>(
                std::popcount(aFlowState.mOccupiedCells));
        anObservation.mSun = aBoardState.mSun;
        anObservation.mSeedRefreshCounter =
            aBoardState.mSeedRefreshCounter;
        anObservation.mSeedRefreshing =
            aBoardState.mSeedRefreshing;
        if (aBoardState.mSeedRefreshing)
        {
            anObservation.mSeedRefreshTime =
                LevelOneBoard::kPeashooterRefreshTime;
        }
        if (aBoardState.mSeedSelection ==
            LevelOneSeedSelection::Peashooter)
        {
            anObservation.mSeedSelection =
                BehaviorSeedSelection::Peashooter;
        }
        if (anObservation.mPlantCount >= 2)
        {
            anObservation.mTutorialPhase =
                BehaviorTutorialPhase::LevelOneCompleted;
        }
        else if (anObservation.mSeedSelection ==
                 BehaviorSeedSelection::Peashooter)
        {
            anObservation.mTutorialPhase =
                BehaviorTutorialPhase::LevelOnePlantPeashooter;
        }
        else if (aBoardState.mSeedRefreshing ||
                 (anObservation.mPlantCount == 1 &&
                  aBoardState.mSun +
                          CountSunBeingCollected(aCombatState) <
                      LevelOneBoard::kPeashooterCost))
        {
            anObservation.mTutorialPhase =
                BehaviorTutorialPhase::LevelOneRefreshPeashooter;
        }
        else
        {
            anObservation.mTutorialPhase =
                BehaviorTutorialPhase::LevelOnePickUpPeashooter;
        }
        anObservation.mFirstSunSpawned =
            aCombatState.mSunsSpawned != 0;
        if (!anObservation.mFirstSunSpawned)
        {
            anObservation.mFirstSunCountdown =
                aCombatState.mSunCountdown;
        }
        anObservation.mCurrentWave =
            aCombatState.mCurrentWave;
        anObservation.mZombieCountdown =
            aCombatState.mZombieCountdown;
        anObservation.mZombieCount =
            aCombatState.mZombieCount;
        anObservation.mLevelOutcome =
            aCombatState.mPhase == LevelOneCombatPhase::Won
            ? BehaviorLevelOutcome::Won
            : (aCombatState.mPhase == LevelOneCombatPhase::Lost
                   ? BehaviorLevelOutcome::Lost
                   : BehaviorLevelOutcome::Playing);
        switch (aCombatState.mMowerPhase)
        {
        case LevelOneMowerPhase::Ready:
            anObservation.mMowerState =
                BehaviorMowerState::Ready;
            break;
        case LevelOneMowerPhase::Triggered:
            anObservation.mMowerState =
                BehaviorMowerState::Triggered;
            break;
        case LevelOneMowerPhase::Spent:
            anObservation.mMowerState =
                BehaviorMowerState::Spent;
            break;
        case LevelOneMowerPhase::Count:
            break;
        }
        anObservation.mLevelAwardSpawned =
            aCombatState.mAwardSpawned;
        if (aCombatState.mPhase != LevelOneCombatPhase::Won &&
            aCombatState.mPhase != LevelOneCombatPhase::Lost)
        {
            anObservation.mProjectileCount =
                aCombatState.mProjectileCount;
        }
        if (aCombatState.mCurrentWave > 0)
        {
            const auto aCurrentWave = static_cast<std::uint8_t>(
                aCombatState.mCurrentWave - 1U);
            for (const auto& aZombie : aCombatState.mZombies)
            {
                if (aZombie.mActive &&
                    aZombie.mFromWave == aCurrentWave)
                {
                    anObservation.mZombieWaveHealth =
                        static_cast<std::uint16_t>(
                            anObservation.mZombieWaveHealth +
                            aZombie.mHealth);
                }
            }
        }
        for (std::size_t aSlot = 0;
             aSlot < aCombatState.mSuns.size();
             ++aSlot)
        {
            const auto& aSun = aCombatState.mSuns[aSlot];
            if (!aSun.mActive)
                continue;
            anObservation.mSuns[aSlot] = {
                .mActive = true,
                .mBeingCollected = aSun.mBeingCollected,
                .mXMilliPixels = aSun.mXMilliPixels,
                .mYMilliPixels = aSun.mYMilliPixels,
                .mGroundYMilliPixels = aSun.mGroundYMilliPixels,
                .mAge = aSun.mAge,
            };
        }
        break;
    }
    case GameScene::Count:
        anObservation.mScene = BehaviorScene::Other;
        break;
    }
    return anObservation;
}

std::uint64_t GameModule::GetReanimationTick() const
{
    return mPeashooterPlayer.GetTick();
}

void GameModule::RebuildUiText()
{
    mUiTextSprites.clear();
    if (!mUiFont.mFont.IsValid())
        return;

    const bool hasNotice = mFlow.GetNoticeTicks() > 0;
    switch (mFlow.GetScene())
    {
    case GameScene::Title:
        AppendCenteredText(
            U"PRESS ENTER OR CLICK",
            565.0F,
            {255, 255, 255, 255});
        break;
    case GameScene::MainMenu:
        AppendCenteredText(
            hasNotice
                ? U"THIS MODE IS STILL BEING PORTED"
                : U"ARROWS OR POINTER TO SELECT",
            575.0F,
            hasNotice
                ? engine::ColorRgba8{255, 210, 80, 255}
                : engine::ColorRgba8{255, 255, 255, 255});
        break;
    case GameScene::StartingAdventure:
        AppendCenteredText(
            U"STARTING ADVENTURE...",
            315.0F,
            {255, 255, 255, 255});
        break;
    case GameScene::AdventureIntro:
        AppendCenteredText(
            U"ADVENTURE INTRO",
            585.0F,
            {255, 255, 255, 255});
        break;
    case GameScene::AdventureDay:
    {
        const auto aBoardState = mLevelOneBoard.GetState();
        const auto aCombatState = mLevelOneCombat.GetState();
        AppendCenteredTextAtX(
            ToText(aBoardState.mSun),
            44.0F,
            78.0F,
            {0, 0, 0, 255});
        std::u32string_view aPrompt =
            U"CLICK SEED PACKET OR PRESS SPACE";
        if (aCombatState.mPhase ==
            LevelOneCombatPhase::Lost)
        {
            aPrompt = U"THE ZOMBIES ATE YOUR BRAINS";
        }
        else if (aCombatState.mPhase ==
                 LevelOneCombatPhase::Won)
        {
            aPrompt = U"LEVEL 1 COMPLETE - REWARD UNLOCKED";
        }
        else if (aCombatState.mPhase ==
                 LevelOneCombatPhase::FirstWaveCleared)
        {
            aPrompt = U"FIRST WAVE CLEARED";
        }
        else if (mLevelOneBoard.IsPeashooterSelected())
        {
            aPrompt =
                U"CLICK CENTER ROW TO PLANT - ESC RETURNS TO MENU";
        }
        else if (aBoardState.mSeedRefreshing)
        {
            aPrompt =
                U"PEASHOOTER RECHARGING - COLLECT FALLING SUN";
        }
        else if (aCombatState.mPhase ==
                 LevelOneCombatPhase::AwaitingSecondPlant)
        {
            aPrompt =
                U"PLANT A SECOND PEASHOOTER TO START THE WAVE";
        }
        AppendCenteredText(
            aPrompt,
            585.0F,
            {255, 255, 255, 255});
        break;
    }
    case GameScene::Count:
        break;
    }
}

void GameModule::AppendCenteredText(
    std::u32string_view theText,
    float theBaseline,
    engine::ColorRgba8 theColor)
{
    engine::TextMetrics aTextMetrics;
    if (!mServices->GetFontResources().MeasureText(
            mUiFont.mFont,
            theText,
            aTextMetrics))
    {
        return;
    }
    static_cast<void>(
        mServices->GetFontResources().AppendTextSprites(
            mUiFont.mFont,
            theText,
            {
                .mX =
                    (kLogicalWidth -
                     static_cast<float>(aTextMetrics.mAdvance)) *
                    0.5F,
                .mY = theBaseline,
            },
            theColor,
            mUiTextSprites));
}

void GameModule::AppendCenteredTextAtX(
    std::u32string_view theText,
    float theCenterX,
    float theBaseline,
    engine::ColorRgba8 theColor)
{
    engine::TextMetrics aTextMetrics;
    if (!mServices->GetFontResources().MeasureText(
            mUiFont.mFont,
            theText,
            aTextMetrics))
    {
        return;
    }
    AppendText(
        theText,
        {
            theCenterX -
                static_cast<float>(
                    aTextMetrics.mAdvance) *
                    0.5F,
            theBaseline,
        },
        theColor);
}

void GameModule::AppendText(
    std::u32string_view theText,
    engine::PointF theBaseline,
    engine::ColorRgba8 theColor)
{
    static_cast<void>(
        mServices->GetFontResources().AppendTextSprites(
            mUiFont.mFont,
            theText,
            theBaseline,
            theColor,
            mUiTextSprites));
}

void GameModule::HandleSceneChange(
    GameScene thePreviousScene,
    GameScene theCurrentScene)
{
    if (!mTitleMusic.mModule.IsValid())
        return;

    switch (theCurrentScene)
    {
    case GameScene::StartingAdventure:
        mServices->GetMusicResources().Stop(
            mTitleMusic.mModule);
        break;
    case GameScene::AdventureIntro:
        static_cast<void>(PlayMusicAt(kAdventureMusicOrder));
        break;
    case GameScene::AdventureDay:
        break;
    case GameScene::MainMenu:
        if (thePreviousScene == GameScene::AdventureDay)
            static_cast<void>(PlayMusicAt(kTitleMusicOrder));
        break;
    case GameScene::Title:
        static_cast<void>(PlayMusicAt(kTitleMusicOrder));
        break;
    case GameScene::Count:
        break;
    }
}

bool GameModule::PlayMusicAt(std::uint32_t theOrder)
{
    if (!mTitleMusic.mModule.IsValid() ||
        theOrder >= mTitleMusic.mDescriptor.mOrderCount)
    {
        return false;
    }
    return mServices->GetMusicResources().Play(
        mTitleMusic.mModule,
        {
            .mPosition = {.mOrder = theOrder, .mRow = 0},
            .mVolume = 1.0F,
            .mLoopMode = engine::MusicLoopMode::Loop,
        });
}

void GameModule::SynchronizeMusic()
{
    if (!mInitialized ||
        !mTitleMusic.mModule.IsValid())
    {
        return;
    }

    if (mFlow.GetScene() == GameScene::StartingAdventure)
    {
        mServices->GetMusicResources().Stop(
            mTitleMusic.mModule);
    }
    else
    {
        const auto anOrder =
            (mFlow.GetScene() == GameScene::AdventureIntro ||
             mFlow.GetScene() == GameScene::AdventureDay)
                ? kAdventureMusicOrder
                : kTitleMusicOrder;
        static_cast<void>(PlayMusicAt(anOrder));
    }
    if (mSuspended)
    {
        mServices->GetMusicResources().Pause(
            mTitleMusic.mModule,
            true);
    }
}

void GameModule::RenderTitle(engine::IRenderFrame& theFrame) const
{
    theFrame.Clear(engine::ColorRgba8{0, 0, 0, 255});
    std::array<engine::SpriteDraw, 2> aDraws;
    std::size_t aDrawCount{};
    if (mTitleScreen.mImage.IsValid())
    {
        aDraws[aDrawCount++] = MakeImageDraw(
            mTitleScreen,
            {{0.0F, 0.0F}, {kLogicalWidth, kLogicalHeight}});
    }
    if (mTitleLogo.mImage.IsValid())
    {
        const auto aLogoWidth =
            static_cast<float>(mTitleLogo.mSize.mWidth);
        const auto aLogoHeight =
            static_cast<float>(mTitleLogo.mSize.mHeight);
        aDraws[aDrawCount++] = MakeImageDraw(
            mTitleLogo,
            {
                {
                    (kLogicalWidth - aLogoWidth) * 0.5F,
                    20.0F,
                },
                {aLogoWidth, aLogoHeight},
            });
    }
    if (aDrawCount > 0)
    {
        theFrame.SubmitSprites(
            std::span<const engine::SpriteDraw>(
                aDraws.data(),
                aDrawCount));
    }
}

void GameModule::RenderMenu(
    engine::IRenderFrame& theFrame,
    bool theStartingAdventure) const
{
    theFrame.Clear(engine::ColorRgba8{24, 42, 16, 255});
    std::array<engine::SpriteDraw, 8> aDraws;
    std::size_t aDrawCount{};
    if (mTitleScreen.mImage.IsValid())
    {
        aDraws[aDrawCount++] = MakeImageDraw(
            mTitleScreen,
            {{0.0F, 0.0F}, {kLogicalWidth, kLogicalHeight}},
            {165, 185, 150, 255});
    }
    if (mTitleLogo.mImage.IsValid())
    {
        const auto aScale = 0.62F;
        const auto aLogoWidth =
            static_cast<float>(mTitleLogo.mSize.mWidth) * aScale;
        const auto aLogoHeight =
            static_cast<float>(mTitleLogo.mSize.mHeight) * aScale;
        aDraws[aDrawCount++] = MakeImageDraw(
            mTitleLogo,
            {{22.0F, 25.0F}, {aLogoWidth, aLogoHeight}});
    }

    for (std::size_t anIndex = 0;
         anIndex < mMenuButtons.size();
         ++anIndex)
    {
        const auto anItem =
            static_cast<MainMenuItem>(anIndex);
        const bool isSelected =
            mFlow.GetSelectedMenuItem() == anItem;
        const auto& aHighlight =
            mMenuButtonHighlights[anIndex];
        const auto& aNormal = mMenuButtons[anIndex];
        const auto& aResource =
            isSelected && aHighlight.mImage.IsValid()
                ? aHighlight
                : aNormal;
        if (!aResource.mImage.IsValid())
            continue;
        const auto aColor =
            mFlow.IsMenuItemAvailable(anItem)
                ? engine::ColorRgba8{255, 255, 255, 255}
                : engine::ColorRgba8{145, 145, 145, 255};
        aDraws[aDrawCount++] = MakeImageDraw(
            aResource,
            ToRectF(GameFlow::GetMenuItemRect(anItem)),
            aColor);
    }

    if (theStartingAdventure && mWhitePixel.IsValid())
    {
        aDraws[aDrawCount++] = MakeSolidDraw(
            mWhitePixel,
            {{0.0F, 0.0F}, {kLogicalWidth, kLogicalHeight}},
            {0, 0, 0, 170});
    }
    if (aDrawCount > 0)
    {
        theFrame.SubmitSprites(
            std::span<const engine::SpriteDraw>(
                aDraws.data(),
                aDrawCount));
    }
}

void GameModule::RenderAdventureDay(
    engine::IRenderFrame& theFrame) const
{
    theFrame.Clear(engine::ColorRgba8{45, 100, 35, 255});
    std::array<engine::SpriteDraw, 64> aDraws;
    std::size_t aDrawCount{};

    if (mDayBackground.mImage.IsValid())
    {
        auto aBackgroundDraw = MakeImageDraw(
            mDayBackground,
            {{0.0F, 0.0F}, {kLogicalWidth, kLogicalHeight}});
        if (mDayBackground.mSize.mWidth >= 1020 &&
            mDayBackground.mSize.mHeight >= 600)
        {
            aBackgroundDraw.mSource = {
                .mOrigin = {220, 0},
                .mSize = {800, 600},
            };
        }
        aDraws[aDrawCount++] = aBackgroundDraw;
    }
    if (mSeedBank.mImage.IsValid())
    {
        aDraws[aDrawCount++] = MakeImageDraw(
            mSeedBank,
            {
                {10.0F, 0.0F},
                {
                    static_cast<float>(mSeedBank.mSize.mWidth),
                    static_cast<float>(mSeedBank.mSize.mHeight),
                },
            });
    }
    const auto aSeedPacketRect =
        LevelOneBoard::GetSeedPacketRect();
    const auto aBoardState = mLevelOneBoard.GetState();
    const auto aCombatState = mLevelOneCombat.GetState();
    if (mSeedPacket.mImage.IsValid())
    {
        const auto aPacketColor =
            mLevelOneBoard.CanSelectPeashooter() ||
                    mLevelOneBoard.IsPeashooterSelected()
                ? engine::ColorRgba8{255, 255, 255, 255}
                : engine::ColorRgba8{125, 125, 125, 255};
        aDraws[aDrawCount++] = MakeImageDraw(
            mSeedPacket,
            ToRectF(aSeedPacketRect),
            aPacketColor);
    }
    if (mWhitePixel.IsValid() &&
        aBoardState.mSeedRefreshing)
    {
        const auto aRemainingTicks =
            LevelOneBoard::kPeashooterRefreshTime -
            aBoardState.mSeedRefreshCounter;
        const auto anOverlayHeight =
            static_cast<float>(aSeedPacketRect.mSize.mHeight) *
            static_cast<float>(aRemainingTicks) /
            static_cast<float>(
                LevelOneBoard::kPeashooterRefreshTime);
        aDraws[aDrawCount++] = MakeSolidDraw(
            mWhitePixel,
            {
                {
                    static_cast<float>(
                        aSeedPacketRect.mOrigin.mX),
                    static_cast<float>(
                        aSeedPacketRect.mOrigin.mY),
                },
                {
                    static_cast<float>(
                        aSeedPacketRect.mSize.mWidth),
                    anOverlayHeight,
                },
            },
            {0, 0, 0, 145});
    }
    if (mWhitePixel.IsValid() &&
        mLevelOneBoard.IsPeashooterSelected())
    {
        constexpr float kPacketLineWidth = 2.0F;
        const auto aPacketX =
            static_cast<float>(aSeedPacketRect.mOrigin.mX);
        const auto aPacketY =
            static_cast<float>(aSeedPacketRect.mOrigin.mY);
        const auto aPacketWidth =
            static_cast<float>(aSeedPacketRect.mSize.mWidth);
        const auto aPacketHeight =
            static_cast<float>(aSeedPacketRect.mSize.mHeight);
        const engine::ColorRgba8 aPacketSelectionColor{
            255, 255, 255, 245};
        aDraws[aDrawCount++] = MakeSolidDraw(
            mWhitePixel,
            {
                {aPacketX, aPacketY},
                {aPacketWidth, kPacketLineWidth},
            },
            aPacketSelectionColor);
        aDraws[aDrawCount++] = MakeSolidDraw(
            mWhitePixel,
            {
                {
                    aPacketX,
                    aPacketY + aPacketHeight -
                        kPacketLineWidth,
                },
                {aPacketWidth, kPacketLineWidth},
            },
            aPacketSelectionColor);
        aDraws[aDrawCount++] = MakeSolidDraw(
            mWhitePixel,
            {
                {aPacketX, aPacketY},
                {kPacketLineWidth, aPacketHeight},
            },
            aPacketSelectionColor);
        aDraws[aDrawCount++] = MakeSolidDraw(
            mWhitePixel,
            {
                {
                    aPacketX + aPacketWidth -
                        kPacketLineWidth,
                    aPacketY,
                },
                {kPacketLineWidth, aPacketHeight},
            },
            aPacketSelectionColor);
    }
    if (aDrawCount > 0)
    {
        theFrame.SubmitSprites(
            std::span<const engine::SpriteDraw>(
                aDraws.data(),
                aDrawCount));
        aDrawCount = 0;
    }

    mReanimationSprites.clear();
    if (mPeashooterPlayer.IsBound())
    {
        for (std::uint8_t aRow = 0; aRow < 5; ++aRow)
        {
            for (std::uint8_t aColumn = 0;
                 aColumn < 9;
                 ++aColumn)
            {
                if (!mFlow.IsGridCellOccupied(aColumn, aRow))
                    continue;
                const auto aCell =
                    GameFlow::GetGridCellRect(aColumn, aRow);
                mPeashooterPlayer.AppendSprites(
                    {
                        static_cast<float>(aCell.mOrigin.mX),
                        static_cast<float>(aCell.mOrigin.mY),
                    },
                    {255, 255, 255, 255},
                    mReanimationSprites);
            }
        }
    }
    else if (mWhitePixel.IsValid())
    {
        for (std::uint8_t aRow = 0; aRow < 5; ++aRow)
        {
            for (std::uint8_t aColumn = 0;
                 aColumn < 9;
                 ++aColumn)
            {
                if (!mFlow.IsGridCellOccupied(aColumn, aRow))
                    continue;
                const auto aCell =
                    GameFlow::GetGridCellRect(aColumn, aRow);
                aDraws[aDrawCount++] = MakeSolidDraw(
                    mWhitePixel,
                    {
                        {
                            static_cast<float>(
                                aCell.mOrigin.mX + 9),
                            static_cast<float>(
                                aCell.mOrigin.mY + 9),
                        },
                        {
                            static_cast<float>(
                                aCell.mSize.mWidth - 18),
                            static_cast<float>(
                                aCell.mSize.mHeight - 18),
                        },
                    },
                    {80, 235, 70, 125});
            }
        }
    }

    for (const auto& aZombie : aCombatState.mZombies)
    {
        if (!aZombie.mActive)
            continue;
        const auto aCell =
            GameFlow::GetGridCellRect(0, aZombie.mRow);
        const auto aZombieX =
            static_cast<float>(
                aZombie.mXMilliPixels) /
            1'000.0F;
        const auto aZombieY =
            static_cast<float>(aCell.mOrigin.mY - 30);
        if (mZombiePlayer.IsBound())
        {
            mZombiePlayer.AppendSprites(
                {aZombieX, aZombieY},
                {255, 255, 255, 255},
                mReanimationSprites);
        }
        else if (mWhitePixel.IsValid())
        {
            aDraws[aDrawCount++] = MakeSolidDraw(
                mWhitePixel,
                {
                    {
                        aZombieX +
                            static_cast<float>(
                                LevelOneCombat::
                                    kNormalZombieRectX),
                        aZombieY,
                    },
                    {
                        static_cast<float>(
                            LevelOneCombat::
                                kNormalZombieRectWidth),
                        115.0F,
                    },
                },
                {120, 155, 95, 255});
        }
    }

    for (const auto& aSun : aCombatState.mSuns)
    {
        if (!aSun.mActive)
            continue;
        const auto aSunX =
            static_cast<float>(
                aSun.mXMilliPixels) /
            1'000.0F;
        const auto aSunY =
            static_cast<float>(
                aSun.mYMilliPixels) /
            1'000.0F;
        if (mSunPlayer.IsBound())
        {
            mSunPlayer.AppendSprites(
                {aSunX + 30.0F, aSunY + 30.0F},
                {255, 255, 255, 255},
                mReanimationSprites);
        }
        else if (mWhitePixel.IsValid())
        {
            aDraws[aDrawCount++] = MakeSolidDraw(
                mWhitePixel,
                {{aSunX, aSunY}, {60.0F, 60.0F}},
                {255, 225, 45, 230});
        }
    }

    if (aCombatState.mMowerPhase !=
        LevelOneMowerPhase::Spent)
    {
        const auto aMowerX = static_cast<float>(
            aCombatState.mMowerXMilliPixels) /
            1'000.0F;
        constexpr float kMowerY = 303.0F;
        if (mLawnMowerPlayer.IsBound())
        {
            mLawnMowerPlayer.AppendSprites(
                {aMowerX + 6.0F, kMowerY + 19.0F},
                {255, 255, 255, 255},
                mReanimationSprites);
        }
        else if (mWhitePixel.IsValid())
        {
            aDraws[aDrawCount++] = MakeSolidDraw(
                mWhitePixel,
                {{aMowerX, kMowerY}, {50.0F, 60.0F}},
                {190, 45, 35, 255});
        }
    }

    if (aCombatState.mAwardSpawned)
    {
        const auto anAwardX = static_cast<float>(
            aCombatState.mAwardXMilliPixels) /
            1'000.0F;
        const auto anAwardY = static_cast<float>(
            aCombatState.mAwardYMilliPixels) /
            1'000.0F;
        if (mSeedPacket.mImage.IsValid())
        {
            aDraws[aDrawCount++] = MakeImageDraw(
                mSeedPacket,
                {
                    {anAwardX - 25.0F, anAwardY - 35.0F},
                    {50.0F, 70.0F},
                },
                {255, 235, 115, 255});
        }
        else if (mWhitePixel.IsValid())
        {
            aDraws[aDrawCount++] = MakeSolidDraw(
                mWhitePixel,
                {
                    {anAwardX - 25.0F, anAwardY - 35.0F},
                    {50.0F, 70.0F},
                },
                {255, 215, 55, 255});
        }
    }

    if (!mReanimationSprites.empty())
        theFrame.SubmitSprites(mReanimationSprites);

    for (const auto& aProjectile :
         aCombatState.mProjectiles)
    {
        if (!aProjectile.mActive)
            continue;
        const auto aProjectileX =
            static_cast<float>(
                aProjectile.mXMilliPixels) /
            1'000.0F;
        const auto aProjectileY =
            static_cast<float>(
                aProjectile.mYMilliPixels) /
            1'000.0F;
        if (mProjectilePea.mImage.IsValid())
        {
            aDraws[aDrawCount++] = MakeImageDraw(
                mProjectilePea,
                {
                    {aProjectileX, aProjectileY},
                    {
                        static_cast<float>(
                            mProjectilePea.mSize.mWidth),
                        static_cast<float>(
                            mProjectilePea.mSize.mHeight),
                    },
                });
        }
        else if (mWhitePixel.IsValid())
        {
            aDraws[aDrawCount++] = MakeSolidDraw(
                mWhitePixel,
                {
                    {aProjectileX, aProjectileY},
                    {18.0F, 18.0F},
                },
                {90, 220, 65, 255});
        }
    }

    if (mWhitePixel.IsValid())
    {
        const auto aColumn = mFlow.GetGridColumn();
        const auto aRow = mFlow.GetGridRow();
        const auto aCell =
            GameFlow::GetGridCellRect(aColumn, aRow);
        if (aCell.mSize.mWidth > 0 && aCell.mSize.mHeight > 0)
        {
            constexpr float kLineWidth = 4.0F;
            const auto anX =
                static_cast<float>(aCell.mOrigin.mX);
            const auto aY =
                static_cast<float>(aCell.mOrigin.mY);
            const auto aWidth =
                static_cast<float>(aCell.mSize.mWidth);
            const auto aHeight =
                static_cast<float>(aCell.mSize.mHeight);
            engine::ColorRgba8 aSelectionColor{
                255, 225, 45, 230};
            if (mLevelOneBoard.IsPeashooterSelected())
            {
                if (mFlow.IsGridCellOccupied(aColumn, aRow))
                {
                    aSelectionColor = {255, 145, 35, 230};
                }
                else if (LevelOneBoard::IsPlantableCell(
                             aColumn,
                             aRow))
                {
                    aSelectionColor = {80, 235, 70, 230};
                }
                else
                {
                    aSelectionColor = {245, 75, 55, 230};
                }
            }
            aDraws[aDrawCount++] = MakeSolidDraw(
                mWhitePixel,
                {{anX, aY}, {aWidth, kLineWidth}},
                aSelectionColor);
            aDraws[aDrawCount++] = MakeSolidDraw(
                mWhitePixel,
                {
                    {anX, aY + aHeight - kLineWidth},
                    {aWidth, kLineWidth},
                },
                aSelectionColor);
            aDraws[aDrawCount++] = MakeSolidDraw(
                mWhitePixel,
                {{anX, aY}, {kLineWidth, aHeight}},
                aSelectionColor);
            aDraws[aDrawCount++] = MakeSolidDraw(
                mWhitePixel,
                {
                    {anX + aWidth - kLineWidth, aY},
                    {kLineWidth, aHeight},
                },
                aSelectionColor);
        }
    }

    if (aDrawCount > 0)
    {
        theFrame.SubmitSprites(
            std::span<const engine::SpriteDraw>(
                aDraws.data(),
                aDrawCount));
    }
}

} // namespace pvz::game
