#include "pvz/engine/core/XmlDocument.h"
#include "pvz/game/TrailDefinition.h"

void Expect(bool theCondition, const char* theMessage);

namespace
{

void TestTrailDefinitionMapping()
{
    pvz::engine::core::XmlDocument aDocument;
    Expect(
        aDocument.ParseFragment(
            "<Image>IMAGE_ICETRAIL</Image>\n"
            "<MaxPoints>20</MaxPoints>\n"
            "<Loops>1</Loops>\n"
            "<MinPointDistance>3</MinPointDistance>\n"
            "<WidthOverLength>12 EaseOut 5</WidthOverLength>\n"
            "<AlphaOverLength>0,0 .3,20</AlphaOverLength>"),
        "trail fixture parses");

    pvz::game::TrailDefinition aDefinition;
    pvz::game::TrailDefinitionMapper aMapper;
    Expect(
        aMapper.Map(aDocument.GetRoots(), aDefinition),
        "trail definition maps");
    Expect(
        aDefinition.mImageId == "IMAGE_ICETRAIL",
        "trail image id maps");
    Expect(
        aDefinition.mMaximumPoints == 20,
        "trail maximum points maps");
    Expect(
        aDefinition.mMinimumPointDistance == 3.0F,
        "trail minimum point distance maps");
    Expect(
        pvz::game::HasTrailFlag(
            aDefinition.mFlags,
            pvz::game::TrailFlag::Loops),
        "trail loop flag maps");
    Expect(
        aDefinition.mWidthOverLength.mNodes.size() == 2,
        "trail width track maps");
    Expect(
        aDefinition.mWidthOverTime.mNodes.size() == 1 &&
            aDefinition.mWidthOverTime.mNodes[0].mLowValue == 1.0F,
        "trail width-over-time default maps");
    Expect(
        aDefinition.mTrailDuration.mNodes.size() == 1 &&
            aDefinition.mTrailDuration.mNodes[0].mLowValue == 100.0F,
        "trail duration default maps");
}

void TestTrailDefinitionRejectsInvalidValues()
{
    pvz::engine::core::XmlDocument aDocument;
    Expect(
        aDocument.ParseFragment("<Loops>2</Loops>"),
        "invalid trail flag fixture parses");

    pvz::game::TrailDefinition aDefinition;
    aDefinition.mImageId = "preserved";
    pvz::game::TrailDefinitionMapper aMapper;
    Expect(
        !aMapper.Map(aDocument.GetRoots(), aDefinition),
        "invalid trail flag fails");
    Expect(
        aMapper.GetError() ==
            pvz::game::TrailDefinitionError::InvalidFlag,
        "invalid trail flag reports error");
    Expect(
        aDefinition.mImageId == "preserved",
        "failed trail mapping preserves destination");

    Expect(
        aDocument.ParseFragment(
            "<TrailDuration>[1 Unknown 2]</TrailDuration>"),
        "invalid trail track fixture parses");
    Expect(
        !aMapper.Map(aDocument.GetRoots(), aDefinition),
        "invalid trail parameter track fails");
    Expect(
        aMapper.GetError() ==
            pvz::game::TrailDefinitionError::InvalidParameterTrack &&
            aMapper.GetParameterTrackError() ==
                pvz::game::ParameterTrackError::UnknownCurve,
        "invalid trail track retains parser error");
}

} // namespace

void RunTrailDefinitionTests()
{
    TestTrailDefinitionMapping();
    TestTrailDefinitionRejectsInvalidValues();
}
