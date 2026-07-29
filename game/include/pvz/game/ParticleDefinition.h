#pragma once

#include "pvz/engine/Xml.h"
#include "pvz/game/ParameterTrack.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pvz::game
{

enum class ParticleEmitterType : std::uint8_t
{
    Circle = 0,
    Box = 1,
    BoxPath = 2,
    CirclePath = 3,
    CircleEvenSpacing = 4,
};

enum class ParticleFieldType : std::uint8_t
{
    Invalid = 0,
    Friction = 1,
    Acceleration = 2,
    Attractor = 3,
    MaximumVelocity = 4,
    Velocity = 5,
    Position = 6,
    SystemPosition = 7,
    GroundConstraint = 8,
    Shake = 9,
    Circle = 10,
    Away = 11,
};

enum class ParticleFlag : std::uint8_t
{
    RandomLaunchSpin = 0,
    AlignLaunchSpin = 1,
    AlignToPixel = 2,
    SystemLoops = 3,
    ParticleLoops = 4,
    ParticlesDoNotFollow = 5,
    RandomStartTime = 6,
    DieIfOverloaded = 7,
    Additive = 8,
    FullScreen = 9,
    SoftwareOnly = 10,
    HardwareOnly = 11,
};

struct ParticleFieldDefinition
{
    ParticleFieldType mType{ParticleFieldType::Invalid};
    ParameterTrack mX;
    ParameterTrack mY;
};

struct ParticleEmitterDefinition
{
    std::string mImageId;
    std::int32_t mImageRow{};
    std::int32_t mImageColumn{};
    std::int32_t mImageFrames{1};
    std::int32_t mAnimated{};
    std::uint32_t mFlags{};
    ParticleEmitterType mEmitterType{ParticleEmitterType::Box};
    std::string mName;
    std::string mOnDuration;

    ParameterTrack mSystemDuration;
    ParameterTrack mCrossFadeDuration;
    ParameterTrack mSpawnRate;
    ParameterTrack mSpawnMinimumActive;
    ParameterTrack mSpawnMaximumActive;
    ParameterTrack mSpawnMaximumLaunched;
    ParameterTrack mEmitterRadius;
    ParameterTrack mEmitterOffsetX;
    ParameterTrack mEmitterOffsetY;
    ParameterTrack mEmitterBoxX;
    ParameterTrack mEmitterBoxY;
    ParameterTrack mEmitterPath;
    ParameterTrack mEmitterSkewX;
    ParameterTrack mEmitterSkewY;
    ParameterTrack mParticleDuration;
    ParameterTrack mSystemRed;
    ParameterTrack mSystemGreen;
    ParameterTrack mSystemBlue;
    ParameterTrack mSystemAlpha;
    ParameterTrack mSystemBrightness;
    ParameterTrack mLaunchSpeed;
    ParameterTrack mLaunchAngle;
    std::vector<ParticleFieldDefinition> mParticleFields;
    std::vector<ParticleFieldDefinition> mSystemFields;
    ParameterTrack mParticleRed;
    ParameterTrack mParticleGreen;
    ParameterTrack mParticleBlue;
    ParameterTrack mParticleAlpha;
    ParameterTrack mParticleBrightness;
    ParameterTrack mParticleSpinAngle;
    ParameterTrack mParticleSpinSpeed;
    ParameterTrack mParticleScale;
    ParameterTrack mParticleStretch;
    ParameterTrack mCollisionReflect;
    ParameterTrack mCollisionSpin;
    ParameterTrack mClipTop;
    ParameterTrack mClipBottom;
    ParameterTrack mClipLeft;
    ParameterTrack mClipRight;
    ParameterTrack mAnimationRate;
};

struct ParticleDefinition
{
    std::vector<ParticleEmitterDefinition> mEmitters;
};

enum class ParticleDefinitionError : std::uint8_t
{
    None,
    EmptyDocument,
    UnknownElement,
    InvalidElementShape,
    InvalidInteger,
    InvalidFlag,
    InvalidEnum,
    InvalidParameterTrack,
    TooManyEmitters,
    TooManyFields,
};

class ParticleDefinitionMapper
{
public:
    [[nodiscard]] bool Map(
        std::span<const engine::XmlNode> theRoots,
        ParticleDefinition& theDefinition);

    [[nodiscard]] ParticleDefinitionError GetError() const;
    [[nodiscard]] ParameterTrackError GetParameterTrackError() const;
    [[nodiscard]] std::size_t GetParameterTrackErrorOffset() const;
    [[nodiscard]] std::uint32_t GetErrorLine() const;
    [[nodiscard]] std::string_view GetErrorElement() const;

private:
    [[nodiscard]] bool MapEmitter(
        const engine::XmlNode& theNode,
        ParticleEmitterDefinition& theEmitter);
    [[nodiscard]] bool MapField(
        const engine::XmlNode& theNode,
        ParticleFieldDefinition& theField);
    [[nodiscard]] bool ReadInteger(
        const engine::XmlNode& theNode,
        std::int32_t& theValue);
    [[nodiscard]] bool ReadString(
        const engine::XmlNode& theNode,
        std::string& theValue);
    [[nodiscard]] bool ReadFlag(
        const engine::XmlNode& theNode,
        ParticleFlag theFlag,
        std::uint32_t& theFlags);
    [[nodiscard]] bool ReadEmitterType(
        const engine::XmlNode& theNode,
        ParticleEmitterType& theType);
    [[nodiscard]] bool ReadFieldType(
        const engine::XmlNode& theNode,
        ParticleFieldType& theType);
    [[nodiscard]] bool ReadParameterTrack(
        const engine::XmlNode& theNode,
        ParameterTrack& theTrack);
    void Reset();
    void Fail(
        ParticleDefinitionError theError,
        const engine::XmlNode& theNode);

    ParticleDefinitionError mError{ParticleDefinitionError::None};
    ParameterTrackError mParameterTrackError{ParameterTrackError::None};
    std::size_t mParameterTrackErrorOffset{};
    std::uint32_t mErrorLine{};
    std::string mErrorElement;
};

[[nodiscard]] bool HasParticleFlag(
    std::uint32_t theFlags,
    ParticleFlag theFlag);

[[nodiscard]] const char* GetParticleDefinitionErrorMessage(
    ParticleDefinitionError theError);

} // namespace pvz::game
