#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace pvz::engine
{

struct XmlAttribute
{
    std::string mName;
    std::string mValue;
};

struct XmlNode
{
    std::string mName;
    std::string mValue;
    std::vector<XmlAttribute> mAttributes;
    std::vector<XmlNode> mChildren;
    std::uint32_t mLine{};

    [[nodiscard]] const std::string* FindAttribute(
        std::string_view theName) const
    {
        for (const auto& anAttribute : mAttributes)
        {
            if (anAttribute.mName == theName)
                return &anAttribute.mValue;
        }
        return nullptr;
    }
};

enum class XmlDocumentMode : std::uint8_t
{
    SingleRoot,
    Fragment,
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

struct XmlDocumentDiagnostic
{
    XmlDocumentError mError{XmlDocumentError::None};
    std::uint32_t mLine{};
};

class IXmlDocumentLoader
{
public:
    virtual ~IXmlDocumentLoader() = default;

    [[nodiscard]] virtual bool Load(
        std::string_view thePath,
        XmlDocumentMode theMode,
        std::vector<XmlNode>& theRoots,
        XmlDocumentDiagnostic& theDiagnostic) const = 0;
};

} // namespace pvz::engine
