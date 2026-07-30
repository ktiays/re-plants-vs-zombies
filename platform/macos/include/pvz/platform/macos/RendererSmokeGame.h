#pragma once

#include "pvz/engine/Game.h"

namespace pvz::platform::macos
{

class RendererSmokeGame final : public engine::IGame
{
public:
    [[nodiscard]] engine::LifecycleResult Initialize(
        engine::IEngineServices& theServices) override;
    void Update(
        const engine::GameTick& theTick,
        const engine::IInputFrame& theInput) override;
    void Render(engine::IRenderFrame& theFrame) const override;
    [[nodiscard]] bool LoadState(
        engine::IStateReader& theReader) override;
    [[nodiscard]] bool SaveState(
        engine::IStateWriter& theWriter) const override;
    void Suspend() override;
    void Resume() override;
    void Shutdown() override;

private:
    engine::IEngineServices* mServices{};
    engine::ImageHandle mCheckerboard{};
    float mRotationRadians{};
    bool mSuspended{};
};

} // namespace pvz::platform::macos
