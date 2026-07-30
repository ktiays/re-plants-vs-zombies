#include "pvz/engine/core/InputFrameAccumulator.h"

#include <algorithm>
#include <cstddef>
#include <limits>

namespace pvz::engine::core
{
namespace
{

[[nodiscard]] std::size_t GetKeyIndex(KeyCode theKey)
{
    return static_cast<std::size_t>(theKey);
}

[[nodiscard]] std::size_t GetPointerButtonIndex(
    PointerButton theButton)
{
    return static_cast<std::size_t>(theButton);
}

} // namespace

void InputFrameAccumulator::OnKeyDown(
    KeyCode theKey,
    bool theRepeat)
{
    const auto anIndex = GetKeyIndex(theKey);
    if (anIndex >= mKeysDown.size())
        return;
    mKeysDown[anIndex] = true;
    if (!theRepeat)
        mKeysPressed[anIndex] = true;
}

void InputFrameAccumulator::OnKeyUp(KeyCode theKey)
{
    const auto anIndex = GetKeyIndex(theKey);
    if (anIndex < mKeysDown.size())
        mKeysDown[anIndex] = false;
}

void InputFrameAccumulator::OnPointerButtonDown(
    PointerButton theButton)
{
    const auto anIndex = GetPointerButtonIndex(theButton);
    if (anIndex >= mPointerButtonsDown.size())
        return;
    mPointerButtonsDown[anIndex] = true;
    mPointerButtonsPressed[anIndex] = true;
}

void InputFrameAccumulator::OnPointerButtonUp(
    PointerButton theButton)
{
    const auto anIndex = GetPointerButtonIndex(theButton);
    if (anIndex < mPointerButtonsDown.size())
        mPointerButtonsDown[anIndex] = false;
}

void InputFrameAccumulator::SetPointerPosition(
    PointI thePosition)
{
    mPointer.mPosition = thePosition;
}

void InputFrameAccumulator::AddWheelDelta(
    std::int64_t theDelta)
{
    const auto aValue =
        static_cast<std::int64_t>(mPointer.mWheelDelta) +
        theDelta;
    mPointer.mWheelDelta = static_cast<std::int32_t>(
        std::clamp(
            aValue,
            static_cast<std::int64_t>(
                std::numeric_limits<std::int32_t>::min()),
            static_cast<std::int64_t>(
                std::numeric_limits<std::int32_t>::max())));
}

void InputFrameAccumulator::AppendText(char32_t theCodePoint)
{
    mTextInput.push_back(theCodePoint);
}

void InputFrameAccumulator::ConsumeTransientEvents()
{
    mKeysPressed.fill(false);
    mPointerButtonsPressed.fill(false);
    mPointer.mWheelDelta = 0;
    mTextInput.clear();
}

bool InputFrameAccumulator::IsKeyDown(KeyCode theKey) const
{
    const auto anIndex = GetKeyIndex(theKey);
    return anIndex < mKeysDown.size() && mKeysDown[anIndex];
}

bool InputFrameAccumulator::WasKeyPressed(
    KeyCode theKey) const
{
    const auto anIndex = GetKeyIndex(theKey);
    return
        anIndex < mKeysPressed.size() &&
        mKeysPressed[anIndex];
}

bool InputFrameAccumulator::IsPointerButtonDown(
    PointerButton theButton) const
{
    const auto anIndex = GetPointerButtonIndex(theButton);
    return
        anIndex < mPointerButtonsDown.size() &&
        mPointerButtonsDown[anIndex];
}

bool InputFrameAccumulator::WasPointerButtonPressed(
    PointerButton theButton) const
{
    const auto anIndex = GetPointerButtonIndex(theButton);
    return
        anIndex < mPointerButtonsPressed.size() &&
        mPointerButtonsPressed[anIndex];
}

PointerState InputFrameAccumulator::GetPointerState() const
{
    return mPointer;
}

std::span<const char32_t>
InputFrameAccumulator::GetTextInput() const
{
    return mTextInput;
}

} // namespace pvz::engine::core
