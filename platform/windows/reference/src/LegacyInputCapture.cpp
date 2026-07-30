#include "pvz/platform/windows/LegacyInputCapture.h"

#include "pvz/engine/core/BinaryStateIO.h"
#include "pvz/platform/windows/ReferenceInputRecorder.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <system_error>
#include <utility>

namespace pvz::platform::windows
{
namespace
{

struct CaptureState
{
    ReferenceInputRecorder mRecorder;
    std::filesystem::path mOutputPath;
    std::string mError;
    bool mRequested{};
    bool mEnabled{};
    bool mFinalized{};
};

[[nodiscard]] CaptureState& GetCaptureState()
{
    static CaptureState aState;
    return aState;
}

void SetError(CaptureState& theState, std::string theError)
{
    if (theState.mError.empty())
        theState.mError = std::move(theError);
    theState.mEnabled = false;
}

[[nodiscard]] bool WriteCapture(
    CaptureState& theState,
    std::span<const std::byte> theBytes)
{
    std::error_code anError;
    const bool hasOutput =
        std::filesystem::exists(theState.mOutputPath, anError);
    if (anError)
    {
        SetError(
            theState,
            "could not inspect replay output path: " +
                anError.message());
        return false;
    }
    if (hasOutput)
    {
        SetError(theState, "replay output already exists");
        return false;
    }

    auto aTemporaryPath = theState.mOutputPath;
    aTemporaryPath += ".tmp";
    const bool hasTemporaryOutput =
        std::filesystem::exists(aTemporaryPath, anError);
    if (anError)
    {
        SetError(
            theState,
            "could not inspect temporary replay path: " +
                anError.message());
        return false;
    }
    if (hasTemporaryOutput)
    {
        SetError(theState, "temporary replay output already exists");
        return false;
    }

    std::ofstream aStream(
        aTemporaryPath,
        std::ios::binary | std::ios::trunc);
    if (!aStream)
    {
        SetError(theState, "could not create temporary replay output");
        return false;
    }
    if (!theBytes.empty())
    {
        aStream.write(
            reinterpret_cast<const char*>(theBytes.data()),
            static_cast<std::streamsize>(theBytes.size()));
    }
    aStream.close();
    if (!aStream)
    {
        SetError(theState, "could not write complete replay output");
        std::filesystem::remove(aTemporaryPath, anError);
        return false;
    }

    std::filesystem::rename(
        aTemporaryPath,
        theState.mOutputPath,
        anError);
    if (anError)
    {
        SetError(
            theState,
            "could not publish replay output: " +
                anError.message());
        std::filesystem::remove(aTemporaryPath, anError);
        return false;
    }
    return true;
}

} // namespace

bool ConfigureLegacyInputCapture(
    std::string_view theOutputPath)
{
    auto& aState = GetCaptureState();
    if (aState.mRequested)
    {
        SetError(aState, "replay capture was configured more than once");
        return false;
    }
    aState.mRequested = true;
    if (theOutputPath.empty())
    {
        SetError(aState, "replay output path is empty");
        return false;
    }

    std::error_code anError;
    aState.mOutputPath = std::filesystem::absolute(
        std::filesystem::path(theOutputPath),
        anError);
    if (anError)
    {
        SetError(
            aState,
            "could not resolve replay output path: " +
                anError.message());
        return false;
    }
    const bool hasOutput =
        std::filesystem::exists(aState.mOutputPath, anError);
    if (anError)
    {
        SetError(
            aState,
            "could not inspect replay output path: " +
                anError.message());
        return false;
    }
    if (hasOutput)
    {
        SetError(aState, "replay output already exists");
        return false;
    }
    aState.mEnabled = true;
    return true;
}

bool WasLegacyInputCaptureRequested()
{
    return GetCaptureState().mRequested;
}

bool IsLegacyInputCaptureEnabled()
{
    return GetCaptureState().mEnabled;
}

void RecordLegacyPointerPosition(
    std::int32_t theX,
    std::int32_t theY)
{
    auto& aState = GetCaptureState();
    if (aState.mEnabled)
        aState.mRecorder.RecordPointerPosition(theX, theY);
}

void RecordLegacyPointerButtonDown(
    ReferencePointerButton theButton)
{
    auto& aState = GetCaptureState();
    if (aState.mEnabled)
        aState.mRecorder.RecordPointerButtonDown(theButton);
}

void RecordLegacyPointerButtonUp(
    ReferencePointerButton theButton)
{
    auto& aState = GetCaptureState();
    if (aState.mEnabled)
        aState.mRecorder.RecordPointerButtonUp(theButton);
}

void RecordLegacyMouseWheel(std::int32_t theStepDelta)
{
    auto& aState = GetCaptureState();
    if (aState.mEnabled)
        aState.mRecorder.RecordMouseWheel(theStepDelta);
}

void RecordLegacyVirtualKeyDown(
    std::uint32_t theVirtualKey,
    bool theRepeat)
{
    auto& aState = GetCaptureState();
    if (aState.mEnabled)
    {
        aState.mRecorder.RecordVirtualKeyDown(
            theVirtualKey,
            theRepeat);
    }
}

void RecordLegacyVirtualKeyUp(std::uint32_t theVirtualKey)
{
    auto& aState = GetCaptureState();
    if (aState.mEnabled)
        aState.mRecorder.RecordVirtualKeyUp(theVirtualKey);
}

void RecordLegacyText(std::uint32_t theCodePoint)
{
    auto& aState = GetCaptureState();
    if (aState.mEnabled)
        aState.mRecorder.RecordText(theCodePoint);
}

void CaptureLegacyInputTick()
{
    auto& aState = GetCaptureState();
    if (!aState.mEnabled)
        return;
    if (!aState.mRecorder.CaptureTick())
    {
        SetError(
            aState,
            std::string(
                engine::core::GetInputReplayErrorMessage(
                    aState.mRecorder.GetError())));
    }
}

bool FinalizeLegacyInputCapture()
{
    auto& aState = GetCaptureState();
    if (!aState.mRequested)
        return true;
    if (aState.mFinalized)
        return aState.mError.empty();
    aState.mFinalized = true;
    if (!aState.mEnabled)
        return false;

    engine::core::BinaryStateWriter aWriter;
    if (!aState.mRecorder.Save(aWriter))
    {
        SetError(
            aState,
            std::string(
                engine::core::GetInputReplayErrorMessage(
                    aState.mRecorder.GetError())));
        return false;
    }
    if (!WriteCapture(aState, aWriter.GetBytes()))
        return false;
    aState.mEnabled = false;
    return true;
}

std::string_view GetLegacyInputCaptureError()
{
    return GetCaptureState().mError;
}

std::uint64_t GetLegacyInputCaptureFrameCount()
{
    return static_cast<std::uint64_t>(
        GetCaptureState().mRecorder.GetReplay().GetFrames().size());
}

} // namespace pvz::platform::windows
