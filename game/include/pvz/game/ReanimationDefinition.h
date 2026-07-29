#pragma once

#include "pvz/engine/Xml.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pvz::game
{

struct ReanimationTransform
{
    float mTranslationX{};
    float mTranslationY{};
    float mSkewX{};
    float mSkewY{};
    float mScaleX{1.0F};
    float mScaleY{1.0F};
    float mFrame{};
    float mAlpha{1.0F};
    std::string mImageId;
    std::string mFontId;
    std::string mText;
};

struct ReanimationTrack
{
    std::string mName;
    std::vector<ReanimationTransform> mTransforms;
};

struct ReanimationDefinition
{
    std::vector<ReanimationTrack> mTracks;
    float mFramesPerSecond{12.0F};
};

enum class ReanimationDefinitionError : std::uint8_t
{
    None,
    EmptyDocument,
    UnknownElement,
    InvalidElementShape,
    InvalidFloat,
    InvalidFramesPerSecond,
    TooManyTracks,
    TooManyTransforms,
};

class ReanimationDefinitionMapper
{
public:
    [[nodiscard]] bool Map(
        std::span<const engine::XmlNode> theRoots,
        ReanimationDefinition& theDefinition);

    [[nodiscard]] ReanimationDefinitionError GetError() const;
    [[nodiscard]] std::uint32_t GetErrorLine() const;
    [[nodiscard]] std::string_view GetErrorElement() const;

private:
    [[nodiscard]] bool MapTrack(
        const engine::XmlNode& theNode,
        ReanimationTrack& theTrack,
        std::size_t& theTransformCount);
    [[nodiscard]] bool MapTransform(
        const engine::XmlNode& theNode,
        const ReanimationTransform& thePrevious,
        ReanimationTransform& theTransform);
    [[nodiscard]] bool ReadFloat(
        const engine::XmlNode& theNode,
        float& theValue);
    [[nodiscard]] bool ReadString(
        const engine::XmlNode& theNode,
        std::string& theValue);
    void Reset();
    void Fail(
        ReanimationDefinitionError theError,
        const engine::XmlNode& theNode);

    ReanimationDefinitionError mError{
        ReanimationDefinitionError::None};
    std::uint32_t mErrorLine{};
    std::string mErrorElement;
};

[[nodiscard]] const char* GetReanimationDefinitionErrorMessage(
    ReanimationDefinitionError theError);

} // namespace pvz::game
