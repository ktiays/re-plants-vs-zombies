#include "pvz/engine/core/XmlPullParser.h"

#include <string_view>

void Expect(bool theCondition, const char* theMessage);

namespace
{

using pvz::engine::core::XmlError;
using pvz::engine::core::XmlPullParser;
using pvz::engine::core::XmlToken;
using pvz::engine::core::XmlTokenType;

void TestXmlTokenStream()
{
    constexpr std::string_view kXml =
        "<?xml version=\"1.0\"?>\n"
        "<root answer=\"42\" escaped=\"A&amp;B\">\n"
        "  <!-- note -->\n"
        "  <child enabled='yes'/>\n"
        "  hello   &lt;PvZ&gt;\n"
        "  <![CDATA[x<y]]>\n"
        "</root>\n";

    XmlPullParser aParser;
    aParser.Reset(kXml);
    XmlToken aToken;

    Expect(aParser.Next(aToken), "XML instruction token");
    Expect(
        aToken.mType == XmlTokenType::Instruction &&
            aToken.mName == "xml" &&
            aToken.mValue == "version=\"1.0\"",
        "XML instruction contents");

    Expect(aParser.Next(aToken), "XML root start token");
    Expect(
        aToken.mType == XmlTokenType::StartElement &&
            aToken.mName == "root" &&
            aToken.mAttributes.size() == 2,
        "XML root start contents");
    if (aToken.mAttributes.size() == 2)
    {
        Expect(
            aToken.mAttributes[0].mName == "answer" &&
                aToken.mAttributes[0].mValue == "42",
            "XML first attribute");
        Expect(
            aToken.mAttributes[1].mName == "escaped" &&
                aToken.mAttributes[1].mValue == "A&B",
            "XML escaped attribute");
    }

    Expect(aParser.Next(aToken), "XML comment token");
    Expect(
        aToken.mType == XmlTokenType::Comment &&
            aToken.mValue == " note ",
        "XML comment contents");

    Expect(aParser.Next(aToken), "XML child start token");
    Expect(
        aToken.mType == XmlTokenType::StartElement &&
            aToken.mName == "child",
        "XML child start contents");
    Expect(aParser.Next(aToken), "XML synthetic child end token");
    Expect(
        aToken.mType == XmlTokenType::EndElement &&
            aToken.mName == "child",
        "XML self-closing element produces end token");

    Expect(aParser.Next(aToken), "XML text token");
    Expect(
        aToken.mType == XmlTokenType::Text &&
            aToken.mValue == "hello <PvZ>",
        "XML text whitespace and entities");

    Expect(aParser.Next(aToken), "XML CDATA token");
    Expect(
        aToken.mType == XmlTokenType::Text &&
            aToken.mValue == "x<y",
        "XML CDATA contents");

    Expect(aParser.Next(aToken), "XML root end token");
    Expect(
        aToken.mType == XmlTokenType::EndElement &&
            aToken.mName == "root",
        "XML root end contents");
    Expect(!aParser.Next(aToken), "XML token stream ends");
    Expect(aParser.IsComplete(), "XML token stream is complete");
    Expect(aParser.GetError() == XmlError::None, "XML stream has no error");
}

void TestXmlNumericEntities()
{
    XmlPullParser aParser;
    aParser.Reset(
        "<v>&#80;&#x76;&#90;&#x1F33B;&nbsp;A&cr;B</v>");
    XmlToken aToken;
    Expect(aParser.Next(aToken), "numeric entity start");
    Expect(aParser.Next(aToken), "numeric entity text");
    Expect(
        aToken.mValue == "PvZ\xF0\x9F\x8C\xBB A\nB",
        "numeric entities decode to UTF-8");
    Expect(aParser.Next(aToken), "numeric entity end");
    Expect(!aParser.Next(aToken), "numeric entity stream ends");
    Expect(aParser.IsComplete(), "numeric entity stream complete");
}

void TestXmlFailures()
{
    XmlPullParser aParser;
    XmlToken aToken;

    aParser.Reset("<root><child></root>");
    while (aParser.Next(aToken))
    {
    }
    Expect(
        aParser.GetError() == XmlError::MismatchedEndElement,
        "mismatched XML end element is rejected");

    aParser.Reset("<root>&unknown;</root>");
    while (aParser.Next(aToken))
    {
    }
    Expect(
        aParser.GetError() == XmlError::InvalidEntity,
        "unknown XML entity is rejected");

    aParser.Reset("<root>");
    while (aParser.Next(aToken))
    {
    }
    Expect(
        aParser.GetError() == XmlError::UnexpectedEnd,
        "truncated XML document is rejected");

    aParser.Reset("<!DOCTYPE root><root/>");
    Expect(!aParser.Next(aToken), "unsupported declaration is rejected");
    Expect(
        aParser.GetError() == XmlError::UnsupportedDeclaration,
        "unsupported declaration reports its error");
}

} // namespace

void RunXmlPullParserTests()
{
    TestXmlTokenStream();
    TestXmlNumericEntities();
    TestXmlFailures();
}
