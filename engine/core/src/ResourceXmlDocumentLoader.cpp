#include "pvz/engine/core/ResourceXmlDocumentLoader.h"

#include "pvz/engine/core/XmlDocument.h"

#include <utility>

namespace pvz::engine::core
{

ResourceXmlDocumentLoader::ResourceXmlDocumentLoader(
    const IResourceStore& theResources)
    : mResources(theResources)
{
}

bool ResourceXmlDocumentLoader::Load(
    std::string_view thePath,
    XmlDocumentMode theMode,
    std::vector<XmlNode>& theRoots,
    XmlDocumentDiagnostic& theDiagnostic) const
{
    XmlDocument aDocument;
    const bool didLoad = theMode == XmlDocumentMode::Fragment
        ? aDocument.LoadFragment(mResources, thePath)
        : aDocument.Load(mResources, thePath);
    if (!didLoad)
    {
        theDiagnostic = {
            .mError = aDocument.GetError(),
            .mLine = aDocument.GetErrorLine(),
        };
        return false;
    }

    auto aRoots = aDocument.TakeRoots();
    theRoots = std::move(aRoots);
    theDiagnostic = {};
    return true;
}

} // namespace pvz::engine::core
