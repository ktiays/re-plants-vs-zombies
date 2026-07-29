#include "pvz/platform/macos/SdlPlatform.h"

#include <SDL.h>
#include <SDL_metal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace pvz::platform::macos
{
namespace
{

constexpr engine::SizeI kLogicalSize{800, 600};
constexpr auto kKeyCount =
    static_cast<std::size_t>(engine::KeyCode::Count);
constexpr auto kPointerButtonCount =
    static_cast<std::size_t>(engine::PointerButton::Count);

[[nodiscard]] std::size_t GetKeyIndex(engine::KeyCode theKey)
{
    return static_cast<std::size_t>(theKey);
}

[[nodiscard]] std::size_t GetPointerButtonIndex(
    engine::PointerButton theButton)
{
    return static_cast<std::size_t>(theButton);
}

[[nodiscard]] engine::KeyCode MapKey(SDL_Scancode theKey)
{
    switch (theKey)
    {
    case SDL_SCANCODE_BACKSPACE:
        return engine::KeyCode::Backspace;
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:
        return engine::KeyCode::Enter;
    case SDL_SCANCODE_ESCAPE:
        return engine::KeyCode::Escape;
    case SDL_SCANCODE_SPACE:
        return engine::KeyCode::Space;
    case SDL_SCANCODE_DELETE:
        return engine::KeyCode::DeleteKey;
    case SDL_SCANCODE_LEFT:
        return engine::KeyCode::ArrowLeft;
    case SDL_SCANCODE_RIGHT:
        return engine::KeyCode::ArrowRight;
    case SDL_SCANCODE_UP:
        return engine::KeyCode::ArrowUp;
    case SDL_SCANCODE_DOWN:
        return engine::KeyCode::ArrowDown;
    default:
        return engine::KeyCode::Unknown;
    }
}

[[nodiscard]] bool MapPointerButton(
    std::uint8_t theSdlButton,
    engine::PointerButton& theButton)
{
    switch (theSdlButton)
    {
    case SDL_BUTTON_LEFT:
        theButton = engine::PointerButton::Primary;
        return true;
    case SDL_BUTTON_RIGHT:
        theButton = engine::PointerButton::Secondary;
        return true;
    case SDL_BUTTON_MIDDLE:
        theButton = engine::PointerButton::Middle;
        return true;
    default:
        return false;
    }
}

void AppendUtf8(
    std::string_view theText,
    std::vector<char32_t>& theOutput)
{
    std::size_t anOffset{};
    while (anOffset < theText.size())
    {
        const auto aFirst =
            static_cast<std::uint8_t>(theText[anOffset]);
        char32_t aCodePoint{};
        std::size_t aLength{};
        if (aFirst < 0x80)
        {
            aCodePoint = aFirst;
            aLength = 1;
        }
        else if ((aFirst & 0xE0) == 0xC0)
        {
            aCodePoint = static_cast<char32_t>(aFirst & 0x1F);
            aLength = 2;
        }
        else if ((aFirst & 0xF0) == 0xE0)
        {
            aCodePoint = static_cast<char32_t>(aFirst & 0x0F);
            aLength = 3;
        }
        else if ((aFirst & 0xF8) == 0xF0)
        {
            aCodePoint = static_cast<char32_t>(aFirst & 0x07);
            aLength = 4;
        }
        else
        {
            ++anOffset;
            continue;
        }

        if (aLength > theText.size() - anOffset)
            break;
        bool isValid = true;
        for (std::size_t anIndex = 1; anIndex < aLength; ++anIndex)
        {
            const auto aContinuation = static_cast<std::uint8_t>(
                theText[anOffset + anIndex]);
            if ((aContinuation & 0xC0) != 0x80)
            {
                isValid = false;
                break;
            }
            aCodePoint =
                static_cast<char32_t>(
                    (aCodePoint << 6) |
                    static_cast<char32_t>(aContinuation & 0x3F));
        }

        if (isValid &&
            aCodePoint <= 0x10FFFF &&
            !(aCodePoint >= 0xD800 && aCodePoint <= 0xDFFF))
        {
            theOutput.push_back(aCodePoint);
            anOffset += aLength;
        }
        else
        {
            ++anOffset;
        }
    }
}

} // namespace

struct SdlPlatform::Implementation final : public engine::IInputFrame
{
    ~Implementation() override
    {
        if (mMetalView != nullptr)
            SDL_Metal_DestroyView(mMetalView);
        if (mWindow != nullptr)
            SDL_DestroyWindow(mWindow);
        if (mInitialized)
            SDL_Quit();
    }

    [[nodiscard]] bool IsKeyDown(
        engine::KeyCode theKey) const override
    {
        const auto anIndex = GetKeyIndex(theKey);
        return anIndex < mKeysDown.size() && mKeysDown[anIndex];
    }

    [[nodiscard]] bool WasKeyPressed(
        engine::KeyCode theKey) const override
    {
        const auto anIndex = GetKeyIndex(theKey);
        return anIndex < mKeysPressed.size() && mKeysPressed[anIndex];
    }

    [[nodiscard]] bool IsPointerButtonDown(
        engine::PointerButton theButton) const override
    {
        const auto anIndex = GetPointerButtonIndex(theButton);
        return anIndex < mPointerButtonsDown.size() &&
               mPointerButtonsDown[anIndex];
    }

    [[nodiscard]] bool WasPointerButtonPressed(
        engine::PointerButton theButton) const override
    {
        const auto anIndex = GetPointerButtonIndex(theButton);
        return anIndex < mPointerButtonsPressed.size() &&
               mPointerButtonsPressed[anIndex];
    }

    [[nodiscard]] engine::PointerState GetPointerState()
        const override
    {
        return mPointer;
    }

    [[nodiscard]] std::span<const char32_t> GetTextInput()
        const override
    {
        return mTextInput;
    }

    void UpdatePointer(std::int32_t theX, std::int32_t theY)
    {
        int aWindowWidth{};
        int aWindowHeight{};
        SDL_GetWindowSize(mWindow, &aWindowWidth, &aWindowHeight);
        if (aWindowWidth <= 0 || aWindowHeight <= 0)
        {
            mPointer.mPosition = {};
            return;
        }

        const double aScale = std::min(
            static_cast<double>(aWindowWidth) /
                static_cast<double>(kLogicalSize.mWidth),
            static_cast<double>(aWindowHeight) /
                static_cast<double>(kLogicalSize.mHeight));
        const double anOriginX =
            (static_cast<double>(aWindowWidth) -
             static_cast<double>(kLogicalSize.mWidth) * aScale) *
            0.5;
        const double anOriginY =
            (static_cast<double>(aWindowHeight) -
             static_cast<double>(kLogicalSize.mHeight) * aScale) *
            0.5;
        mPointer.mPosition = {
            static_cast<std::int32_t>(std::floor(
                (static_cast<double>(theX) - anOriginX) / aScale)),
            static_cast<std::int32_t>(std::floor(
                (static_cast<double>(theY) - anOriginY) / aScale)),
        };
    }

    SDL_Window* mWindow{};
    SDL_MetalView mMetalView{};
    std::array<bool, kKeyCount> mKeysDown{};
    std::array<bool, kKeyCount> mKeysPressed{};
    std::array<bool, kPointerButtonCount> mPointerButtonsDown{};
    std::array<bool, kPointerButtonCount> mPointerButtonsPressed{};
    engine::PointerState mPointer;
    std::vector<char32_t> mTextInput;
    std::string mLastError;
    std::uint64_t mPerformanceFrequency{};
    bool mInitialized{};
    bool mQuitRequested{};
    bool mSuspended{};
};

SdlPlatform::SdlPlatform()
    : mImplementation(std::make_unique<Implementation>())
{
}

SdlPlatform::~SdlPlatform() = default;

bool SdlPlatform::Initialize(
    std::string_view theTitle,
    std::uint32_t theWidth,
    std::uint32_t theHeight)
{
    if (mImplementation->mInitialized)
        return true;

    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0)
    {
        mImplementation->mLastError = SDL_GetError();
        return false;
    }
    mImplementation->mInitialized = true;

    const std::string aTitle(theTitle);
    mImplementation->mWindow = SDL_CreateWindow(
        aTitle.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        static_cast<int>(theWidth),
        static_cast<int>(theHeight),
        SDL_WINDOW_ALLOW_HIGHDPI |
            SDL_WINDOW_METAL |
            SDL_WINDOW_RESIZABLE);
    if (mImplementation->mWindow == nullptr)
    {
        mImplementation->mLastError = SDL_GetError();
        return false;
    }

    mImplementation->mMetalView =
        SDL_Metal_CreateView(mImplementation->mWindow);
    if (mImplementation->mMetalView == nullptr)
    {
        mImplementation->mLastError = SDL_GetError();
        return false;
    }

    mImplementation->mPerformanceFrequency =
        SDL_GetPerformanceFrequency();
    if (mImplementation->mPerformanceFrequency == 0)
    {
        mImplementation->mLastError =
            "SDL returned a zero performance-counter frequency";
        return false;
    }
    SDL_StartTextInput();
    return true;
}

void* SdlPlatform::GetMetalLayer() const
{
    if (mImplementation->mMetalView == nullptr)
        return nullptr;
    return SDL_Metal_GetLayer(mImplementation->mMetalView);
}

std::string_view SdlPlatform::GetLastError() const
{
    return mImplementation->mLastError;
}

engine::MonotonicTimeMicroseconds SdlPlatform::GetTime() const
{
    const auto aFrequency = mImplementation->mPerformanceFrequency;
    if (aFrequency == 0)
        return 0;

    const std::uint64_t aCounter = SDL_GetPerformanceCounter();
    constexpr std::uint64_t kMicrosecondsPerSecond = 1'000'000;
    return (aCounter / aFrequency) * kMicrosecondsPerSecond +
           ((aCounter % aFrequency) * kMicrosecondsPerSecond) /
               aFrequency;
}

void SdlPlatform::PumpEvents()
{
    SDL_Event anEvent;
    while (SDL_PollEvent(&anEvent) != 0)
    {
        switch (anEvent.type)
        {
        case SDL_QUIT:
            mImplementation->mQuitRequested = true;
            break;
        case SDL_KEYDOWN:
        {
            const auto aKey = MapKey(anEvent.key.keysym.scancode);
            const auto anIndex = GetKeyIndex(aKey);
            if (anIndex < mImplementation->mKeysDown.size())
            {
                mImplementation->mKeysDown[anIndex] = true;
                if (anEvent.key.repeat == 0)
                    mImplementation->mKeysPressed[anIndex] = true;
            }
            break;
        }
        case SDL_KEYUP:
        {
            const auto aKey = MapKey(anEvent.key.keysym.scancode);
            const auto anIndex = GetKeyIndex(aKey);
            if (anIndex < mImplementation->mKeysDown.size())
                mImplementation->mKeysDown[anIndex] = false;
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        {
            engine::PointerButton aButton{};
            if (MapPointerButton(anEvent.button.button, aButton))
            {
                const auto anIndex = GetPointerButtonIndex(aButton);
                const bool isDown =
                    anEvent.type == SDL_MOUSEBUTTONDOWN;
                mImplementation->mPointerButtonsDown[anIndex] = isDown;
                if (isDown)
                {
                    mImplementation->mPointerButtonsPressed[anIndex] =
                        true;
                }
            }
            mImplementation->UpdatePointer(
                anEvent.button.x,
                anEvent.button.y);
            break;
        }
        case SDL_MOUSEMOTION:
            mImplementation->UpdatePointer(
                anEvent.motion.x,
                anEvent.motion.y);
            break;
        case SDL_MOUSEWHEEL:
            mImplementation->mPointer.mWheelDelta +=
                anEvent.wheel.y * 120;
            break;
        case SDL_TEXTINPUT:
            AppendUtf8(
                anEvent.text.text,
                mImplementation->mTextInput);
            break;
        case SDL_WINDOWEVENT:
            if (anEvent.window.event == SDL_WINDOWEVENT_MINIMIZED)
                mImplementation->mSuspended = true;
            else if (anEvent.window.event == SDL_WINDOWEVENT_RESTORED ||
                     anEvent.window.event == SDL_WINDOWEVENT_SHOWN)
                mImplementation->mSuspended = false;
            break;
        default:
            break;
        }
    }
}

bool SdlPlatform::IsQuitRequested() const
{
    return mImplementation->mQuitRequested;
}

bool SdlPlatform::IsSuspended() const
{
    return mImplementation->mSuspended;
}

void SdlPlatform::WaitUntil(
    engine::MonotonicTimeMicroseconds theDeadline)
{
    const auto aCurrentTime = GetTime();
    if (theDeadline <= aCurrentTime)
        return;

    const auto aRemaining = theDeadline - aCurrentTime;
    const auto aDelay = std::min<std::uint64_t>(
        (aRemaining + 999) / 1'000,
        std::numeric_limits<std::uint32_t>::max());
    SDL_Delay(static_cast<std::uint32_t>(aDelay));
}

const engine::IInputFrame& SdlPlatform::GetFrame() const
{
    return *mImplementation;
}

void SdlPlatform::ConsumeTransientEvents()
{
    mImplementation->mKeysPressed.fill(false);
    mImplementation->mPointerButtonsPressed.fill(false);
    mImplementation->mPointer.mWheelDelta = 0;
    mImplementation->mTextInput.clear();
}

} // namespace pvz::platform::macos
