#include "pvz/engine/core/XmlDocument.h"
#include "pvz/game/ParticleDefinition.h"

void Expect(bool theCondition, const char* theMessage);

namespace
{

void TestParticleDefinitionMapping()
{
    pvz::engine::core::XmlDocument aDocument;
    Expect(
        aDocument.ParseFragment(
            "<Emitter>\n"
            "  <Name>Snow</Name>\n"
            "  <Image>IMAGE_SNOW</Image>\n"
            "  <ImageRow>2</ImageRow>\n"
            "  <ImageCol>3</ImageCol>\n"
            "  <ImageFrames>4</ImageFrames>\n"
            "  <Animated>1</Animated>\n"
            "  <EmitterType>Circle</EmitterType>\n"
            "  <Additive>1</Additive>\n"
            "  <SystemLoops>0</SystemLoops>\n"
            "  <SpawnRate>1 2</SpawnRate>\n"
            "  <ParticleAlpha>1,80 0</ParticleAlpha>\n"
            "  <Field><FieldType>Acceleration</FieldType>"
            "<X>[1 2]</X><Y>5</Y></Field>\n"
            "  <SystemField><FieldType>SystemPosition</FieldType>"
            "<x>10</x></SystemField>\n"
            "</Emitter>"
            "<Emitter></Emitter>"),
        "particle fixture parses");

    pvz::game::ParticleDefinitionMapper aMapper;
    pvz::game::ParticleDefinition aDefinition;
    Expect(
        aMapper.Map(aDocument.GetRoots(), aDefinition),
        "particle definition maps");
    Expect(aDefinition.mEmitters.size() == 2, "particle emitters map");

    const auto& anEmitter = aDefinition.mEmitters[0];
    Expect(anEmitter.mName == "Snow", "particle emitter name maps");
    Expect(anEmitter.mImageId == "IMAGE_SNOW", "particle image id maps");
    Expect(
        anEmitter.mImageRow == 2 &&
            anEmitter.mImageColumn == 3 &&
            anEmitter.mImageFrames == 4,
        "particle image layout maps");
    Expect(anEmitter.mAnimated == 1, "particle animation mode maps");
    Expect(
        anEmitter.mEmitterType ==
            pvz::game::ParticleEmitterType::Circle,
        "particle emitter type maps");
    Expect(
        pvz::game::HasParticleFlag(
            anEmitter.mFlags,
            pvz::game::ParticleFlag::Additive),
        "enabled particle flag maps");
    Expect(
        !pvz::game::HasParticleFlag(
            anEmitter.mFlags,
            pvz::game::ParticleFlag::SystemLoops),
        "disabled particle flag maps");
    Expect(
        anEmitter.mSpawnRate.mNodes.size() == 2,
        "particle parameter track maps");
    Expect(
        anEmitter.mParticleFields.size() == 1 &&
            anEmitter.mSystemFields.size() == 1,
        "particle fields map");
    Expect(
        anEmitter.mParticleFields[0].mType ==
            pvz::game::ParticleFieldType::Acceleration,
        "particle field type maps");
    Expect(
        anEmitter.mSystemFields[0].mType ==
            pvz::game::ParticleFieldType::SystemPosition,
        "system field type maps");

    const auto& aDefaultEmitter = aDefinition.mEmitters[1];
    Expect(
        aDefaultEmitter.mImageFrames == 1,
        "particle image-frame default maps");
    Expect(
        aDefaultEmitter.mEmitterType ==
            pvz::game::ParticleEmitterType::Box,
        "particle emitter-type default maps");
    Expect(
        aDefaultEmitter.mFlags == 0,
        "particle flags default to zero");
    Expect(
        aDefaultEmitter.mParticleDuration.mNodes.size() == 1 &&
            aDefaultEmitter.mParticleDuration.mNodes[0].mLowValue ==
                100.0F,
        "particle duration receives the legacy runtime default");
    Expect(
        aDefaultEmitter.mSystemAlpha.mNodes.size() == 1 &&
            aDefaultEmitter.mSystemAlpha.mNodes[0].mLowValue == 1.0F,
        "particle alpha receives the legacy runtime default");
    Expect(
        aDefaultEmitter.mSpawnRate.mNodes.empty(),
        "zero-valued particle defaults remain empty tracks");
}

void TestParticleDefinitionRejectsInvalidValues()
{
    pvz::engine::core::XmlDocument anEnumDocument;
    Expect(
        anEnumDocument.ParseFragment(
            "<Emitter><EmitterType>Triangle</EmitterType></Emitter>"),
        "invalid particle enum fixture parses");

    pvz::game::ParticleDefinition aDestination;
    aDestination.mEmitters.push_back({.mName = "preserved"});
    pvz::game::ParticleDefinitionMapper aMapper;
    Expect(
        !aMapper.Map(anEnumDocument.GetRoots(), aDestination),
        "invalid particle enum fails");
    Expect(
        aMapper.GetError() ==
            pvz::game::ParticleDefinitionError::InvalidEnum,
        "invalid particle enum reports error");
    Expect(
        aDestination.mEmitters.size() == 1 &&
            aDestination.mEmitters[0].mName == "preserved",
        "failed particle mapping preserves destination");

    pvz::engine::core::XmlDocument aFlagDocument;
    Expect(
        aFlagDocument.ParseFragment(
            "<Emitter><Additive>2</Additive></Emitter>"),
        "invalid particle flag fixture parses");
    Expect(
        !aMapper.Map(aFlagDocument.GetRoots(), aDestination),
        "invalid particle flag fails");
    Expect(
        aMapper.GetError() ==
            pvz::game::ParticleDefinitionError::InvalidFlag,
        "invalid particle flag reports error");
}

void TestParticleDefinitionReportsTrackErrors()
{
    pvz::engine::core::XmlDocument aDocument;
    Expect(
        aDocument.ParseFragment(
            "<Emitter>\n"
            "<SpawnRate>[1 Unknown 2]</SpawnRate>\n"
            "</Emitter>"),
        "invalid particle track fixture parses");

    pvz::game::ParticleDefinition aDefinition;
    pvz::game::ParticleDefinitionMapper aMapper;
    Expect(
        !aMapper.Map(aDocument.GetRoots(), aDefinition),
        "invalid particle parameter track fails");
    Expect(
        aMapper.GetError() ==
            pvz::game::ParticleDefinitionError::
                InvalidParameterTrack,
        "invalid particle parameter track reports definition error");
    Expect(
        aMapper.GetParameterTrackError() ==
            pvz::game::ParameterTrackError::UnknownCurve,
        "invalid particle parameter track retains parser error");
    Expect(
        aMapper.GetErrorElement() == "SpawnRate",
        "invalid particle parameter track identifies field");
    Expect(aMapper.GetErrorLine() == 2, "particle error line maps");
}

} // namespace

void RunParticleDefinitionTests()
{
    TestParticleDefinitionMapping();
    TestParticleDefinitionRejectsInvalidValues();
    TestParticleDefinitionReportsTrackErrors();
}
