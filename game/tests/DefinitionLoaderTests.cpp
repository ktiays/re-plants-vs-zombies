#include "pvz/engine/core/ResourceXmlDocumentLoader.h"
#include "pvz/game/DefinitionLoader.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

void Expect(bool theCondition, const char* theMessage);

namespace
{

class MemoryResourceStore final : public pvz::engine::IResourceStore
{
public:
    void Add(std::string thePath, std::string_view theText)
    {
        std::vector<std::byte> aBytes;
        aBytes.reserve(theText.size());
        for (const char aCharacter : theText)
        {
            aBytes.push_back(
                static_cast<std::byte>(
                    static_cast<unsigned char>(aCharacter)));
        }
        mEntries.emplace(std::move(thePath), std::move(aBytes));
    }

    [[nodiscard]] bool Contains(
        std::string_view thePath) const override
    {
        return mEntries.contains(std::string(thePath));
    }

    [[nodiscard]] bool GetSize(
        std::string_view thePath,
        std::uint64_t& theSize) const override
    {
        const auto anEntry = mEntries.find(std::string(thePath));
        if (anEntry == mEntries.end())
            return false;
        theSize = static_cast<std::uint64_t>(anEntry->second.size());
        return true;
    }

    [[nodiscard]] bool ReadAll(
        std::string_view thePath,
        std::vector<std::byte>& theBytes) const override
    {
        const auto anEntry = mEntries.find(std::string(thePath));
        if (anEntry == mEntries.end())
            return false;
        theBytes = anEntry->second;
        return true;
    }

private:
    std::unordered_map<std::string, std::vector<std::byte>> mEntries;
};

void TestDefinitionLoaderUsesEngineProtocol()
{
    MemoryResourceStore aResources;
    aResources.Add(
        "reanim/test.reanim",
        "<fps>24</fps><track><name>body</name><t><x>5</x></t>"
        "</track>");
    aResources.Add(
        "particles/test.xml",
        "<Emitter><Name>portable</Name><ParticleScale>2</ParticleScale>"
        "</Emitter>");
    aResources.Add(
        "particles/test.trail",
        "<Image>IMAGE_TRAIL</Image><Loops>1</Loops>");

    const pvz::engine::core::ResourceXmlDocumentLoader aDocuments(
        aResources);
    pvz::game::DefinitionLoader aLoader(aDocuments);

    pvz::game::ReanimationDefinition aReanimation;
    Expect(
        aLoader.LoadReanimation(
            "reanim/test.reanim",
            aReanimation),
        "definition loader loads a reanimation");
    Expect(
        aReanimation.mFramesPerSecond == 24.0F &&
            aReanimation.mTracks.size() == 1,
        "definition loader maps reanimation contents");

    pvz::game::ParticleDefinition aParticle;
    Expect(
        aLoader.LoadParticle("particles/test.xml", aParticle),
        "definition loader loads a particle");
    Expect(
        aParticle.mEmitters.size() == 1 &&
            aParticle.mEmitters[0].mName == "portable",
        "definition loader maps particle contents");

    pvz::game::TrailDefinition aTrail;
    Expect(
        aLoader.LoadTrail("particles/test.trail", aTrail),
        "definition loader loads a trail");
    Expect(
        aTrail.mImageId == "IMAGE_TRAIL" &&
            pvz::game::HasTrailFlag(
                aTrail.mFlags,
                pvz::game::TrailFlag::Loops),
        "definition loader maps trail contents");
}

void TestDefinitionLoaderPreservesDiagnostics()
{
    MemoryResourceStore aResources;
    aResources.Add(
        "particles/invalid.xml",
        "<Emitter>\n<SpawnRate>[1 Unknown 2]</SpawnRate>\n</Emitter>");

    const pvz::engine::core::ResourceXmlDocumentLoader aDocuments(
        aResources);
    pvz::game::DefinitionLoader aLoader(aDocuments);
    pvz::game::ParticleDefinition aDefinition;
    aDefinition.mEmitters.push_back({.mName = "preserved"});

    Expect(
        !aLoader.LoadParticle(
            "particles/missing.xml",
            aDefinition),
        "definition loader reports a missing source");
    Expect(
        aLoader.GetError() ==
            pvz::game::DefinitionLoadError::SourceDocument &&
            aLoader.GetSourceDiagnostic().mError ==
                pvz::engine::XmlDocumentError::ResourceReadFailed,
        "definition loader preserves source diagnostics");
    Expect(
        aDefinition.mEmitters[0].mName == "preserved",
        "missing source preserves definition destination");

    Expect(
        !aLoader.LoadParticle(
            "particles/invalid.xml",
            aDefinition),
        "definition loader reports invalid mapped contents");
    Expect(
        aLoader.GetError() ==
            pvz::game::DefinitionLoadError::InvalidDefinition &&
            aLoader.GetErrorLine() == 2 &&
            aLoader.GetErrorElement() == "SpawnRate",
        "definition loader preserves mapper diagnostics");
    Expect(
        aDefinition.mEmitters[0].mName == "preserved",
        "invalid definition preserves destination");
}

} // namespace

void RunDefinitionLoaderTests()
{
    TestDefinitionLoaderUsesEngineProtocol();
    TestDefinitionLoaderPreservesDiagnostics();
}
