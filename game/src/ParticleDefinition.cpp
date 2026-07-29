#include "pvz/game/ParticleDefinition.h"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <system_error>
#include <utility>

namespace pvz::game
{
namespace
{

constexpr std::size_t kMaximumEmitters = 10'000;
constexpr std::size_t kMaximumFieldsPerEmitter = 1'024;

struct TrackMember
{
    std::string_view mName;
    ParameterTrack ParticleEmitterDefinition::*mMember;
};

constexpr std::array<TrackMember, 38> kTrackMembers{{
    {"SystemDuration", &ParticleEmitterDefinition::mSystemDuration},
    {"CrossFadeDuration", &ParticleEmitterDefinition::mCrossFadeDuration},
    {"SpawnRate", &ParticleEmitterDefinition::mSpawnRate},
    {"SpawnMinActive", &ParticleEmitterDefinition::mSpawnMinimumActive},
    {"SpawnMaxActive", &ParticleEmitterDefinition::mSpawnMaximumActive},
    {"SpawnMaxLaunched", &ParticleEmitterDefinition::mSpawnMaximumLaunched},
    {"EmitterRadius", &ParticleEmitterDefinition::mEmitterRadius},
    {"EmitterOffsetX", &ParticleEmitterDefinition::mEmitterOffsetX},
    {"EmitterOffsetY", &ParticleEmitterDefinition::mEmitterOffsetY},
    {"EmitterBoxX", &ParticleEmitterDefinition::mEmitterBoxX},
    {"EmitterBoxY", &ParticleEmitterDefinition::mEmitterBoxY},
    {"EmitterPath", &ParticleEmitterDefinition::mEmitterPath},
    {"EmitterSkewX", &ParticleEmitterDefinition::mEmitterSkewX},
    {"EmitterSkewY", &ParticleEmitterDefinition::mEmitterSkewY},
    {"ParticleDuration", &ParticleEmitterDefinition::mParticleDuration},
    {"SystemRed", &ParticleEmitterDefinition::mSystemRed},
    {"SystemGreen", &ParticleEmitterDefinition::mSystemGreen},
    {"SystemBlue", &ParticleEmitterDefinition::mSystemBlue},
    {"SystemAlpha", &ParticleEmitterDefinition::mSystemAlpha},
    {"SystemBrightness", &ParticleEmitterDefinition::mSystemBrightness},
    {"LaunchSpeed", &ParticleEmitterDefinition::mLaunchSpeed},
    {"LaunchAngle", &ParticleEmitterDefinition::mLaunchAngle},
    {"ParticleRed", &ParticleEmitterDefinition::mParticleRed},
    {"ParticleGreen", &ParticleEmitterDefinition::mParticleGreen},
    {"ParticleBlue", &ParticleEmitterDefinition::mParticleBlue},
    {"ParticleAlpha", &ParticleEmitterDefinition::mParticleAlpha},
    {"ParticleBrightness", &ParticleEmitterDefinition::mParticleBrightness},
    {"ParticleSpinAngle", &ParticleEmitterDefinition::mParticleSpinAngle},
    {"ParticleSpinSpeed", &ParticleEmitterDefinition::mParticleSpinSpeed},
    {"ParticleScale", &ParticleEmitterDefinition::mParticleScale},
    {"ParticleStretch", &ParticleEmitterDefinition::mParticleStretch},
    {"CollisionReflect", &ParticleEmitterDefinition::mCollisionReflect},
    {"CollisionSpin", &ParticleEmitterDefinition::mCollisionSpin},
    {"ClipTop", &ParticleEmitterDefinition::mClipTop},
    {"ClipBottom", &ParticleEmitterDefinition::mClipBottom},
    {"ClipLeft", &ParticleEmitterDefinition::mClipLeft},
    {"ClipRight", &ParticleEmitterDefinition::mClipRight},
    {"AnimationRate", &ParticleEmitterDefinition::mAnimationRate},
}};

struct FlagName
{
    std::string_view mName;
    ParticleFlag mFlag;
};

constexpr std::array<FlagName, 12> kFlagNames{{
    {"RandomLaunchSpin", ParticleFlag::RandomLaunchSpin},
    {"AlignLaunchSpin", ParticleFlag::AlignLaunchSpin},
    {"AlignToPixel", ParticleFlag::AlignToPixel},
    {"SystemLoops", ParticleFlag::SystemLoops},
    {"ParticleLoops", ParticleFlag::ParticleLoops},
    {"ParticlesDontFollow", ParticleFlag::ParticlesDoNotFollow},
    {"RandomStartTime", ParticleFlag::RandomStartTime},
    {"DieIfOverloaded", ParticleFlag::DieIfOverloaded},
    {"Additive", ParticleFlag::Additive},
    {"FullScreen", ParticleFlag::FullScreen},
    {"SoftwareOnly", ParticleFlag::SoftwareOnly},
    {"HardwareOnly", ParticleFlag::HardwareOnly},
}};

struct EmitterTypeName
{
    std::string_view mName;
    ParticleEmitterType mType;
};

constexpr std::array<EmitterTypeName, 5> kEmitterTypeNames{{
    {"Circle", ParticleEmitterType::Circle},
    {"Box", ParticleEmitterType::Box},
    {"BoxPath", ParticleEmitterType::BoxPath},
    {"CirclePath", ParticleEmitterType::CirclePath},
    {"CircleEvenSpacing", ParticleEmitterType::CircleEvenSpacing},
}};

struct FieldTypeName
{
    std::string_view mName;
    ParticleFieldType mType;
};

constexpr std::array<FieldTypeName, 11> kFieldTypeNames{{
    {"Friction", ParticleFieldType::Friction},
    {"Acceleration", ParticleFieldType::Acceleration},
    {"Attractor", ParticleFieldType::Attractor},
    {"MaxVelocity", ParticleFieldType::MaximumVelocity},
    {"Velocity", ParticleFieldType::Velocity},
    {"Position", ParticleFieldType::Position},
    {"SystemPosition", ParticleFieldType::SystemPosition},
    {"GroundConstraint", ParticleFieldType::GroundConstraint},
    {"Shake", ParticleFieldType::Shake},
    {"Circle", ParticleFieldType::Circle},
    {"Away", ParticleFieldType::Away},
}};

[[nodiscard]] char LowercaseAscii(char theCharacter)
{
    if (theCharacter >= 'A' && theCharacter <= 'Z')
        return static_cast<char>(theCharacter - 'A' + 'a');
    return theCharacter;
}

[[nodiscard]] bool NamesEqual(
    std::string_view theLeft,
    std::string_view theRight)
{
    if (theLeft.size() != theRight.size())
        return false;
    for (std::size_t anIndex = 0; anIndex < theLeft.size(); ++anIndex)
    {
        if (LowercaseAscii(theLeft[anIndex]) !=
            LowercaseAscii(theRight[anIndex]))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool IsAsciiWhitespace(char theCharacter)
{
    return theCharacter == ' ' ||
           theCharacter == '\t' ||
           theCharacter == '\r' ||
           theCharacter == '\n';
}

[[nodiscard]] std::string_view TrimAscii(std::string_view theValue)
{
    while (!theValue.empty() && IsAsciiWhitespace(theValue.front()))
        theValue.remove_prefix(1);
    while (!theValue.empty() && IsAsciiWhitespace(theValue.back()))
        theValue.remove_suffix(1);
    return theValue;
}

[[nodiscard]] bool HasOnlyWhitespace(std::string_view theValue)
{
    return TrimAscii(theValue).empty();
}

[[nodiscard]] ParameterTrack* FindTrack(
    ParticleEmitterDefinition& theEmitter,
    std::string_view theName)
{
    for (const auto& aTrackMember : kTrackMembers)
    {
        if (NamesEqual(theName, aTrackMember.mName))
            return &(theEmitter.*(aTrackMember.mMember));
    }
    return nullptr;
}

[[nodiscard]] const FlagName* FindFlag(std::string_view theName)
{
    for (const auto& aFlagName : kFlagNames)
    {
        if (NamesEqual(theName, aFlagName.mName))
            return &aFlagName;
    }
    return nullptr;
}

void ApplyEmitterDefaults(ParticleEmitterDefinition& theEmitter)
{
    SetParameterTrackDefault(theEmitter.mSystemDuration, 0.0F);
    SetParameterTrackDefault(theEmitter.mSpawnRate, 0.0F);
    SetParameterTrackDefault(theEmitter.mSpawnMinimumActive, -1.0F);
    SetParameterTrackDefault(theEmitter.mSpawnMaximumActive, -1.0F);
    SetParameterTrackDefault(theEmitter.mSpawnMaximumLaunched, -1.0F);
    SetParameterTrackDefault(theEmitter.mEmitterRadius, 0.0F);
    SetParameterTrackDefault(theEmitter.mEmitterOffsetX, 0.0F);
    SetParameterTrackDefault(theEmitter.mEmitterOffsetY, 0.0F);
    SetParameterTrackDefault(theEmitter.mEmitterBoxX, 0.0F);
    SetParameterTrackDefault(theEmitter.mEmitterBoxY, 0.0F);
    SetParameterTrackDefault(theEmitter.mEmitterSkewX, 0.0F);
    SetParameterTrackDefault(theEmitter.mEmitterSkewY, 0.0F);
    SetParameterTrackDefault(theEmitter.mParticleDuration, 100.0F);
    SetParameterTrackDefault(theEmitter.mLaunchSpeed, 0.0F);
    SetParameterTrackDefault(theEmitter.mSystemRed, 1.0F);
    SetParameterTrackDefault(theEmitter.mSystemGreen, 1.0F);
    SetParameterTrackDefault(theEmitter.mSystemBlue, 1.0F);
    SetParameterTrackDefault(theEmitter.mSystemAlpha, 1.0F);
    SetParameterTrackDefault(theEmitter.mSystemBrightness, 1.0F);
    SetParameterTrackDefault(theEmitter.mLaunchAngle, 0.0F);
    SetParameterTrackDefault(theEmitter.mCrossFadeDuration, 0.0F);
    SetParameterTrackDefault(theEmitter.mParticleRed, 1.0F);
    SetParameterTrackDefault(theEmitter.mParticleGreen, 1.0F);
    SetParameterTrackDefault(theEmitter.mParticleBlue, 1.0F);
    SetParameterTrackDefault(theEmitter.mParticleAlpha, 1.0F);
    SetParameterTrackDefault(theEmitter.mParticleBrightness, 1.0F);
    SetParameterTrackDefault(theEmitter.mParticleSpinAngle, 0.0F);
    SetParameterTrackDefault(theEmitter.mParticleSpinSpeed, 0.0F);
    SetParameterTrackDefault(theEmitter.mParticleScale, 1.0F);
    SetParameterTrackDefault(theEmitter.mParticleStretch, 1.0F);
    SetParameterTrackDefault(theEmitter.mCollisionReflect, 0.0F);
    SetParameterTrackDefault(theEmitter.mCollisionSpin, 0.0F);
    SetParameterTrackDefault(theEmitter.mClipTop, 0.0F);
    SetParameterTrackDefault(theEmitter.mClipBottom, 0.0F);
    SetParameterTrackDefault(theEmitter.mClipLeft, 0.0F);
    SetParameterTrackDefault(theEmitter.mClipRight, 0.0F);
    SetParameterTrackDefault(theEmitter.mAnimationRate, 0.0F);
}

} // namespace

bool ParticleDefinitionMapper::Map(
    std::span<const engine::XmlNode> theRoots,
    ParticleDefinition& theDefinition)
{
    Reset();
    if (theRoots.empty())
    {
        mError = ParticleDefinitionError::EmptyDocument;
        return false;
    }

    ParticleDefinition aDefinition;
    for (const auto& aRoot : theRoots)
    {
        if (!NamesEqual(aRoot.mName, "Emitter"))
        {
            Fail(ParticleDefinitionError::UnknownElement, aRoot);
            return false;
        }
        if (!HasOnlyWhitespace(aRoot.mValue))
        {
            Fail(ParticleDefinitionError::InvalidElementShape, aRoot);
            return false;
        }
        if (aDefinition.mEmitters.size() >= kMaximumEmitters)
        {
            Fail(ParticleDefinitionError::TooManyEmitters, aRoot);
            return false;
        }

        ParticleEmitterDefinition anEmitter;
        if (!MapEmitter(aRoot, anEmitter))
            return false;
        ApplyEmitterDefaults(anEmitter);
        aDefinition.mEmitters.push_back(std::move(anEmitter));
    }

    theDefinition = std::move(aDefinition);
    return true;
}

bool ParticleDefinitionMapper::MapEmitter(
    const engine::XmlNode& theNode,
    ParticleEmitterDefinition& theEmitter)
{
    ParticleEmitterDefinition anEmitter;
    for (const auto& aChild : theNode.mChildren)
    {
        ParameterTrack* aTrack = FindTrack(anEmitter, aChild.mName);
        if (aTrack != nullptr)
        {
            if (!ReadParameterTrack(aChild, *aTrack))
                return false;
            continue;
        }

        const FlagName* aFlag = FindFlag(aChild.mName);
        if (aFlag != nullptr)
        {
            if (!ReadFlag(aChild, aFlag->mFlag, anEmitter.mFlags))
                return false;
            continue;
        }

        if (NamesEqual(aChild.mName, "Image"))
        {
            if (!ReadString(aChild, anEmitter.mImageId))
                return false;
        }
        else if (NamesEqual(aChild.mName, "ImageRow"))
        {
            if (!ReadInteger(aChild, anEmitter.mImageRow))
                return false;
        }
        else if (NamesEqual(aChild.mName, "ImageCol"))
        {
            if (!ReadInteger(aChild, anEmitter.mImageColumn))
                return false;
        }
        else if (NamesEqual(aChild.mName, "ImageFrames"))
        {
            if (!ReadInteger(aChild, anEmitter.mImageFrames))
                return false;
        }
        else if (NamesEqual(aChild.mName, "Animated"))
        {
            if (!ReadInteger(aChild, anEmitter.mAnimated))
                return false;
        }
        else if (NamesEqual(aChild.mName, "EmitterType"))
        {
            if (!ReadEmitterType(aChild, anEmitter.mEmitterType))
                return false;
        }
        else if (NamesEqual(aChild.mName, "Name"))
        {
            if (!ReadString(aChild, anEmitter.mName))
                return false;
        }
        else if (NamesEqual(aChild.mName, "OnDuration"))
        {
            if (!ReadString(aChild, anEmitter.mOnDuration))
                return false;
        }
        else if (NamesEqual(aChild.mName, "Field") ||
                 NamesEqual(aChild.mName, "SystemField"))
        {
            auto& aFields = NamesEqual(aChild.mName, "Field")
                ? anEmitter.mParticleFields
                : anEmitter.mSystemFields;
            if (aFields.size() >= kMaximumFieldsPerEmitter)
            {
                Fail(ParticleDefinitionError::TooManyFields, aChild);
                return false;
            }

            ParticleFieldDefinition aField;
            if (!MapField(aChild, aField))
                return false;
            aFields.push_back(std::move(aField));
        }
        else
        {
            Fail(ParticleDefinitionError::UnknownElement, aChild);
            return false;
        }
    }

    theEmitter = std::move(anEmitter);
    return true;
}

bool ParticleDefinitionMapper::MapField(
    const engine::XmlNode& theNode,
    ParticleFieldDefinition& theField)
{
    if (!HasOnlyWhitespace(theNode.mValue))
    {
        Fail(ParticleDefinitionError::InvalidElementShape, theNode);
        return false;
    }

    ParticleFieldDefinition aField;
    for (const auto& aChild : theNode.mChildren)
    {
        if (NamesEqual(aChild.mName, "FieldType"))
        {
            if (!ReadFieldType(aChild, aField.mType))
                return false;
        }
        else if (NamesEqual(aChild.mName, "x"))
        {
            if (!ReadParameterTrack(aChild, aField.mX))
                return false;
        }
        else if (NamesEqual(aChild.mName, "y"))
        {
            if (!ReadParameterTrack(aChild, aField.mY))
                return false;
        }
        else
        {
            Fail(ParticleDefinitionError::UnknownElement, aChild);
            return false;
        }
    }

    theField = std::move(aField);
    return true;
}

bool ParticleDefinitionMapper::ReadInteger(
    const engine::XmlNode& theNode,
    std::int32_t& theValue)
{
    if (!theNode.mChildren.empty())
    {
        Fail(ParticleDefinitionError::InvalidElementShape, theNode);
        return false;
    }

    const auto aText = TrimAscii(theNode.mValue);
    std::int32_t aValue{};
    const auto [anEnd, anError] = std::from_chars(
        aText.data(),
        aText.data() + aText.size(),
        aValue);
    if (aText.empty() ||
        anError != std::errc{} ||
        anEnd != aText.data() + aText.size())
    {
        Fail(ParticleDefinitionError::InvalidInteger, theNode);
        return false;
    }

    theValue = aValue;
    return true;
}

bool ParticleDefinitionMapper::ReadString(
    const engine::XmlNode& theNode,
    std::string& theValue)
{
    if (!theNode.mChildren.empty())
    {
        Fail(ParticleDefinitionError::InvalidElementShape, theNode);
        return false;
    }
    theValue = theNode.mValue;
    return true;
}

bool ParticleDefinitionMapper::ReadFlag(
    const engine::XmlNode& theNode,
    ParticleFlag theFlag,
    std::uint32_t& theFlags)
{
    std::int32_t aValue{};
    if (!ReadInteger(theNode, aValue))
        return false;
    if (aValue != 0 && aValue != 1)
    {
        Fail(ParticleDefinitionError::InvalidFlag, theNode);
        return false;
    }

    const auto aBit = static_cast<std::uint8_t>(theFlag);
    const std::uint32_t aMask = std::uint32_t{1} << aBit;
    if (aValue == 1)
        theFlags |= aMask;
    else
        theFlags &= ~aMask;
    return true;
}

bool ParticleDefinitionMapper::ReadEmitterType(
    const engine::XmlNode& theNode,
    ParticleEmitterType& theType)
{
    std::string aValue;
    if (!ReadString(theNode, aValue))
        return false;
    const auto aName = TrimAscii(aValue);
    for (const auto& aTypeName : kEmitterTypeNames)
    {
        if (NamesEqual(aName, aTypeName.mName))
        {
            theType = aTypeName.mType;
            return true;
        }
    }

    Fail(ParticleDefinitionError::InvalidEnum, theNode);
    return false;
}

bool ParticleDefinitionMapper::ReadFieldType(
    const engine::XmlNode& theNode,
    ParticleFieldType& theType)
{
    std::string aValue;
    if (!ReadString(theNode, aValue))
        return false;
    const auto aName = TrimAscii(aValue);
    for (const auto& aTypeName : kFieldTypeNames)
    {
        if (NamesEqual(aName, aTypeName.mName))
        {
            theType = aTypeName.mType;
            return true;
        }
    }

    Fail(ParticleDefinitionError::InvalidEnum, theNode);
    return false;
}

bool ParticleDefinitionMapper::ReadParameterTrack(
    const engine::XmlNode& theNode,
    ParameterTrack& theTrack)
{
    if (!theNode.mChildren.empty())
    {
        Fail(ParticleDefinitionError::InvalidElementShape, theNode);
        return false;
    }

    ParameterTrackParser aParser;
    if (!aParser.Parse(theNode.mValue, theTrack))
    {
        mParameterTrackError = aParser.GetError();
        mParameterTrackErrorOffset = aParser.GetErrorOffset();
        Fail(ParticleDefinitionError::InvalidParameterTrack, theNode);
        return false;
    }
    return true;
}

void ParticleDefinitionMapper::Reset()
{
    mError = ParticleDefinitionError::None;
    mParameterTrackError = ParameterTrackError::None;
    mParameterTrackErrorOffset = 0;
    mErrorLine = 0;
    mErrorElement.clear();
}

void ParticleDefinitionMapper::Fail(
    ParticleDefinitionError theError,
    const engine::XmlNode& theNode)
{
    if (mError != ParticleDefinitionError::None)
        return;
    mError = theError;
    mErrorLine = theNode.mLine;
    mErrorElement = theNode.mName;
}

ParticleDefinitionError ParticleDefinitionMapper::GetError() const
{
    return mError;
}

ParameterTrackError ParticleDefinitionMapper::GetParameterTrackError() const
{
    return mParameterTrackError;
}

std::size_t ParticleDefinitionMapper::GetParameterTrackErrorOffset() const
{
    return mParameterTrackErrorOffset;
}

std::uint32_t ParticleDefinitionMapper::GetErrorLine() const
{
    return mErrorLine;
}

std::string_view ParticleDefinitionMapper::GetErrorElement() const
{
    return mErrorElement;
}

bool HasParticleFlag(
    std::uint32_t theFlags,
    ParticleFlag theFlag)
{
    const auto aBit = static_cast<std::uint8_t>(theFlag);
    return (theFlags & (std::uint32_t{1} << aBit)) != 0;
}

const char* GetParticleDefinitionErrorMessage(
    ParticleDefinitionError theError)
{
    switch (theError)
    {
    case ParticleDefinitionError::None:
        return "no error";
    case ParticleDefinitionError::EmptyDocument:
        return "particle definition is empty";
    case ParticleDefinitionError::UnknownElement:
        return "particle definition contains an unknown element";
    case ParticleDefinitionError::InvalidElementShape:
        return "particle definition element has invalid contents";
    case ParticleDefinitionError::InvalidInteger:
        return "particle definition contains an invalid integer";
    case ParticleDefinitionError::InvalidFlag:
        return "particle definition flag must be zero or one";
    case ParticleDefinitionError::InvalidEnum:
        return "particle definition contains an invalid enum value";
    case ParticleDefinitionError::InvalidParameterTrack:
        return "particle definition contains an invalid parameter track";
    case ParticleDefinitionError::TooManyEmitters:
        return "particle definition contains too many emitters";
    case ParticleDefinitionError::TooManyFields:
        return "particle emitter contains too many fields";
    }
    return "unknown particle definition error";
}

} // namespace pvz::game
