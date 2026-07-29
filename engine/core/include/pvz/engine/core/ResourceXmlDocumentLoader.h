#pragma once

#include "pvz/engine/Resources.h"
#include "pvz/engine/Xml.h"

namespace pvz::engine::core
{

class ResourceXmlDocumentLoader final : public IXmlDocumentLoader
{
public:
    explicit ResourceXmlDocumentLoader(
        const IResourceStore& theResources);

    [[nodiscard]] bool Load(
        std::string_view thePath,
        XmlDocumentMode theMode,
        std::vector<XmlNode>& theRoots,
        XmlDocumentDiagnostic& theDiagnostic) const override;

private:
    const IResourceStore& mResources;
};

} // namespace pvz::engine::core
