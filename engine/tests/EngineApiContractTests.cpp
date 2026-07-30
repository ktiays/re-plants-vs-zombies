#include "pvz/engine/Game.h"
#include "pvz/engine/Audio.h"
#include "pvz/engine/Font.h"
#include "pvz/engine/Resources.h"
#include "pvz/engine/Runtime.h"
#include "pvz/engine/StateIO.h"
#include "pvz/engine/Types.h"
#include "pvz/engine/Xml.h"

#include <cstdint>
#include <type_traits>

static_assert(pvz::engine::kSimulationFrequencyHz == 100);
static_assert(pvz::engine::kSimulationTickMicroseconds == 10'000);
static_assert(sizeof(pvz::engine::TickIndex) == sizeof(std::uint64_t));
static_assert(sizeof(pvz::engine::AssetId) == sizeof(std::uint64_t));
static_assert(sizeof(pvz::engine::ColorRgba8) == 4);
static_assert(sizeof(pvz::engine::ImageHandle) == 8);
static_assert(sizeof(pvz::engine::FontHandle) == 8);
static_assert(sizeof(pvz::engine::SoundHandle) == 8);
static_assert(sizeof(pvz::engine::VoiceHandle) == 8);
static_assert(sizeof(pvz::engine::FontMetrics) == 20);
static_assert(sizeof(pvz::engine::TextMetrics) == 4);
static_assert(
    std::is_trivially_copyable_v<pvz::engine::ImageHandle>);
static_assert(
    std::is_trivially_copyable_v<pvz::engine::FontHandle>);
static_assert(
    std::is_trivially_copyable_v<pvz::engine::SoundHandle>);
static_assert(
    std::is_trivially_copyable_v<pvz::engine::VoiceHandle>);
static_assert(std::has_virtual_destructor_v<pvz::engine::IGame>);
static_assert(std::has_virtual_destructor_v<pvz::engine::IImageDecoder>);
static_assert(std::has_virtual_destructor_v<pvz::engine::IImageStore>);
static_assert(
    std::has_virtual_destructor_v<pvz::engine::IImageResources>);
static_assert(
    std::has_virtual_destructor_v<pvz::engine::IFontResources>);
static_assert(
    std::has_virtual_destructor_v<pvz::engine::IAudioDecoder>);
static_assert(
    std::has_virtual_destructor_v<pvz::engine::IAudioDevice>);
static_assert(
    std::has_virtual_destructor_v<pvz::engine::ISoundResources>);
static_assert(std::has_virtual_destructor_v<pvz::engine::IResourceStore>);
static_assert(std::has_virtual_destructor_v<pvz::engine::IStateReader>);
static_assert(std::has_virtual_destructor_v<pvz::engine::IStateWriter>);
static_assert(
    std::has_virtual_destructor_v<pvz::engine::IXmlDocumentLoader>);
static_assert(
    std::has_virtual_destructor_v<pvz::engine::IMonotonicClock>);
static_assert(
    std::has_virtual_destructor_v<pvz::engine::IPlatformEventLoop>);
static_assert(
    std::has_virtual_destructor_v<pvz::engine::IInputSystem>);
static_assert(
    std::has_virtual_destructor_v<pvz::engine::IRenderDevice>);
