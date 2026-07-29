#include "pvz/engine/core/XmlDocument.h"

#include <cstddef>
#include <limits>
#include <string>
#include <utility>

namespace pvz::engine::core
{
namespace
{

constexpr std::uint32_t kXmlCacheMagic = 0x445A5650;
constexpr std::uint32_t kXmlCacheVersion = 1;
constexpr std::uint32_t kMaximumCacheRoots = 1'000'000;
constexpr std::uint32_t kMaximumCacheAttributesPerNode = 65'536;
constexpr std::uint32_t kMaximumCacheNodes = 5'000'000;
constexpr std::uint32_t kMaximumCacheDepth = 1'024;
constexpr std::uint32_t kMinimumEncodedNodeSize = 21;
constexpr std::uint32_t kMinimumEncodedAttributeSize = 9;

[[nodiscard]] bool WriteCount(
    IStateWriter& theWriter,
    std::size_t theCount)
{
    return theCount <= std::numeric_limits<std::uint32_t>::max() &&
           theWriter.WriteU32(static_cast<std::uint32_t>(theCount));
}

[[nodiscard]] bool WriteCacheNode(
    IStateWriter& theWriter,
    const XmlNode& theNode,
    std::uint32_t theDepth)
{
    if (theDepth > kMaximumCacheDepth ||
        theNode.mName.empty() ||
        theNode.mAttributes.size() > kMaximumCacheAttributesPerNode ||
        theNode.mChildren.size() > kMaximumCacheNodes ||
        !theWriter.WriteUtf8(theNode.mName) ||
        !theWriter.WriteUtf8(theNode.mValue) ||
        !theWriter.WriteU32(theNode.mLine) ||
        !WriteCount(theWriter, theNode.mAttributes.size()))
    {
        return false;
    }

    for (const auto& anAttribute : theNode.mAttributes)
    {
        if (!theWriter.WriteUtf8(anAttribute.mName) ||
            !theWriter.WriteUtf8(anAttribute.mValue))
        {
            return false;
        }
    }

    if (!WriteCount(theWriter, theNode.mChildren.size()))
        return false;
    for (const auto& aChild : theNode.mChildren)
    {
        if (!WriteCacheNode(theWriter, aChild, theDepth + 1))
            return false;
    }
    return true;
}

[[nodiscard]] bool ReadCacheNode(
    IStateReader& theReader,
    XmlNode& theNode,
    std::uint32_t theDepth,
    std::uint32_t& theNodeCount)
{
    if (theDepth > kMaximumCacheDepth ||
        theNodeCount >= kMaximumCacheNodes)
    {
        return false;
    }
    ++theNodeCount;

    std::uint32_t anAttributeCount{};
    if (!theReader.ReadUtf8(theNode.mName) ||
        theNode.mName.empty() ||
        !theReader.ReadUtf8(theNode.mValue) ||
        !theReader.ReadU32(theNode.mLine) ||
        !theReader.ReadU32(anAttributeCount) ||
        anAttributeCount > kMaximumCacheAttributesPerNode ||
        anAttributeCount >
            theReader.GetBytesRemaining() /
                kMinimumEncodedAttributeSize)
    {
        return false;
    }

    theNode.mAttributes.reserve(anAttributeCount);
    for (std::uint32_t anIndex = 0; anIndex < anAttributeCount; ++anIndex)
    {
        XmlAttribute anAttribute;
        if (!theReader.ReadUtf8(anAttribute.mName) ||
            anAttribute.mName.empty() ||
            !theReader.ReadUtf8(anAttribute.mValue))
        {
            return false;
        }

        for (const auto& anExistingAttribute : theNode.mAttributes)
        {
            if (anExistingAttribute.mName == anAttribute.mName)
                return false;
        }
        theNode.mAttributes.push_back(std::move(anAttribute));
    }

    std::uint32_t aChildCount{};
    if (!theReader.ReadU32(aChildCount) ||
        aChildCount > kMaximumCacheNodes - theNodeCount ||
        aChildCount >
            theReader.GetBytesRemaining() /
                kMinimumEncodedNodeSize)
    {
        return false;
    }

    theNode.mChildren.reserve(aChildCount);
    for (std::uint32_t anIndex = 0; anIndex < aChildCount; ++anIndex)
    {
        XmlNode aChild;
        if (!ReadCacheNode(
                theReader,
                aChild,
                theDepth + 1,
                theNodeCount))
        {
            return false;
        }
        theNode.mChildren.push_back(std::move(aChild));
    }
    return true;
}

} // namespace

const std::string* XmlNode::FindAttribute(std::string_view theName) const
{
    for (const auto& anAttribute : mAttributes)
    {
        if (anAttribute.mName == theName)
            return &anAttribute.mValue;
    }
    return nullptr;
}

bool XmlDocument::Load(
    const IResourceStore& theResources,
    std::string_view thePath)
{
    return LoadInternal(theResources, thePath, false);
}

bool XmlDocument::LoadFragment(
    const IResourceStore& theResources,
    std::string_view thePath)
{
    return LoadInternal(theResources, thePath, true);
}

bool XmlDocument::LoadInternal(
    const IResourceStore& theResources,
    std::string_view thePath,
    bool theAllowMultipleRoots)
{
    Reset();
    std::vector<std::byte> aBytes;
    if (!theResources.ReadAll(thePath, aBytes))
    {
        Fail(XmlDocumentError::ResourceReadFailed, 0);
        return false;
    }

    std::string anXml;
    if (!aBytes.empty())
    {
        anXml.assign(
            reinterpret_cast<const char*>(aBytes.data()),
            aBytes.size());
    }
    return ParseInternal(anXml, theAllowMultipleRoots);
}

bool XmlDocument::Parse(std::string_view theUtf8)
{
    return ParseInternal(theUtf8, false);
}

bool XmlDocument::ParseFragment(std::string_view theUtf8)
{
    return ParseInternal(theUtf8, true);
}

bool XmlDocument::ParseInternal(
    std::string_view theUtf8,
    bool theAllowMultipleRoots)
{
    Reset();

    XmlPullParser aParser;
    aParser.Reset(theUtf8);
    XmlToken aToken;
    std::vector<XmlNode*> aNodeStack;
    while (aParser.Next(aToken))
    {
        switch (aToken.mType)
        {
        case XmlTokenType::Instruction:
        case XmlTokenType::Comment:
            break;

        case XmlTokenType::StartElement:
        {
            XmlNode aNode{
                .mName = std::move(aToken.mName),
                .mValue = {},
                .mAttributes = std::move(aToken.mAttributes),
                .mChildren = {},
                .mLine = aToken.mLine,
            };

            XmlNode* aNewNode{};
            if (aNodeStack.empty())
            {
                if (!theAllowMultipleRoots && !mRoots.empty())
                {
                    Fail(XmlDocumentError::MultipleRoots, aToken.mLine);
                    return false;
                }
                mRoots.push_back(std::move(aNode));
                aNewNode = &mRoots.back();
            }
            else
            {
                aNodeStack.back()->mChildren.push_back(std::move(aNode));
                aNewNode = &aNodeStack.back()->mChildren.back();
            }
            aNodeStack.push_back(aNewNode);
            ++mNodeCount;
            break;
        }

        case XmlTokenType::EndElement:
            if (aNodeStack.empty())
            {
                Fail(XmlDocumentError::ParserFailed, aToken.mLine);
                return false;
            }
            aNodeStack.pop_back();
            break;

        case XmlTokenType::Text:
            if (aNodeStack.empty())
            {
                Fail(XmlDocumentError::TextOutsideRoot, aToken.mLine);
                return false;
            }
            aNodeStack.back()->mValue.append(aToken.mValue);
            break;
        }
    }

    if (aParser.GetError() != XmlError::None)
    {
        mParserError = aParser.GetError();
        Fail(XmlDocumentError::ParserFailed, aParser.GetLine());
        return false;
    }
    if (mRoots.empty())
    {
        Fail(XmlDocumentError::EmptyDocument, aParser.GetLine());
        return false;
    }
    return true;
}

bool XmlDocument::WriteCache(IStateWriter& theWriter) const
{
    if (mRoots.empty() ||
        mRoots.size() > kMaximumCacheRoots ||
        mNodeCount > kMaximumCacheNodes ||
        !theWriter.WriteU32(kXmlCacheMagic) ||
        !theWriter.WriteU32(kXmlCacheVersion) ||
        !WriteCount(theWriter, mRoots.size()))
    {
        return false;
    }

    for (const auto& aRoot : mRoots)
    {
        if (!WriteCacheNode(theWriter, aRoot, 1))
            return false;
    }
    return true;
}

bool XmlDocument::ReadCache(IStateReader& theReader)
{
    Reset();

    std::uint32_t aMagic{};
    std::uint32_t aVersion{};
    std::uint32_t aRootCount{};
    if (!theReader.ReadU32(aMagic) || aMagic != kXmlCacheMagic)
    {
        Fail(XmlDocumentError::InvalidCache, 0);
        return false;
    }
    if (!theReader.ReadU32(aVersion))
    {
        Fail(XmlDocumentError::InvalidCache, 0);
        return false;
    }
    if (aVersion != kXmlCacheVersion)
    {
        Fail(XmlDocumentError::UnsupportedCacheVersion, 0);
        return false;
    }
    if (!theReader.ReadU32(aRootCount) ||
        aRootCount == 0 ||
        aRootCount > kMaximumCacheRoots ||
        aRootCount >
            theReader.GetBytesRemaining() /
                kMinimumEncodedNodeSize)
    {
        Fail(XmlDocumentError::InvalidCache, 0);
        return false;
    }

    std::vector<XmlNode> aRoots;
    aRoots.reserve(aRootCount);
    std::uint32_t aNodeCount{};
    for (std::uint32_t anIndex = 0; anIndex < aRootCount; ++anIndex)
    {
        XmlNode aRoot;
        if (!ReadCacheNode(theReader, aRoot, 1, aNodeCount))
        {
            Fail(XmlDocumentError::InvalidCache, 0);
            return false;
        }
        aRoots.push_back(std::move(aRoot));
    }
    if (theReader.GetBytesRemaining() != 0)
    {
        Fail(XmlDocumentError::InvalidCache, 0);
        return false;
    }

    mRoots = std::move(aRoots);
    mNodeCount = aNodeCount;
    return true;
}

const XmlNode* XmlDocument::GetRoot() const
{
    return mRoots.size() == 1 ? &mRoots.front() : nullptr;
}

std::span<const XmlNode> XmlDocument::GetRoots() const
{
    return mRoots;
}

std::size_t XmlDocument::GetNodeCount() const
{
    return mNodeCount;
}

XmlDocumentError XmlDocument::GetError() const
{
    return mError;
}

XmlError XmlDocument::GetParserError() const
{
    return mParserError;
}

std::uint32_t XmlDocument::GetErrorLine() const
{
    return mErrorLine;
}

void XmlDocument::Reset()
{
    mRoots.clear();
    mNodeCount = 0;
    mError = XmlDocumentError::None;
    mParserError = XmlError::None;
    mErrorLine = 0;
}

void XmlDocument::Fail(
    XmlDocumentError theError,
    std::uint32_t theLine)
{
    if (mError != XmlDocumentError::None)
        return;
    mError = theError;
    mErrorLine = theLine;
}

const char* GetXmlDocumentErrorMessage(XmlDocumentError theError)
{
    switch (theError)
    {
    case XmlDocumentError::None:
        return "no error";
    case XmlDocumentError::ResourceReadFailed:
        return "could not read XML resource";
    case XmlDocumentError::EmptyDocument:
        return "XML document does not contain a root element";
    case XmlDocumentError::MultipleRoots:
        return "XML document contains more than one root element";
    case XmlDocumentError::TextOutsideRoot:
        return "XML document contains text outside its root element";
    case XmlDocumentError::ParserFailed:
        return "XML parser failed";
    case XmlDocumentError::InvalidCache:
        return "XML cache is invalid";
    case XmlDocumentError::UnsupportedCacheVersion:
        return "XML cache version is unsupported";
    }
    return "unknown XML document error";
}

} // namespace pvz::engine::core
