#pragma once

#include "pvz/engine/Xml.h"
#include "pvz/game/ParameterTrack.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace pvz::game
{

enum class TrailFlag : std::uint8_t
{
    Loops = 0,
};

struct TrailDefinition
{
    std::string mImageId;
    std::int32_t mMaximumPoints{2};
    float mMinimumPointDistance{1.0F};
    std::uint32_t mFlags{};
    ParameterTrack mTrailDuration;
    ParameterTrack mWidthOverLength;
    ParameterTrack mWidthOverTime;
    ParameterTrack mAlphaOverLength;
    ParameterTrack mAlphaOverTime;
};

enum class TrailDefinitionError : std::uint8_t
{
    None,
    EmptyDocument,
    UnknownElement,
    InvalidElementShape,
    InvalidInteger,
    InvalidFloat,
    InvalidFlag,
    InvalidParameterTrack,
};

class TrailDefinitionMapper
{
public:
    [[nodiscard]] bool Map(
        std::span<const engine::XmlNode> theRoots,
        TrailDefinition& theDefinition);

    [[nodiscard]] TrailDefinitionError GetError() const;
    [[nodiscard]] ParameterTrackError GetParameterTrackError() const;
    [[nodiscard]] std::size_t GetParameterTrackErrorOffset() const;
    [[nodiscard]] std::uint32_t GetErrorLine() const;
    [[nodiscard]] std::string_view GetErrorElement() const;

private:
    [[nodiscard]] bool ReadInteger(
        const engine::XmlNode& theNode,
        std::int32_t& theValue);
    [[nodiscard]] bool ReadFloat(
        const engine::XmlNode& theNode,
        float& theValue);
    [[nodiscard]] bool ReadString(
        const engine::XmlNode& theNode,
        std::string& theValue);
    [[nodiscard]] bool ReadFlag(
        const engine::XmlNode& theNode,
        TrailFlag theFlag,
        std::uint32_t& theFlags);
    [[nodiscard]] bool ReadParameterTrack(
        const engine::XmlNode& theNode,
        ParameterTrack& theTrack);
    void Reset();
    void Fail(
        TrailDefinitionError theError,
        const engine::XmlNode& theNode);

    TrailDefinitionError mError{TrailDefinitionError::None};
    ParameterTrackError mParameterTrackError{ParameterTrackError::None};
    std::size_t mParameterTrackErrorOffset{};
    std::uint32_t mErrorLine{};
    std::string mErrorElement;
};

[[nodiscard]] bool HasTrailFlag(
    std::uint32_t theFlags,
    TrailFlag theFlag);

[[nodiscard]] const char* GetTrailDefinitionErrorMessage(
    TrailDefinitionError theError);

} // namespace pvz::game
