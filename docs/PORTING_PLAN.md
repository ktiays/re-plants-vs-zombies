# Native macOS renovation plan

## Objective

Renovate the reconstructed Plants vs. Zombies codebase into two major parts:

1. A portable game module containing gameplay, user-interface state, animation
   decisions, save schemas, and other game-specific behavior.
2. An engine that provides platform, rendering, audio, resource, persistence,
   timing, and diagnostics services through stable interfaces.

The game may depend only on the public engine API. It must not know whether an
application uses SDL, Win32, Metal, Direct3D, BASS, or another backend.

The existing Windows executable remains available as a parity reference while
the boundary is extracted. The native macOS application will initially compose
an SDL platform backend, a Metal renderer, and a native BASS audio backend.

## Current execution status

Last updated: 2026-07-30

- [x] Portable engine API and core CMake targets
- [x] Automated platform-leakage and architecture-dependent-type checks
- [x] Explicit little-endian state reader/writer with malformed-input tests
- [x] Portable game lifecycle target and headless 100 Hz smoke runner
- [x] Engine-owned rendering, input, logging, state, and resource protocols
- [x] Portable PAK indexing, normalization, validation, and resource reads
- [x] Validation against the supplied retail PAK: 3,198 entries and
  45,309,674 payload bytes
- [x] Portable XML documents loaded through the resource protocol, including
  the multi-root fragments used by particle definitions
- [x] Source parsing validated against all 261 XML and reanimation definitions
  in the supplied retail PAK: 971,910 nodes
- [x] Versioned architecture-neutral XML definition document cache, round-trip
  validated against those 261 sources (23,424,177 encoded bytes)
- [x] Legacy version-12 player profiles use fixed-width, fieldwise encoding
  while retaining the historical Windows byte layout
- [x] Legacy gameplay-save header and stream use explicit fixed-width
  little-endian fields with bounds-checked block reads
- [x] Legacy cursor, cursor-preview, message, seed-bank, seed-packet, and music
  snapshots use fieldwise schemas while retaining their version-2 Windows
  record sizes
- [x] macOS Release build and tests under Apple Clang
- [ ] Windows runtime parity baseline for the reconstructed legacy target
- [ ] Migration of the remaining gameplay `Board`, `Challenge`, data-array, and
  effect snapshots from raw object blocks to fieldwise fixed-width schemas
- [ ] Mapping portable XML tokens into runtime definitions
- [ ] Existing Windows backend adapters
- [ ] SDL macOS platform backend
- [ ] Metal renderer
- [ ] Native audio backend
- [ ] Full gameplay parity and productization

## Non-negotiable architecture rules

- `game/` may include only its own headers, `engine/api/`, and portable C++.
- `game/`, `engine/api/`, and `engine/core/` may not include platform SDKs.
- Backend-native objects never cross the engine API.
- Backend selection happens only in an application composition target.
- The portable game must build and run against a headless engine.
- Platform conditionals are forbidden in the portable game and engine API.
- Persistent formats use fixed-width fields and explicit byte order.
- Native C++ object layouts, pointers, padding, and `sizeof(struct)` are never
  persisted.
- Architecture-dependent integer types such as `long`, `unsigned long`,
  `size_t`, `DWORD`, and `ULONG` are forbidden in persistent contracts.
- Game updates retain the original fixed 100 Hz simulation cadence. Rendering
  and display refresh are independent from simulation timing.

The build runs an automated source-boundary check over portable code. Violating
the platform or integer-type rules is a build failure rather than a review
convention.

## Target dependency graph

```text
pvz_app_macos ──┬── pvz_game
                ├── pvz_engine_core
                ├── pvz_platform_sdl
                ├── pvz_renderer_metal
                └── pvz_audio_bass

pvz_game ──────────> pvz_engine_api
pvz_engine_core ───> pvz_engine_api

pvz_app_windows ─┬── pvz_game
                 ├── pvz_engine_core
                 ├── pvz_platform_win32
                 ├── pvz_renderer_direct3d
                 └── pvz_audio_bass
```

No arrow may point from `pvz_game` to an engine implementation or backend.

## Data-layout policy

Public contracts and persistent state use `<cstdint>` exact-width types:

- `std::uint8_t`, `std::uint16_t`, `std::uint32_t`, `std::uint64_t`
- `std::int8_t`, `std::int16_t`, `std::int32_t`, `std::int64_t`
- Explicit enum storage such as `enum class BlendMode : std::uint8_t`

`std::size_t` is permitted for transient in-process container indexing but is
never written to a file. Pointer-sized types are permitted only for local
pointer arithmetic and never enter an engine protocol.

Every new persistent stream has:

- A magic value and schema version
- An explicitly documented byte order
- Fixed-width length fields
- Bounds checks before reading or allocating
- A failure result for truncation, overflow, and invalid values
- Cross-platform golden-byte fixtures

The imported version-12 Windows profile format is an explicit compatibility
exception: it retains its existing version word and byte sequence, but its
implementation now writes each fixed-width field and historical padding word
explicitly. It no longer writes native object layouts.

The version-2 gameplay-save migration is incremental:

| Record | Current treatment |
| --- | --- |
| Header and block transport | Explicit little-endian fields, checked lengths, and truncation errors |
| Cursor object and preview | Fieldwise 76-byte and 44-byte compatibility records |
| Message widget | Fieldwise 796-byte record with fixed one-byte text and 32-bit IDs |
| Seed bank and packets | Fieldwise 848-byte record containing ten explicit 80-byte packet records |
| Music | Fieldwise 76-byte record; runtime pointers are not persisted |
| Board tail and challenge | Native blocks remain; migrate after verified 32-bit layout fixtures |
| Data-array items and effect track instances | Native blocks remain; capacity and overflow checks now guard reads |

Compatibility pointer and alignment slots are consumed when loading historical
saves but written as zero. Runtime pointers are retained or reconstructed after
load and never sourced from the file.

`Challenge` needs a Windows fixture before schema extraction: the reconstructed
source declares `mBeghouledEated` as 54 integers, while its reversed member
offsets allocate 54 bytes. The migration must establish whether compatibility
means the currently reconstructed executable layout or the original 32-bit
layout before changing this record.

Packaged compiled definition caches contain native 32-bit Windows structures
and are ignored on macOS in favor of source XML. The replacement portable
document cache is versioned and fieldwise encoded; runtime integration will
regenerate it in Application Support rather than treating packaged native
structures as portable.

## Execution phases

### Phase 0 — Preserve and measure the reference

Deliverables:

- Build the current reconstruction on Windows against the supplied GOTY assets.
- Record which game modes and GOTY features currently work.
- Capture representative screenshots, audio transitions, save files, and input
  replays.
- Record hashes and a resource manifest for the user-supplied `main.pak`.

Exit gate:

- The original retail game and reconstructed Windows target have a documented
  parity matrix.
- Reference assets are not committed to this repository.

### Phase 1 — Establish the portability firewall

Deliverables:

- Add `pvz_engine_api` and `pvz_engine_core` targets.
- Introduce exact-width engine protocol types.
- Add an explicit little-endian state reader/writer.
- Add automated dependency and forbidden-type checks.
- Make the portable targets configure, build, and test with Apple Clang.

Exit gate:

- macOS builds the portable targets without Windows headers or libraries.
- Golden serialization tests pass.
- Introducing a platform header or `unsigned long` into portable code fails the
  build.

### Phase 2 — Extract the portable game

Deliverables:

- Add `pvz_game` and `pvz_game_headless`.
- Define lifecycle, input, rendering, audio, resource, save, and timing
  interfaces.
- Move gameplay code incrementally from the legacy executable.
- Replace renderer downcasts and platform calls with engine protocols.
- Add deterministic fixed-tick replay and game-state hash tests.

Exit gate:

- `pvz_game` builds on Windows, macOS, and Linux without a platform SDK.
- A headless executable can initialize, update, save, load, and shut down.
- The same input fixture produces the same gameplay-state hashes.

### Phase 3 — Make resources and persistence portable

Deliverables:

- Rewrite PAK reading with portable file access and explicit little-endian
  fields.
- Normalize virtual asset paths independently of host filesystem case rules.
- Load XML definitions when packaged compiled caches are incompatible.
- Replace registry and AppData persistence with engine storage services.
- Import and round-trip representative Windows saves.

Exit gate:

- All packaged resources can be enumerated and validated on macOS.
- Malformed or truncated archives fail safely.
- Windows save fixtures load on macOS and serialize identically.

### Phase 4 — Adapt the Windows reference

Deliverables:

- Wrap the existing Win32 application behavior behind the platform protocol.
- Wrap the DirectDraw/Direct3D renderer behind the rendering protocol.
- Wrap the current BASS/DirectSound paths behind audio protocols.
- Retain a runnable Windows reference throughout extraction.

Exit gate:

- Windows gameplay behavior remains unchanged while `pvz_game` contains no
  Win32 or Direct3D dependency.

### Phase 5 — Native macOS and Metal

Deliverables:

- SDL event, window, input, cursor, fullscreen, high-DPI, and lifecycle backend.
- `CAMetalLayer` drawable lifecycle.
- An 800×600 logical `BGRA8Unorm` target with aspect-preserving presentation.
- Metal pipelines for textured/untextured geometry, normal/additive blending,
  point/linear sampling, clamp/wrap addressing, transforms, and scissoring.
- Dirty-generation texture uploads, private GPU textures, a per-frame vertex
  ring, state batching, and one command buffer per presented frame.
- Golden-image tests for sprite edges, clipping, transforms, text, pool
  caustics, and Direct3D half-pixel differences.

Exit gate:

- Representative title and gameplay captures match the Windows reference.
- Metal API validation reports no errors.
- No normal frame performs synchronous GPU readback or unchanged texture upload.

### Phase 6 — Audio and content parity

Deliverables:

- Native macOS BASS integration or an approved open-source replacement.
- PAK-backed OGG effects and MO3 music.
- Pattern/order jumps, tempo, layered-track muting, focus, pause, volume, and
  panning parity.
- Validation of day, night, pool, fog, roof, minigames, puzzle, survival, Zen
  Garden, credits, saves, achievements, and localization.
- Zombatar tracked independently until the reconstructed game implements it.

Exit gate:

- The content matrix passes on the supported macOS versions.
- Long-running and repeated transition tests show no resource or audio leaks.

### Phase 7 — Productization

Deliverables:

- First-launch importer for user-owned game data.
- Clean app bundle with no Windows DLLs, DRM, or proprietary assets.
- Release optimization, diagnostics, signing, notarization, and clean-machine
  tests.
- Reproducible local release artifacts for supported Apple-silicon macOS
  versions.

Exit gate:

- A signed build launches on a clean Mac, imports purchased assets, and passes
  the release parity suite.

## Validation matrix

Every completed phase updates the following evidence:

| Area | Required evidence |
| --- | --- |
| Dependency boundary | Automated source scan and target link graph |
| Data formats | Golden byte fixtures on Windows and macOS |
| Gameplay | Headless replay and state hashes |
| Rendering | Golden screenshots plus Metal frame validation |
| Audio | Transition/order fixtures and audible reference checks |
| Resources | Full manifest enumeration and malformed-input tests |
| Saves | Cross-platform load and round-trip fixtures |
| Performance | CPU/GPU frame captures on the baseline Apple-silicon Mac |

## Initial target

The first vertical slice is complete when the portable engine contracts and
state codec build on macOS, the real PAK can be enumerated, XML definitions can
load, a Metal window renders the title screen, one effect and one MO3 transition
play, and a Windows save fixture loads. Work proceeds in that dependency order.
