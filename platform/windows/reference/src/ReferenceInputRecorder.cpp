#include "pvz/platform/windows/ReferenceInputRecorder.h"

#include <cstdint>

namespace pvz::platform::windows
{
namespace
{

[[nodiscard]] engine::KeyCode MapVirtualKey(
    std::uint32_t theVirtualKey)
{
    switch (theVirtualKey)
    {
    case 0x08:
        return engine::KeyCode::Backspace;
    case 0x0D:
        return engine::KeyCode::Enter;
    case 0x1B:
        return engine::KeyCode::Escape;
    case 0x20:
        return engine::KeyCode::Space;
    case 0x25:
        return engine::KeyCode::ArrowLeft;
    case 0x26:
        return engine::KeyCode::ArrowUp;
    case 0x27:
        return engine::KeyCode::ArrowRight;
    case 0x28:
        return engine::KeyCode::ArrowDown;
    case 0x2E:
        return engine::KeyCode::DeleteKey;
    default:
        return engine::KeyCode::Unknown;
    }
}

[[nodiscard]] engine::PointerButton MapPointerButton(
    ReferencePointerButton theButton)
{
    switch (theButton)
    {
    case ReferencePointerButton::Primary:
        return engine::PointerButton::Primary;
    case ReferencePointerButton::Secondary:
        return engine::PointerButton::Secondary;
    case ReferencePointerButton::Middle:
        return engine::PointerButton::Middle;
    }
    return engine::PointerButton::Count;
}

[[nodiscard]] bool IsValidCodePoint(std::uint32_t theCodePoint)
{
    return
        theCodePoint <= 0x10FFFF &&
        !(theCodePoint >= 0xD800 &&
          theCodePoint <= 0xDFFF);
}

} // namespace

void ReferenceInputRecorder::RecordPointerPosition(
    std::int32_t theX,
    std::int32_t theY)
{
    mInput.SetPointerPosition({theX, theY});
}

void ReferenceInputRecorder::RecordPointerButtonDown(
    ReferencePointerButton theButton)
{
    mInput.OnPointerButtonDown(MapPointerButton(theButton));
}

void ReferenceInputRecorder::RecordPointerButtonUp(
    ReferencePointerButton theButton)
{
    mInput.OnPointerButtonUp(MapPointerButton(theButton));
}

void ReferenceInputRecorder::RecordMouseWheel(
    std::int32_t theStepDelta)
{
    mInput.AddWheelDelta(
        static_cast<std::int64_t>(theStepDelta) * 120);
}

void ReferenceInputRecorder::RecordVirtualKeyDown(
    std::uint32_t theVirtualKey,
    bool theRepeat)
{
    mInput.OnKeyDown(
        MapVirtualKey(theVirtualKey),
        theRepeat);
}

void ReferenceInputRecorder::RecordVirtualKeyUp(
    std::uint32_t theVirtualKey)
{
    mInput.OnKeyUp(MapVirtualKey(theVirtualKey));
}

void ReferenceInputRecorder::RecordText(
    std::uint32_t theCodePoint)
{
    if (IsValidCodePoint(theCodePoint))
    {
        mInput.AppendText(
            static_cast<char32_t>(theCodePoint));
    }
}

bool ReferenceInputRecorder::CaptureTick()
{
    if (mError != engine::core::InputReplayError::None)
        return false;

    const auto aTick = static_cast<engine::TickIndex>(
        mReplay.GetFrames().size());
    const bool hasCaptured =
        mReplay.AppendFrame(aTick, mInput, mError);
    mInput.ConsumeTransientEvents();
    return hasCaptured;
}

bool ReferenceInputRecorder::Save(
    engine::IStateWriter& theWriter)
{
    if (mError != engine::core::InputReplayError::None)
        return false;
    return mReplay.Save(theWriter, mError);
}

const engine::core::InputReplay&
ReferenceInputRecorder::GetReplay() const
{
    return mReplay;
}

engine::core::InputReplayError
ReferenceInputRecorder::GetError() const
{
    return mError;
}

} // namespace pvz::platform::windows
