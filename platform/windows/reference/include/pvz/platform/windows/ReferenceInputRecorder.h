#pragma once

#include "pvz/engine/core/InputFrameAccumulator.h"
#include "pvz/engine/core/InputReplay.h"
#include "pvz/platform/windows/LegacyInputCapture.h"

#include <cstdint>

namespace pvz::platform::windows
{

class ReferenceInputRecorder
{
public:
    void RecordPointerPosition(
        std::int32_t theX,
        std::int32_t theY);
    void RecordPointerButtonDown(
        ReferencePointerButton theButton);
    void RecordPointerButtonUp(
        ReferencePointerButton theButton);
    void RecordMouseWheel(std::int32_t theStepDelta);
    void RecordVirtualKeyDown(
        std::uint32_t theVirtualKey,
        bool theRepeat);
    void RecordVirtualKeyUp(std::uint32_t theVirtualKey);
    void RecordText(std::uint32_t theCodePoint);

    [[nodiscard]] bool CaptureTick();
    [[nodiscard]] bool Save(
        engine::IStateWriter& theWriter);

    [[nodiscard]] const engine::core::InputReplay&
        GetReplay() const;
    [[nodiscard]] engine::core::InputReplayError
        GetError() const;

private:
    engine::core::InputFrameAccumulator mInput;
    engine::core::InputReplay mReplay;
    engine::core::InputReplayError mError{
        engine::core::InputReplayError::None};
};

} // namespace pvz::platform::windows
