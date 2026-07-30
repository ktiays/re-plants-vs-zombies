#include "pvz/platform/macos/RendererSmokeGame.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace pvz::platform::macos
{
namespace
{

inline constexpr std::uint32_t kTextureWidth = 8;
inline constexpr std::uint32_t kTextureHeight = 8;
inline constexpr std::uint32_t kBytesPerPixel = 4;
inline constexpr std::uint32_t kTextureBytesPerRow =
    kTextureWidth * kBytesPerPixel;

[[nodiscard]] std::array<
    std::byte,
    kTextureWidth * kTextureHeight * kBytesPerPixel>
MakeCheckerboard()
{
    std::array<
        std::byte,
        kTextureWidth * kTextureHeight * kBytesPerPixel>
        aPixels{};
    for (std::uint32_t aY = 0; aY < kTextureHeight; ++aY)
    {
        for (std::uint32_t anX = 0; anX < kTextureWidth; ++anX)
        {
            const bool anEven = ((anX + aY) % 2) == 0;
            const auto anOffset =
                static_cast<std::size_t>(
                    aY * kTextureWidth + anX) *
                kBytesPerPixel;
            aPixels[anOffset + 0] =
                std::byte{anEven ? std::uint8_t{32}
                                 : std::uint8_t{220}};
            aPixels[anOffset + 1] =
                std::byte{anEven ? std::uint8_t{210}
                                 : std::uint8_t{48}};
            aPixels[anOffset + 2] =
                std::byte{anEven ? std::uint8_t{255}
                                 : std::uint8_t{40}};
            aPixels[anOffset + 3] =
                std::byte{anEven ? std::uint8_t{255}
                                 : std::uint8_t{150}};
        }
    }
    return aPixels;
}

} // namespace

engine::LifecycleResult RendererSmokeGame::Initialize(
    engine::IEngineServices& theServices)
{
    if (mServices != nullptr)
        return engine::LifecycleResult::Failure;

    const auto aPixels = MakeCheckerboard();
    if (!theServices.GetImages().CreateImage(
            {
                .mSize = {kTextureWidth, kTextureHeight},
                .mPixelFormat =
                    engine::ImagePixelFormat::Bgra8Unorm,
            },
            aPixels,
            kTextureBytesPerRow,
            mCheckerboard))
    {
        theServices.GetLogger().Log(
            engine::LogLevel::Error,
            "Metal smoke texture creation failed");
        return engine::LifecycleResult::Failure;
    }

    constexpr std::array<std::byte, 16> kWhitePatch{
        std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255},
        std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255},
        std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255},
        std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255},
    };
    if (!theServices.GetImages().UpdateImage(
            mCheckerboard,
            {
                .mDestination =
                    {
                        .mOrigin = {3, 3},
                        .mSize = {2, 2},
                    },
                .mSourceBytesPerRow = 8,
                .mPixels = kWhitePatch,
            }))
    {
        theServices.GetImages().DestroyImage(mCheckerboard);
        mCheckerboard = {};
        theServices.GetLogger().Log(
            engine::LogLevel::Error,
            "Metal smoke texture update failed");
        return engine::LifecycleResult::Failure;
    }

    mServices = &theServices;
    mRotationRadians = 0.0F;
    mSuspended = false;
    mServices->GetLogger().Log(
        engine::LogLevel::Information,
        "Metal renderer smoke scene initialized");
    return engine::LifecycleResult::Success;
}

void RendererSmokeGame::Update(
    const engine::GameTick& theTick,
    const engine::IInputFrame& theInput)
{
    static_cast<void>(theTick);
    static_cast<void>(theInput);
    if (!mSuspended)
        mRotationRadians += 0.004F;
}

void RendererSmokeGame::Render(engine::IRenderFrame& theFrame) const
{
    if (mServices == nullptr)
        return;

    theFrame.Clear({8, 16, 32, 255});
    const engine::RectI aFullSource{
        .mOrigin = {0, 0},
        .mSize = {kTextureWidth, kTextureHeight},
    };
    const std::array<engine::SpriteDraw, 8> aDraws{
        engine::SpriteDraw{
            .mImage = mCheckerboard,
            .mSource = aFullSource,
            .mDestination = {{40.0F, 40.0F}, {180.0F, 180.0F}},
        },
        engine::SpriteDraw{
            .mImage = mCheckerboard,
            .mSource = aFullSource,
            .mDestination = {{230.0F, 40.0F}, {90.0F, 90.0F}},
            .mColor = {180, 255, 220, 255},
        },
        engine::SpriteDraw{
            .mImage = mCheckerboard,
            .mSource = aFullSource,
            .mDestination = {{355.0F, 40.0F}, {190.0F, 190.0F}},
            .mRotationCenter = {95.0F, 95.0F},
            .mRotationRadians = mRotationRadians,
            .mFilterMode = engine::FilterMode::Linear,
            .mMirrorMode = engine::MirrorMode::Horizontal,
        },
        engine::SpriteDraw{
            .mImage = mCheckerboard,
            .mSource = aFullSource,
            .mDestination = {{470.0F, 105.0F}, {190.0F, 190.0F}},
            .mColor = {255, 80, 80, 210},
            .mRotationCenter = {95.0F, 95.0F},
            .mRotationRadians = -mRotationRadians,
            .mBlendMode = engine::BlendMode::Additive,
            .mFilterMode = engine::FilterMode::Linear,
            .mMirrorMode = engine::MirrorMode::Vertical,
        },
        engine::SpriteDraw{
            .mImage = mCheckerboard,
            .mSource = aFullSource,
            .mDestination = {{40.0F, 340.0F}, {300.0F, 180.0F}},
            .mClip = {{120, 375}, {150, 100}},
            .mClipMode = engine::ClipMode::Enabled,
        },
        engine::SpriteDraw{
            .mImage = mCheckerboard,
            .mSource = {{0, 0}, {24, 16}},
            .mDestination = {{400.0F, 340.0F}, {300.0F, 200.0F}},
            .mFilterMode = engine::FilterMode::Linear,
            .mAddressMode = engine::AddressMode::Repeat,
        },
        engine::SpriteDraw{
            .mImage = mCheckerboard,
            .mSource = aFullSource,
            .mDestination = {{690.0F, 500.0F}, {80.0F, 80.0F}},
            .mMirrorMode = engine::MirrorMode::Both,
        },
        engine::SpriteDraw{
            .mImage = mCheckerboard,
            .mSource = aFullSource,
            .mDestinationQuad =
                {
                    .mTopLeft = {585.0F, 360.0F},
                    .mTopRight = {745.0F, 330.0F},
                    .mBottomLeft = {610.0F, 470.0F},
                    .mBottomRight = {770.0F, 505.0F},
                },
            .mGeometryMode =
                engine::SpriteGeometryMode::DestinationQuad,
            .mColor = {205, 235, 255, 235},
            .mFilterMode = engine::FilterMode::Linear,
        },
    };
    theFrame.SubmitSprites(aDraws);
}

bool RendererSmokeGame::LoadState(engine::IStateReader& theReader)
{
    static_cast<void>(theReader);
    return true;
}

bool RendererSmokeGame::SaveState(
    engine::IStateWriter& theWriter) const
{
    static_cast<void>(theWriter);
    return true;
}

void RendererSmokeGame::Suspend()
{
    mSuspended = true;
}

void RendererSmokeGame::Resume()
{
    mSuspended = false;
}

void RendererSmokeGame::Shutdown()
{
    if (mServices == nullptr)
        return;
    mServices->GetImages().DestroyImage(mCheckerboard);
    mCheckerboard = {};
    mServices->GetLogger().Log(
        engine::LogLevel::Information,
        "Metal renderer smoke scene shut down");
    mServices = nullptr;
}

} // namespace pvz::platform::macos
