#include "pvz/engine/core/XmlPullParser.h"

#include <algorithm>
#include <limits>

namespace pvz::engine::core
{
namespace
{

[[nodiscard]] bool IsWhitespace(char theCharacter)
{
    return theCharacter == ' ' ||
           theCharacter == '\t' ||
           theCharacter == '\r' ||
           theCharacter == '\n';
}

[[nodiscard]] bool IsNameTerminator(char theCharacter)
{
    return IsWhitespace(theCharacter) ||
           theCharacter == '/' ||
           theCharacter == '>' ||
           theCharacter == '=' ||
           theCharacter == '?' ||
           theCharacter == '\0';
}

void TrimWhitespace(std::string& theText)
{
    const auto aFirst = std::find_if_not(
        theText.begin(),
        theText.end(),
        IsWhitespace);
    const auto aLast = std::find_if_not(
        theText.rbegin(),
        theText.rend(),
        IsWhitespace).base();
    if (aFirst >= aLast)
    {
        theText.clear();
        return;
    }
    theText.assign(aFirst, aLast);
}

void NormalizeTextWhitespace(std::string& theText)
{
    std::string aNormalized;
    aNormalized.reserve(theText.size());
    bool aPendingSpace = false;
    for (const char aCharacter : theText)
    {
        if (IsWhitespace(aCharacter))
        {
            if (!aNormalized.empty())
                aPendingSpace = true;
            continue;
        }

        if (aPendingSpace)
            aNormalized.push_back(' ');
        aNormalized.push_back(aCharacter);
        aPendingSpace = false;
    }
    theText = std::move(aNormalized);
}

[[nodiscard]] bool AppendUtf8(
    std::uint32_t theCodePoint,
    std::string& theOutput)
{
    if (theCodePoint == 0 ||
        theCodePoint > 0x10FFFF ||
        (theCodePoint >= 0xD800 && theCodePoint <= 0xDFFF))
    {
        return false;
    }

    if (theCodePoint <= 0x7F)
    {
        theOutput.push_back(static_cast<char>(theCodePoint));
    }
    else if (theCodePoint <= 0x7FF)
    {
        theOutput.push_back(
            static_cast<char>(0xC0 | (theCodePoint >> 6)));
        theOutput.push_back(
            static_cast<char>(0x80 | (theCodePoint & 0x3F)));
    }
    else if (theCodePoint <= 0xFFFF)
    {
        theOutput.push_back(
            static_cast<char>(0xE0 | (theCodePoint >> 12)));
        theOutput.push_back(
            static_cast<char>(0x80 | ((theCodePoint >> 6) & 0x3F)));
        theOutput.push_back(
            static_cast<char>(0x80 | (theCodePoint & 0x3F)));
    }
    else
    {
        theOutput.push_back(
            static_cast<char>(0xF0 | (theCodePoint >> 18)));
        theOutput.push_back(
            static_cast<char>(0x80 | ((theCodePoint >> 12) & 0x3F)));
        theOutput.push_back(
            static_cast<char>(0x80 | ((theCodePoint >> 6) & 0x3F)));
        theOutput.push_back(
            static_cast<char>(0x80 | (theCodePoint & 0x3F)));
    }
    return true;
}

} // namespace

void XmlPullParser::Reset(std::string_view theUtf8)
{
    mInput.assign(theUtf8);
    mElementStack.clear();
    mPendingEndElement.clear();
    mOffset =
        mInput.size() >= 3 &&
        static_cast<unsigned char>(mInput[0]) == 0xEF &&
        static_cast<unsigned char>(mInput[1]) == 0xBB &&
        static_cast<unsigned char>(mInput[2]) == 0xBF
            ? 3
            : 0;
    mLine = 1;
    mError = XmlError::None;
}

bool XmlPullParser::Next(XmlToken& theToken)
{
    if (mError != XmlError::None)
        return false;

    theToken = {};
    if (!mPendingEndElement.empty())
    {
        theToken.mType = XmlTokenType::EndElement;
        theToken.mName = std::move(mPendingEndElement);
        theToken.mLine = mLine;
        mPendingEndElement.clear();
        return true;
    }

    for (;;)
    {
        if (mOffset >= mInput.size())
        {
            if (!mElementStack.empty())
                Fail(XmlError::UnexpectedEnd);
            return false;
        }

        if (Peek() == '<')
            return ParseMarkup(theToken);

        const auto aStart = mOffset;
        const auto aLine = mLine;
        while (mOffset < mInput.size() && Peek() != '<')
            static_cast<void>(Advance());

        std::string aText(
            mInput.data() + aStart,
            mOffset - aStart);
        NormalizeTextWhitespace(aText);
        if (aText.empty())
            continue;

        theToken.mType = XmlTokenType::Text;
        theToken.mLine = aLine;
        if (!DecodeEntities(aText, theToken.mValue))
            return false;
        return true;
    }
}

XmlError XmlPullParser::GetError() const
{
    return mError;
}

std::uint32_t XmlPullParser::GetLine() const
{
    return mLine;
}

bool XmlPullParser::IsComplete() const
{
    return mError == XmlError::None &&
           mOffset == mInput.size() &&
           mElementStack.empty() &&
           mPendingEndElement.empty();
}

bool XmlPullParser::ParseMarkup(XmlToken& theToken)
{
    if (StartsWith("<!--"))
        return ParseComment(theToken);
    if (StartsWith("<?"))
        return ParseInstruction(theToken);
    if (StartsWith("<![CDATA["))
        return ParseCData(theToken);
    if (StartsWith("<!"))
    {
        Fail(XmlError::UnsupportedDeclaration);
        return false;
    }
    if (StartsWith("</"))
        return ParseEndElement(theToken);
    return ParseStartElement(theToken);
}

bool XmlPullParser::ParseStartElement(XmlToken& theToken)
{
    const auto aLine = mLine;
    if (!Consume("<") || !ParseName(theToken.mName))
    {
        Fail(XmlError::MalformedToken);
        return false;
    }

    theToken.mType = XmlTokenType::StartElement;
    theToken.mLine = aLine;
    for (;;)
    {
        SkipWhitespace();
        if (Consume(">"))
        {
            mElementStack.push_back(theToken.mName);
            return true;
        }
        if (Consume("/>"))
        {
            mPendingEndElement = theToken.mName;
            return true;
        }

        XmlAttribute anAttribute;
        if (!ParseName(anAttribute.mName))
        {
            Fail(XmlError::MalformedToken);
            return false;
        }

        const auto aDuplicate = std::ranges::find(
            theToken.mAttributes,
            anAttribute.mName,
            &XmlAttribute::mName);
        if (aDuplicate != theToken.mAttributes.end())
        {
            Fail(XmlError::MalformedToken);
            return false;
        }

        SkipWhitespace();
        if (!Consume("="))
        {
            Fail(XmlError::MalformedToken);
            return false;
        }
        SkipWhitespace();

        const char aQuote = Peek();
        if (aQuote != '"' && aQuote != '\'')
        {
            Fail(XmlError::MalformedToken);
            return false;
        }
        static_cast<void>(Advance());

        const auto aValueStart = mOffset;
        while (mOffset < mInput.size() && Peek() != aQuote)
            static_cast<void>(Advance());
        if (mOffset >= mInput.size())
        {
            Fail(XmlError::UnexpectedEnd);
            return false;
        }

        const std::string_view anEncodedValue(
            mInput.data() + aValueStart,
            mOffset - aValueStart);
        if (!DecodeEntities(anEncodedValue, anAttribute.mValue))
            return false;
        static_cast<void>(Advance());
        theToken.mAttributes.push_back(std::move(anAttribute));
    }
}

bool XmlPullParser::ParseEndElement(XmlToken& theToken)
{
    const auto aLine = mLine;
    if (!Consume("</") || !ParseName(theToken.mName))
    {
        Fail(XmlError::MalformedToken);
        return false;
    }
    SkipWhitespace();
    if (!Consume(">"))
    {
        Fail(XmlError::MalformedToken);
        return false;
    }

    if (mElementStack.empty() ||
        mElementStack.back() != theToken.mName)
    {
        Fail(XmlError::MismatchedEndElement);
        return false;
    }
    mElementStack.pop_back();
    theToken.mType = XmlTokenType::EndElement;
    theToken.mLine = aLine;
    return true;
}

bool XmlPullParser::ParseComment(XmlToken& theToken)
{
    const auto aLine = mLine;
    if (!Consume("<!--"))
        return false;

    const auto aStart = mOffset;
    while (!StartsWith("-->"))
    {
        if (mOffset >= mInput.size())
        {
            Fail(XmlError::UnexpectedEnd);
            return false;
        }
        static_cast<void>(Advance());
    }

    theToken.mType = XmlTokenType::Comment;
    theToken.mValue.assign(mInput.data() + aStart, mOffset - aStart);
    theToken.mLine = aLine;
    return Consume("-->");
}

bool XmlPullParser::ParseInstruction(XmlToken& theToken)
{
    const auto aLine = mLine;
    if (!Consume("<?") || !ParseName(theToken.mName))
    {
        Fail(XmlError::MalformedToken);
        return false;
    }

    const auto aStart = mOffset;
    while (!StartsWith("?>"))
    {
        if (mOffset >= mInput.size())
        {
            Fail(XmlError::UnexpectedEnd);
            return false;
        }
        static_cast<void>(Advance());
    }

    theToken.mType = XmlTokenType::Instruction;
    theToken.mValue.assign(mInput.data() + aStart, mOffset - aStart);
    TrimWhitespace(theToken.mValue);
    theToken.mLine = aLine;
    return Consume("?>");
}

bool XmlPullParser::ParseCData(XmlToken& theToken)
{
    const auto aLine = mLine;
    if (!Consume("<![CDATA["))
        return false;

    const auto aStart = mOffset;
    while (!StartsWith("]]>"))
    {
        if (mOffset >= mInput.size())
        {
            Fail(XmlError::UnexpectedEnd);
            return false;
        }
        static_cast<void>(Advance());
    }

    theToken.mType = XmlTokenType::Text;
    theToken.mValue.assign(mInput.data() + aStart, mOffset - aStart);
    theToken.mLine = aLine;
    return Consume("]]>");
}

bool XmlPullParser::ParseName(std::string& theName)
{
    theName.clear();
    while (mOffset < mInput.size() && !IsNameTerminator(Peek()))
        theName.push_back(Advance());
    return !theName.empty();
}

bool XmlPullParser::DecodeEntities(
    std::string_view theEncoded,
    std::string& theDecoded)
{
    theDecoded.clear();
    theDecoded.reserve(theEncoded.size());
    for (std::size_t anIndex = 0; anIndex < theEncoded.size();)
    {
        if (theEncoded[anIndex] != '&')
        {
            theDecoded.push_back(theEncoded[anIndex]);
            ++anIndex;
            continue;
        }

        const auto anEnd = theEncoded.find(';', anIndex + 1);
        if (anEnd == std::string_view::npos)
        {
            Fail(XmlError::InvalidEntity);
            return false;
        }

        const auto anEntity =
            theEncoded.substr(anIndex + 1, anEnd - anIndex - 1);
        if (anEntity == "lt")
            theDecoded.push_back('<');
        else if (anEntity == "gt")
            theDecoded.push_back('>');
        else if (anEntity == "amp")
            theDecoded.push_back('&');
        else if (anEntity == "quot")
            theDecoded.push_back('"');
        else if (anEntity == "apos")
            theDecoded.push_back('\'');
        else if (anEntity == "nbsp")
            theDecoded.push_back(' ');
        else if (anEntity == "cr")
            theDecoded.push_back('\n');
        else if (anEntity.starts_with("#"))
        {
            const bool isHex =
                anEntity.size() > 1 &&
                (anEntity[1] == 'x' || anEntity[1] == 'X');
            const std::size_t aDigitStart = isHex ? 2 : 1;
            if (aDigitStart >= anEntity.size())
            {
                Fail(XmlError::InvalidEntity);
                return false;
            }

            std::uint32_t aCodePoint = 0;
            const std::uint32_t aBase = isHex ? 16 : 10;
            for (std::size_t aDigitIndex = aDigitStart;
                 aDigitIndex < anEntity.size();
                 ++aDigitIndex)
            {
                const char aDigit = anEntity[aDigitIndex];
                std::uint32_t aValue{};
                if (aDigit >= '0' && aDigit <= '9')
                {
                    aValue = static_cast<std::uint32_t>(aDigit - '0');
                }
                else if (isHex && aDigit >= 'a' && aDigit <= 'f')
                {
                    aValue =
                        static_cast<std::uint32_t>(aDigit - 'a' + 10);
                }
                else if (isHex && aDigit >= 'A' && aDigit <= 'F')
                {
                    aValue =
                        static_cast<std::uint32_t>(aDigit - 'A' + 10);
                }
                else
                {
                    Fail(XmlError::InvalidEntity);
                    return false;
                }

                if (aCodePoint >
                    (std::numeric_limits<std::uint32_t>::max() - aValue) /
                        aBase)
                {
                    Fail(XmlError::InvalidEntity);
                    return false;
                }
                aCodePoint = aCodePoint * aBase + aValue;
            }

            if (!AppendUtf8(aCodePoint, theDecoded))
            {
                Fail(XmlError::InvalidEntity);
                return false;
            }
        }
        else
        {
            Fail(XmlError::InvalidEntity);
            return false;
        }
        anIndex = anEnd + 1;
    }
    return true;
}

bool XmlPullParser::Consume(std::string_view theText)
{
    if (!StartsWith(theText))
        return false;
    for (std::size_t anIndex = 0; anIndex < theText.size(); ++anIndex)
        static_cast<void>(Advance());
    return true;
}

bool XmlPullParser::StartsWith(std::string_view theText) const
{
    return theText.size() <= mInput.size() - mOffset &&
           std::string_view(mInput).substr(mOffset, theText.size()) ==
               theText;
}

char XmlPullParser::Peek() const
{
    if (mOffset >= mInput.size())
        return '\0';
    return mInput[mOffset];
}

char XmlPullParser::Advance()
{
    const char aCharacter = Peek();
    if (mOffset < mInput.size())
        ++mOffset;
    if (aCharacter == '\n' && mLine < std::numeric_limits<std::uint32_t>::max())
        ++mLine;
    return aCharacter;
}

void XmlPullParser::SkipWhitespace()
{
    while (mOffset < mInput.size() && IsWhitespace(Peek()))
        static_cast<void>(Advance());
}

void XmlPullParser::Fail(XmlError theError)
{
    if (mError == XmlError::None)
        mError = theError;
}

const char* GetXmlErrorMessage(XmlError theError)
{
    switch (theError)
    {
    case XmlError::None:
        return "no error";
    case XmlError::UnexpectedEnd:
        return "XML input ended unexpectedly";
    case XmlError::MalformedToken:
        return "XML token is malformed";
    case XmlError::InvalidEntity:
        return "XML entity is invalid";
    case XmlError::MismatchedEndElement:
        return "XML end element does not match its start element";
    case XmlError::UnsupportedDeclaration:
        return "XML declaration type is unsupported";
    }
    return "unknown XML error";
}

} // namespace pvz::engine::core
