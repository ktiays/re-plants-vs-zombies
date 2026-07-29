#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace pvz::game
{

enum class ParameterCurve : std::uint8_t
{
    Constant = 0,
    Linear = 1,
    EaseIn = 2,
    EaseOut = 3,
    EaseInOut = 4,
    EaseInOutWeak = 5,
    FastInOut = 6,
    FastInOutWeak = 7,
    WeakFastInOut = 8,
    Bounce = 9,
    BounceFastMiddle = 10,
    BounceSlowMiddle = 11,
    SinWave = 12,
    EaseSinWave = 13,
};

struct ParameterTrackNode
{
    float mTime{};
    float mLowValue{};
    float mHighValue{};
    ParameterCurve mCurve{ParameterCurve::Linear};
    ParameterCurve mDistribution{ParameterCurve::Linear};
};

struct ParameterTrack
{
    std::vector<ParameterTrackNode> mNodes;
};

void SetParameterTrackDefault(
    ParameterTrack& theTrack,
    float theValue);

enum class ParameterTrackError : std::uint8_t
{
    None,
    EmptyTrack,
    InvalidNumber,
    InvalidSyntax,
    UnknownCurve,
    TooManyNodes,
};

class ParameterTrackParser
{
public:
    [[nodiscard]] bool Parse(
        std::string_view theText,
        ParameterTrack& theTrack);

    [[nodiscard]] ParameterTrackError GetError() const;
    [[nodiscard]] std::size_t GetErrorOffset() const;

private:
    [[nodiscard]] bool ParseNode(ParameterTrackNode& theNode);
    [[nodiscard]] bool ParseRange(ParameterTrackNode& theNode);
    [[nodiscard]] bool ParseNumber(float& theValue);
    [[nodiscard]] bool ParseOptionalTime(ParameterTrackNode& theNode);
    [[nodiscard]] bool TryParseCurve(ParameterCurve& theCurve);
    [[nodiscard]] bool TryParseIdentifier(std::string_view& theIdentifier);
    bool SkipWhitespace();
    [[nodiscard]] bool IsAtEnd() const;
    [[nodiscard]] char Peek() const;
    void InterpolateTimes(ParameterTrack& theTrack) const;
    void Fail(ParameterTrackError theError);

    std::string_view mInput;
    std::size_t mOffset{};
    ParameterTrackError mError{ParameterTrackError::None};
    std::size_t mErrorOffset{};
};

[[nodiscard]] const char* GetParameterTrackErrorMessage(
    ParameterTrackError theError);

} // namespace pvz::game
