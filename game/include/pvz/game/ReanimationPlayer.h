#pragma once

#include "pvz/engine/Render.h"
#include "pvz/engine/Resources.h"
#include "pvz/engine/Xml.h"
#include "pvz/game/DefinitionLoader.h"
#include "pvz/game/ReanimationDefinition.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pvz::game
{

enum class ReanimationClipError : std::uint8_t
{
    None,
    AlreadyLoaded,
    DefinitionLoadFailed,
    EmptyDefinition,
    MismatchedTransformCount,
    TooManyTransforms,
    ImageLoadFailed,
    InvalidImageGeometry,
};

struct ReanimationClipDiagnostic
{
    ReanimationClipError mError{ReanimationClipError::None};
    DefinitionLoadError mDefinitionError{DefinitionLoadError::None};
    engine::XmlDocumentDiagnostic mSourceDiagnostic;
    engine::ImageResourceDiagnostic mImageDiagnostic;
    std::string mDetail;
};

struct ReanimationLayer
{
    std::uint32_t mFrameStart{};
    std::uint32_t mFrameCount{};
};

class ReanimationClip
{
public:
    ReanimationClip() = default;
    ReanimationClip(const ReanimationClip&) = delete;
    ReanimationClip& operator=(const ReanimationClip&) = delete;
    ReanimationClip(ReanimationClip&&) = delete;
    ReanimationClip& operator=(ReanimationClip&&) = delete;

    [[nodiscard]] bool Load(
        const engine::IXmlDocumentLoader& theDocuments,
        engine::IImageResources& theImages,
        std::string_view thePath,
        ReanimationClipDiagnostic& theDiagnostic);
    void Release(engine::IImageResources& theImages);

    [[nodiscard]] bool IsLoaded() const;
    [[nodiscard]] bool FindLayer(
        std::string_view theTrackName,
        ReanimationLayer& theLayer) const;
    [[nodiscard]] std::uint32_t GetTrackCount() const;

private:
    struct ImageEntry
    {
        std::string mId;
        engine::ImageResource mResource;
    };

    [[nodiscard]] const engine::ImageResource* FindImage(
        std::string_view theImageId) const;

    ReanimationDefinition mDefinition;
    std::vector<ImageEntry> mImages;

    friend class ReanimationPlayer;
};

class ReanimationPlayer
{
public:
    [[nodiscard]] bool Bind(
        const ReanimationClip& theClip);
    [[nodiscard]] bool Bind(
        const ReanimationClip& theClip,
        std::string_view theLayerName);
    void Reset();
    void Update();
    void RestoreTick(std::uint64_t theTick);
    [[nodiscard]] bool SetFramesPerSecond(
        float theFramesPerSecond);
    void SetHiddenTrackPrefixes(
        std::span<const std::string_view> thePrefixes);

    [[nodiscard]] bool IsBound() const;
    [[nodiscard]] std::uint64_t GetTick() const;
    void AppendSprites(
        engine::PointF thePosition,
        engine::ColorRgba8 theColor,
        std::vector<engine::SpriteDraw>& theSprites) const;

private:
    const ReanimationClip* mClip{};
    ReanimationLayer mLayer;
    std::uint64_t mTick{};
    float mFramesPerSecond{};
    std::vector<std::string> mHiddenTrackPrefixes;
};

[[nodiscard]] const char* GetReanimationClipErrorMessage(
    ReanimationClipError theError);

} // namespace pvz::game
