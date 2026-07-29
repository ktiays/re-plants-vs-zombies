#include "pvz/game/ReanimationDefinition.h"

#include <charconv>
#include <cmath>
#include <cstddef>
#include <string_view>
#include <system_error>
#include <utility>

namespace pvz::game
{
namespace
{

constexpr std::size_t kMaximumTracks = 10'000;
constexpr std::size_t kMaximumTransforms = 1'000'000;

[[nodiscard]] char LowercaseAscii(char theCharacter)
{
    if (theCharacter >= 'A' && theCharacter <= 'Z')
        return static_cast<char>(theCharacter - 'A' + 'a');
    return theCharacter;
}

[[nodiscard]] bool NamesEqual(
    std::string_view theLeft,
    std::string_view theRight)
{
    if (theLeft.size() != theRight.size())
        return false;
    for (std::size_t anIndex = 0; anIndex < theLeft.size(); ++anIndex)
    {
        if (LowercaseAscii(theLeft[anIndex]) !=
            LowercaseAscii(theRight[anIndex]))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool IsAsciiWhitespace(char theCharacter)
{
    return theCharacter == ' ' ||
           theCharacter == '\t' ||
           theCharacter == '\r' ||
           theCharacter == '\n';
}

[[nodiscard]] std::string_view TrimAscii(std::string_view theValue)
{
    while (!theValue.empty() && IsAsciiWhitespace(theValue.front()))
        theValue.remove_prefix(1);
    while (!theValue.empty() && IsAsciiWhitespace(theValue.back()))
        theValue.remove_suffix(1);
    return theValue;
}

[[nodiscard]] bool HasOnlyWhitespace(std::string_view theValue)
{
    return TrimAscii(theValue).empty();
}

} // namespace

bool ReanimationDefinitionMapper::Map(
    std::span<const engine::XmlNode> theRoots,
    ReanimationDefinition& theDefinition)
{
    Reset();
    if (theRoots.empty())
    {
        mError = ReanimationDefinitionError::EmptyDocument;
        return false;
    }

    ReanimationDefinition aDefinition;
    std::size_t aTransformCount{};
    for (const auto& aRoot : theRoots)
    {
        if (NamesEqual(aRoot.mName, "fps"))
        {
            float aFramesPerSecond{};
            if (!ReadFloat(aRoot, aFramesPerSecond))
                return false;
            if (aFramesPerSecond <= 0.0F)
            {
                Fail(
                    ReanimationDefinitionError::InvalidFramesPerSecond,
                    aRoot);
                return false;
            }
            aDefinition.mFramesPerSecond = aFramesPerSecond;
            continue;
        }

        if (NamesEqual(aRoot.mName, "track"))
        {
            if (!HasOnlyWhitespace(aRoot.mValue))
            {
                Fail(
                    ReanimationDefinitionError::InvalidElementShape,
                    aRoot);
                return false;
            }
            if (aDefinition.mTracks.size() >= kMaximumTracks)
            {
                Fail(
                    ReanimationDefinitionError::TooManyTracks,
                    aRoot);
                return false;
            }

            ReanimationTrack aTrack;
            if (!MapTrack(aRoot, aTrack, aTransformCount))
                return false;
            aDefinition.mTracks.push_back(std::move(aTrack));
            continue;
        }

        Fail(ReanimationDefinitionError::UnknownElement, aRoot);
        return false;
    }

    theDefinition = std::move(aDefinition);
    return true;
}

bool ReanimationDefinitionMapper::MapTrack(
    const engine::XmlNode& theNode,
    ReanimationTrack& theTrack,
    std::size_t& theTransformCount)
{
    ReanimationTrack aTrack;
    ReanimationTransform aPrevious;
    for (const auto& aChild : theNode.mChildren)
    {
        if (NamesEqual(aChild.mName, "name"))
        {
            if (!ReadString(aChild, aTrack.mName))
                return false;
            continue;
        }

        if (NamesEqual(aChild.mName, "t"))
        {
            if (!HasOnlyWhitespace(aChild.mValue))
            {
                Fail(
                    ReanimationDefinitionError::InvalidElementShape,
                    aChild);
                return false;
            }
            if (theTransformCount >= kMaximumTransforms)
            {
                Fail(
                    ReanimationDefinitionError::TooManyTransforms,
                    aChild);
                return false;
            }

            ReanimationTransform aTransform;
            if (!MapTransform(aChild, aPrevious, aTransform))
                return false;
            aTrack.mTransforms.push_back(aTransform);
            aPrevious = std::move(aTransform);
            ++theTransformCount;
            continue;
        }

        Fail(ReanimationDefinitionError::UnknownElement, aChild);
        return false;
    }

    theTrack = std::move(aTrack);
    return true;
}

bool ReanimationDefinitionMapper::MapTransform(
    const engine::XmlNode& theNode,
    const ReanimationTransform& thePrevious,
    ReanimationTransform& theTransform)
{
    ReanimationTransform aTransform = thePrevious;
    for (const auto& aChild : theNode.mChildren)
    {
        float* aFloatValue{};
        if (NamesEqual(aChild.mName, "x"))
            aFloatValue = &aTransform.mTranslationX;
        else if (NamesEqual(aChild.mName, "y"))
            aFloatValue = &aTransform.mTranslationY;
        else if (NamesEqual(aChild.mName, "kx"))
            aFloatValue = &aTransform.mSkewX;
        else if (NamesEqual(aChild.mName, "ky"))
            aFloatValue = &aTransform.mSkewY;
        else if (NamesEqual(aChild.mName, "sx"))
            aFloatValue = &aTransform.mScaleX;
        else if (NamesEqual(aChild.mName, "sy"))
            aFloatValue = &aTransform.mScaleY;
        else if (NamesEqual(aChild.mName, "f"))
            aFloatValue = &aTransform.mFrame;
        else if (NamesEqual(aChild.mName, "a"))
            aFloatValue = &aTransform.mAlpha;

        if (aFloatValue != nullptr)
        {
            if (!ReadFloat(aChild, *aFloatValue))
                return false;
            continue;
        }

        std::string* aStringValue{};
        if (NamesEqual(aChild.mName, "i"))
            aStringValue = &aTransform.mImageId;
        else if (NamesEqual(aChild.mName, "font"))
            aStringValue = &aTransform.mFontId;
        else if (NamesEqual(aChild.mName, "text"))
            aStringValue = &aTransform.mText;

        if (aStringValue != nullptr)
        {
            std::string aValue;
            if (!ReadString(aChild, aValue))
                return false;
            if (!aValue.empty())
                *aStringValue = std::move(aValue);
            continue;
        }

        Fail(ReanimationDefinitionError::UnknownElement, aChild);
        return false;
    }

    theTransform = std::move(aTransform);
    return true;
}

bool ReanimationDefinitionMapper::ReadFloat(
    const engine::XmlNode& theNode,
    float& theValue)
{
    if (!theNode.mChildren.empty())
    {
        Fail(ReanimationDefinitionError::InvalidElementShape, theNode);
        return false;
    }

    const auto aText = TrimAscii(theNode.mValue);
    float aValue{};
    const auto [anEnd, anError] = std::from_chars(
        aText.data(),
        aText.data() + aText.size(),
        aValue);
    if (aText.empty() ||
        anError != std::errc{} ||
        anEnd != aText.data() + aText.size() ||
        !std::isfinite(aValue))
    {
        Fail(ReanimationDefinitionError::InvalidFloat, theNode);
        return false;
    }

    theValue = aValue;
    return true;
}

bool ReanimationDefinitionMapper::ReadString(
    const engine::XmlNode& theNode,
    std::string& theValue)
{
    if (!theNode.mChildren.empty())
    {
        Fail(ReanimationDefinitionError::InvalidElementShape, theNode);
        return false;
    }
    theValue = theNode.mValue;
    return true;
}

void ReanimationDefinitionMapper::Reset()
{
    mError = ReanimationDefinitionError::None;
    mErrorLine = 0;
    mErrorElement.clear();
}

void ReanimationDefinitionMapper::Fail(
    ReanimationDefinitionError theError,
    const engine::XmlNode& theNode)
{
    if (mError != ReanimationDefinitionError::None)
        return;
    mError = theError;
    mErrorLine = theNode.mLine;
    mErrorElement = theNode.mName;
}

ReanimationDefinitionError ReanimationDefinitionMapper::GetError() const
{
    return mError;
}

std::uint32_t ReanimationDefinitionMapper::GetErrorLine() const
{
    return mErrorLine;
}

std::string_view ReanimationDefinitionMapper::GetErrorElement() const
{
    return mErrorElement;
}

const char* GetReanimationDefinitionErrorMessage(
    ReanimationDefinitionError theError)
{
    switch (theError)
    {
    case ReanimationDefinitionError::None:
        return "no error";
    case ReanimationDefinitionError::EmptyDocument:
        return "reanimation definition is empty";
    case ReanimationDefinitionError::UnknownElement:
        return "reanimation definition contains an unknown element";
    case ReanimationDefinitionError::InvalidElementShape:
        return "reanimation definition element has invalid contents";
    case ReanimationDefinitionError::InvalidFloat:
        return "reanimation definition contains an invalid number";
    case ReanimationDefinitionError::InvalidFramesPerSecond:
        return "reanimation frame rate must be positive";
    case ReanimationDefinitionError::TooManyTracks:
        return "reanimation definition contains too many tracks";
    case ReanimationDefinitionError::TooManyTransforms:
        return "reanimation definition contains too many transforms";
    }
    return "unknown reanimation definition error";
}

} // namespace pvz::game
