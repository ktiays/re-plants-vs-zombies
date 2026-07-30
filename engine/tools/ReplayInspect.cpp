#include "pvz/engine/core/BinaryStateIO.h"
#include "pvz/engine/core/ReplaySession.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

namespace
{

inline constexpr std::uint64_t kMaximumSessionFileSize =
    320ULL * 1'024ULL * 1'024ULL;

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

[[nodiscard]] bool LoadSession(
    const std::filesystem::path& thePath,
    pvz::engine::core::ReplaySession& theSession)
{
    std::vector<std::byte> aBytes;
    if (!LoadFile(thePath, aBytes))
        return false;

    pvz::engine::core::BinaryStateReader aReader(aBytes);
    pvz::engine::core::ReplaySessionError anError{};
    if (!theSession.Load(aReader, anError))
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

void PrintSummary(
    std::string_view theLabel,
    const pvz::engine::core::ReplaySession& theSession)
{
    std::cout
        << theLabel
        << " frames="
        << theSession.GetInputReplay().GetFrames().size()
        << " hashes="
        << theSession.GetStateHashes().size()
        << " final-state-fnv1a="
        << theSession.GetFinalStateHash()
        << " transcript-fnv1a="
        << theSession.GetTranscriptHash()
        << '\n';
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
    const pvz::engine::core::ReplaySession& theLeft,
    const pvz::engine::core::ReplaySession& theRight)
{
    const auto aLeft = theLeft.GetInputReplay().GetFrames();
    const auto aRight = theRight.GetInputReplay().GetFrames();
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
               "capture.pvzc [reference.pvzc]\n";
        return 2;
    }

    const std::filesystem::path aLeftPath(theArguments[1]);
    pvz::engine::core::ReplaySession aLeft;
    if (!LoadSession(aLeftPath, aLeft))
        return 2;
    PrintSummary(aLeftPath.string(), aLeft);
    if (theArgumentCount == 2)
        return 0;

    const std::filesystem::path aRightPath(theArguments[2]);
    pvz::engine::core::ReplaySession aRight;
    if (!LoadSession(aRightPath, aRight))
        return 2;
    PrintSummary(aRightPath.string(), aRight);

    const auto anInputDifference =
        FindFirstInputDifference(aLeft, aRight);
    const auto aStateDifference =
        FindFirstStateDifference(aLeft, aRight);
    const auto aNoDifference =
        std::numeric_limits<std::uint64_t>::max();
    const bool inputsMatch = anInputDifference == aNoDifference;
    const bool statesMatch =
        aStateDifference == aNoDifference &&
        aLeft.GetTranscriptHash() ==
            aRight.GetTranscriptHash();

    if (!inputsMatch)
    {
        std::cout
            << "input-mismatch-tick="
            << anInputDifference
            << '\n';
    }
    if (!statesMatch)
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
        std::cout << "captures-match\n";
        return 0;
    }
    return 1;
}
