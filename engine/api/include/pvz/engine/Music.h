#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace pvz::engine
{

struct ModuleHandle
{
    std::uint32_t mIndex{};
    std::uint32_t mGeneration{};

    [[nodiscard]] constexpr bool IsValid() const
    {
        return mGeneration != 0;
    }
};

struct MusicPosition
{
    std::uint32_t mOrder{};
    std::uint32_t mRow{};
};

struct ModuleDescriptor
{
    std::uint32_t mChannelCount{};
    std::uint32_t mOrderCount{};
};

enum class MusicLoopMode : std::uint8_t
{
    Once,
    Loop,
};

struct MusicPlayback
{
    MusicPosition mPosition;
    float mVolume{1.0F};
    MusicLoopMode mLoopMode{MusicLoopMode::Loop};
    std::array<std::byte, 3> mReserved{};
};

class IModuleMusicDevice
{
public:
    virtual ~IModuleMusicDevice() = default;

    [[nodiscard]] virtual bool CreateModule(
        std::span<const std::byte> theEncodedBytes,
        ModuleHandle& theModule,
        ModuleDescriptor& theDescriptor) = 0;
    virtual void DestroyModule(ModuleHandle theModule) = 0;
    [[nodiscard]] virtual bool PlayModule(
        ModuleHandle theModule,
        const MusicPlayback& thePlayback) = 0;
    virtual void StopModule(ModuleHandle theModule) = 0;
    virtual void PauseModule(
        ModuleHandle theModule,
        bool thePaused) = 0;
    [[nodiscard]] virtual bool IsModulePlaying(
        ModuleHandle theModule) const = 0;
    [[nodiscard]] virtual bool SetModulePosition(
        ModuleHandle theModule,
        MusicPosition thePosition) = 0;
    [[nodiscard]] virtual bool GetModulePosition(
        ModuleHandle theModule,
        MusicPosition& thePosition) const = 0;
    [[nodiscard]] virtual bool SetModuleChannelEnabled(
        ModuleHandle theModule,
        std::uint32_t theChannel,
        bool theEnabled) = 0;
    [[nodiscard]] virtual bool SetModuleVolume(
        ModuleHandle theModule,
        float theVolume) = 0;
    [[nodiscard]] virtual bool SetModuleTempoFactor(
        ModuleHandle theModule,
        float theFactor) = 0;
    virtual void SetMusicMasterVolume(float theVolume) = 0;
};

enum class MusicResourceError : std::uint8_t
{
    None,
    ResourceNotFound,
    ResourceReadFailed,
    DeviceCreateFailed,
    ReferenceCountOverflow,
};

struct MusicResourceDiagnostic
{
    MusicResourceError mError{MusicResourceError::None};
    std::string mPath;
};

struct MusicResource
{
    ModuleHandle mModule;
    ModuleDescriptor mDescriptor;
};

class IMusicResources
{
public:
    virtual ~IMusicResources() = default;

    [[nodiscard]] virtual bool Load(
        std::string_view thePath,
        MusicResource& theResource,
        MusicResourceDiagnostic& theDiagnostic) = 0;
    virtual void Release(ModuleHandle theModule) = 0;
    [[nodiscard]] virtual bool Play(
        ModuleHandle theModule,
        const MusicPlayback& thePlayback) = 0;
    virtual void Stop(ModuleHandle theModule) = 0;
    virtual void Pause(ModuleHandle theModule, bool thePaused) = 0;
    [[nodiscard]] virtual bool IsPlaying(
        ModuleHandle theModule) const = 0;
    [[nodiscard]] virtual bool SetPosition(
        ModuleHandle theModule,
        MusicPosition thePosition) = 0;
    [[nodiscard]] virtual bool GetPosition(
        ModuleHandle theModule,
        MusicPosition& thePosition) const = 0;
    [[nodiscard]] virtual bool SetChannelEnabled(
        ModuleHandle theModule,
        std::uint32_t theChannel,
        bool theEnabled) = 0;
    [[nodiscard]] virtual bool SetVolume(
        ModuleHandle theModule,
        float theVolume) = 0;
    [[nodiscard]] virtual bool SetTempoFactor(
        ModuleHandle theModule,
        float theFactor) = 0;
    virtual void SetMasterVolume(float theVolume) = 0;
};

static_assert(sizeof(ModuleHandle) == 8);
static_assert(sizeof(MusicPosition) == 8);
static_assert(sizeof(ModuleDescriptor) == 8);
static_assert(sizeof(MusicLoopMode) == 1);
static_assert(sizeof(MusicPlayback) == 16);
static_assert(sizeof(MusicResourceError) == 1);

} // namespace pvz::engine
