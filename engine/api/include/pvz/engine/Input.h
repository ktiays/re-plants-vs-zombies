#pragma once

#include "pvz/engine/Types.h"

#include <cstdint>
#include <span>

namespace pvz::engine
{

enum class KeyCode : std::uint16_t
{
    Unknown,
    Backspace,
    Enter,
    Escape,
    Space,
    DeleteKey,
    ArrowLeft,
    ArrowRight,
    ArrowUp,
    ArrowDown,
    Count,
};

enum class PointerButton : std::uint8_t
{
    Primary,
    Secondary,
    Middle,
    Count,
};

struct PointerState
{
    PointI mPosition{};
    std::int32_t mWheelDelta{};
};

class IInputFrame
{
public:
    virtual ~IInputFrame() = default;

    [[nodiscard]] virtual bool IsKeyDown(KeyCode theKey) const = 0;
    [[nodiscard]] virtual bool WasKeyPressed(KeyCode theKey) const = 0;
    [[nodiscard]] virtual bool IsPointerButtonDown(
        PointerButton theButton) const = 0;
    [[nodiscard]] virtual bool WasPointerButtonPressed(
        PointerButton theButton) const = 0;
    [[nodiscard]] virtual PointerState GetPointerState() const = 0;
    [[nodiscard]] virtual std::span<const char32_t> GetTextInput() const = 0;
};

static_assert(sizeof(char32_t) == 4);

} // namespace pvz::engine
