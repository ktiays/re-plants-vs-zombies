#pragma once

#include "pvz/engine/Image.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace pvz::engine
{

class IResourceStore
{
public:
    virtual ~IResourceStore() = default;

    [[nodiscard]] virtual bool Contains(
        std::string_view thePath) const = 0;
    [[nodiscard]] virtual bool GetSize(
        std::string_view thePath,
        std::uint64_t& theSize) const = 0;
    [[nodiscard]] virtual bool ReadAll(
        std::string_view thePath,
        std::vector<std::byte>& theBytes) const = 0;
};

enum class ImageResourceError : std::uint8_t
{
    None,
    ManifestNotLoaded,
    ResourceNotFound,
    SourceNotFound,
    SourceReadFailed,
    DecodeFailed,
    AlphaSizeMismatch,
    UploadFailed,
};

struct ImageResourceDiagnostic
{
    ImageResourceError mError{ImageResourceError::None};
    ImageDecodeError mDecodeError{ImageDecodeError::None};
    std::string mPath;
};

struct ImageResource
{
    ImageHandle mImage{};
    SizeI mSize{};
    std::uint32_t mRows{1};
    std::uint32_t mColumns{1};
};

class IImageResources
{
public:
    virtual ~IImageResources() = default;

    [[nodiscard]] virtual bool Load(
        std::string_view theResourceId,
        ImageResource& theResource,
        ImageResourceDiagnostic& theDiagnostic) = 0;
    virtual void Release(ImageHandle theImage) = 0;
};

} // namespace pvz::engine
