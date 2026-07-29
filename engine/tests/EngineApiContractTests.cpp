#include "pvz/engine/Game.h"
#include "pvz/engine/Resources.h"
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
static_assert(std::has_virtual_destructor_v<pvz::engine::IGame>);
static_assert(std::has_virtual_destructor_v<pvz::engine::IResourceStore>);
static_assert(std::has_virtual_destructor_v<pvz::engine::IStateReader>);
static_assert(std::has_virtual_destructor_v<pvz::engine::IStateWriter>);
static_assert(
    std::has_virtual_destructor_v<pvz::engine::IXmlDocumentLoader>);
