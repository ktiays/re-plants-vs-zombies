#include "pvz/engine/core/BinaryStateIO.h"
#include "pvz/engine/core/ResourceXmlDocumentLoader.h"
#include "pvz/engine/core/XmlDocument.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

void Expect(bool theCondition, const char* theMessage);

namespace
{

using pvz::engine::IResourceStore;
using pvz::engine::core::XmlDocument;
using pvz::engine::core::XmlDocumentError;
using pvz::engine::core::XmlError;
using pvz::engine::core::BinaryStateReader;
using pvz::engine::core::BinaryStateWriter;
using pvz::engine::core::ResourceXmlDocumentLoader;

class MemoryResourceStore final : public IResourceStore
{
public:
    MemoryResourceStore(std::string thePath, std::string_view theText)
        : mPath(std::move(thePath))
    {
        mBytes.reserve(theText.size());
        for (const char aCharacter : theText)
        {
            mBytes.push_back(
                static_cast<std::byte>(
                    static_cast<unsigned char>(aCharacter)));
        }
    }

    [[nodiscard]] bool Contains(
        std::string_view thePath) const override
    {
        return thePath == mPath;
    }

    [[nodiscard]] bool GetSize(
        std::string_view thePath,
        std::uint64_t& theSize) const override
    {
        if (!Contains(thePath))
            return false;
        theSize = static_cast<std::uint64_t>(mBytes.size());
        return true;
    }

    [[nodiscard]] bool ReadAll(
        std::string_view thePath,
        std::vector<std::byte>& theBytes) const override
    {
        if (!Contains(thePath))
            return false;
        theBytes = mBytes;
        return true;
    }

private:
    std::string mPath;
    std::vector<std::byte> mBytes;
};

void TestXmlDocumentTree()
{
    XmlDocument aDocument;
    Expect(
        aDocument.Parse(
            "\xEF\xBB\xBF<?xml version=\"1.0\"?>"
            "<root mode=\"portable\">"
            "<item id=\"sun\">50</item>"
            "<empty/>"
            "</root>"),
        "XML document parses");
    Expect(aDocument.GetNodeCount() == 3, "XML document node count");

    const auto* aRoot = aDocument.GetRoot();
    Expect(aRoot != nullptr, "XML document root exists");
    if (aRoot == nullptr)
        return;

    Expect(aRoot->mName == "root", "XML document root name");
    const auto* aMode = aRoot->FindAttribute("mode");
    Expect(
        aMode != nullptr && *aMode == "portable",
        "XML document root attribute");
    Expect(aRoot->mChildren.size() == 2, "XML document children");
    if (aRoot->mChildren.size() != 2)
        return;

    const auto& anItem = aRoot->mChildren[0];
    Expect(
        anItem.mName == "item" && anItem.mValue == "50",
        "XML document text node value");
    const auto* anId = anItem.FindAttribute("id");
    Expect(anId != nullptr && *anId == "sun", "XML child attribute");
    Expect(
        aRoot->mChildren[1].mName == "empty",
        "XML self-closing child");
}

void TestXmlDocumentResourceProtocol()
{
    const MemoryResourceStore aResources(
        "definitions/plant.xml",
        "<plant><cost>100</cost></plant>");
    XmlDocument aDocument;
    Expect(
        aDocument.Load(aResources, "definitions/plant.xml"),
        "XML document loads through resource protocol");
    Expect(
        aDocument.GetRoot() != nullptr &&
            aDocument.GetRoot()->mName == "plant",
        "resource XML root");

    Expect(
        !aDocument.Load(aResources, "definitions/missing.xml"),
        "missing XML resource fails");
    Expect(
        aDocument.GetError() == XmlDocumentError::ResourceReadFailed,
        "missing XML resource error");
}

void TestXmlDocumentLoaderProtocol()
{
    const MemoryResourceStore aResources(
        "definitions/effects.xml",
        "<first/><second/>");
    const ResourceXmlDocumentLoader aLoader(aResources);
    std::vector<pvz::engine::XmlNode> aRoots;
    pvz::engine::XmlDocumentDiagnostic aDiagnostic;
    Expect(
        aLoader.Load(
            "definitions/effects.xml",
            pvz::engine::XmlDocumentMode::Fragment,
            aRoots,
            aDiagnostic),
        "XML loader protocol loads a fragment");
    Expect(
        aRoots.size() == 2 &&
            aDiagnostic.mError ==
                pvz::engine::XmlDocumentError::None,
        "XML loader returns roots and a clear diagnostic");

    Expect(
        !aLoader.Load(
            "definitions/missing.xml",
            pvz::engine::XmlDocumentMode::Fragment,
            aRoots,
            aDiagnostic),
        "XML loader protocol reports missing resources");
    Expect(
        aRoots.size() == 2,
        "failed XML protocol load preserves destination");
    Expect(
        aDiagnostic.mError ==
            pvz::engine::XmlDocumentError::ResourceReadFailed,
        "XML loader protocol returns a typed diagnostic");
}

void TestXmlDocumentFailures()
{
    XmlDocument aDocument;
    Expect(!aDocument.Parse(""), "empty XML document fails");
    Expect(
        aDocument.GetError() == XmlDocumentError::EmptyDocument,
        "empty XML document error");

    Expect(
        !aDocument.Parse("<one/><two/>"),
        "multiple XML roots fail");
    Expect(
        aDocument.GetError() == XmlDocumentError::MultipleRoots,
        "multiple XML root error");

    Expect(
        aDocument.ParseFragment("<one/><two/>"),
        "XML fragment permits multiple roots");
    Expect(
        aDocument.GetRoot() == nullptr &&
            aDocument.GetRoots().size() == 2,
        "XML fragment exposes all roots");

    Expect(
        !aDocument.Parse("<one><two></one>"),
        "parser failure reaches XML document");
    Expect(
        aDocument.GetError() == XmlDocumentError::ParserFailed &&
            aDocument.GetParserError() == XmlError::MismatchedEndElement,
        "XML document preserves parser error");
}

void TestXmlDocumentCache()
{
    XmlDocument aSource;
    Expect(
        aSource.ParseFragment(
            "<Emitter name=\"first\"><Rate>12.5</Rate></Emitter>"
            "<Emitter name=\"second\"/>"),
        "cache source XML parses");

    BinaryStateWriter aWriter;
    Expect(aSource.WriteCache(aWriter), "XML cache writes");

    constexpr std::array<std::uint8_t, 12> kHeader{
        'P', 'V', 'Z', 'D',
        1, 0, 0, 0,
        2, 0, 0, 0,
    };
    const auto aBytes = aWriter.GetBytes();
    Expect(aBytes.size() >= kHeader.size(), "XML cache has header");
    if (aBytes.size() >= kHeader.size())
    {
        for (std::size_t anIndex = 0; anIndex < kHeader.size(); ++anIndex)
        {
            Expect(
                std::to_integer<std::uint8_t>(aBytes[anIndex]) ==
                    kHeader[anIndex],
                "XML cache golden header");
        }
    }

    BinaryStateReader aReader(aBytes);
    XmlDocument aResult;
    Expect(aResult.ReadCache(aReader), "XML cache reads");
    Expect(aResult.GetNodeCount() == 3, "XML cache node count");
    Expect(aResult.GetRoots().size() == 2, "XML cache root count");
    if (aResult.GetRoots().size() == 2)
    {
        const auto& aFirst = aResult.GetRoots()[0];
        const auto& aSecond = aResult.GetRoots()[1];
        const auto* aFirstName = aFirst.FindAttribute("name");
        const auto* aSecondName = aSecond.FindAttribute("name");
        Expect(
            aFirstName != nullptr && *aFirstName == "first",
            "XML cache first root attribute");
        Expect(
            aFirst.mChildren.size() == 1 &&
                aFirst.mChildren[0].mName == "Rate" &&
                aFirst.mChildren[0].mValue == "12.5",
            "XML cache child contents");
        Expect(
            aSecondName != nullptr && *aSecondName == "second",
            "XML cache second root attribute");
    }

    std::vector<std::byte> anUnsupportedVersion(
        aBytes.begin(),
        aBytes.end());
    anUnsupportedVersion[4] = std::byte{2};
    BinaryStateReader aVersionReader(anUnsupportedVersion);
    XmlDocument aVersionResult;
    Expect(
        !aVersionResult.ReadCache(aVersionReader),
        "unsupported XML cache version fails");
    Expect(
        aVersionResult.GetError() ==
            XmlDocumentError::UnsupportedCacheVersion,
        "unsupported XML cache version error");

    BinaryStateReader aTruncatedReader(aBytes.first(aBytes.size() - 1));
    XmlDocument aTruncatedResult;
    Expect(
        !aTruncatedResult.ReadCache(aTruncatedReader),
        "truncated XML cache fails");
    Expect(
        aTruncatedResult.GetError() == XmlDocumentError::InvalidCache,
        "truncated XML cache error");
}

} // namespace

void RunXmlDocumentTests()
{
    TestXmlDocumentTree();
    TestXmlDocumentResourceProtocol();
    TestXmlDocumentLoaderProtocol();
    TestXmlDocumentFailures();
    TestXmlDocumentCache();
}
