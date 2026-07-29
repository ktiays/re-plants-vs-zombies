#include "pvz/engine/core/BinaryStateIO.h"
#include "pvz/engine/core/PakArchive.h"
#include "pvz/engine/core/XmlDocument.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

[[nodiscard]] char LowercaseAscii(char theCharacter)
{
    if (theCharacter >= 'A' && theCharacter <= 'Z')
        return static_cast<char>(theCharacter - 'A' + 'a');
    return theCharacter;
}

[[nodiscard]] bool EndsWithAsciiInsensitive(
    std::string_view theText,
    std::string_view theSuffix)
{
    if (theText.size() < theSuffix.size())
        return false;

    const auto aStart = theText.size() - theSuffix.size();
    for (std::size_t anIndex = 0; anIndex < theSuffix.size(); ++anIndex)
    {
        if (LowercaseAscii(theText[aStart + anIndex]) !=
            LowercaseAscii(theSuffix[anIndex]))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool IsXmlSource(std::string_view thePath)
{
    return EndsWithAsciiInsensitive(thePath, ".xml") ||
           EndsWithAsciiInsensitive(thePath, ".reanim");
}

[[nodiscard]] std::string_view GetSourceLine(
    std::string_view theText,
    std::uint32_t theLine)
{
    if (theLine == 0)
        return {};

    std::uint32_t aCurrentLine = 1;
    std::size_t aLineStart = 0;
    for (std::size_t anIndex = 0; anIndex < theText.size(); ++anIndex)
    {
        if (aCurrentLine == theLine && theText[anIndex] == '\n')
            return theText.substr(aLineStart, anIndex - aLineStart);
        if (theText[anIndex] == '\n')
        {
            ++aCurrentLine;
            aLineStart = anIndex + 1;
        }
    }
    if (aCurrentLine == theLine)
        return theText.substr(aLineStart);
    return {};
}

[[nodiscard]] bool XmlNodesEqual(
    const pvz::engine::core::XmlNode& theLeft,
    const pvz::engine::core::XmlNode& theRight)
{
    if (theLeft.mName != theRight.mName ||
        theLeft.mValue != theRight.mValue ||
        theLeft.mLine != theRight.mLine ||
        theLeft.mAttributes.size() != theRight.mAttributes.size() ||
        theLeft.mChildren.size() != theRight.mChildren.size())
    {
        return false;
    }

    for (std::size_t anIndex = 0;
         anIndex < theLeft.mAttributes.size();
         ++anIndex)
    {
        if (theLeft.mAttributes[anIndex].mName !=
                theRight.mAttributes[anIndex].mName ||
            theLeft.mAttributes[anIndex].mValue !=
                theRight.mAttributes[anIndex].mValue)
        {
            return false;
        }
    }

    for (std::size_t anIndex = 0;
         anIndex < theLeft.mChildren.size();
         ++anIndex)
    {
        if (!XmlNodesEqual(
                theLeft.mChildren[anIndex],
                theRight.mChildren[anIndex]))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool XmlDocumentsEqual(
    const pvz::engine::core::XmlDocument& theLeft,
    const pvz::engine::core::XmlDocument& theRight)
{
    const auto aLeftRoots = theLeft.GetRoots();
    const auto aRightRoots = theRight.GetRoots();
    if (aLeftRoots.size() != aRightRoots.size())
        return false;
    for (std::size_t anIndex = 0; anIndex < aLeftRoots.size(); ++anIndex)
    {
        if (!XmlNodesEqual(aLeftRoots[anIndex], aRightRoots[anIndex]))
            return false;
    }
    return true;
}

[[nodiscard]] bool ValidateXmlSources(
    const pvz::engine::core::PakArchive& theArchive)
{
    std::size_t aSourceCount{};
    std::size_t aNodeCount{};
    std::uint64_t aCacheByteCount{};
    std::vector<std::byte> aBytes;
    for (const auto& anEntry : theArchive.GetEntries())
    {
        if (!IsXmlSource(anEntry.mPath))
            continue;

        ++aSourceCount;
        if (!theArchive.ReadEntry(anEntry, aBytes))
        {
            std::cerr << anEntry.mPath << ": could not read entry\n";
            return false;
        }

        const std::string anXml(
            reinterpret_cast<const char*>(aBytes.data()),
            aBytes.size());
        pvz::engine::core::XmlDocument aDocument;
        if (!aDocument.ParseFragment(anXml))
        {
            const auto anErrorLine = aDocument.GetErrorLine();
            std::cerr
                << anEntry.mPath << ':' << anErrorLine << ": "
                << pvz::engine::core::GetXmlDocumentErrorMessage(
                       aDocument.GetError());
            if (aDocument.GetParserError() !=
                pvz::engine::core::XmlError::None)
            {
                std::cerr
                    << ": "
                    << pvz::engine::core::GetXmlErrorMessage(
                           aDocument.GetParserError());
            }
            std::cerr << '\n';
            const auto aSourceLine = GetSourceLine(anXml, anErrorLine);
            if (!aSourceLine.empty())
                std::cerr << "  " << aSourceLine << '\n';
            return false;
        }
        aNodeCount += aDocument.GetNodeCount();

        pvz::engine::core::BinaryStateWriter aCacheWriter;
        if (!aDocument.WriteCache(aCacheWriter))
        {
            std::cerr << anEntry.mPath << ": could not write XML cache\n";
            return false;
        }
        pvz::engine::core::BinaryStateReader aCacheReader(
            aCacheWriter.GetBytes());
        pvz::engine::core::XmlDocument aCachedDocument;
        if (!aCachedDocument.ReadCache(aCacheReader) ||
            !XmlDocumentsEqual(aDocument, aCachedDocument))
        {
            std::cerr
                << anEntry.mPath
                << ": XML cache round-trip did not preserve the document\n";
            return false;
        }
        aCacheByteCount += aCacheWriter.GetBytesWritten();
    }

    std::cout << "xml-sources=" << aSourceCount
              << " xml-nodes=" << aNodeCount
              << " xml-cache-bytes=" << aCacheByteCount << '\n';
    return true;
}

} // namespace

int main(int theArgumentCount, char** theArguments)
{
    if (theArgumentCount < 2 || theArgumentCount > 3)
    {
        std::cerr
            << "usage: pvz_pak_inspect <path-to-main.pak> "
               "[--validate-xml]\n";
        return 2;
    }
    const bool shouldValidateXml =
        theArgumentCount == 3 &&
        std::string_view(theArguments[2]) == "--validate-xml";
    if (theArgumentCount == 3 && !shouldValidateXml)
    {
        std::cerr << "unknown option: " << theArguments[2] << '\n';
        return 2;
    }

    pvz::engine::core::PakArchive anArchive;
    if (!anArchive.LoadFromFile(std::filesystem::path(theArguments[1])))
    {
        std::cerr
            << pvz::engine::core::GetPakErrorMessage(anArchive.GetError())
            << '\n';
        return 1;
    }

    std::cout << "entries=" << anArchive.GetEntries().size()
              << " payload-bytes=" << anArchive.GetPayloadSize() << '\n';

    const auto* aResourceManifest =
        anArchive.FindEntry("properties/resources.xml");
    if (aResourceManifest == nullptr)
    {
        std::cerr << "properties/resources.xml is missing\n";
        return 1;
    }

    std::cout << "resources.xml-bytes=" << aResourceManifest->mDataSize
              << '\n';
    if (shouldValidateXml && !ValidateXmlSources(anArchive))
        return 1;
    return 0;
}
