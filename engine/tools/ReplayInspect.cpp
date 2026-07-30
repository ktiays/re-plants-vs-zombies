#include "pvz/engine/core/BinaryStateIO.h"
#include "pvz/engine/core/ReplaySession.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace
{

inline constexpr std::uint64_t kMaximumSessionFileSize =
    320ULL * 1'024ULL * 1'024ULL;
inline constexpr std::uint32_t kInputReplayMagic = 0x525A5650;
inline constexpr std::uint32_t kReplaySessionMagic = 0x435A5650;

struct LoadedCapture
{
    [[nodiscard]] const pvz::engine::core::InputReplay&
    GetInputReplay() const
    {
        if (mSession.has_value())
            return mSession->GetInputReplay();
        return mInputReplay;
    }

    pvz::engine::core::InputReplay mInputReplay;
    std::optional<pvz::engine::core::ReplaySession> mSession;
};

[[nodiscard]] bool LoadFile(
    const std::filesystem::path& thePath,
    std::vector<std::byte>& theBytes)
{
    std::error_code anError;
    const auto aFileSize =
        std::filesystem::file_size(thePath, anError);
    if (anError)
    {
        std::cerr
            << thePath.string()
            << ": could not read file size: "
            << anError.message()
            << '\n';
        return false;
    }
    if (aFileSize > kMaximumSessionFileSize)
    {
        std::cerr
            << thePath.string()
            << ": capture exceeds the inspector size limit\n";
        return false;
    }

    std::ifstream aStream(thePath, std::ios::binary);
    if (!aStream)
    {
        std::cerr
            << thePath.string()
            << ": could not open capture\n";
        return false;
    }
    theBytes.resize(static_cast<std::size_t>(aFileSize));
    if (!theBytes.empty())
    {
        aStream.read(
            reinterpret_cast<char*>(theBytes.data()),
            static_cast<std::streamsize>(theBytes.size()));
    }
    if (!aStream ||
        aStream.gcount() !=
            static_cast<std::streamsize>(theBytes.size()))
    {
        std::cerr
            << thePath.string()
            << ": could not read complete capture\n";
        return false;
    }
    return true;
}

[[nodiscard]] bool LoadCapture(
    const std::filesystem::path& thePath,
    LoadedCapture& theCapture)
{
    std::vector<std::byte> aBytes;
    if (!LoadFile(thePath, aBytes))
        return false;

    pvz::engine::core::BinaryStateReader aHeaderReader(aBytes);
    std::uint32_t aMagic{};
    if (!aHeaderReader.ReadU32(aMagic))
    {
        std::cerr
            << thePath.string()
            << ": capture header is truncated"
            << '\n';
        return false;
    }
    if (aMagic == kInputReplayMagic)
    {
        pvz::engine::core::BinaryStateReader aReader(aBytes);
        pvz::engine::core::InputReplayError anError{};
        if (!theCapture.mInputReplay.Load(aReader, anError))
        {
            std::cerr
                << thePath.string()
                << ": "
                << pvz::engine::core::
                       GetInputReplayErrorMessage(anError)
                << '\n';
            return false;
        }
        return true;
    }
    if (aMagic == kReplaySessionMagic)
    {
        theCapture.mSession.emplace();
        pvz::engine::core::BinaryStateReader aReader(aBytes);
        pvz::engine::core::ReplaySessionError anError{};
        if (!theCapture.mSession->Load(aReader, anError))
        {
            std::cerr
                << thePath.string()
                << ": "
                << pvz::engine::core::
                       GetReplaySessionErrorMessage(anError)
                << '\n';
            return false;
        }
        return true;
    }

    std::cerr
        << thePath.string()
        << ": capture magic is neither PVZR nor PVZC\n";
    return false;
}

void PrintSummary(
    std::string_view theLabel,
    const LoadedCapture& theCapture)
{
    const auto& anInput = theCapture.GetInputReplay();
    std::cout
        << theLabel
        << " frames="
        << anInput.GetFrames().size();
    if (theCapture.mSession.has_value())
    {
        std::cout
            << " hashes="
            << theCapture.mSession->GetStateHashes().size()
            << " final-state-fnv1a="
            << theCapture.mSession->GetFinalStateHash()
            << " transcript-fnv1a="
            << theCapture.mSession->GetTranscriptHash();
    }
    else
    {
        std::cout << " input-only";
    }
    std::cout << '\n';
}

[[nodiscard]] bool FramesEqual(
    const pvz::engine::core::RecordedInputFrame& theLeft,
    const pvz::engine::core::RecordedInputFrame& theRight)
{
    return
        theLeft.mTick == theRight.mTick &&
        theLeft.mKeysDown == theRight.mKeysDown &&
        theLeft.mKeysPressed == theRight.mKeysPressed &&
        theLeft.mPointerButtonsDown ==
            theRight.mPointerButtonsDown &&
        theLeft.mPointerButtonsPressed ==
            theRight.mPointerButtonsPressed &&
        theLeft.mPointer.mPosition.mX ==
            theRight.mPointer.mPosition.mX &&
        theLeft.mPointer.mPosition.mY ==
            theRight.mPointer.mPosition.mY &&
        theLeft.mPointer.mWheelDelta ==
            theRight.mPointer.mWheelDelta &&
        theLeft.mTextInput == theRight.mTextInput;
}

[[nodiscard]] std::uint64_t FindFirstInputDifference(
    const pvz::engine::core::InputReplay& theLeft,
    const pvz::engine::core::InputReplay& theRight)
{
    const auto aLeft = theLeft.GetFrames();
    const auto aRight = theRight.GetFrames();
    const auto aCount = std::min(aLeft.size(), aRight.size());
    for (std::size_t anIndex = 0; anIndex < aCount; ++anIndex)
    {
        if (!FramesEqual(aLeft[anIndex], aRight[anIndex]))
            return static_cast<std::uint64_t>(anIndex);
    }
    if (aLeft.size() != aRight.size())
        return static_cast<std::uint64_t>(aCount);
    return std::numeric_limits<std::uint64_t>::max();
}

[[nodiscard]] std::uint64_t FindFirstStateDifference(
    const pvz::engine::core::ReplaySession& theLeft,
    const pvz::engine::core::ReplaySession& theRight)
{
    const auto aLeft = theLeft.GetStateHashes();
    const auto aRight = theRight.GetStateHashes();
    const auto aCount = std::min(aLeft.size(), aRight.size());
    for (std::size_t anIndex = 0; anIndex < aCount; ++anIndex)
    {
        if (aLeft[anIndex] != aRight[anIndex])
            return static_cast<std::uint64_t>(anIndex);
    }
    if (aLeft.size() != aRight.size())
        return static_cast<std::uint64_t>(aCount);
    return std::numeric_limits<std::uint64_t>::max();
}

} // namespace

int main(int theArgumentCount, char** theArguments)
{
    if (theArgumentCount != 2 &&
        theArgumentCount != 3)
    {
        std::cerr
            << "usage: pvz_replay_inspect "
               "capture.pvzr-or-pvzc "
               "[reference.pvzr-or-pvzc]\n";
        return 2;
    }

    const std::filesystem::path aLeftPath(theArguments[1]);
    LoadedCapture aLeft;
    if (!LoadCapture(aLeftPath, aLeft))
        return 2;
    PrintSummary(aLeftPath.string(), aLeft);
    if (theArgumentCount == 2)
        return 0;

    const std::filesystem::path aRightPath(theArguments[2]);
    LoadedCapture aRight;
    if (!LoadCapture(aRightPath, aRight))
        return 2;
    PrintSummary(aRightPath.string(), aRight);

    const auto anInputDifference =
        FindFirstInputDifference(
            aLeft.GetInputReplay(),
            aRight.GetInputReplay());
    const auto aNoDifference =
        std::numeric_limits<std::uint64_t>::max();
    const bool inputsMatch = anInputDifference == aNoDifference;
    const bool statesComparable =
        aLeft.mSession.has_value() &&
        aRight.mSession.has_value();
    auto aStateDifference = aNoDifference;
    bool statesMatch = true;
    if (statesComparable)
    {
        aStateDifference =
            FindFirstStateDifference(
                *aLeft.mSession,
                *aRight.mSession);
        statesMatch =
            aStateDifference == aNoDifference &&
            aLeft.mSession->GetTranscriptHash() ==
                aRight.mSession->GetTranscriptHash();
    }

    if (!inputsMatch)
    {
        std::cout
            << "input-mismatch-tick="
            << anInputDifference
            << '\n';
    }
    if (statesComparable && !statesMatch)
    {
        if (aStateDifference != aNoDifference)
        {
            std::cout
                << "state-mismatch-tick="
                << aStateDifference
                << '\n';
        }
        else
        {
            std::cout << "transcript-hash-mismatch\n";
        }
    }
    if (inputsMatch && statesMatch)
    {
        std::cout
            << (statesComparable
                    ? "captures-match\n"
                    : "inputs-match\n");
        return 0;
    }
    return 1;
}
