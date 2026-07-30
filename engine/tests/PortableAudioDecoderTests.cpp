#include "pvz/engine/audio/PortableAudioDecoder.h"

#include <array>
#include <cstddef>
#include <iostream>

namespace
{

int gFailureCount{};

void Expect(bool theCondition, const char* theMessage)
{
    if (theCondition)
        return;
    std::cerr << "FAIL: " << theMessage << '\n';
    ++gFailureCount;
}

void TestInputDiagnostics()
{
    const pvz::engine::audio::PortableAudioDecoder aDecoder;
    pvz::engine::DecodedSound aSound;
    pvz::engine::SoundDecodeError anError{};

    Expect(
        !aDecoder.Decode({}, aSound, anError),
        "empty audio input fails");
    Expect(
        anError == pvz::engine::SoundDecodeError::EmptyInput,
        "empty audio input reports its error");

    constexpr std::array<std::byte, 4> kWaveMagic{
        std::byte{'R'},
        std::byte{'I'},
        std::byte{'F'},
        std::byte{'F'},
    };
    Expect(
        !aDecoder.Decode(kWaveMagic, aSound, anError),
        "unsupported audio input fails");
    Expect(
        anError ==
            pvz::engine::SoundDecodeError::UnsupportedFormat,
        "unsupported audio input reports its error");

    constexpr std::array<std::byte, 8> kTruncatedOgg{
        std::byte{'O'},
        std::byte{'g'},
        std::byte{'g'},
        std::byte{'S'},
        std::byte{},
        std::byte{},
        std::byte{},
        std::byte{},
    };
    Expect(
        !aDecoder.Decode(kTruncatedOgg, aSound, anError),
        "truncated OGG input fails");
    Expect(
        anError == pvz::engine::SoundDecodeError::InvalidData,
        "truncated OGG input reports invalid data");
}

} // namespace

int main()
{
    TestInputDiagnostics();
    if (gFailureCount != 0)
    {
        std::cerr << gFailureCount << " test assertion(s) failed\n";
        return 1;
    }
    std::cout << "Portable audio decoder tests passed\n";
    return 0;
}
