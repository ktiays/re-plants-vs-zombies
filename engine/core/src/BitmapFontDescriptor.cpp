#include "pvz/engine/core/BitmapFontDescriptor.h"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pvz::engine::core
{
namespace
{

struct DescriptorValue
{
    std::string mScalar;
    std::vector<DescriptorValue> mItems;
    std::uint32_t mLine{};
    bool mIsList{};
    bool mIsQuoted{};
};

struct DescriptorCommand
{
    std::string mName;
    std::vector<DescriptorValue> mArguments;
    std::uint32_t mLine{};
};

class SyntaxParser
{
public:
    SyntaxParser(
        std::span<const std::byte> theBytes,
        BitmapFontDescriptorDiagnostic& theDiagnostic)
        : mBytes(theBytes),
          mDiagnostic(theDiagnostic)
    {
    }

    [[nodiscard]] bool Parse(
        std::vector<DescriptorCommand>& theCommands)
    {
        while (true)
        {
            SkipSeparators(true);
            if (AtEnd())
                return true;

            DescriptorValue aCommandName;
            if (!ParseValue(aCommandName) ||
                aCommandName.mIsList ||
                aCommandName.mScalar.empty())
            {
                return Fail(aCommandName.mLine);
            }

            DescriptorCommand aCommand{
                .mName = std::move(aCommandName.mScalar),
                .mLine = aCommandName.mLine,
            };
            while (true)
            {
                SkipSeparators(false);
                if (AtEnd())
                {
                    theCommands.push_back(std::move(aCommand));
                    return true;
                }
                if (Peek() == ';')
                {
                    Advance();
                    break;
                }
                DescriptorValue anArgument;
                if (!ParseValue(anArgument))
                    return false;
                aCommand.mArguments.push_back(std::move(anArgument));
            }
            theCommands.push_back(std::move(aCommand));
        }
    }

private:
    [[nodiscard]] bool ParseValue(DescriptorValue& theValue)
    {
        SkipSeparators(false);
        if (AtEnd())
            return Fail(mLine);

        theValue.mLine = mLine;
        const char aFirst = Peek();
        if (aFirst == '(')
        {
            theValue.mIsList = true;
            Advance();
            while (true)
            {
                SkipSeparators(false);
                if (AtEnd() || Peek() == ';')
                    return Fail(theValue.mLine);
                if (Peek() == ')')
                {
                    Advance();
                    return true;
                }
                DescriptorValue anItem;
                if (!ParseValue(anItem))
                    return false;
                theValue.mItems.push_back(std::move(anItem));
            }
        }
        if (aFirst == ')' || aFirst == ';')
            return Fail(theValue.mLine);

        if (aFirst == '\'' || aFirst == '"')
        {
            theValue.mIsQuoted = true;
            const char aQuote = aFirst;
            Advance();
            bool isEscaped = false;
            while (!AtEnd())
            {
                const char aCharacter = Peek();
                Advance();
                if (isEscaped)
                {
                    theValue.mScalar.push_back(aCharacter);
                    isEscaped = false;
                }
                else if (aCharacter == '\\')
                {
                    isEscaped = true;
                }
                else if (aCharacter == aQuote)
                {
                    return true;
                }
                else
                {
                    theValue.mScalar.push_back(aCharacter);
                }
            }
            return Fail(theValue.mLine);
        }

        while (!AtEnd())
        {
            const char aCharacter = Peek();
            if (aCharacter == ' ' ||
                aCharacter == '\t' ||
                aCharacter == '\r' ||
                aCharacter == '\n' ||
                aCharacter == ',' ||
                aCharacter == '(' ||
                aCharacter == ')' ||
                aCharacter == ';')
            {
                break;
            }
            theValue.mScalar.push_back(aCharacter);
            Advance();
        }
        return !theValue.mScalar.empty() || Fail(theValue.mLine);
    }

    void SkipSeparators(bool theIncludeCommands)
    {
        while (!AtEnd())
        {
            const char aCharacter = Peek();
            if (aCharacter == ' ' ||
                aCharacter == '\t' ||
                aCharacter == '\r' ||
                aCharacter == '\n' ||
                aCharacter == ',' ||
                (theIncludeCommands && aCharacter == ';'))
            {
                Advance();
                continue;
            }
            break;
        }
    }

    [[nodiscard]] bool AtEnd() const
    {
        return mOffset >= mBytes.size();
    }

    [[nodiscard]] char Peek() const
    {
        return static_cast<char>(
            std::to_integer<unsigned char>(mBytes[mOffset]));
    }

    void Advance()
    {
        if (Peek() == '\n' &&
            mLine != std::numeric_limits<std::uint32_t>::max())
        {
            ++mLine;
        }
        ++mOffset;
    }

    [[nodiscard]] bool Fail(std::uint32_t theLine)
    {
        mDiagnostic = {
            .mError = BitmapFontDescriptorError::Syntax,
            .mLine = theLine,
        };
        return false;
    }

    std::span<const std::byte> mBytes;
    BitmapFontDescriptorDiagnostic& mDiagnostic;
    std::size_t mOffset{};
    std::uint32_t mLine{1};
};

using DefinitionMap =
    std::unordered_map<std::string, DescriptorValue>;

[[nodiscard]] const DescriptorValue* GetList(
    const DescriptorValue& theValue,
    const DefinitionMap& theDefinitions)
{
    if (theValue.mIsList)
        return &theValue;
    if (theValue.mIsQuoted)
        return nullptr;
    const auto aDefinition = theDefinitions.find(theValue.mScalar);
    if (aDefinition == theDefinitions.end() ||
        !aDefinition->second.mIsList)
    {
        return nullptr;
    }
    return &aDefinition->second;
}

[[nodiscard]] bool GetScalar(
    const DescriptorValue& theValue,
    std::string_view& theScalar)
{
    if (theValue.mIsList)
        return false;
    theScalar = theValue.mScalar;
    return true;
}

[[nodiscard]] bool ParseI32(
    std::string_view theText,
    std::int32_t& theValue)
{
    if (theText.empty())
        return false;
    std::int32_t aValue{};
    const auto [anEnd, anError] = std::from_chars(
        theText.data(),
        theText.data() + theText.size(),
        aValue);
    if (anError != std::errc{} ||
        anEnd != theText.data() + theText.size())
    {
        return false;
    }
    theValue = aValue;
    return true;
}

[[nodiscard]] bool ParseU32(
    std::string_view theText,
    std::uint32_t& theValue)
{
    if (theText.empty())
        return false;
    std::uint32_t aValue{};
    const auto [anEnd, anError] = std::from_chars(
        theText.data(),
        theText.data() + theText.size(),
        aValue);
    if (anError != std::errc{} ||
        anEnd != theText.data() + theText.size())
    {
        return false;
    }
    theValue = aValue;
    return true;
}

[[nodiscard]] bool GetInteger(
    const DescriptorValue& theValue,
    std::int32_t& theInteger)
{
    std::string_view aScalar;
    return GetScalar(theValue, aScalar) &&
           ParseI32(aScalar, theInteger);
}

[[nodiscard]] bool GetPositiveInteger(
    const DescriptorValue& theValue,
    std::uint32_t& theInteger)
{
    std::string_view aScalar;
    return GetScalar(theValue, aScalar) &&
           ParseU32(aScalar, theInteger) &&
           theInteger != 0;
}

[[nodiscard]] std::vector<char32_t> DecodeCharacters(
    std::string_view theBytes)
{
    std::vector<char32_t> aCharacters;
    for (std::size_t anOffset = 0;
         anOffset < theBytes.size();)
    {
        const auto aLead = static_cast<std::uint8_t>(
            static_cast<unsigned char>(theBytes[anOffset]));
        if (aLead < 0x80)
        {
            aCharacters.push_back(aLead);
            ++anOffset;
            continue;
        }

        std::size_t aLength{};
        char32_t aCodePoint{};
        if ((aLead & 0xE0) == 0xC0)
        {
            aLength = 2;
            aCodePoint = aLead & 0x1F;
        }
        else if ((aLead & 0xF0) == 0xE0)
        {
            aLength = 3;
            aCodePoint = aLead & 0x0F;
        }
        else if ((aLead & 0xF8) == 0xF0)
        {
            aLength = 4;
            aCodePoint = aLead & 0x07;
        }
        else
        {
            aCharacters.push_back(aLead);
            ++anOffset;
            continue;
        }

        if (aLength > theBytes.size() - anOffset)
        {
            aCharacters.push_back(aLead);
            ++anOffset;
            continue;
        }
        bool isValid = true;
        for (std::size_t anIndex = 1;
             anIndex < aLength;
             ++anIndex)
        {
            const auto aContinuation = static_cast<std::uint8_t>(
                static_cast<unsigned char>(
                    theBytes[anOffset + anIndex]));
            if ((aContinuation & 0xC0) != 0x80)
            {
                isValid = false;
                break;
            }
            aCodePoint =
                (aCodePoint << 6) |
                static_cast<char32_t>(aContinuation & 0x3F);
        }
        if (!isValid ||
            aCodePoint > 0x10FFFF ||
            (aCodePoint >= 0xD800 && aCodePoint <= 0xDFFF))
        {
            aCharacters.push_back(aLead);
            ++anOffset;
            continue;
        }
        aCharacters.push_back(aCodePoint);
        anOffset += aLength;
    }
    return aCharacters;
}

[[nodiscard]] bool GetCharacterList(
    const DescriptorValue& theValue,
    const DefinitionMap& theDefinitions,
    std::vector<char32_t>& theCharacters)
{
    const auto* aList = GetList(theValue, theDefinitions);
    if (aList == nullptr)
        return false;
    theCharacters.clear();
    theCharacters.reserve(aList->mItems.size());
    for (const auto& anItem : aList->mItems)
    {
        std::string_view aText;
        if (!GetScalar(anItem, aText))
            return false;
        const auto aDecoded = DecodeCharacters(aText);
        if (aDecoded.size() != 1)
            return false;
        theCharacters.push_back(aDecoded.front());
    }
    return true;
}

[[nodiscard]] bool GetIntegerList(
    const DescriptorValue& theValue,
    const DefinitionMap& theDefinitions,
    std::vector<std::int32_t>& theIntegers)
{
    const auto* aList = GetList(theValue, theDefinitions);
    if (aList == nullptr)
        return false;
    theIntegers.clear();
    theIntegers.reserve(aList->mItems.size());
    for (const auto& anItem : aList->mItems)
    {
        std::int32_t anInteger{};
        if (!GetInteger(anItem, anInteger))
            return false;
        theIntegers.push_back(anInteger);
    }
    return true;
}

template <std::size_t Size>
[[nodiscard]] bool GetTupleList(
    const DescriptorValue& theValue,
    const DefinitionMap& theDefinitions,
    std::vector<std::array<std::int32_t, Size>>& theTuples)
{
    const auto* aList = GetList(theValue, theDefinitions);
    if (aList == nullptr)
        return false;
    theTuples.clear();
    theTuples.reserve(aList->mItems.size());
    for (const auto& anItem : aList->mItems)
    {
        if (!anItem.mIsList || anItem.mItems.size() != Size)
            return false;
        std::array<std::int32_t, Size> aTuple{};
        for (std::size_t anIndex = 0; anIndex < Size; ++anIndex)
        {
            if (!GetInteger(anItem.mItems[anIndex], aTuple[anIndex]))
                return false;
        }
        theTuples.push_back(aTuple);
    }
    return true;
}

[[nodiscard]] bool GetPairList(
    const DescriptorValue& theValue,
    const DefinitionMap& theDefinitions,
    std::vector<std::array<char32_t, 2>>& thePairs)
{
    const auto* aList = GetList(theValue, theDefinitions);
    if (aList == nullptr)
        return false;
    thePairs.clear();
    thePairs.reserve(aList->mItems.size());
    for (const auto& anItem : aList->mItems)
    {
        std::string_view aText;
        if (!GetScalar(anItem, aText))
            return false;
        const auto aDecoded = DecodeCharacters(aText);
        if (aDecoded.size() != 2)
            return false;
        thePairs.push_back({aDecoded[0], aDecoded[1]});
    }
    return true;
}

[[nodiscard]] BitmapFontLayerDescriptor* FindLayer(
    std::string_view theName,
    BitmapFontDescriptor& theDescriptor)
{
    for (auto& aLayer : theDescriptor.mLayers)
    {
        if (aLayer.mName == theName)
            return &aLayer;
    }
    return nullptr;
}

[[nodiscard]] bool GetLayer(
    const DescriptorCommand& theCommand,
    BitmapFontDescriptor& theDescriptor,
    BitmapFontLayerDescriptor*& theLayer,
    BitmapFontDescriptorDiagnostic& theDiagnostic)
{
    if (theCommand.mArguments.empty())
    {
        theDiagnostic = {
            .mError =
                BitmapFontDescriptorError::InvalidParameterCount,
            .mLine = theCommand.mLine,
            .mCommand = theCommand.mName,
        };
        return false;
    }
    std::string_view aLayerName;
    if (!GetScalar(theCommand.mArguments.front(), aLayerName))
    {
        theDiagnostic = {
            .mError = BitmapFontDescriptorError::InvalidParameter,
            .mLine = theCommand.mLine,
            .mCommand = theCommand.mName,
        };
        return false;
    }
    theLayer = FindLayer(aLayerName, theDescriptor);
    if (theLayer == nullptr)
    {
        theDiagnostic = {
            .mError = BitmapFontDescriptorError::UndefinedLayer,
            .mLine = theCommand.mLine,
            .mCommand = theCommand.mName,
        };
        return false;
    }
    return true;
}

[[nodiscard]] bool FailCommand(
    const DescriptorCommand& theCommand,
    BitmapFontDescriptorError theError,
    BitmapFontDescriptorDiagnostic& theDiagnostic)
{
    theDiagnostic = {
        .mError = theError,
        .mLine = theCommand.mLine,
        .mCommand = theCommand.mName,
    };
    return false;
}

[[nodiscard]] ColorRgba8 DecodeColor(std::uint32_t theColor)
{
    auto anAlpha = static_cast<std::uint8_t>((theColor >> 24) & 0xFF);
    if (anAlpha == 0)
        anAlpha = 255;
    return {
        .mRed = static_cast<std::uint8_t>((theColor >> 16) & 0xFF),
        .mGreen = static_cast<std::uint8_t>((theColor >> 8) & 0xFF),
        .mBlue = static_cast<std::uint8_t>(theColor & 0xFF),
        .mAlpha = anAlpha,
    };
}

} // namespace

bool BitmapFontDescriptorParser::Parse(
    std::span<const std::byte> theBytes,
    BitmapFontDescriptor& theDescriptor,
    BitmapFontDescriptorDiagnostic& theDiagnostic) const
{
    theDescriptor = {};
    theDiagnostic = {};

    std::vector<DescriptorCommand> aCommands;
    SyntaxParser aParser(theBytes, theDiagnostic);
    if (!aParser.Parse(aCommands))
        return false;

    DefinitionMap aDefinitions;
    for (const auto& aCommand : aCommands)
    {
        if (aCommand.mName == "Define")
        {
            if (aCommand.mArguments.size() != 2)
            {
                return FailCommand(
                    aCommand,
                    BitmapFontDescriptorError::
                        InvalidParameterCount,
                    theDiagnostic);
            }
            std::string_view aName;
            if (!GetScalar(aCommand.mArguments[0], aName) ||
                aName.empty())
            {
                return FailCommand(
                    aCommand,
                    BitmapFontDescriptorError::InvalidParameter,
                    theDiagnostic);
            }
            if (!aDefinitions.emplace(
                    std::string(aName),
                    aCommand.mArguments[1]).second)
            {
                return FailCommand(
                    aCommand,
                    BitmapFontDescriptorError::DuplicateDefinition,
                    theDiagnostic);
            }
            continue;
        }

        if (aCommand.mName == "CreateLayer")
        {
            if (aCommand.mArguments.size() != 1)
            {
                return FailCommand(
                    aCommand,
                    BitmapFontDescriptorError::
                        InvalidParameterCount,
                    theDiagnostic);
            }
            std::string_view aName;
            if (!GetScalar(aCommand.mArguments[0], aName) ||
                aName.empty())
            {
                return FailCommand(
                    aCommand,
                    BitmapFontDescriptorError::InvalidParameter,
                    theDiagnostic);
            }
            if (FindLayer(aName, theDescriptor) != nullptr)
            {
                return FailCommand(
                    aCommand,
                    BitmapFontDescriptorError::DuplicateLayer,
                    theDiagnostic);
            }
            theDescriptor.mLayers.push_back(
                BitmapFontLayerDescriptor{
                    .mName = std::string(aName),
                });
            continue;
        }

        if (aCommand.mName == "SetDefaultPointSize")
        {
            if (aCommand.mArguments.size() != 1 ||
                !GetPositiveInteger(
                    aCommand.mArguments[0],
                    theDescriptor.mDefaultPointSize))
            {
                return FailCommand(
                    aCommand,
                    aCommand.mArguments.size() != 1
                        ? BitmapFontDescriptorError::
                              InvalidParameterCount
                        : BitmapFontDescriptorError::
                              InvalidParameter,
                    theDiagnostic);
            }
            continue;
        }

        BitmapFontLayerDescriptor* aLayer{};
        if (!GetLayer(
                aCommand,
                theDescriptor,
                aLayer,
                theDiagnostic))
        {
            return false;
        }

        if (aCommand.mName == "LayerSetImage")
        {
            std::string_view anImagePath;
            if (aCommand.mArguments.size() != 2 ||
                !GetScalar(aCommand.mArguments[1], anImagePath) ||
                anImagePath.empty())
            {
                return FailCommand(
                    aCommand,
                    aCommand.mArguments.size() != 2
                        ? BitmapFontDescriptorError::
                              InvalidParameterCount
                        : BitmapFontDescriptorError::
                              InvalidParameter,
                    theDiagnostic);
            }
            aLayer->mImagePath = anImagePath;
        }
        else if (aCommand.mName == "LayerSetAscent")
        {
            if (aCommand.mArguments.size() != 2 ||
                !GetInteger(
                    aCommand.mArguments[1],
                    aLayer->mAscent))
            {
                return FailCommand(
                    aCommand,
                    aCommand.mArguments.size() != 2
                        ? BitmapFontDescriptorError::
                              InvalidParameterCount
                        : BitmapFontDescriptorError::
                              InvalidParameter,
                    theDiagnostic);
            }
        }
        else if (aCommand.mName == "LayerSetAscentPadding")
        {
            if (aCommand.mArguments.size() != 2 ||
                !GetInteger(
                    aCommand.mArguments[1],
                    aLayer->mAscentPadding))
            {
                return FailCommand(
                    aCommand,
                    aCommand.mArguments.size() != 2
                        ? BitmapFontDescriptorError::
                              InvalidParameterCount
                        : BitmapFontDescriptorError::
                              InvalidParameter,
                    theDiagnostic);
            }
        }
        else if (aCommand.mName == "LayerSetLineSpacingOffset")
        {
            if (aCommand.mArguments.size() != 2 ||
                !GetInteger(
                    aCommand.mArguments[1],
                    aLayer->mLineSpacingOffset))
            {
                return FailCommand(
                    aCommand,
                    aCommand.mArguments.size() != 2
                        ? BitmapFontDescriptorError::
                              InvalidParameterCount
                        : BitmapFontDescriptorError::
                              InvalidParameter,
                    theDiagnostic);
            }
        }
        else if (aCommand.mName == "LayerSetPointSize")
        {
            if (aCommand.mArguments.size() != 2 ||
                !GetPositiveInteger(
                    aCommand.mArguments[1],
                    aLayer->mPointSize))
            {
                return FailCommand(
                    aCommand,
                    aCommand.mArguments.size() != 2
                        ? BitmapFontDescriptorError::
                              InvalidParameterCount
                        : BitmapFontDescriptorError::
                              InvalidParameter,
                    theDiagnostic);
            }
        }
        else if (aCommand.mName == "LayerSetColorMult")
        {
            std::string_view aColorText;
            std::uint32_t aColor{};
            if (aCommand.mArguments.size() != 2 ||
                !GetScalar(aCommand.mArguments[1], aColorText) ||
                !ParseU32(aColorText, aColor))
            {
                return FailCommand(
                    aCommand,
                    aCommand.mArguments.size() != 2
                        ? BitmapFontDescriptorError::
                              InvalidParameterCount
                        : BitmapFontDescriptorError::
                              InvalidParameter,
                    theDiagnostic);
            }
            aLayer->mColorMultiplier = DecodeColor(aColor);
        }
        else if (aCommand.mName == "LayerSetCharWidths")
        {
            std::vector<char32_t> aCharacters;
            std::vector<std::int32_t> aWidths;
            if (aCommand.mArguments.size() != 3 ||
                !GetCharacterList(
                    aCommand.mArguments[1],
                    aDefinitions,
                    aCharacters) ||
                !GetIntegerList(
                    aCommand.mArguments[2],
                    aDefinitions,
                    aWidths))
            {
                return FailCommand(
                    aCommand,
                    aCommand.mArguments.size() != 3
                        ? BitmapFontDescriptorError::
                              InvalidParameterCount
                        : BitmapFontDescriptorError::
                              InvalidParameter,
                    theDiagnostic);
            }
            if (aCharacters.size() != aWidths.size())
            {
                return FailCommand(
                    aCommand,
                    BitmapFontDescriptorError::ListSizeMismatch,
                    theDiagnostic);
            }
            for (std::size_t anIndex = 0;
                 anIndex < aCharacters.size();
                 ++anIndex)
            {
                aLayer->mGlyphs[aCharacters[anIndex]].mAdvance =
                    aWidths[anIndex];
            }
        }
        else if (aCommand.mName == "LayerSetImageMap")
        {
            std::vector<char32_t> aCharacters;
            std::vector<std::array<std::int32_t, 4>> aRectangles;
            if (aCommand.mArguments.size() != 3 ||
                !GetCharacterList(
                    aCommand.mArguments[1],
                    aDefinitions,
                    aCharacters) ||
                !GetTupleList<4>(
                    aCommand.mArguments[2],
                    aDefinitions,
                    aRectangles))
            {
                return FailCommand(
                    aCommand,
                    aCommand.mArguments.size() != 3
                        ? BitmapFontDescriptorError::
                              InvalidParameterCount
                        : BitmapFontDescriptorError::
                              InvalidParameter,
                    theDiagnostic);
            }
            if (aCharacters.size() != aRectangles.size())
            {
                return FailCommand(
                    aCommand,
                    BitmapFontDescriptorError::ListSizeMismatch,
                    theDiagnostic);
            }
            for (std::size_t anIndex = 0;
                 anIndex < aCharacters.size();
                 ++anIndex)
            {
                const auto& aRectangle = aRectangles[anIndex];
                if (aRectangle[0] < 0 ||
                    aRectangle[1] < 0 ||
                    aRectangle[2] < 0 ||
                    aRectangle[3] < 0)
                {
                    return FailCommand(
                        aCommand,
                        BitmapFontDescriptorError::InvalidParameter,
                        theDiagnostic);
                }
                aLayer->mGlyphs[aCharacters[anIndex]].mSource = {
                    .mOrigin =
                        {
                            aRectangle[0],
                            aRectangle[1],
                        },
                    .mSize =
                        {
                            static_cast<std::uint32_t>(
                                aRectangle[2]),
                            static_cast<std::uint32_t>(
                                aRectangle[3]),
                        },
                };
            }
        }
        else if (aCommand.mName == "LayerSetCharOffsets")
        {
            std::vector<char32_t> aCharacters;
            std::vector<std::array<std::int32_t, 2>> anOffsets;
            if (aCommand.mArguments.size() != 3 ||
                !GetCharacterList(
                    aCommand.mArguments[1],
                    aDefinitions,
                    aCharacters) ||
                !GetTupleList<2>(
                    aCommand.mArguments[2],
                    aDefinitions,
                    anOffsets))
            {
                return FailCommand(
                    aCommand,
                    aCommand.mArguments.size() != 3
                        ? BitmapFontDescriptorError::
                              InvalidParameterCount
                        : BitmapFontDescriptorError::
                              InvalidParameter,
                    theDiagnostic);
            }
            if (aCharacters.size() != anOffsets.size())
            {
                return FailCommand(
                    aCommand,
                    BitmapFontDescriptorError::ListSizeMismatch,
                    theDiagnostic);
            }
            for (std::size_t anIndex = 0;
                 anIndex < aCharacters.size();
                 ++anIndex)
            {
                aLayer->mGlyphs[aCharacters[anIndex]].mOffset = {
                    anOffsets[anIndex][0],
                    anOffsets[anIndex][1],
                };
            }
        }
        else if (aCommand.mName == "LayerSetKerningPairs")
        {
            std::vector<std::array<char32_t, 2>> aPairs;
            std::vector<std::int32_t> anOffsets;
            if (aCommand.mArguments.size() != 3 ||
                !GetPairList(
                    aCommand.mArguments[1],
                    aDefinitions,
                    aPairs) ||
                !GetIntegerList(
                    aCommand.mArguments[2],
                    aDefinitions,
                    anOffsets))
            {
                return FailCommand(
                    aCommand,
                    aCommand.mArguments.size() != 3
                        ? BitmapFontDescriptorError::
                              InvalidParameterCount
                        : BitmapFontDescriptorError::
                              InvalidParameter,
                    theDiagnostic);
            }
            if (aPairs.size() != anOffsets.size())
            {
                return FailCommand(
                    aCommand,
                    BitmapFontDescriptorError::ListSizeMismatch,
                    theDiagnostic);
            }
            for (std::size_t anIndex = 0;
                 anIndex < aPairs.size();
                 ++anIndex)
            {
                aLayer->mGlyphs[aPairs[anIndex][0]]
                    .mKerning[aPairs[anIndex][1]] =
                    anOffsets[anIndex];
            }
        }
        else
        {
            return FailCommand(
                aCommand,
                BitmapFontDescriptorError::UnsupportedCommand,
                theDiagnostic);
        }
    }

    if (theDescriptor.mDefaultPointSize == 0 ||
        theDescriptor.mLayers.empty())
    {
        theDiagnostic.mError =
            BitmapFontDescriptorError::MissingFontData;
        return false;
    }
    for (const auto& aLayer : theDescriptor.mLayers)
    {
        if (aLayer.mImagePath.empty() ||
            aLayer.mPointSize == 0 ||
            aLayer.mGlyphs.empty())
        {
            theDiagnostic = {
                .mError =
                    BitmapFontDescriptorError::MissingFontData,
                .mCommand = aLayer.mName,
            };
            return false;
        }
    }
    return true;
}

const char* GetBitmapFontDescriptorErrorMessage(
    BitmapFontDescriptorError theError)
{
    switch (theError)
    {
    case BitmapFontDescriptorError::None:
        return "no error";
    case BitmapFontDescriptorError::Syntax:
        return "font descriptor syntax is invalid";
    case BitmapFontDescriptorError::InvalidParameterCount:
        return "font descriptor command has the wrong parameter count";
    case BitmapFontDescriptorError::InvalidParameter:
        return "font descriptor command has an invalid parameter";
    case BitmapFontDescriptorError::UndefinedValue:
        return "font descriptor value is undefined";
    case BitmapFontDescriptorError::DuplicateDefinition:
        return "font descriptor definition is duplicated";
    case BitmapFontDescriptorError::DuplicateLayer:
        return "font descriptor layer is duplicated";
    case BitmapFontDescriptorError::UndefinedLayer:
        return "font descriptor layer is undefined";
    case BitmapFontDescriptorError::ListSizeMismatch:
        return "font descriptor list sizes do not match";
    case BitmapFontDescriptorError::UnsupportedCommand:
        return "font descriptor command is unsupported";
    case BitmapFontDescriptorError::MissingFontData:
        return "font descriptor is missing required data";
    }
    return "unknown font descriptor error";
}

} // namespace pvz::engine::core
