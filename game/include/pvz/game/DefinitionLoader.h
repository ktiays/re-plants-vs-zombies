#pragma once

#include "pvz/engine/Xml.h"
#include "pvz/game/ParticleDefinition.h"
#include "pvz/game/ReanimationDefinition.h"
#include "pvz/game/TrailDefinition.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace pvz::game
{

enum class DefinitionLoadError : std::uint8_t
{
    None,
    SourceDocument,
    InvalidDefinition,
};

class DefinitionLoader
{
public:
    explicit DefinitionLoader(
        const engine::IXmlDocumentLoader& theDocuments);

    [[nodiscard]] bool LoadReanimation(
        std::string_view thePath,
        ReanimationDefinition& theDefinition);
    [[nodiscard]] bool LoadParticle(
        std::string_view thePath,
        ParticleDefinition& theDefinition);
    [[nodiscard]] bool LoadTrail(
        std::string_view thePath,
        TrailDefinition& theDefinition);

    [[nodiscard]] DefinitionLoadError GetError() const;
    [[nodiscard]] engine::XmlDocumentDiagnostic
        GetSourceDiagnostic() const;
    [[nodiscard]] std::uint32_t GetErrorLine() const;
    [[nodiscard]] std::string_view GetErrorElement() const;
    [[nodiscard]] std::string_view GetErrorDetail() const;

private:
    [[nodiscard]] bool LoadRoots(
        std::string_view thePath,
        std::vector<engine::XmlNode>& theRoots);
    void Reset();
    void FailDefinition(
        std::uint32_t theLine,
        std::string_view theElement,
        std::string_view theDetail);

    const engine::IXmlDocumentLoader& mDocuments;
    DefinitionLoadError mError{DefinitionLoadError::None};
    engine::XmlDocumentDiagnostic mSourceDiagnostic;
    std::uint32_t mErrorLine{};
    std::string mErrorElement;
    std::string mErrorDetail;
};

[[nodiscard]] const char* GetDefinitionLoadErrorMessage(
    DefinitionLoadError theError);

} // namespace pvz::game
