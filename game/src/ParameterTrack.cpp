#include "pvz/game/ParameterTrack.h"

#include <array>
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

constexpr std::size_t kMaximumTrackNodes = 1'000'000;

struct CurveName
{
    std::string_view mName;
    ParameterCurve mCurve;
};

constexpr std::array<CurveName, 12> kCurveNames{{
    {"EaseInOutWeak", ParameterCurve::EaseInOutWeak},
    {"FastInOutWeak", ParameterCurve::FastInOutWeak},
    {"EaseInOut", ParameterCurve::EaseInOut},
    {"FastInOut", ParameterCurve::FastInOut},
    {"EaseIn", ParameterCurve::EaseIn},
    {"EaseOut", ParameterCurve::EaseOut},
    {"EaseSinWave", ParameterCurve::EaseSinWave},
    {"BounceFastMiddle", ParameterCurve::BounceFastMiddle},
    {"BounceSlowMiddle", ParameterCurve::BounceSlowMiddle},
    {"Bounce", ParameterCurve::Bounce},
    {"SinWave", ParameterCurve::SinWave},
    {"Linear", ParameterCurve::Linear},
}};

[[nodiscard]] bool IsAsciiWhitespace(char theCharacter)
{
    return theCharacter == ' ' ||
           theCharacter == '\t' ||
           theCharacter == '\r' ||
           theCharacter == '\n';
}

[[nodiscard]] bool IsAsciiLetter(char theCharacter)
{
    return (theCharacter >= 'A' && theCharacter <= 'Z') ||
           (theCharacter >= 'a' && theCharacter <= 'z');
}

} // namespace

void SetParameterTrackDefault(
    ParameterTrack& theTrack,
    float theValue)
{
    if (!theTrack.mNodes.empty() || theValue == 0.0F)
        return;

    theTrack.mNodes.push_back({
        .mTime = 0.0F,
        .mLowValue = theValue,
        .mHighValue = theValue,
        .mCurve = ParameterCurve::Constant,
        .mDistribution = ParameterCurve::Linear,
    });
}

bool ParameterTrackParser::Parse(
    std::string_view theText,
    ParameterTrack& theTrack)
{
    mInput = theText;
    mOffset = 0;
    mError = ParameterTrackError::None;
    mErrorOffset = 0;

    ParameterTrack aTrack;
    SkipWhitespace();
    if (IsAtEnd())
    {
        Fail(ParameterTrackError::EmptyTrack);
        return false;
    }

    while (!IsAtEnd())
    {
        if (aTrack.mNodes.size() >= kMaximumTrackNodes)
        {
            Fail(ParameterTrackError::TooManyNodes);
            return false;
        }

        ParameterTrackNode aNode{
            .mTime = -1.0F,
            .mLowValue = 0.0F,
            .mHighValue = 0.0F,
            .mCurve = ParameterCurve::Linear,
            .mDistribution = ParameterCurve::Linear,
        };
        if (!ParseNode(aNode))
            return false;
        aTrack.mNodes.push_back(aNode);
        SkipWhitespace();
    }

    InterpolateTimes(aTrack);
    theTrack = std::move(aTrack);
    return true;
}

bool ParameterTrackParser::ParseNode(ParameterTrackNode& theNode)
{
    const bool isRange = Peek() == '[';
    if (isRange)
    {
        if (!ParseRange(theNode))
            return false;
    }
    else
    {
        if (!ParseNumber(theNode.mLowValue))
            return false;
        theNode.mHighValue = theNode.mLowValue;
    }

    if (!ParseOptionalTime(theNode))
        return false;

    const auto aBeforeWhitespace = mOffset;
    const bool hadWhitespace = SkipWhitespace();
    if (!isRange && hadWhitespace && !IsAtEnd() &&
        IsAsciiLetter(Peek()))
    {
        if (!TryParseCurve(theNode.mCurve))
        {
            Fail(ParameterTrackError::UnknownCurve);
            return false;
        }
    }
    else if (!hadWhitespace)
    {
        mOffset = aBeforeWhitespace;
    }
    return true;
}

bool ParameterTrackParser::ParseRange(ParameterTrackNode& theNode)
{
    ++mOffset;
    SkipWhitespace();
    if (!ParseNumber(theNode.mLowValue))
        return false;
    theNode.mHighValue = theNode.mLowValue;

    const bool hadWhitespace = SkipWhitespace();
    if (IsAtEnd())
    {
        Fail(ParameterTrackError::InvalidSyntax);
        return false;
    }
    if (Peek() == ']')
    {
        ++mOffset;
        return true;
    }
    if (IsAsciiLetter(Peek()))
    {
        if (!hadWhitespace)
        {
            Fail(ParameterTrackError::InvalidSyntax);
            return false;
        }
        if (!TryParseCurve(theNode.mDistribution))
        {
            Fail(ParameterTrackError::UnknownCurve);
            return false;
        }
        if (!SkipWhitespace())
        {
            Fail(ParameterTrackError::InvalidSyntax);
            return false;
        }
    }

    if (!ParseNumber(theNode.mHighValue))
        return false;
    SkipWhitespace();
    if (IsAtEnd() || Peek() != ']')
    {
        Fail(ParameterTrackError::InvalidSyntax);
        return false;
    }
    ++mOffset;
    return true;
}

bool ParameterTrackParser::ParseNumber(float& theValue)
{
    if (IsAtEnd())
    {
        Fail(ParameterTrackError::InvalidNumber);
        return false;
    }

    float aValue{};
    const char* aStart = mInput.data() + mOffset;
    const char* anInputEnd = mInput.data() + mInput.size();
    const auto [anEnd, anError] =
        std::from_chars(aStart, anInputEnd, aValue);
    if (anError != std::errc{} ||
        anEnd == aStart ||
        !std::isfinite(aValue))
    {
        Fail(ParameterTrackError::InvalidNumber);
        return false;
    }

    mOffset += static_cast<std::size_t>(anEnd - aStart);
    theValue = aValue;
    return true;
}

bool ParameterTrackParser::ParseOptionalTime(
    ParameterTrackNode& theNode)
{
    if (IsAtEnd() || Peek() != ',')
        return true;

    ++mOffset;
    float aPercentage{};
    if (!ParseNumber(aPercentage))
        return false;
    theNode.mTime = aPercentage * 0.01F;
    return true;
}

bool ParameterTrackParser::TryParseCurve(ParameterCurve& theCurve)
{
    std::string_view anIdentifier;
    if (!TryParseIdentifier(anIdentifier))
        return false;
    for (const auto& aCurveName : kCurveNames)
    {
        if (anIdentifier == aCurveName.mName)
        {
            theCurve = aCurveName.mCurve;
            return true;
        }
    }
    return false;
}

bool ParameterTrackParser::TryParseIdentifier(
    std::string_view& theIdentifier)
{
    const auto aStart = mOffset;
    while (!IsAtEnd() && IsAsciiLetter(Peek()))
        ++mOffset;
    if (mOffset == aStart)
        return false;
    theIdentifier = mInput.substr(aStart, mOffset - aStart);
    return true;
}

bool ParameterTrackParser::SkipWhitespace()
{
    const auto aStart = mOffset;
    while (!IsAtEnd() && IsAsciiWhitespace(Peek()))
        ++mOffset;
    return mOffset != aStart;
}

bool ParameterTrackParser::IsAtEnd() const
{
    return mOffset >= mInput.size();
}

char ParameterTrackParser::Peek() const
{
    return IsAtEnd() ? '\0' : mInput[mOffset];
}

void ParameterTrackParser::InterpolateTimes(
    ParameterTrack& theTrack) const
{
    std::size_t aBaseIndex{};
    float aLowTime{};
    while (aBaseIndex < theTrack.mNodes.size())
    {
        std::size_t anExplicitIndex = aBaseIndex;
        while (anExplicitIndex < theTrack.mNodes.size() &&
               theTrack.mNodes[anExplicitIndex].mTime < 0.0F)
        {
            ++anExplicitIndex;
        }

        const float aHighTime =
            anExplicitIndex < theTrack.mNodes.size()
                ? theTrack.mNodes[anExplicitIndex].mTime
                : 1.0F;
        for (std::size_t anIndex = aBaseIndex;
             anIndex < anExplicitIndex;
             ++anIndex)
        {
            float anInterpolation{};
            if (anExplicitIndex > aBaseIndex + 1)
            {
                anInterpolation =
                    static_cast<float>(anIndex - aBaseIndex) /
                    static_cast<float>(
                        anExplicitIndex - aBaseIndex - 1);
            }
            else if (aBaseIndex != 0)
            {
                anInterpolation = 1.0F;
            }

            theTrack.mNodes[anIndex].mTime =
                aHighTime * anInterpolation +
                aLowTime * (1.0F - anInterpolation);
        }

        if (anExplicitIndex >= theTrack.mNodes.size())
            break;
        aLowTime = aHighTime;
        aBaseIndex = anExplicitIndex + 1;
    }
}

void ParameterTrackParser::Fail(ParameterTrackError theError)
{
    if (mError != ParameterTrackError::None)
        return;
    mError = theError;
    mErrorOffset = mOffset;
}

ParameterTrackError ParameterTrackParser::GetError() const
{
    return mError;
}

std::size_t ParameterTrackParser::GetErrorOffset() const
{
    return mErrorOffset;
}

const char* GetParameterTrackErrorMessage(ParameterTrackError theError)
{
    switch (theError)
    {
    case ParameterTrackError::None:
        return "no error";
    case ParameterTrackError::EmptyTrack:
        return "parameter track is empty";
    case ParameterTrackError::InvalidNumber:
        return "parameter track contains an invalid number";
    case ParameterTrackError::InvalidSyntax:
        return "parameter track syntax is invalid";
    case ParameterTrackError::UnknownCurve:
        return "parameter track contains an unknown curve";
    case ParameterTrackError::TooManyNodes:
        return "parameter track contains too many nodes";
    }
    return "unknown parameter track error";
}

} // namespace pvz::game
