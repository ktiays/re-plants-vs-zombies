#include "pvz/engine/core/PakResourceStore.h"
#include "pvz/engine/core/ResourceXmlDocumentLoader.h"
#include "pvz/engine/core/XmlDocument.h"
#include "pvz/game/DefinitionLoader.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string_view>

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

[[nodiscard]] bool StartsWithAsciiInsensitive(
    std::string_view theText,
    std::string_view thePrefix)
{
    if (theText.size() < thePrefix.size())
        return false;
    for (std::size_t anIndex = 0; anIndex < thePrefix.size(); ++anIndex)
    {
        if (LowercaseAscii(theText[anIndex]) !=
            LowercaseAscii(thePrefix[anIndex]))
        {
            return false;
        }
    }
    return true;
}

void WriteDefinitionError(
    std::string_view thePath,
    const pvz::game::DefinitionLoader& theLoader)
{
    std::cerr << thePath << ':' << theLoader.GetErrorLine() << ": ";
    if (theLoader.GetError() ==
        pvz::game::DefinitionLoadError::SourceDocument)
    {
        std::cerr
            << pvz::engine::core::GetXmlDocumentErrorMessage(
                   theLoader.GetSourceDiagnostic().mError);
    }
    else
    {
        std::cerr << theLoader.GetErrorDetail();
        if (!theLoader.GetErrorElement().empty())
            std::cerr << " (" << theLoader.GetErrorElement() << ')';
    }
    std::cerr << '\n';
}

} // namespace

int main(int theArgumentCount, char** theArguments)
{
    if (theArgumentCount != 2)
    {
        std::cerr
            << "usage: pvz_definition_inspect <path-to-main.pak>\n";
        return 2;
    }

    pvz::engine::core::PakResourceStore aResources;
    if (!aResources.LoadFromFile(std::filesystem::path(theArguments[1])))
    {
        std::cerr
            << pvz::engine::core::GetPakErrorMessage(
                   aResources.GetError())
            << '\n';
        return 1;
    }
    const pvz::engine::core::ResourceXmlDocumentLoader aDocuments(
        aResources);
    pvz::game::DefinitionLoader aLoader(aDocuments);

    std::uint64_t aReanimationCount{};
    std::uint64_t aTrackCount{};
    std::uint64_t aTransformCount{};
    std::uint64_t aParticleCount{};
    std::uint64_t anEmitterCount{};
    std::uint64_t aFieldCount{};
    std::uint64_t aTrailCount{};
    for (const auto& anEntry : aResources.GetArchive().GetEntries())
    {
        const bool isReanimation =
            EndsWithAsciiInsensitive(anEntry.mPath, ".reanim");
        const bool isParticle =
            StartsWithAsciiInsensitive(anEntry.mPath, "particles\\") &&
            EndsWithAsciiInsensitive(anEntry.mPath, ".xml");
        const bool isTrail =
            StartsWithAsciiInsensitive(anEntry.mPath, "particles\\") &&
            EndsWithAsciiInsensitive(anEntry.mPath, ".trail");
        if (!isReanimation && !isParticle && !isTrail)
            continue;

        if (isReanimation)
        {
            pvz::game::ReanimationDefinition aDefinition;
            if (!aLoader.LoadReanimation(anEntry.mPath, aDefinition))
            {
                WriteDefinitionError(anEntry.mPath, aLoader);
                return 1;
            }

            ++aReanimationCount;
            aTrackCount +=
                static_cast<std::uint64_t>(
                    aDefinition.mTracks.size());
            for (const auto& aTrack : aDefinition.mTracks)
            {
                aTransformCount +=
                    static_cast<std::uint64_t>(
                        aTrack.mTransforms.size());
            }
            continue;
        }

        if (isTrail)
        {
            pvz::game::TrailDefinition aDefinition;
            if (!aLoader.LoadTrail(anEntry.mPath, aDefinition))
            {
                WriteDefinitionError(anEntry.mPath, aLoader);
                return 1;
            }
            ++aTrailCount;
            continue;
        }

        pvz::game::ParticleDefinition aDefinition;
        if (!aLoader.LoadParticle(anEntry.mPath, aDefinition))
        {
            WriteDefinitionError(anEntry.mPath, aLoader);
            return 1;
        }

        ++aParticleCount;
        anEmitterCount +=
            static_cast<std::uint64_t>(
                aDefinition.mEmitters.size());
        for (const auto& anEmitter : aDefinition.mEmitters)
        {
            aFieldCount +=
                static_cast<std::uint64_t>(
                    anEmitter.mParticleFields.size());
            aFieldCount +=
                static_cast<std::uint64_t>(
                    anEmitter.mSystemFields.size());
        }
    }

    std::cout
        << "reanimations=" << aReanimationCount
        << " tracks=" << aTrackCount
        << " transforms=" << aTransformCount
        << " particles=" << aParticleCount
        << " emitters=" << anEmitterCount
        << " fields=" << aFieldCount
        << " trails=" << aTrailCount
        << '\n';
    return 0;
}
