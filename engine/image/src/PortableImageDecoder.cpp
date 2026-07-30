#include "pvz/engine/image/PortableImageDecoder.h"

#include <gif_lib.h>
#include <jpeglib.h>
#include <png.h>

#include <array>
#include <csetjmp>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <span>
#include <vector>

namespace pvz::engine::image
{
namespace
{

inline constexpr std::uint32_t kBytesPerPixel = 4;
inline constexpr std::array<std::byte, 8> kPngSignature{
    std::byte{0x89},
    std::byte{0x50},
    std::byte{0x4E},
    std::byte{0x47},
    std::byte{0x0D},
    std::byte{0x0A},
    std::byte{0x1A},
    std::byte{0x0A},
};

[[nodiscard]] bool IsPng(std::span<const std::byte> theBytes)
{
    return theBytes.size() >= kPngSignature.size() &&
           std::memcmp(
               theBytes.data(),
               kPngSignature.data(),
               kPngSignature.size()) == 0;
}

[[nodiscard]] bool IsJpeg(std::span<const std::byte> theBytes)
{
    return theBytes.size() >= 2 &&
           theBytes[0] == std::byte{0xFF} &&
           theBytes[1] == std::byte{0xD8};
}

[[nodiscard]] bool IsGif(std::span<const std::byte> theBytes)
{
    constexpr std::array<std::byte, 4> kSignature{
        std::byte{'G'},
        std::byte{'I'},
        std::byte{'F'},
        std::byte{'8'},
    };
    return theBytes.size() >= kSignature.size() &&
           std::memcmp(
               theBytes.data(),
               kSignature.data(),
               kSignature.size()) == 0;
}

[[nodiscard]] bool CalculateLayout(
    std::uint32_t theWidth,
    std::uint32_t theHeight,
    std::uint32_t& theBytesPerRow,
    std::size_t& theByteCount,
    ImageDecodeError& theError)
{
    if (theWidth == 0 || theHeight == 0 ||
        theWidth >
            std::numeric_limits<std::uint32_t>::max() /
                kBytesPerPixel)
    {
        theError = ImageDecodeError::DimensionsUnsupported;
        return false;
    }

    theBytesPerRow = theWidth * kBytesPerPixel;
    if (theHeight >
        std::numeric_limits<std::size_t>::max() /
            theBytesPerRow)
    {
        theError = ImageDecodeError::SizeOverflow;
        return false;
    }
    theByteCount =
        static_cast<std::size_t>(theHeight) * theBytesPerRow;
    return true;
}

[[nodiscard]] bool TryMultiply(
    std::size_t theLeft,
    std::size_t theRight,
    std::size_t& theResult)
{
    if (theLeft != 0 &&
        theRight >
            std::numeric_limits<std::size_t>::max() / theLeft)
    {
        return false;
    }
    theResult = theLeft * theRight;
    return true;
}

[[nodiscard]] bool DecodePng(
    std::span<const std::byte> theBytes,
    DecodedImage& theImage,
    ImageDecodeError& theError)
{
    png_image aPng{};
    aPng.version = PNG_IMAGE_VERSION;
    if (png_image_begin_read_from_memory(
            &aPng,
            theBytes.data(),
            theBytes.size()) == 0)
    {
        theError = ImageDecodeError::InvalidData;
        return false;
    }

    const auto aWidth = static_cast<std::uint32_t>(aPng.width);
    const auto aHeight = static_cast<std::uint32_t>(aPng.height);
    std::uint32_t aBytesPerRow{};
    std::size_t aByteCount{};
    if (!CalculateLayout(
            aWidth,
            aHeight,
            aBytesPerRow,
            aByteCount,
            theError) ||
        aBytesPerRow >
            static_cast<std::uint32_t>(
                std::numeric_limits<png_int_32>::max()))
    {
        png_image_free(&aPng);
        if (theError == ImageDecodeError::None)
            theError = ImageDecodeError::DimensionsUnsupported;
        return false;
    }

    std::vector<std::byte> aPixels;
    try
    {
        aPixels.resize(aByteCount);
    }
    catch (const std::bad_alloc&)
    {
        png_image_free(&aPng);
        theError = ImageDecodeError::AllocationFailed;
        return false;
    }

    aPng.format = PNG_FORMAT_BGRA;
    if (png_image_finish_read(
            &aPng,
            nullptr,
            aPixels.data(),
            static_cast<png_int_32>(aBytesPerRow),
            nullptr) == 0)
    {
        png_image_free(&aPng);
        theError = ImageDecodeError::InvalidData;
        return false;
    }
    png_image_free(&aPng);

    theImage = {
        .mDescriptor =
            {
                .mSize = {aWidth, aHeight},
                .mPixelFormat = ImagePixelFormat::Bgra8Unorm,
            },
        .mBytesPerRow = aBytesPerRow,
        .mPixels = std::move(aPixels),
    };
    return true;
}

struct JpegErrorManager
{
    jpeg_error_mgr mBase;
    std::jmp_buf mJump;
};

void HandleJpegError(j_common_ptr theCommon)
{
    auto* anError = reinterpret_cast<JpegErrorManager*>(
        theCommon->err);
    std::longjmp(anError->mJump, 1);
}

[[nodiscard]] bool DecodeJpeg(
    std::span<const std::byte> theBytes,
    DecodedImage& theImage,
    ImageDecodeError& theError)
{
    if (theBytes.size() >
        std::numeric_limits<std::uint32_t>::max())
    {
        theError = ImageDecodeError::SizeOverflow;
        return false;
    }

    jpeg_decompress_struct aJpeg{};
    JpegErrorManager anError{};
    aJpeg.err = jpeg_std_error(&anError.mBase);
    anError.mBase.error_exit = HandleJpegError;
    if (setjmp(anError.mJump) != 0)
    {
        jpeg_destroy_decompress(&aJpeg);
        theError = ImageDecodeError::InvalidData;
        return false;
    }

    jpeg_create_decompress(&aJpeg);
    const auto aSourceSize =
        static_cast<std::uint32_t>(theBytes.size());
    jpeg_mem_src(
        &aJpeg,
        reinterpret_cast<const unsigned char*>(theBytes.data()),
        aSourceSize);
    if (jpeg_read_header(&aJpeg, TRUE) != JPEG_HEADER_OK)
    {
        jpeg_destroy_decompress(&aJpeg);
        theError = ImageDecodeError::InvalidData;
        return false;
    }

    aJpeg.out_color_space = JCS_RGB;
    if (jpeg_start_decompress(&aJpeg) == FALSE ||
        aJpeg.output_components != 3)
    {
        jpeg_destroy_decompress(&aJpeg);
        theError = ImageDecodeError::InvalidData;
        return false;
    }

    const auto aWidth =
        static_cast<std::uint32_t>(aJpeg.output_width);
    const auto aHeight =
        static_cast<std::uint32_t>(aJpeg.output_height);
    std::uint32_t aBytesPerRow{};
    std::size_t aByteCount{};
    std::size_t aSourceRowByteCount{};
    if (!CalculateLayout(
            aWidth,
            aHeight,
            aBytesPerRow,
            aByteCount,
            theError) ||
        !TryMultiply(
            static_cast<std::size_t>(aWidth),
            3,
            aSourceRowByteCount))
    {
        jpeg_destroy_decompress(&aJpeg);
        if (theError == ImageDecodeError::None)
            theError = ImageDecodeError::SizeOverflow;
        return false;
    }

    std::vector<std::byte> aPixels;
    std::vector<JSAMPLE> aSourceRow;
    try
    {
        aPixels.resize(aByteCount);
        aSourceRow.resize(aSourceRowByteCount);
    }
    catch (const std::bad_alloc&)
    {
        jpeg_destroy_decompress(&aJpeg);
        theError = ImageDecodeError::AllocationFailed;
        return false;
    }

    while (aJpeg.output_scanline < aJpeg.output_height)
    {
        JSAMPROW aRowPointer = aSourceRow.data();
        if (jpeg_read_scanlines(&aJpeg, &aRowPointer, 1) != 1)
        {
            jpeg_destroy_decompress(&aJpeg);
            theError = ImageDecodeError::InvalidData;
            return false;
        }

        const auto aDestinationRow =
            static_cast<std::size_t>(aJpeg.output_scanline - 1);
        auto* aDestination =
            aPixels.data() + aDestinationRow * aBytesPerRow;
        for (std::uint32_t aColumn = 0;
             aColumn < aWidth;
             ++aColumn)
        {
            const auto aSourceOffset =
                static_cast<std::size_t>(aColumn) * 3;
            const auto aDestinationOffset =
                static_cast<std::size_t>(aColumn) *
                kBytesPerPixel;
            aDestination[aDestinationOffset + 0] =
                static_cast<std::byte>(
                    aSourceRow[aSourceOffset + 2]);
            aDestination[aDestinationOffset + 1] =
                static_cast<std::byte>(
                    aSourceRow[aSourceOffset + 1]);
            aDestination[aDestinationOffset + 2] =
                static_cast<std::byte>(
                    aSourceRow[aSourceOffset + 0]);
            aDestination[aDestinationOffset + 3] =
                std::byte{0xFF};
        }
    }

    if (jpeg_finish_decompress(&aJpeg) == FALSE)
    {
        jpeg_destroy_decompress(&aJpeg);
        theError = ImageDecodeError::InvalidData;
        return false;
    }
    jpeg_destroy_decompress(&aJpeg);

    theImage = {
        .mDescriptor =
            {
                .mSize = {aWidth, aHeight},
                .mPixelFormat = ImagePixelFormat::Bgra8Unorm,
            },
        .mBytesPerRow = aBytesPerRow,
        .mPixels = std::move(aPixels),
    };
    return true;
}

struct GifMemorySource
{
    std::span<const std::byte> mBytes;
    std::size_t mOffset{};
};

int ReadGifBytes(
    GifFileType* theGif,
    GifByteType* theDestination,
    int theByteCount)
{
    if (theByteCount <= 0)
        return 0;
    auto* aSource =
        static_cast<GifMemorySource*>(theGif->UserData);
    const auto aRemaining =
        aSource->mBytes.size() - aSource->mOffset;
    const auto aReadSize = std::min(
        static_cast<std::size_t>(theByteCount),
        aRemaining);
    std::memcpy(
        theDestination,
        aSource->mBytes.data() + aSource->mOffset,
        aReadSize);
    aSource->mOffset += aReadSize;
    return static_cast<int>(aReadSize);
}

[[nodiscard]] bool DecodeGif(
    std::span<const std::byte> theBytes,
    DecodedImage& theImage,
    ImageDecodeError& theError)
{
    GifMemorySource aSource{.mBytes = theBytes};
    int aGifError{};
    GifFileType* aGif = DGifOpen(
        &aSource,
        ReadGifBytes,
        &aGifError);
    if (aGif == nullptr)
    {
        theError = ImageDecodeError::InvalidData;
        return false;
    }
    const auto aCloseGif = [&]()
    {
        int aCloseError{};
        static_cast<void>(DGifCloseFile(aGif, &aCloseError));
    };

    if (DGifSlurp(aGif) != GIF_OK ||
        aGif->ImageCount < 1)
    {
        aCloseGif();
        theError = ImageDecodeError::InvalidData;
        return false;
    }

    const auto& aSavedImage = aGif->SavedImages[0];
    const auto& aDescription = aSavedImage.ImageDesc;
    if (aDescription.Width <= 0 || aDescription.Height <= 0)
    {
        aCloseGif();
        theError = ImageDecodeError::DimensionsUnsupported;
        return false;
    }

    const auto aWidth =
        static_cast<std::uint32_t>(aDescription.Width);
    const auto aHeight =
        static_cast<std::uint32_t>(aDescription.Height);
    std::uint32_t aBytesPerRow{};
    std::size_t aByteCount{};
    if (!CalculateLayout(
            aWidth,
            aHeight,
            aBytesPerRow,
            aByteCount,
            theError))
    {
        aCloseGif();
        return false;
    }

    const ColorMapObject* aColorMap =
        aDescription.ColorMap != nullptr
            ? aDescription.ColorMap
            : aGif->SColorMap;
    if (aColorMap == nullptr || aColorMap->ColorCount <= 0)
    {
        aCloseGif();
        theError = ImageDecodeError::InvalidData;
        return false;
    }

    GraphicsControlBlock aControlBlock{};
    const int hasControlBlock = DGifSavedExtensionToGCB(
        aGif,
        0,
        &aControlBlock);
    const int aTransparentIndex =
        hasControlBlock == GIF_OK
            ? aControlBlock.TransparentColor
            : NO_TRANSPARENT_COLOR;

    std::vector<std::byte> aPixels;
    try
    {
        aPixels.resize(aByteCount);
    }
    catch (const std::bad_alloc&)
    {
        aCloseGif();
        theError = ImageDecodeError::AllocationFailed;
        return false;
    }

    std::uint32_t aSourceRow{};
    const auto aCopyRow =
        [&](std::uint32_t theDestinationRow) -> bool
    {
        const auto* aSourcePixels =
            aSavedImage.RasterBits +
            static_cast<std::size_t>(aSourceRow) * aWidth;
        auto* aDestination =
            aPixels.data() +
            static_cast<std::size_t>(theDestinationRow) *
                aBytesPerRow;
        for (std::uint32_t aColumn = 0;
             aColumn < aWidth;
             ++aColumn)
        {
            const int aColorIndex = aSourcePixels[aColumn];
            if (aColorIndex < 0 ||
                aColorIndex >= aColorMap->ColorCount)
            {
                return false;
            }
            const auto& aColor = aColorMap->Colors[aColorIndex];
            const auto anOffset =
                static_cast<std::size_t>(aColumn) *
                kBytesPerPixel;
            aDestination[anOffset + 0] =
                static_cast<std::byte>(aColor.Blue);
            aDestination[anOffset + 1] =
                static_cast<std::byte>(aColor.Green);
            aDestination[anOffset + 2] =
                static_cast<std::byte>(aColor.Red);
            aDestination[anOffset + 3] =
                aColorIndex == aTransparentIndex
                    ? std::byte{0}
                    : std::byte{0xFF};
        }
        ++aSourceRow;
        return true;
    };

    bool didCopy = true;
    if (aDescription.Interlace == false)
    {
        for (std::uint32_t aRow = 0;
             aRow < aHeight && didCopy;
             ++aRow)
        {
            didCopy = aCopyRow(aRow);
        }
    }
    else
    {
        constexpr std::array<std::uint32_t, 4> kStarts{
            0, 4, 2, 1,
        };
        constexpr std::array<std::uint32_t, 4> kSteps{
            8, 8, 4, 2,
        };
        for (std::size_t aPass = 0;
             aPass < kStarts.size() && didCopy;
             ++aPass)
        {
            for (std::uint32_t aRow = kStarts[aPass];
                 aRow < aHeight && didCopy;
                 aRow += kSteps[aPass])
            {
                didCopy = aCopyRow(aRow);
            }
        }
    }
    aCloseGif();
    if (!didCopy || aSourceRow != aHeight)
    {
        theError = ImageDecodeError::InvalidData;
        return false;
    }

    theImage = {
        .mDescriptor =
            {
                .mSize = {aWidth, aHeight},
                .mPixelFormat = ImagePixelFormat::Bgra8Unorm,
            },
        .mBytesPerRow = aBytesPerRow,
        .mPixels = std::move(aPixels),
    };
    return true;
}

} // namespace

bool PortableImageDecoder::Decode(
    std::span<const std::byte> theEncodedBytes,
    DecodedImage& theImage,
    ImageDecodeError& theError) const
{
    theImage = {};
    theError = ImageDecodeError::None;
    if (theEncodedBytes.empty())
    {
        theError = ImageDecodeError::EmptyInput;
        return false;
    }
    if (IsPng(theEncodedBytes))
        return DecodePng(theEncodedBytes, theImage, theError);
    if (IsJpeg(theEncodedBytes))
        return DecodeJpeg(theEncodedBytes, theImage, theError);
    if (IsGif(theEncodedBytes))
        return DecodeGif(theEncodedBytes, theImage, theError);

    theError = ImageDecodeError::UnsupportedFormat;
    return false;
}

} // namespace pvz::engine::image
