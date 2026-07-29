#pragma once

#include "pvz/engine/Xml.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace pvz::engine::core
{

using ::pvz::engine::XmlAttribute;

enum class XmlTokenType : std::uint8_t
{
    StartElement,
    EndElement,
    Text,
    Instruction,
    Comment,
};

enum class XmlError : std::uint8_t
{
    None,
    UnexpectedEnd,
    MalformedToken,
    InvalidEntity,
    MismatchedEndElement,
    UnsupportedDeclaration,
};

struct XmlToken
{
    XmlTokenType mType{XmlTokenType::Text};
    std::string mName;
    std::string mValue;
    std::vector<XmlAttribute> mAttributes;
    std::uint32_t mLine{};
};

class XmlPullParser
{
public:
    void Reset(std::string_view theUtf8);
    [[nodiscard]] bool Next(XmlToken& theToken);

    [[nodiscard]] XmlError GetError() const;
    [[nodiscard]] std::uint32_t GetLine() const;
    [[nodiscard]] bool IsComplete() const;

private:
    [[nodiscard]] bool ParseMarkup(XmlToken& theToken);
    [[nodiscard]] bool ParseStartElement(XmlToken& theToken);
    [[nodiscard]] bool ParseEndElement(XmlToken& theToken);
    [[nodiscard]] bool ParseComment(XmlToken& theToken);
    [[nodiscard]] bool ParseInstruction(XmlToken& theToken);
    [[nodiscard]] bool ParseCData(XmlToken& theToken);
    [[nodiscard]] bool ParseName(std::string& theName);
    [[nodiscard]] bool DecodeEntities(
        std::string_view theEncoded,
        std::string& theDecoded);
    [[nodiscard]] bool Consume(std::string_view theText);
    [[nodiscard]] bool StartsWith(std::string_view theText) const;
    [[nodiscard]] char Peek() const;
    [[nodiscard]] char Advance();
    void SkipWhitespace();
    void Fail(XmlError theError);

    std::string mInput;
    std::vector<std::string> mElementStack;
    std::string mPendingEndElement;
    std::size_t mOffset{};
    std::uint32_t mLine{1};
    XmlError mError{XmlError::None};
};

[[nodiscard]] const char* GetXmlErrorMessage(XmlError theError);

} // namespace pvz::engine::core
