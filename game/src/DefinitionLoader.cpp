#include "pvz/game/DefinitionLoader.h"

#include <utility>
#include <vector>

namespace pvz::game
{

DefinitionLoader::DefinitionLoader(
    const engine::IXmlDocumentLoader& theDocuments)
    : mDocuments(theDocuments)
{
}

bool DefinitionLoader::LoadReanimation(
    std::string_view thePath,
    ReanimationDefinition& theDefinition)
{
    Reset();
    std::vector<engine::XmlNode> aRoots;
    if (!LoadRoots(thePath, aRoots))
        return false;

    ReanimationDefinitionMapper aMapper;
    if (!aMapper.Map(aRoots, theDefinition))
    {
        FailDefinition(
            aMapper.GetErrorLine(),
            aMapper.GetErrorElement(),
            GetReanimationDefinitionErrorMessage(aMapper.GetError()));
        return false;
    }
    return true;
}

bool DefinitionLoader::LoadParticle(
    std::string_view thePath,
    ParticleDefinition& theDefinition)
{
    Reset();
    std::vector<engine::XmlNode> aRoots;
    if (!LoadRoots(thePath, aRoots))
        return false;

    ParticleDefinitionMapper aMapper;
    if (!aMapper.Map(aRoots, theDefinition))
    {
        FailDefinition(
            aMapper.GetErrorLine(),
            aMapper.GetErrorElement(),
            GetParticleDefinitionErrorMessage(aMapper.GetError()));
        if (aMapper.GetError() ==
            ParticleDefinitionError::InvalidParameterTrack)
        {
            mErrorDetail.append(": ");
            mErrorDetail.append(
                GetParameterTrackErrorMessage(
                    aMapper.GetParameterTrackError()));
        }
        return false;
    }
    return true;
}

bool DefinitionLoader::LoadTrail(
    std::string_view thePath,
    TrailDefinition& theDefinition)
{
    Reset();
    std::vector<engine::XmlNode> aRoots;
    if (!LoadRoots(thePath, aRoots))
        return false;

    TrailDefinitionMapper aMapper;
    if (!aMapper.Map(aRoots, theDefinition))
    {
        FailDefinition(
            aMapper.GetErrorLine(),
            aMapper.GetErrorElement(),
            GetTrailDefinitionErrorMessage(aMapper.GetError()));
        if (aMapper.GetError() ==
            TrailDefinitionError::InvalidParameterTrack)
        {
            mErrorDetail.append(": ");
            mErrorDetail.append(
                GetParameterTrackErrorMessage(
                    aMapper.GetParameterTrackError()));
        }
        return false;
    }
    return true;
}

bool DefinitionLoader::LoadRoots(
    std::string_view thePath,
    std::vector<engine::XmlNode>& theRoots)
{
    engine::XmlDocumentDiagnostic aDiagnostic;
    if (!mDocuments.Load(
            thePath,
            engine::XmlDocumentMode::Fragment,
            theRoots,
            aDiagnostic))
    {
        mError = DefinitionLoadError::SourceDocument;
        mSourceDiagnostic = aDiagnostic;
        mErrorLine = aDiagnostic.mLine;
        return false;
    }
    return true;
}

void DefinitionLoader::Reset()
{
    mError = DefinitionLoadError::None;
    mSourceDiagnostic = {};
    mErrorLine = 0;
    mErrorElement.clear();
    mErrorDetail.clear();
}

void DefinitionLoader::FailDefinition(
    std::uint32_t theLine,
    std::string_view theElement,
    std::string_view theDetail)
{
    mError = DefinitionLoadError::InvalidDefinition;
    mErrorLine = theLine;
    mErrorElement = theElement;
    mErrorDetail = theDetail;
}

DefinitionLoadError DefinitionLoader::GetError() const
{
    return mError;
}

engine::XmlDocumentDiagnostic
DefinitionLoader::GetSourceDiagnostic() const
{
    return mSourceDiagnostic;
}

std::uint32_t DefinitionLoader::GetErrorLine() const
{
    return mErrorLine;
}

std::string_view DefinitionLoader::GetErrorElement() const
{
    return mErrorElement;
}

std::string_view DefinitionLoader::GetErrorDetail() const
{
    return mErrorDetail;
}

const char* GetDefinitionLoadErrorMessage(
    DefinitionLoadError theError)
{
    switch (theError)
    {
    case DefinitionLoadError::None:
        return "no error";
    case DefinitionLoadError::SourceDocument:
        return "definition source document could not be loaded";
    case DefinitionLoadError::InvalidDefinition:
        return "definition source is invalid";
    }
    return "unknown definition load error";
}

} // namespace pvz::game
