#include "pvz/engine/core/InputReplay.h"

#include <limits>
#include <utility>

namespace pvz::engine::core
{
namespace
{

inline constexpr std::uint32_t kReplayMagic = 0x525A5650;
inline constexpr std::uint16_t kReplayVersion = 1;
inline constexpr std::uint32_t kMaximumFrameCount = 1'000'000;
inline constexpr std::uint16_t kMaximumTextLength = 4'096;

static_assert(
    static_cast<std::uint16_t>(KeyCode::Count) <= 16);
static_assert(
    static_cast<std::uint8_t>(PointerButton::Count) <= 8);

[[nodiscard]] std::uint16_t GetKeyMask(KeyCode theKey)
{
    const auto anIndex = static_cast<std::uint16_t>(theKey);
    if (anIndex >= static_cast<std::uint16_t>(KeyCode::Count))
        return 0;
    return static_cast<std::uint16_t>(
        std::uint16_t{1} << anIndex);
}

[[nodiscard]] std::uint8_t GetPointerButtonMask(
    PointerButton theButton)
{
    const auto anIndex = static_cast<std::uint8_t>(theButton);
    if (anIndex >=
        static_cast<std::uint8_t>(PointerButton::Count))
    {
        return 0;
    }
    return static_cast<std::uint8_t>(
        std::uint8_t{1} << anIndex);
}

[[nodiscard]] std::uint16_t GetValidKeyMask()
{
    const auto aCount =
        static_cast<std::uint16_t>(KeyCode::Count);
    return static_cast<std::uint16_t>(
        (std::uint16_t{1} << aCount) - 1U);
}

[[nodiscard]] std::uint8_t GetValidPointerButtonMask()
{
    const auto aCount =
        static_cast<std::uint8_t>(PointerButton::Count);
    return static_cast<std::uint8_t>(
        (std::uint8_t{1} << aCount) - 1U);
}

[[nodiscard]] bool IsValidCodePoint(char32_t theCodePoint)
{
    return
        theCodePoint <= 0x10FFFF &&
        !(theCodePoint >= 0xD800 &&
          theCodePoint <= 0xDFFF);
}

[[nodiscard]] bool ValidateFrame(
    const RecordedInputFrame& theFrame,
    TickIndex theExpectedTick,
    InputReplayError& theError)
{
    if (theFrame.mTick != theExpectedTick)
    {
        theError = InputReplayError::NonSequentialTick;
        return false;
    }

    const auto aValidKeyMask = GetValidKeyMask();
    if ((theFrame.mKeysDown & ~aValidKeyMask) != 0 ||
        (theFrame.mKeysPressed & ~aValidKeyMask) != 0)
    {
        theError = InputReplayError::InvalidKeyMask;
        return false;
    }

    const auto aValidPointerMask =
        GetValidPointerButtonMask();
    if ((theFrame.mPointerButtonsDown &
         static_cast<std::uint8_t>(~aValidPointerMask)) != 0 ||
        (theFrame.mPointerButtonsPressed &
         static_cast<std::uint8_t>(~aValidPointerMask)) != 0)
    {
        theError = InputReplayError::InvalidPointerMask;
        return false;
    }

    if (theFrame.mTextInput.size() > kMaximumTextLength)
    {
        theError = InputReplayError::TooMuchText;
        return false;
    }
    for (const auto aCodePoint : theFrame.mTextInput)
    {
        if (!IsValidCodePoint(aCodePoint))
        {
            theError = InputReplayError::InvalidText;
            return false;
        }
    }
    theError = InputReplayError::None;
    return true;
}

void SetMaskBit(
    std::uint16_t& theMask,
    std::uint16_t theBit,
    bool theValue)
{
    if (theBit == 0)
        return;
    if (theValue)
        theMask = static_cast<std::uint16_t>(theMask | theBit);
    else
        theMask = static_cast<std::uint16_t>(theMask & ~theBit);
}

void SetMaskBit(
    std::uint8_t& theMask,
    std::uint8_t theBit,
    bool theValue)
{
    if (theBit == 0)
        return;
    if (theValue)
        theMask = static_cast<std::uint8_t>(theMask | theBit);
    else
        theMask = static_cast<std::uint8_t>(theMask & ~theBit);
}

} // namespace

void RecordedInputFrame::SetKeyDown(
    KeyCode theKey,
    bool theDown)
{
    SetMaskBit(mKeysDown, GetKeyMask(theKey), theDown);
}

void RecordedInputFrame::SetKeyPressed(
    KeyCode theKey,
    bool thePressed)
{
    SetMaskBit(mKeysPressed, GetKeyMask(theKey), thePressed);
}

void RecordedInputFrame::SetPointerButtonDown(
    PointerButton theButton,
    bool theDown)
{
    SetMaskBit(
        mPointerButtonsDown,
        GetPointerButtonMask(theButton),
        theDown);
}

void RecordedInputFrame::SetPointerButtonPressed(
    PointerButton theButton,
    bool thePressed)
{
    SetMaskBit(
        mPointerButtonsPressed,
        GetPointerButtonMask(theButton),
        thePressed);
}

ReplayInputFrame::ReplayInputFrame(
    const RecordedInputFrame& theFrame)
    : mFrame(theFrame)
{
}

bool ReplayInputFrame::IsKeyDown(KeyCode theKey) const
{
    const auto aMask = GetKeyMask(theKey);
    return aMask != 0 && (mFrame.mKeysDown & aMask) != 0;
}

bool ReplayInputFrame::WasKeyPressed(KeyCode theKey) const
{
    const auto aMask = GetKeyMask(theKey);
    return aMask != 0 && (mFrame.mKeysPressed & aMask) != 0;
}

bool ReplayInputFrame::IsPointerButtonDown(
    PointerButton theButton) const
{
    const auto aMask = GetPointerButtonMask(theButton);
    return
        aMask != 0 &&
        (mFrame.mPointerButtonsDown & aMask) != 0;
}

bool ReplayInputFrame::WasPointerButtonPressed(
    PointerButton theButton) const
{
    const auto aMask = GetPointerButtonMask(theButton);
    return
        aMask != 0 &&
        (mFrame.mPointerButtonsPressed & aMask) != 0;
}

PointerState ReplayInputFrame::GetPointerState() const
{
    return mFrame.mPointer;
}

std::span<const char32_t> ReplayInputFrame::GetTextInput()
    const
{
    return mFrame.mTextInput;
}

bool InputReplay::AppendFrame(
    TickIndex theTick,
    const IInputFrame& theInput,
    InputReplayError& theError)
{
    RecordedInputFrame aFrame;
    aFrame.mTick = theTick;
    for (std::uint16_t anIndex = 0;
         anIndex <
            static_cast<std::uint16_t>(KeyCode::Count);
         ++anIndex)
    {
        const auto aKey = static_cast<KeyCode>(anIndex);
        aFrame.SetKeyDown(
            aKey,
            theInput.IsKeyDown(aKey));
        aFrame.SetKeyPressed(
            aKey,
            theInput.WasKeyPressed(aKey));
    }
    for (std::uint8_t anIndex = 0;
         anIndex <
            static_cast<std::uint8_t>(
                PointerButton::Count);
         ++anIndex)
    {
        const auto aButton =
            static_cast<PointerButton>(anIndex);
        aFrame.SetPointerButtonDown(
            aButton,
            theInput.IsPointerButtonDown(aButton));
        aFrame.SetPointerButtonPressed(
            aButton,
            theInput.WasPointerButtonPressed(aButton));
    }
    aFrame.mPointer = theInput.GetPointerState();
    const auto aTextInput = theInput.GetTextInput();
    aFrame.mTextInput.assign(
        aTextInput.begin(),
        aTextInput.end());
    return AppendFrame(std::move(aFrame), theError);
}

bool InputReplay::AppendFrame(
    RecordedInputFrame theFrame,
    InputReplayError& theError)
{
    if (mFrames.size() >= kMaximumFrameCount)
    {
        theError = InputReplayError::TooManyFrames;
        return false;
    }
    if (!ValidateFrame(
            theFrame,
            static_cast<TickIndex>(mFrames.size()),
            theError))
    {
        return false;
    }
    mFrames.push_back(std::move(theFrame));
    return true;
}

void InputReplay::Clear()
{
    mFrames.clear();
}

bool InputReplay::Save(
    IStateWriter& theWriter,
    InputReplayError& theError) const
{
    if (mFrames.size() >
        std::numeric_limits<std::uint32_t>::max())
    {
        theError = InputReplayError::TooManyFrames;
        return false;
    }

    if (!theWriter.WriteU32(kReplayMagic) ||
        !theWriter.WriteU16(kReplayVersion) ||
        !theWriter.WriteU32(kSimulationFrequencyHz) ||
        !theWriter.WriteU32(
            static_cast<std::uint32_t>(mFrames.size())))
    {
        theError = InputReplayError::IoError;
        return false;
    }

    for (std::size_t anIndex = 0;
         anIndex < mFrames.size();
         ++anIndex)
    {
        const auto& aFrame = mFrames[anIndex];
        if (!ValidateFrame(
                aFrame,
                static_cast<TickIndex>(anIndex),
                theError))
        {
            return false;
        }

        if (!theWriter.WriteU64(aFrame.mTick) ||
            !theWriter.WriteU16(aFrame.mKeysDown) ||
            !theWriter.WriteU16(aFrame.mKeysPressed) ||
            !theWriter.WriteU8(aFrame.mPointerButtonsDown) ||
            !theWriter.WriteU8(aFrame.mPointerButtonsPressed) ||
            !theWriter.WriteI32(aFrame.mPointer.mPosition.mX) ||
            !theWriter.WriteI32(aFrame.mPointer.mPosition.mY) ||
            !theWriter.WriteI32(aFrame.mPointer.mWheelDelta) ||
            !theWriter.WriteU16(
                static_cast<std::uint16_t>(
                    aFrame.mTextInput.size())))
        {
            theError = InputReplayError::IoError;
            return false;
        }
        for (const auto aCodePoint : aFrame.mTextInput)
        {
            if (!theWriter.WriteU32(
                    static_cast<std::uint32_t>(aCodePoint)))
            {
                theError = InputReplayError::IoError;
                return false;
            }
        }
    }
    theError = InputReplayError::None;
    return true;
}

bool InputReplay::Load(
    IStateReader& theReader,
    InputReplayError& theError)
{
    std::uint32_t aMagic{};
    std::uint16_t aVersion{};
    std::uint32_t aTickFrequency{};
    std::uint32_t aFrameCount{};
    if (!theReader.ReadU32(aMagic) ||
        !theReader.ReadU16(aVersion) ||
        !theReader.ReadU32(aTickFrequency) ||
        !theReader.ReadU32(aFrameCount))
    {
        theError = InputReplayError::IoError;
        return false;
    }
    if (aMagic != kReplayMagic)
    {
        theError = InputReplayError::InvalidMagic;
        return false;
    }
    if (aVersion != kReplayVersion)
    {
        theError = InputReplayError::UnsupportedVersion;
        return false;
    }
    if (aTickFrequency != kSimulationFrequencyHz)
    {
        theError = InputReplayError::InvalidTickFrequency;
        return false;
    }
    if (aFrameCount > kMaximumFrameCount)
    {
        theError = InputReplayError::TooManyFrames;
        return false;
    }

    std::vector<RecordedInputFrame> aFrames;
    aFrames.reserve(aFrameCount);
    for (std::uint32_t anIndex = 0;
         anIndex < aFrameCount;
         ++anIndex)
    {
        RecordedInputFrame aFrame;
        std::uint16_t aTextLength{};
        if (!theReader.ReadU64(aFrame.mTick) ||
            !theReader.ReadU16(aFrame.mKeysDown) ||
            !theReader.ReadU16(aFrame.mKeysPressed) ||
            !theReader.ReadU8(aFrame.mPointerButtonsDown) ||
            !theReader.ReadU8(
                aFrame.mPointerButtonsPressed) ||
            !theReader.ReadI32(aFrame.mPointer.mPosition.mX) ||
            !theReader.ReadI32(aFrame.mPointer.mPosition.mY) ||
            !theReader.ReadI32(aFrame.mPointer.mWheelDelta) ||
            !theReader.ReadU16(aTextLength))
        {
            theError = InputReplayError::IoError;
            return false;
        }
        if (aTextLength > kMaximumTextLength)
        {
            theError = InputReplayError::TooMuchText;
            return false;
        }
        aFrame.mTextInput.reserve(aTextLength);
        for (std::uint16_t aTextIndex = 0;
             aTextIndex < aTextLength;
             ++aTextIndex)
        {
            std::uint32_t aCodePoint{};
            if (!theReader.ReadU32(aCodePoint))
            {
                theError = InputReplayError::IoError;
                return false;
            }
            if (!IsValidCodePoint(
                    static_cast<char32_t>(aCodePoint)))
            {
                theError = InputReplayError::InvalidText;
                return false;
            }
            aFrame.mTextInput.push_back(
                static_cast<char32_t>(aCodePoint));
        }
        if (!ValidateFrame(
                aFrame,
                static_cast<TickIndex>(anIndex),
                theError))
        {
            return false;
        }
        aFrames.push_back(std::move(aFrame));
    }
    if (theReader.GetBytesRemaining() != 0)
    {
        theError = InputReplayError::TrailingData;
        return false;
    }

    mFrames = std::move(aFrames);
    theError = InputReplayError::None;
    return true;
}

std::span<const RecordedInputFrame>
InputReplay::GetFrames() const
{
    return mFrames;
}

} // namespace pvz::engine::core
