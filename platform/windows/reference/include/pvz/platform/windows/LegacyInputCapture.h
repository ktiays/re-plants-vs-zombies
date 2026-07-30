#pragma once

#include <cstdint>
#include <string_view>

namespace pvz::platform::windows
{

enum class ReferencePointerButton : std::uint8_t
{
    Primary,
    Secondary,
    Middle,
};

[[nodiscard]] bool ConfigureLegacyInputCapture(
    std::string_view theOutputPath);
[[nodiscard]] bool WasLegacyInputCaptureRequested();
[[nodiscard]] bool IsLegacyInputCaptureEnabled();

void RecordLegacyPointerPosition(
    std::int32_t theX,
    std::int32_t theY);
void RecordLegacyPointerButtonDown(
    ReferencePointerButton theButton);
void RecordLegacyPointerButtonUp(
    ReferencePointerButton theButton);
void RecordLegacyMouseWheel(std::int32_t theStepDelta);
void RecordLegacyVirtualKeyDown(
    std::uint32_t theVirtualKey,
    bool theRepeat);
void RecordLegacyVirtualKeyUp(std::uint32_t theVirtualKey);
void RecordLegacyText(std::uint32_t theCodePoint);
void CaptureLegacyInputTick();

[[nodiscard]] bool FinalizeLegacyInputCapture();
[[nodiscard]] std::string_view GetLegacyInputCaptureError();
[[nodiscard]] std::uint64_t GetLegacyInputCaptureFrameCount();

static_assert(sizeof(ReferencePointerButton) == 1);

} // namespace pvz::platform::windows
