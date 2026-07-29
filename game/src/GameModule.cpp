#include "pvz/game/GameModule.h"

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
