#include "pvz/engine/core/BinaryStateIO.h"
#include "pvz/parity/BehaviorCapture.h"
#include "pvz/parity/BehaviorComparison.h"

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

inline constexpr std::uint64_t kMaximumCaptureFileSize =
    320ULL * 1'024ULL * 1'024ULL;
inline constexpr std::uint64_t kNoDifference =
    std::numeric_limits<std::uint64_t>::max();

[[nodiscard]] std::string_view GetSceneName(
    pvz::game::BehaviorScene theScene)
{
    switch (theScene)
    {
    case pvz::game::BehaviorScene::Loading:
        return "loading";
    case pvz::game::BehaviorScene::Title:
        return "title";
    case pvz::game::BehaviorScene::MainMenu:
        return "main-menu";
    case pvz::game::BehaviorScene::AdventureIntro:
        return "adventure-intro";
    case pvz::game::BehaviorScene::AdventurePlaying:
        return "adventure-playing";
    case pvz::game::BehaviorScene::Other:
        return "other";
    case pvz::game::BehaviorScene::Count:
        break;
    }
    return "invalid";
}

[[nodiscard]] std::string_view GetBoardStageName(
    pvz::game::BehaviorBoardStage theStage)
{
    switch (theStage)
    {
    case pvz::game::BehaviorBoardStage::None:
        return "none";
    case pvz::game::BehaviorBoardStage::Day:
        return "day";
    case pvz::game::BehaviorBoardStage::Night:
        return "night";
    case pvz::game::BehaviorBoardStage::Pool:
        return "pool";
    case pvz::game::BehaviorBoardStage::Fog:
        return "fog";
    case pvz::game::BehaviorBoardStage::Roof:
        return "roof";
    case pvz::game::BehaviorBoardStage::Boss:
        return "boss";
    case pvz::game::BehaviorBoardStage::Other:
        return "other";
    case pvz::game::BehaviorBoardStage::Count:
        break;
    }
    return "invalid";
}

[[nodiscard]] bool LoadCapture(
    const std::filesystem::path& thePath,
    pvz::parity::BehaviorCapture& theCapture)
{
    std::error_code anError;
    const auto aFileSize =
        std::filesystem::file_size(thePath, anError);
    if (anError)
    {
        std::cerr
            << thePath.string()
            << ": could not read capture size: "
            << anError.message()
            << '\n';
        return false;
    }
    if (aFileSize > kMaximumCaptureFileSize)
    {
        std::cerr
            << thePath.string()
            << ": behavior capture exceeds the inspector size limit\n";
        return false;
    }

    std::ifstream aStream(thePath, std::ios::binary);
    if (!aStream)
    {
        std::cerr
            << thePath.string()
            << ": could not open behavior capture\n";
        return false;
    }
    std::vector<std::byte> aBytes(
        static_cast<std::size_t>(aFileSize));
    if (!aBytes.empty())
    {
        aStream.read(
            reinterpret_cast<char*>(aBytes.data()),
            static_cast<std::streamsize>(aBytes.size()));
    }
    if (!aStream ||
        aStream.gcount() !=
            static_cast<std::streamsize>(aBytes.size()))
    {
        std::cerr
            << thePath.string()
            << ": could not read complete behavior capture\n";
        return false;
    }

    pvz::engine::core::BinaryStateReader aReader(aBytes);
    pvz::parity::BehaviorCaptureError aCaptureError{};
    if (!theCapture.Load(aReader, aCaptureError))
    {
        std::cerr
            << thePath.string()
            << ": "
            << pvz::parity::GetBehaviorCaptureErrorMessage(
                   aCaptureError)
            << '\n';
        return false;
    }
    return true;
}

void PrintSummary(
    std::string_view theLabel,
    const pvz::parity::BehaviorCapture& theCapture)
{
    std::cout
        << theLabel
        << " producer="
        << pvz::parity::GetBehaviorProducerName(
               theCapture.GetProducer())
        << " frames="
        << theCapture.GetInputReplay().GetFrames().size()
        << " observations="
        << theCapture.GetObservations().size()
        << '\n';
}

void PrintTimeline(
    const pvz::parity::BehaviorCapture& theCapture)
{
    const auto anObservations = theCapture.GetObservations();
    for (std::size_t anIndex = 0;
         anIndex < anObservations.size();
         ++anIndex)
    {
        const auto& anObservation = anObservations[anIndex];
        const bool hasSceneTransition =
            anIndex == 0 ||
            anObservation.mScene !=
                anObservations[anIndex - 1].mScene ||
            anObservation.mBoardStage !=
                anObservations[anIndex - 1].mBoardStage;
        if (hasSceneTransition)
        {
            std::cout
                << "scene-tick=" << anObservation.mTick
                << " scene="
                << GetSceneName(anObservation.mScene)
                << " board="
                << GetBoardStageName(
                       anObservation.mBoardStage)
                << '\n';
        }
    }

    for (const auto& aFrame :
         theCapture.GetInputReplay().GetFrames())
    {
        if (aFrame.mKeysPressed == 0 &&
            aFrame.mPointerButtonsPressed == 0 &&
            aFrame.mPointer.mWheelDelta == 0 &&
            aFrame.mTextInput.empty())
        {
            continue;
        }
        std::cout
            << "input-tick=" << aFrame.mTick
            << " keys-pressed="
            << aFrame.mKeysPressed
            << " pointer-pressed="
            << static_cast<std::uint32_t>(
                   aFrame.mPointerButtonsPressed)
            << " pointer="
            << aFrame.mPointer.mPosition.mX
            << ','
            << aFrame.mPointer.mPosition.mY
            << " wheel="
            << aFrame.mPointer.mWheelDelta
            << " text-count="
            << aFrame.mTextInput.size()
            << '\n';
    }
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
    return kNoDifference;
}

void PrintBehaviorDifference(
    const pvz::parity::BehaviorDifference& theDifference,
    const pvz::parity::BehaviorCapture& theLeft,
    const pvz::parity::BehaviorCapture& theRight)
{
    std::cout
        << "behavior-mismatch-tick="
        << theDifference.mTick
        << " field="
        << pvz::parity::GetBehaviorFieldName(
               theDifference.mField);

    const auto aLeft = theLeft.GetObservations();
    const auto aRight = theRight.GetObservations();
    if (theDifference.mTick < aLeft.size() &&
        theDifference.mTick < aRight.size())
    {
        const auto& aLeftValue =
            aLeft[static_cast<std::size_t>(theDifference.mTick)];
        const auto& aRightValue =
            aRight[static_cast<std::size_t>(theDifference.mTick)];
        if (theDifference.mField ==
            pvz::parity::BehaviorField::Scene)
        {
            std::cout
                << " left=" << GetSceneName(aLeftValue.mScene)
                << " right=" << GetSceneName(aRightValue.mScene);
        }
        else if (theDifference.mField ==
                 pvz::parity::BehaviorField::BoardStage)
        {
            std::cout
                << " left="
                << GetBoardStageName(aLeftValue.mBoardStage)
                << " right="
                << GetBoardStageName(aRightValue.mBoardStage);
        }
        else if (theDifference.mField ==
                 pvz::parity::BehaviorField::GridColumn)
        {
            std::cout
                << " left="
                << static_cast<std::uint32_t>(
                       aLeftValue.mGridColumn)
                << " right="
                << static_cast<std::uint32_t>(
                       aRightValue.mGridColumn);
        }
        else if (theDifference.mField ==
                 pvz::parity::BehaviorField::GridRow)
        {
            std::cout
                << " left="
                << static_cast<std::uint32_t>(
                       aLeftValue.mGridRow)
                << " right="
                << static_cast<std::uint32_t>(
                       aRightValue.mGridRow);
        }
        else if (theDifference.mField ==
                 pvz::parity::BehaviorField::OccupiedCells)
        {
            std::cout
                << " left=" << aLeftValue.mOccupiedCells
                << " right=" << aRightValue.mOccupiedCells;
        }
        else if (theDifference.mField ==
                 pvz::parity::BehaviorField::PlantCount)
        {
            std::cout
                << " left=" << aLeftValue.mPlantCount
                << " right=" << aRightValue.mPlantCount;
        }
    }
    std::cout << '\n';
}

} // namespace

int main(int theArgumentCount, char** theArguments)
{
    if (theArgumentCount != 2 && theArgumentCount != 3)
    {
        std::cerr
            << "usage: pvz_behavior_inspect "
               "capture.pvzb [reference.pvzb]\n";
        return 2;
    }

    const std::filesystem::path aLeftPath(theArguments[1]);
    pvz::parity::BehaviorCapture aLeft;
    if (!LoadCapture(aLeftPath, aLeft))
        return 2;
    PrintSummary(aLeftPath.string(), aLeft);
    PrintTimeline(aLeft);
    if (theArgumentCount == 2)
        return 0;

    const std::filesystem::path aRightPath(theArguments[2]);
    pvz::parity::BehaviorCapture aRight;
    if (!LoadCapture(aRightPath, aRight))
        return 2;
    PrintSummary(aRightPath.string(), aRight);
    PrintTimeline(aRight);

    const auto anInputDifference =
        FindFirstInputDifference(
            aLeft.GetInputReplay(),
            aRight.GetInputReplay());
    const auto aBehaviorDifference =
        pvz::parity::FindFirstBehaviorDifference(
            aLeft.GetObservations(),
            aRight.GetObservations());
    if (anInputDifference != kNoDifference)
    {
        std::cout
            << "input-mismatch-tick="
            << anInputDifference
            << '\n';
    }
    if (aBehaviorDifference.mTick !=
        pvz::parity::kNoBehaviorDifferenceTick)
    {
        PrintBehaviorDifference(
            aBehaviorDifference,
            aLeft,
            aRight);
    }
    if (anInputDifference == kNoDifference &&
        aBehaviorDifference.mTick ==
            pvz::parity::kNoBehaviorDifferenceTick)
    {
        std::cout << "behavior-captures-match\n";
        return 0;
    }
    return 1;
}
