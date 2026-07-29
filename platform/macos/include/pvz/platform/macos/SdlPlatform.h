#pragma once

#include "pvz/engine/Runtime.h"

#include <memory>
#include <string>
#include <string_view>

namespace pvz::platform::macos
{

class SdlPlatform final
    : public engine::IMonotonicClock,
      public engine::IPlatformEventLoop,
      public engine::IInputSystem
{
public:
    SdlPlatform();
    ~SdlPlatform() override;

    SdlPlatform(const SdlPlatform&) = delete;
    SdlPlatform& operator=(const SdlPlatform&) = delete;

    [[nodiscard]] bool Initialize(
        std::string_view theTitle,
        std::uint32_t theWidth,
        std::uint32_t theHeight);
    [[nodiscard]] void* GetMetalLayer() const;
    [[nodiscard]] std::string_view GetLastError() const;

    [[nodiscard]] engine::MonotonicTimeMicroseconds
    GetTime() const override;

    void PumpEvents() override;
    [[nodiscard]] bool IsQuitRequested() const override;
    [[nodiscard]] bool IsSuspended() const override;
    void WaitUntil(
        engine::MonotonicTimeMicroseconds theDeadline) override;

    [[nodiscard]] const engine::IInputFrame&
    GetFrame() const override;
    void ConsumeTransientEvents() override;

private:
    struct Implementation;
    std::unique_ptr<Implementation> mImplementation;
};

} // namespace pvz::platform::macos
