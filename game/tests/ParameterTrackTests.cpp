#include "pvz/game/ParameterTrack.h"

#include <cmath>

void Expect(bool theCondition, const char* theMessage);

namespace
{

[[nodiscard]] bool NearlyEqual(float theLeft, float theRight)
{
    return std::fabs(theLeft - theRight) < 0.00001F;
}

void TestParameterTrackGrammar()
{
    pvz::game::ParameterTrackParser aParser;
    pvz::game::ParameterTrack aTrack;
    Expect(
        aParser.Parse(
            "0 [10 20],50 30 EaseIn "
            "[-5 Bounce 5],75 40",
            aTrack),
        "parameter track grammar parses");
    Expect(aTrack.mNodes.size() == 5, "parameter track node count");

    Expect(
        aTrack.mNodes[0].mLowValue == 0.0F &&
            aTrack.mNodes[0].mHighValue == 0.0F,
        "scalar parameter node maps");
    Expect(
        NearlyEqual(aTrack.mNodes[0].mTime, 0.0F),
        "initial implicit parameter time maps");

    Expect(
        aTrack.mNodes[1].mLowValue == 10.0F &&
            aTrack.mNodes[1].mHighValue == 20.0F,
        "range parameter node maps");
    Expect(
        NearlyEqual(aTrack.mNodes[1].mTime, 0.5F),
        "explicit parameter time maps from percent");

    Expect(
        aTrack.mNodes[2].mCurve ==
            pvz::game::ParameterCurve::EaseIn,
        "parameter transition curve maps");
    Expect(
        NearlyEqual(aTrack.mNodes[2].mTime, 0.75F),
        "implicit time reaches next explicit node");

    Expect(
        aTrack.mNodes[3].mDistribution ==
            pvz::game::ParameterCurve::Bounce,
        "range distribution curve maps");
    Expect(
        NearlyEqual(aTrack.mNodes[3].mTime, 0.75F),
        "second explicit parameter time maps");
    Expect(
        NearlyEqual(aTrack.mNodes[4].mTime, 1.0F),
        "final implicit parameter time maps");
}

void TestParameterTrackInterpolatesImplicitTimes()
{
    pvz::game::ParameterTrackParser aParser;
    pvz::game::ParameterTrack aTrack;
    Expect(
        aParser.Parse("1 2 3 4", aTrack),
        "implicit-time parameter track parses");
    Expect(aTrack.mNodes.size() == 4, "implicit-time node count");
    Expect(NearlyEqual(aTrack.mNodes[0].mTime, 0.0F), "time zero");
    Expect(
        NearlyEqual(aTrack.mNodes[1].mTime, 1.0F / 3.0F),
        "time one third");
    Expect(
        NearlyEqual(aTrack.mNodes[2].mTime, 2.0F / 3.0F),
        "time two thirds");
    Expect(NearlyEqual(aTrack.mNodes[3].mTime, 1.0F), "time one");
}

void TestParameterTrackRetainsRetailRangeCompatibility()
{
    pvz::game::ParameterTrackParser aParser;
    pvz::game::ParameterTrack aTrack;
    Expect(
        aParser.Parse("[.3.8],80 0", aTrack),
        "retail range without separator parses");
    Expect(aTrack.mNodes.size() == 2, "retail compatibility node count");
    Expect(
        NearlyEqual(aTrack.mNodes[0].mLowValue, 0.3F) &&
            NearlyEqual(aTrack.mNodes[0].mHighValue, 0.8F),
        "retail range without separator preserves values");
    Expect(
        NearlyEqual(aTrack.mNodes[0].mTime, 0.8F),
        "retail compatibility timestamp maps");
}

void TestParameterTrackRejectsMalformedInput()
{
    pvz::game::ParameterTrackParser aParser;
    pvz::game::ParameterTrack aDestination;
    aDestination.mNodes.push_back({
        .mTime = 0.5F,
        .mLowValue = 42.0F,
        .mHighValue = 42.0F,
    });

    Expect(
        !aParser.Parse("[1 Unknown 2]", aDestination),
        "unknown parameter curve fails");
    Expect(
        aParser.GetError() ==
            pvz::game::ParameterTrackError::UnknownCurve,
        "unknown parameter curve reports error");
    Expect(
        aDestination.mNodes.size() == 1 &&
            aDestination.mNodes[0].mLowValue == 42.0F,
        "failed parameter parse preserves destination");

    Expect(
        !aParser.Parse("[1 2", aDestination),
        "unterminated parameter range fails");
    Expect(
        aParser.GetError() ==
            pvz::game::ParameterTrackError::InvalidSyntax,
        "unterminated range reports syntax error");

    Expect(!aParser.Parse(" ", aDestination), "empty parameter track fails");
    Expect(
        aParser.GetError() ==
            pvz::game::ParameterTrackError::EmptyTrack,
        "empty parameter track reports error");

    Expect(
        !aParser.Parse("not-a-number", aDestination),
        "invalid parameter number fails");
    Expect(
        aParser.GetError() ==
            pvz::game::ParameterTrackError::InvalidNumber,
        "invalid parameter number reports error");
}

void TestParameterTrackDefaults()
{
    pvz::game::ParameterTrack aTrack;
    pvz::game::SetParameterTrackDefault(aTrack, 1.0F);
    Expect(aTrack.mNodes.size() == 1, "nonzero parameter default maps");
    Expect(
        aTrack.mNodes[0].mLowValue == 1.0F &&
            aTrack.mNodes[0].mHighValue == 1.0F &&
            aTrack.mNodes[0].mCurve ==
                pvz::game::ParameterCurve::Constant,
        "parameter default is a constant node");

    pvz::game::SetParameterTrackDefault(aTrack, 2.0F);
    Expect(
        aTrack.mNodes.size() == 1 &&
            aTrack.mNodes[0].mLowValue == 1.0F,
        "parameter default preserves an existing track");

    pvz::game::ParameterTrack aZeroTrack;
    pvz::game::SetParameterTrackDefault(aZeroTrack, 0.0F);
    Expect(
        aZeroTrack.mNodes.empty(),
        "zero parameter default retains legacy empty track");
}

} // namespace

void RunParameterTrackTests()
{
    TestParameterTrackGrammar();
    TestParameterTrackInterpolatesImplicitTimes();
    TestParameterTrackRetainsRetailRangeCompatibility();
    TestParameterTrackRejectsMalformedInput();
    TestParameterTrackDefaults();
}
