#include "pvz/engine/image/PortableImageDecoder.h"

#include <jpeglib.h>
#include <png.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <type_traits>
#include <vector>

namespace
{

int gCodecFailureCount{};

void Expect(bool theCondition, const char* theMessage)
{
    if (theCondition)
        return;
    std::cerr << "FAIL: " << theMessage << '\n';
    ++gCodecFailureCount;
}

[[nodiscard]] std::vector<std::byte> EncodePng()
{
    png_image anImage{};
    anImage.version = PNG_IMAGE_VERSION;
    anImage.width = 2;
    anImage.height = 1;
    anImage.format = PNG_FORMAT_RGBA;
    constexpr std::array<std::uint8_t, 8> kPixels{
        255, 0, 0, 255,
        0, 255, 0, 128,
    };

    png_alloc_size_t anEncodedSize{};
    if (png_image_write_to_memory(
            &anImage,
            nullptr,
            &anEncodedSize,
            0,
            kPixels.data(),
            0,
            nullptr) == 0)
    {
        return {};
    }

    std::vector<std::byte> anEncoded(anEncodedSize);
    if (png_image_write_to_memory(
            &anImage,
            anEncoded.data(),
            &anEncodedSize,
            0,
            kPixels.data(),
            0,
            nullptr) == 0)
    {
        return {};
    }
    anEncoded.resize(anEncodedSize);
    return anEncoded;
}

template<typename>
struct ThirdArgument;

template<typename Return, typename First, typename Second, typename Third>
struct ThirdArgument<Return (*)(First, Second, Third)>
{
    using Type = std::remove_pointer_t<Third>;
};

[[nodiscard]] std::vector<std::byte> EncodeJpeg()
{
    jpeg_compress_struct aJpeg{};
    jpeg_error_mgr anError{};
    aJpeg.err = jpeg_std_error(&anError);
    jpeg_create_compress(&aJpeg);

    unsigned char* anEncodedBytes = nullptr;
    using JpegMemorySize =
        ThirdArgument<decltype(&jpeg_mem_dest)>::Type;
    JpegMemorySize anEncodedSize{};
    jpeg_mem_dest(&aJpeg, &anEncodedBytes, &anEncodedSize);

    aJpeg.image_width = 2;
    aJpeg.image_height = 1;
    aJpeg.input_components = 3;
    aJpeg.in_color_space = JCS_RGB;
    jpeg_set_defaults(&aJpeg);
    jpeg_set_quality(&aJpeg, 95, TRUE);
    jpeg_start_compress(&aJpeg, TRUE);

    std::array<JSAMPLE, 6> aPixels{
        255, 0, 0,
        0, 255, 0,
    };
    JSAMPROW aRow = aPixels.data();
    static_cast<void>(
        jpeg_write_scanlines(&aJpeg, &aRow, 1));
    jpeg_finish_compress(&aJpeg);

    std::vector<std::byte> anEncoded(
        static_cast<std::size_t>(anEncodedSize));
    std::memcpy(
        anEncoded.data(),
        anEncodedBytes,
        anEncoded.size());
    std::free(anEncodedBytes);
    jpeg_destroy_compress(&aJpeg);
    return anEncoded;
}

void TestPngDecode()
{
    const auto anEncoded = EncodePng();
    Expect(!anEncoded.empty(), "PNG fixture encodes");

    pvz::engine::image::PortableImageDecoder aDecoder;
    pvz::engine::DecodedImage anImage;
    pvz::engine::ImageDecodeError anError{};
    Expect(
        aDecoder.Decode(anEncoded, anImage, anError),
        "PNG fixture decodes");
    Expect(
        anImage.mDescriptor.mSize.mWidth == 2 &&
            anImage.mDescriptor.mSize.mHeight == 1,
        "PNG dimensions");
    Expect(anImage.mBytesPerRow == 8, "PNG BGRA row stride");
    if (anImage.mPixels.size() == 8)
    {
        Expect(
            anImage.mPixels[0] == std::byte{0} &&
                anImage.mPixels[1] == std::byte{0} &&
                anImage.mPixels[2] == std::byte{255} &&
                anImage.mPixels[3] == std::byte{255},
            "PNG red pixel converts to straight BGRA");
        Expect(
            anImage.mPixels[4] == std::byte{0} &&
                anImage.mPixels[5] == std::byte{255} &&
                anImage.mPixels[6] == std::byte{0} &&
                anImage.mPixels[7] == std::byte{128},
            "PNG alpha remains straight");
    }
}

void TestJpegDecode()
{
    const auto anEncoded = EncodeJpeg();
    Expect(!anEncoded.empty(), "JPEG fixture encodes");

    pvz::engine::image::PortableImageDecoder aDecoder;
    pvz::engine::DecodedImage anImage;
    pvz::engine::ImageDecodeError anError{};
    Expect(
        aDecoder.Decode(anEncoded, anImage, anError),
        "JPEG fixture decodes");
    Expect(
        anImage.mDescriptor.mSize.mWidth == 2 &&
            anImage.mDescriptor.mSize.mHeight == 1,
        "JPEG dimensions");
    Expect(anImage.mBytesPerRow == 8, "JPEG BGRA row stride");
    Expect(
        anImage.mPixels.size() == 8 &&
            anImage.mPixels[3] == std::byte{255} &&
            anImage.mPixels[7] == std::byte{255},
        "JPEG output is opaque");
}

void TestGifDecode()
{
    constexpr std::array<std::byte, 43> kTransparentGif{
        std::byte{0x47}, std::byte{0x49}, std::byte{0x46},
        std::byte{0x38}, std::byte{0x39}, std::byte{0x61},
        std::byte{0x01}, std::byte{0x00}, std::byte{0x01},
        std::byte{0x00}, std::byte{0x80}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0xFF}, std::byte{0xFF},
        std::byte{0xFF}, std::byte{0x21}, std::byte{0xF9},
        std::byte{0x04}, std::byte{0x01}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x2C}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x01},
        std::byte{0x00}, std::byte{0x01}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x02}, std::byte{0x02},
        std::byte{0x44}, std::byte{0x01}, std::byte{0x00},
        std::byte{0x3B},
    };

    pvz::engine::image::PortableImageDecoder aDecoder;
    pvz::engine::DecodedImage anImage;
    pvz::engine::ImageDecodeError anError{};
    Expect(
        aDecoder.Decode(kTransparentGif, anImage, anError),
        "GIF fixture decodes");
    Expect(
        anImage.mDescriptor.mSize.mWidth == 1 &&
            anImage.mDescriptor.mSize.mHeight == 1,
        "GIF dimensions");
    Expect(
        anImage.mPixels.size() == 4 &&
            anImage.mPixels[3] == std::byte{0},
        "GIF transparent color is preserved");
}

void TestDecodeErrors()
{
    pvz::engine::image::PortableImageDecoder aDecoder;
    pvz::engine::DecodedImage anImage;
    pvz::engine::ImageDecodeError anError{};
    Expect(
        !aDecoder.Decode({}, anImage, anError) &&
            anError == pvz::engine::ImageDecodeError::EmptyInput,
        "empty image input is rejected");

    constexpr std::array<std::byte, 4> kUnknown{
        std::byte{1},
        std::byte{2},
        std::byte{3},
        std::byte{4},
    };
    Expect(
        !aDecoder.Decode(kUnknown, anImage, anError) &&
            anError ==
                pvz::engine::ImageDecodeError::UnsupportedFormat,
        "unknown image input is rejected");
}

} // namespace

int main()
{
    TestPngDecode();
    TestJpegDecode();
    TestGifDecode();
    TestDecodeErrors();
    if (gCodecFailureCount != 0)
    {
        std::cerr
            << gCodecFailureCount
            << " image codec assertion(s) failed\n";
        return 1;
    }
    std::cout << "Portable image codec tests passed\n";
    return 0;
}
