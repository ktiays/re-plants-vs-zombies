#pragma once

#include "pvz/engine/Input.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace pvz::engine::core
{

class InputFrameAccumulator final : public IInputFrame
{
public:
    void OnKeyDown(KeyCode theKey, bool theRepeat);
    void OnKeyUp(KeyCode theKey);
    void OnPointerButtonDown(PointerButton theButton);
    void OnPointerButtonUp(PointerButton theButton);
    void SetPointerPosition(PointI thePosition);
    void AddWheelDelta(std::int64_t theDelta);
    void AppendText(char32_t theCodePoint);
    void ConsumeTransientEvents();

    [[nodiscard]] bool IsKeyDown(
        KeyCode theKey) const override;
    [[nodiscard]] bool WasKeyPressed(
        KeyCode theKey) const override;
    [[nodiscard]] bool IsPointerButtonDown(
        PointerButton theButton) const override;
    [[nodiscard]] bool WasPointerButtonPressed(
        PointerButton theButton) const override;
    [[nodiscard]] PointerState GetPointerState() const override;
    [[nodiscard]] std::span<const char32_t> GetTextInput()
        const override;

private:
    static constexpr auto kKeyCount =
        static_cast<std::size_t>(KeyCode::Count);
    static constexpr auto kPointerButtonCount =
        static_cast<std::size_t>(PointerButton::Count);

    std::array<bool, kKeyCount> mKeysDown{};
    std::array<bool, kKeyCount> mKeysPressed{};
    std::array<bool, kPointerButtonCount>
        mPointerButtonsDown{};
    std::array<bool, kPointerButtonCount>
        mPointerButtonsPressed{};
    PointerState mPointer;
    std::vector<char32_t> mTextInput;
};

} // namespace pvz::engine::core
