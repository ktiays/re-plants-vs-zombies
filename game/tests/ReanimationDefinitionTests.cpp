#include "pvz/engine/core/XmlDocument.h"
#include "pvz/game/ReanimationDefinition.h"

#include <cmath>
#include <span>

void Expect(bool theCondition, const char* theMessage);

namespace
{

void TestReanimationDefinitionMapping()
{
    pvz::engine::core::XmlDocument aDocument;
    Expect(
        aDocument.ParseFragment(
            "<FPS> 24 </FPS>"
            "<TRACK>\n"
            "  <name>body</name>\n"
            "  <t><x>10</x><f>-1</f><i>IMAGE_BODY</i>"
            "<text>Hello</text></t>\n"
            "  <t><Y>5</Y></t>\n"
            "  <t><i></i><text></text><sx>2</sx></t>\n"
            "</TRACK>"
            "<track><name>shadow</name><t></t></track>"),
        "reanimation fixture parses");

    pvz::game::ReanimationDefinitionMapper aMapper;
    pvz::game::ReanimationDefinition aDefinition;
    Expect(
        aMapper.Map(aDocument.GetRoots(), aDefinition),
        "reanimation definition maps");
    Expect(
        aDefinition.mFramesPerSecond == 24.0F,
        "reanimation frame rate maps case-insensitively");
    Expect(aDefinition.mTracks.size() == 2, "reanimation tracks map");
    Expect(
        aDefinition.mTracks[0].mName == "body",
        "reanimation track name maps");
    Expect(
        aDefinition.mTracks[0].mTransforms.size() == 3,
        "reanimation transforms map");

    const auto& aFirst = aDefinition.mTracks[0].mTransforms[0];
    Expect(aFirst.mTranslationX == 10.0F, "transform x maps");
    Expect(aFirst.mTranslationY == 0.0F, "transform y defaults");
    Expect(aFirst.mScaleX == 1.0F, "transform scale defaults");
    Expect(aFirst.mFrame == -1.0F, "transform frame maps");
    Expect(aFirst.mAlpha == 1.0F, "transform alpha defaults");
    Expect(aFirst.mImageId == "IMAGE_BODY", "image id maps");
    Expect(aFirst.mText == "Hello", "transform text maps");

    const auto& aSecond = aDefinition.mTracks[0].mTransforms[1];
    Expect(
        aSecond.mTranslationX == 10.0F,
        "missing transform x inherits");
    Expect(aSecond.mTranslationY == 5.0F, "transform y maps");
    Expect(
        aSecond.mImageId == "IMAGE_BODY",
        "missing image id inherits");
    Expect(aSecond.mText == "Hello", "missing text inherits");

    const auto& aThird = aDefinition.mTracks[0].mTransforms[2];
    Expect(
        aThird.mImageId == "IMAGE_BODY",
        "empty image id retains previous value");
    Expect(
        aThird.mText == "Hello",
        "empty text retains previous value");
    Expect(aThird.mScaleX == 2.0F, "later transform overrides field");

    const auto& aShadow = aDefinition.mTracks[1].mTransforms[0];
    Expect(
        aShadow.mTranslationX == 0.0F &&
            aShadow.mScaleX == 1.0F &&
            aShadow.mAlpha == 1.0F,
        "each track starts with runtime defaults");
    Expect(aShadow.mImageId.empty(), "track image inheritance resets");
}

void TestReanimationDefinitionRejectsUnknownElement()
{
    pvz::engine::core::XmlDocument aDocument;
    Expect(
        aDocument.ParseFragment(
            "<fps>12</fps>\n"
            "<track><name>body</name><bad>1</bad></track>"),
        "unknown-element fixture parses");

    pvz::game::ReanimationDefinition aDestination;
    aDestination.mFramesPerSecond = 99.0F;
    aDestination.mTracks.push_back({.mName = "preserved"});
    pvz::game::ReanimationDefinitionMapper aMapper;
    Expect(
        !aMapper.Map(aDocument.GetRoots(), aDestination),
        "unknown reanimation element fails");
    Expect(
        aMapper.GetError() ==
            pvz::game::ReanimationDefinitionError::UnknownElement,
        "unknown reanimation element reports error");
    Expect(
        aMapper.GetErrorElement() == "bad",
        "unknown reanimation element is identified");
    Expect(aMapper.GetErrorLine() == 2, "mapping error line is retained");
    Expect(
        aDestination.mFramesPerSecond == 99.0F &&
            aDestination.mTracks.size() == 1,
        "failed reanimation mapping preserves destination");
}

void TestReanimationDefinitionRejectsInvalidNumbers()
{
    pvz::engine::core::XmlDocument anInvalidFloatDocument;
    Expect(
        anInvalidFloatDocument.ParseFragment("<fps>12fps</fps>"),
        "invalid-float fixture parses");

    pvz::game::ReanimationDefinition aDefinition;
    pvz::game::ReanimationDefinitionMapper aMapper;
    Expect(
        !aMapper.Map(anInvalidFloatDocument.GetRoots(), aDefinition),
        "invalid reanimation float fails");
    Expect(
        aMapper.GetError() ==
            pvz::game::ReanimationDefinitionError::InvalidFloat,
        "invalid reanimation float reports error");

    pvz::engine::core::XmlDocument anInvalidRateDocument;
    Expect(
        anInvalidRateDocument.ParseFragment("<fps>0</fps>"),
        "invalid-rate fixture parses");
    Expect(
        !aMapper.Map(anInvalidRateDocument.GetRoots(), aDefinition),
        "non-positive reanimation frame rate fails");
    Expect(
        aMapper.GetError() ==
            pvz::game::ReanimationDefinitionError::
                InvalidFramesPerSecond,
        "invalid reanimation frame rate reports error");
}

void TestReanimationDefinitionRejectsInvalidShapes()
{
    pvz::engine::core::XmlDocument aDocument;
    Expect(
        aDocument.ParseFragment(
            "<track><name><nested/></name></track>"),
        "invalid-shape fixture parses");

    pvz::game::ReanimationDefinition aDefinition;
    pvz::game::ReanimationDefinitionMapper aMapper;
    Expect(
        !aMapper.Map(aDocument.GetRoots(), aDefinition),
        "nested scalar reanimation element fails");
    Expect(
        aMapper.GetError() ==
            pvz::game::ReanimationDefinitionError::
                InvalidElementShape,
        "nested scalar reports invalid shape");

    Expect(
        !aMapper.Map(
            std::span<const pvz::engine::XmlNode>{},
            aDefinition),
        "empty reanimation document fails");
    Expect(
        aMapper.GetError() ==
            pvz::game::ReanimationDefinitionError::EmptyDocument,
        "empty reanimation document reports error");
}

} // namespace

void RunReanimationDefinitionTests()
{
    TestReanimationDefinitionMapping();
    TestReanimationDefinitionRejectsUnknownElement();
    TestReanimationDefinitionRejectsInvalidNumbers();
    TestReanimationDefinitionRejectsInvalidShapes();
}
