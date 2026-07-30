#include "pvz/engine/core/InputFrameAccumulator.h"

#include <cstdint>
#include <limits>

extern void Expect(bool theCondition, const char* theMessage);

namespace
{

void TestPersistentAndTransientInput()
{
    pvz::engine::core::InputFrameAccumulator anInput;
    anInput.OnKeyDown(pvz::engine::KeyCode::Enter, false);
    anInput.OnPointerButtonDown(
        pvz::engine::PointerButton::Primary);
    anInput.SetPointerPosition({120, -8});
    anInput.AddWheelDelta(120);
    anInput.AppendText(U'P');

    Expect(
        anInput.IsKeyDown(pvz::engine::KeyCode::Enter) &&
            anInput.WasKeyPressed(
                pvz::engine::KeyCode::Enter),
        "input accumulator records persistent and pressed key state");
    Expect(
        anInput.IsPointerButtonDown(
            pvz::engine::PointerButton::Primary) &&
            anInput.WasPointerButtonPressed(
                pvz::engine::PointerButton::Primary),
        "input accumulator records persistent and pressed pointer state");
    Expect(
        anInput.GetPointerState().mPosition.mX == 120 &&
            anInput.GetPointerState().mPosition.mY == -8 &&
            anInput.GetPointerState().mWheelDelta == 120 &&
            anInput.GetTextInput().size() == 1 &&
            anInput.GetTextInput()[0] == U'P',
        "input accumulator records pointer, wheel, and text");

    anInput.ConsumeTransientEvents();
    Expect(
        anInput.IsKeyDown(pvz::engine::KeyCode::Enter) &&
            !anInput.WasKeyPressed(
                pvz::engine::KeyCode::Enter) &&
            anInput.IsPointerButtonDown(
                pvz::engine::PointerButton::Primary) &&
            !anInput.WasPointerButtonPressed(
                pvz::engine::PointerButton::Primary) &&
            anInput.GetPointerState().mWheelDelta == 0 &&
            anInput.GetTextInput().empty(),
        "consuming transients preserves only persistent state");

    anInput.OnKeyDown(pvz::engine::KeyCode::Enter, true);
    Expect(
        !anInput.WasKeyPressed(pvz::engine::KeyCode::Enter),
        "repeated key-down does not create a pressed edge");
    anInput.OnKeyUp(pvz::engine::KeyCode::Enter);
    anInput.OnPointerButtonUp(
        pvz::engine::PointerButton::Primary);
    Expect(
        !anInput.IsKeyDown(pvz::engine::KeyCode::Enter) &&
            !anInput.IsPointerButtonDown(
                pvz::engine::PointerButton::Primary),
        "input accumulator records key and pointer releases");
}

void TestWheelSaturationAndInvalidEnums()
{
    pvz::engine::core::InputFrameAccumulator anInput;
    anInput.AddWheelDelta(
        std::numeric_limits<std::int64_t>::max());
    Expect(
        anInput.GetPointerState().mWheelDelta ==
            std::numeric_limits<std::int32_t>::max(),
        "positive wheel accumulation saturates");
    anInput.ConsumeTransientEvents();
    anInput.AddWheelDelta(
        std::numeric_limits<std::int64_t>::min());
    Expect(
        anInput.GetPointerState().mWheelDelta ==
            std::numeric_limits<std::int32_t>::min(),
        "negative wheel accumulation saturates");

    anInput.OnKeyDown(pvz::engine::KeyCode::Count, false);
    anInput.OnPointerButtonDown(
        pvz::engine::PointerButton::Count);
    Expect(
        !anInput.IsKeyDown(pvz::engine::KeyCode::Count) &&
            !anInput.IsPointerButtonDown(
                pvz::engine::PointerButton::Count),
        "out-of-range input enums are ignored");
}

} // namespace

void RunInputFrameAccumulatorTests()
{
    TestPersistentAndTransientInput();
    TestWheelSaturationAndInvalidEnums();
}
