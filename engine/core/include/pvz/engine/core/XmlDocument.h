#pragma once

#include "pvz/engine/Resources.h"
#include "pvz/engine/StateIO.h"
#include "pvz/engine/core/XmlPullParser.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pvz::engine::core
{

struct XmlNode
{
    std::string mName;
    std::string mValue;
    std::vector<XmlAttribute> mAttributes;
    std::vector<XmlNode> mChildren;
    std::uint32_t mLine{};

    [[nodiscard]] const std::string* FindAttribute(
        std::string_view theName) const;
};

enum class XmlDocumentError : std::uint8_t
{
    None,
    ResourceReadFailed,
    EmptyDocument,
    MultipleRoots,
    TextOutsideRoot,
    ParserFailed,
    InvalidCache,
    UnsupportedCacheVersion,
};

class XmlDocument
{
public:
    [[nodiscard]] bool Load(
        const IResourceStore& theResources,
        std::string_view thePath);
    [[nodiscard]] bool LoadFragment(
        const IResourceStore& theResources,
        std::string_view thePath);
    [[nodiscard]] bool Parse(std::string_view theUtf8);
    [[nodiscard]] bool ParseFragment(std::string_view theUtf8);
    [[nodiscard]] bool WriteCache(IStateWriter& theWriter) const;
    [[nodiscard]] bool ReadCache(IStateReader& theReader);

    [[nodiscard]] const XmlNode* GetRoot() const;
    [[nodiscard]] std::span<const XmlNode> GetRoots() const;
    [[nodiscard]] std::size_t GetNodeCount() const;
    [[nodiscard]] XmlDocumentError GetError() const;
    [[nodiscard]] XmlError GetParserError() const;
    [[nodiscard]] std::uint32_t GetErrorLine() const;

private:
    [[nodiscard]] bool LoadInternal(
        const IResourceStore& theResources,
        std::string_view thePath,
        bool theAllowMultipleRoots);
    [[nodiscard]] bool ParseInternal(
        std::string_view theUtf8,
        bool theAllowMultipleRoots);
    void Reset();
    void Fail(XmlDocumentError theError, std::uint32_t theLine);

    std::vector<XmlNode> mRoots;
    std::size_t mNodeCount{};
    XmlDocumentError mError{XmlDocumentError::None};
    XmlError mParserError{XmlError::None};
    std::uint32_t mErrorLine{};
};

[[nodiscard]] const char* GetXmlDocumentErrorMessage(
    XmlDocumentError theError);

} // namespace pvz::engine::core
