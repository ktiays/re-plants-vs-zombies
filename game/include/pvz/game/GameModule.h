#pragma once

#include "pvz/engine/Game.h"
#include "pvz/game/GameFlow.h"
#include "pvz/game/ReanimationPlayer.h"

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace pvz::game
{

class GameModule final : public engine::IGame
{
public:
    [[nodiscard]] engine::LifecycleResult Initialize(
        engine::IEngineServices& theServices) override;
    void Update(
        const engine::GameTick& theTick,
        const engine::IInputFrame& theInput) override;
    void Render(engine::IRenderFrame& theFrame) const override;
    [[nodiscard]] bool LoadState(engine::IStateReader& theReader) override;
    [[nodiscard]] bool SaveState(
        engine::IStateWriter& theWriter) const override;
    void Suspend() override;
    void Resume() override;
    void Shutdown() override;

    [[nodiscard]] bool IsInitialized() const;
    [[nodiscard]] bool IsSuspended() const;
    [[nodiscard]] engine::TickIndex GetLastTick() const;
    [[nodiscard]] std::uint64_t GetUpdateCount() const;
    [[nodiscard]] GameScene GetScene() const;
    [[nodiscard]] GameFlowState GetFlowState() const;
    [[nodiscard]] std::uint64_t GetReanimationTick() const;

private:
    void RebuildUiText();
    void AppendCenteredText(
        std::u32string_view theText,
        float theBaseline,
        engine::ColorRgba8 theColor);
    void HandleSceneChange(
        GameScene thePreviousScene,
        GameScene theCurrentScene);
    [[nodiscard]] bool PlayMusicAt(std::uint32_t theOrder);
    void SynchronizeMusic();
    void RenderTitle(engine::IRenderFrame& theFrame) const;
    void RenderMenu(
        engine::IRenderFrame& theFrame,
        bool theStartingAdventure) const;
    void RenderAdventureDay(engine::IRenderFrame& theFrame) const;

    engine::IEngineServices* mServices{};
    GameFlow mFlow;
    engine::ImageResource mTitleScreen;
    engine::ImageResource mTitleLogo;
    engine::ImageResource mDayBackground;
    engine::ImageResource mSeedBank;
    std::array<engine::ImageResource, 4> mMenuButtons;
    std::array<engine::ImageResource, 4> mMenuButtonHighlights;
    engine::ImageHandle mWhitePixel;
    engine::FontResource mUiFont;
    engine::SoundResource mLoadingSound;
    engine::VoiceHandle mLoadingVoice;
    engine::MusicResource mTitleMusic;
    ReanimationClip mPeashooterClip;
    ReanimationPlayer mPeashooterPlayer;
    std::vector<engine::SpriteDraw> mUiTextSprites;
    mutable std::vector<engine::SpriteDraw> mReanimationSprites;
    engine::TickIndex mLastTick{};
    std::uint64_t mUpdateCount{};
    bool mInitialized{};
    bool mSuspended{};
};

} // namespace pvz::game
