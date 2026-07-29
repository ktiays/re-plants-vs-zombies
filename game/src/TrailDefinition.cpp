#include "pvz/game/TrailDefinition.h"

#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <system_error>
#include <utility>

namespace pvz::game
{
namespace
{

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

void ApplyTrailDefaults(TrailDefinition& theDefinition)
{
    SetParameterTrackDefault(theDefinition.mWidthOverLength, 1.0F);
    SetParameterTrackDefault(theDefinition.mWidthOverTime, 1.0F);
    SetParameterTrackDefault(theDefinition.mTrailDuration, 100.0F);
    SetParameterTrackDefault(theDefinition.mAlphaOverLength, 1.0F);
    SetParameterTrackDefault(theDefinition.mAlphaOverTime, 1.0F);
}

} // namespace

bool TrailDefinitionMapper::Map(
    std::span<const engine::XmlNode> theRoots,
    TrailDefinition& theDefinition)
{
    Reset();
    if (theRoots.empty())
    {
        mError = TrailDefinitionError::EmptyDocument;
        return false;
    }

    TrailDefinition aDefinition;
    for (const auto& aRoot : theRoots)
    {
        if (NamesEqual(aRoot.mName, "Image"))
        {
            if (!ReadString(aRoot, aDefinition.mImageId))
                return false;
        }
        else if (NamesEqual(aRoot.mName, "MaxPoints"))
        {
            if (!ReadInteger(aRoot, aDefinition.mMaximumPoints))
                return false;
        }
        else if (NamesEqual(aRoot.mName, "MinPointDistance"))
        {
            if (!ReadFloat(aRoot, aDefinition.mMinimumPointDistance))
                return false;
        }
        else if (NamesEqual(aRoot.mName, "Loops"))
        {
            if (!ReadFlag(aRoot, TrailFlag::Loops, aDefinition.mFlags))
                return false;
        }
        else
        {
            ParameterTrack* aTrack{};
            if (NamesEqual(aRoot.mName, "WidthOverLength"))
                aTrack = &aDefinition.mWidthOverLength;
            else if (NamesEqual(aRoot.mName, "WidthOverTime"))
                aTrack = &aDefinition.mWidthOverTime;
            else if (NamesEqual(aRoot.mName, "AlphaOverLength"))
                aTrack = &aDefinition.mAlphaOverLength;
            else if (NamesEqual(aRoot.mName, "AlphaOverTime"))
                aTrack = &aDefinition.mAlphaOverTime;
            else if (NamesEqual(aRoot.mName, "TrailDuration"))
                aTrack = &aDefinition.mTrailDuration;

            if (aTrack == nullptr)
            {
                Fail(TrailDefinitionError::UnknownElement, aRoot);
                return false;
            }
            if (!ReadParameterTrack(aRoot, *aTrack))
                return false;
        }
    }

    ApplyTrailDefaults(aDefinition);
    theDefinition = std::move(aDefinition);
    return true;
}

bool TrailDefinitionMapper::ReadInteger(
    const engine::XmlNode& theNode,
    std::int32_t& theValue)
{
    if (!theNode.mChildren.empty())
    {
        Fail(TrailDefinitionError::InvalidElementShape, theNode);
        return false;
    }

    const auto aText = TrimAscii(theNode.mValue);
    std::int32_t aValue{};
    const auto [anEnd, anError] = std::from_chars(
        aText.data(),
        aText.data() + aText.size(),
        aValue);
    if (aText.empty() ||
        anError != std::errc{} ||
        anEnd != aText.data() + aText.size())
    {
        Fail(TrailDefinitionError::InvalidInteger, theNode);
        return false;
    }

    theValue = aValue;
    return true;
}

bool TrailDefinitionMapper::ReadFloat(
    const engine::XmlNode& theNode,
    float& theValue)
{
    if (!theNode.mChildren.empty())
    {
        Fail(TrailDefinitionError::InvalidElementShape, theNode);
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
        Fail(TrailDefinitionError::InvalidFloat, theNode);
        return false;
    }

    theValue = aValue;
    return true;
}

bool TrailDefinitionMapper::ReadString(
    const engine::XmlNode& theNode,
    std::string& theValue)
{
    if (!theNode.mChildren.empty())
    {
        Fail(TrailDefinitionError::InvalidElementShape, theNode);
        return false;
    }
    theValue = theNode.mValue;
    return true;
}

bool TrailDefinitionMapper::ReadFlag(
    const engine::XmlNode& theNode,
    TrailFlag theFlag,
    std::uint32_t& theFlags)
{
    std::int32_t aValue{};
    if (!ReadInteger(theNode, aValue))
        return false;
    if (aValue != 0 && aValue != 1)
    {
        Fail(TrailDefinitionError::InvalidFlag, theNode);
        return false;
    }

    const auto aBit = static_cast<std::uint8_t>(theFlag);
    const std::uint32_t aMask = std::uint32_t{1} << aBit;
    if (aValue == 1)
        theFlags |= aMask;
    else
        theFlags &= ~aMask;
    return true;
}

bool TrailDefinitionMapper::ReadParameterTrack(
    const engine::XmlNode& theNode,
    ParameterTrack& theTrack)
{
    if (!theNode.mChildren.empty())
    {
        Fail(TrailDefinitionError::InvalidElementShape, theNode);
        return false;
    }

    ParameterTrackParser aParser;
    if (!aParser.Parse(theNode.mValue, theTrack))
    {
        mParameterTrackError = aParser.GetError();
        mParameterTrackErrorOffset = aParser.GetErrorOffset();
        Fail(TrailDefinitionError::InvalidParameterTrack, theNode);
        return false;
    }
    return true;
}

void TrailDefinitionMapper::Reset()
{
    mError = TrailDefinitionError::None;
    mParameterTrackError = ParameterTrackError::None;
    mParameterTrackErrorOffset = 0;
    mErrorLine = 0;
    mErrorElement.clear();
}

void TrailDefinitionMapper::Fail(
    TrailDefinitionError theError,
    const engine::XmlNode& theNode)
{
    if (mError != TrailDefinitionError::None)
        return;
    mError = theError;
    mErrorLine = theNode.mLine;
    mErrorElement = theNode.mName;
}

TrailDefinitionError TrailDefinitionMapper::GetError() const
{
    return mError;
}

ParameterTrackError TrailDefinitionMapper::GetParameterTrackError() const
{
    return mParameterTrackError;
}

std::size_t TrailDefinitionMapper::GetParameterTrackErrorOffset() const
{
    return mParameterTrackErrorOffset;
}

std::uint32_t TrailDefinitionMapper::GetErrorLine() const
{
    return mErrorLine;
}

std::string_view TrailDefinitionMapper::GetErrorElement() const
{
    return mErrorElement;
}

bool HasTrailFlag(
    std::uint32_t theFlags,
    TrailFlag theFlag)
{
    const auto aBit = static_cast<std::uint8_t>(theFlag);
    return (theFlags & (std::uint32_t{1} << aBit)) != 0;
}

const char* GetTrailDefinitionErrorMessage(
    TrailDefinitionError theError)
{
    switch (theError)
    {
    case TrailDefinitionError::None:
        return "no error";
    case TrailDefinitionError::EmptyDocument:
        return "trail definition is empty";
    case TrailDefinitionError::UnknownElement:
        return "trail definition contains an unknown element";
    case TrailDefinitionError::InvalidElementShape:
        return "trail definition element has invalid contents";
    case TrailDefinitionError::InvalidInteger:
        return "trail definition contains an invalid integer";
    case TrailDefinitionError::InvalidFloat:
        return "trail definition contains an invalid float";
    case TrailDefinitionError::InvalidFlag:
        return "trail definition flag must be zero or one";
    case TrailDefinitionError::InvalidParameterTrack:
        return "trail definition contains an invalid parameter track";
    }
    return "unknown trail definition error";
}

} // namespace pvz::game
