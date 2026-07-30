#pragma once

#include "pvz/engine/Game.h"

#include <cstdint>
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

private:
    engine::IEngineServices* mServices{};
    engine::ImageResource mTitleScreen;
    engine::ImageResource mTitleLogo;
    engine::FontResource mLoadingFont;
    engine::SoundResource mLoadingSound;
    engine::VoiceHandle mLoadingVoice;
    std::vector<engine::SpriteDraw> mLoadingTextSprites;
    engine::TickIndex mLastTick{};
    std::uint64_t mUpdateCount{};
    bool mInitialized{};
    bool mSuspended{};
};

} // namespace pvz::game
