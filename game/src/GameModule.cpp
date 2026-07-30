#include "pvz/game/GameModule.h"

#include <array>
#include <cstdint>

namespace pvz::game
{
namespace
{

inline constexpr std::uint32_t kStateMagic = 0x475A5650;
inline constexpr std::uint16_t kStateVersion = 1;

} // namespace

engine::LifecycleResult GameModule::Initialize(
    engine::IEngineServices& theServices)
{
    if (mInitialized)
        return engine::LifecycleResult::Failure;

    mServices = &theServices;
    mInitialized = true;
    mSuspended = false;
    engine::ImageResourceDiagnostic aDiagnostic;
    static_cast<void>(
        mServices->GetImageResources().Load(
            "IMAGE_TITLESCREEN",
            mTitleScreen,
            aDiagnostic));
    static_cast<void>(
        mServices->GetImageResources().Load(
            "IMAGE_PVZ_LOGO",
            mTitleLogo,
            aDiagnostic));
    if (mTitleScreen.mImage.IsValid())
    {
        mServices->GetLogger().Log(
            engine::LogLevel::Information,
            mTitleLogo.mImage.IsValid()
                ? "Portable title and alpha-logo resources loaded"
                : "Portable title resource loaded");
    }
    mServices->GetLogger().Log(
        engine::LogLevel::Information,
        "Portable game module initialized");
    return engine::LifecycleResult::Success;
}

void GameModule::Update(
    const engine::GameTick& theTick,
    const engine::IInputFrame& theInput)
{
    if (!mInitialized || mSuspended)
        return;

    static_cast<void>(theInput);
    mLastTick = theTick.mIndex;
    ++mUpdateCount;
}

void GameModule::Render(engine::IRenderFrame& theFrame) const
{
    if (!mInitialized)
        return;

    theFrame.Clear(engine::ColorRgba8{0, 0, 0, 255});
    if (!mTitleScreen.mImage.IsValid())
        return;

    std::array<engine::SpriteDraw, 2> aDraws;
    std::size_t aDrawCount{};
    aDraws[aDrawCount++] = {
        .mImage = mTitleScreen.mImage,
        .mSource =
            {
                .mOrigin = {0, 0},
                .mSize = mTitleScreen.mSize,
            },
        .mDestination =
            {
                .mOrigin = {0.0F, 0.0F},
                .mSize = {800.0F, 600.0F},
            },
        .mFilterMode = engine::FilterMode::Linear,
    };
    if (mTitleLogo.mImage.IsValid())
    {
        const float aLogoWidth =
            static_cast<float>(mTitleLogo.mSize.mWidth);
        const float aLogoHeight =
            static_cast<float>(mTitleLogo.mSize.mHeight);
        aDraws[aDrawCount++] = {
            .mImage = mTitleLogo.mImage,
            .mSource =
                {
                    .mOrigin = {0, 0},
                    .mSize = mTitleLogo.mSize,
                },
            .mDestination =
                {
                    .mOrigin =
                        {
                            (800.0F - aLogoWidth) * 0.5F,
                            20.0F,
                        },
                    .mSize = {aLogoWidth, aLogoHeight},
                },
            .mFilterMode = engine::FilterMode::Linear,
        };
    }
    theFrame.SubmitSprites(
        std::span<const engine::SpriteDraw>(
            aDraws.data(),
            aDrawCount));
}

bool GameModule::LoadState(engine::IStateReader& theReader)
{
    std::uint32_t aMagic{};
    std::uint16_t aVersion{};
    engine::TickIndex aLastTick{};
    std::uint64_t anUpdateCount{};
    bool aSuspended{};

    if (!theReader.ReadU32(aMagic) ||
        !theReader.ReadU16(aVersion) ||
        !theReader.ReadU64(aLastTick) ||
        !theReader.ReadU64(anUpdateCount) ||
        !theReader.ReadBool(aSuspended))
    {
        return false;
    }

    if (aMagic != kStateMagic || aVersion != kStateVersion)
        return false;

    mLastTick = aLastTick;
    mUpdateCount = anUpdateCount;
    mSuspended = aSuspended;
    return true;
}

bool GameModule::SaveState(engine::IStateWriter& theWriter) const
{
    if (!mInitialized)
        return false;

    return theWriter.WriteU32(kStateMagic) &&
           theWriter.WriteU16(kStateVersion) &&
           theWriter.WriteU64(mLastTick) &&
           theWriter.WriteU64(mUpdateCount) &&
           theWriter.WriteBool(mSuspended);
}

void GameModule::Suspend()
{
    if (mInitialized)
        mSuspended = true;
}

void GameModule::Resume()
{
    if (mInitialized)
        mSuspended = false;
}

void GameModule::Shutdown()
{
    if (!mInitialized)
        return;

    mServices->GetLogger().Log(
        engine::LogLevel::Information,
        "Portable game module shut down");
    if (mTitleLogo.mImage.IsValid())
        mServices->GetImageResources().Release(mTitleLogo.mImage);
    if (mTitleScreen.mImage.IsValid())
        mServices->GetImageResources().Release(mTitleScreen.mImage);
    mTitleLogo = {};
    mTitleScreen = {};
    mServices = nullptr;
    mInitialized = false;
    mSuspended = false;
}

bool GameModule::IsInitialized() const
{
    return mInitialized;
}

bool GameModule::IsSuspended() const
{
    return mSuspended;
}

engine::TickIndex GameModule::GetLastTick() const
{
    return mLastTick;
}

std::uint64_t GameModule::GetUpdateCount() const
{
    return mUpdateCount;
}

} // namespace pvz::game
