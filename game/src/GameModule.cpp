#include "pvz/game/GameModule.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace pvz::game
{
namespace
{

inline constexpr std::uint32_t kStateMagic = 0x475A5650;
inline constexpr std::uint16_t kLegacyStateVersion = 1;
inline constexpr std::uint16_t kFlowStateVersion = 2;
inline constexpr std::uint16_t kStateVersion = 3;
inline constexpr std::uint32_t kTitleMusicOrder = 0x98;
inline constexpr std::uint32_t kAdventureMusicOrder = 0;
inline constexpr float kLogicalWidth = 800.0F;
inline constexpr float kLogicalHeight = 600.0F;

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

} // namespace

engine::LifecycleResult GameModule::Initialize(
    engine::IEngineServices& theServices)
{
    if (mInitialized)
        return engine::LifecycleResult::Failure;

    mServices = &theServices;
    mFlow.Reset();
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
    const bool hadNotice = mFlow.GetNoticeTicks() > 0;
    mFlow.Update(theInput);
    mLastTick = theTick.mIndex;
    ++mUpdateCount;
    if (mFlow.GetScene() == GameScene::AdventureDay)
        mPeashooterPlayer.Update();

    if (aPreviousScene != mFlow.GetScene())
        HandleSceneChange(aPreviousScene, mFlow.GetScene());

    const bool hasNotice = mFlow.GetNoticeTicks() > 0;
    if (aPreviousScene != mFlow.GetScene() ||
        aPreviousMenuItem != mFlow.GetSelectedMenuItem() ||
        hadNotice != hasNotice)
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
         aVersion != kStateVersion))
    {
        return false;
    }

    GameFlow aFlow;
    if (aVersion == kFlowStateVersion ||
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
            !theReader.ReadU64(aState.mOccupiedCells))
        {
            return false;
        }
        aState.mScene = static_cast<GameScene>(aScene);
        aState.mMenuItem = static_cast<MainMenuItem>(aMenuItem);
        if (!aFlow.RestoreState(aState))
            return false;
    }
    if (aVersion == kStateVersion &&
        !theReader.ReadU64(aReanimationTick))
    {
        return false;
    }

    mLastTick = aLastTick;
    mUpdateCount = anUpdateCount;
    mSuspended = aSuspended;
    mFlow = aFlow;
    mPeashooterPlayer.RestoreTick(aReanimationTick);
    RebuildUiText();
    SynchronizeMusic();
    return true;
}

bool GameModule::SaveState(engine::IStateWriter& theWriter) const
{
    if (!mInitialized)
        return false;

    const auto aState = mFlow.GetState();
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
        theWriter.WriteU64(mPeashooterPlayer.GetTick());
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
    mUiFont = {};
    mLoadingVoice = {};
    mLoadingSound = {};
    mTitleMusic = {};
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
        anObservation.mScene = BehaviorScene::AdventureIntro;
        break;
    case GameScene::AdventureDay:
        anObservation.mScene = BehaviorScene::AdventurePlaying;
        anObservation.mBoardStage = BehaviorBoardStage::Day;
        anObservation.mGridColumn = aFlowState.mGridColumn;
        anObservation.mGridRow = aFlowState.mGridRow;
        anObservation.mOccupiedCells =
            aFlowState.mOccupiedCells;
        anObservation.mPlantCount =
            static_cast<std::uint32_t>(
                std::popcount(aFlowState.mOccupiedCells));
        break;
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
    case GameScene::AdventureDay:
        AppendCenteredText(
            U"CLICK OR ENTER TO PLACE - ESC RETURNS TO MENU",
            585.0F,
            {255, 255, 255, 255});
        break;
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
    case GameScene::AdventureDay:
        static_cast<void>(PlayMusicAt(kAdventureMusicOrder));
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
            mFlow.GetScene() == GameScene::AdventureDay
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
                {0.0F, 0.0F},
                {
                    static_cast<float>(mSeedBank.mSize.mWidth),
                    static_cast<float>(mSeedBank.mSize.mHeight),
                },
            });
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
        if (!mReanimationSprites.empty())
            theFrame.SubmitSprites(mReanimationSprites);
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
            const engine::ColorRgba8 aSelectionColor{
                255, 225, 45, 230};
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
