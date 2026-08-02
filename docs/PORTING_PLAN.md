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
the boundary is extracted. The native macOS application initially composes an
SDL platform backend, a Metal renderer, and a callback-driven SDL audio
backend. Encoded formats remain isolated behind decoder interfaces so the
backend can be replaced without changing game code.

## Current execution status

Last updated: 2026-08-02

- [x] Portable engine API and core CMake targets
- [x] Shared 100 Hz application runner with bounded catch-up, suspension,
  transient-input consumption, and render-device lifecycle tests
- [x] Automated platform-leakage and architecture-dependent-type checks
- [x] Explicit little-endian state reader/writer with malformed-input tests
- [x] Portable game lifecycle target and headless 100 Hz smoke runner
- [x] Versioned fixed-width input replay: sequential 100 Hz frames capture
  persistent and transient keyboard/pointer state, logical coordinates, wheel,
  and bounded Unicode text; malformed streams fail transactionally
- [x] Deterministic headless Adventure replay: 1,310 ticks cover title, menu,
  selector transition, first-level intro, seed selection, legal pointer
  placement, sun spending, and packet recharge while final-state and rolling
  per-tick transcript hashes provide a cross-platform comparison gate
- [x] Engine-neutral runtime recording decorator and versioned `.pvzc` session
  files combine exact logical-tick input, per-tick state hashes, and transcript
  hashes; malformed sessions fail transactionally
- [x] Opt-in macOS session recording writes captures atomically after shutdown,
  and the portable inspector reports the first input or state divergence
  between macOS and headless captures
- [x] Opt-in reconstructed-Windows logical-input capture records remapped
  `WidgetManager` input at each actual legacy update into non-overwriting
  `PVZR` files; the headless runner consumes those streams and the inspector
  compares raw input with portable sessions
- [x] Versioned fixed-width `PVZB` behavior captures combine exact logical
  input with post-update normalized scene, board stage, grid focus, occupied
  cells, plant count, and Level 1 economy state; version 4 adds complete-level
  wave, zombie, outcome, mower, and award observations, and version 5 adds
  fixed-width wave-health/projectile diagnostics and Peashooter launch
  scheduling to the strict game-semantic tape for falling-sun, normal-zombie,
  wave-schedule, Peashooter-schedule, and animation-derived projectile-spawn
  choices, plus per-live-slot semantic zombie motion; version 6 adds exact
  synchronized projectile motion without native floats, and version 7 adds
  stable fixed-width per-slot sun trajectory and collection observations;
  independent legacy and portable exporters plus the behavior inspector report
  the first differing tick, field, slot, or decision while versions 1 through 6
  remain readable
- [x] Engine-owned rendering, input, logging, state, and resource protocols
- [x] Portable PAK indexing, normalization, validation, and resource reads
- [x] Validation against the supplied retail PAK: 3,198 entries and
  45,309,674 payload bytes
- [x] Portable XML documents loaded through the resource protocol, including
  the multi-root fragments used by particle definitions
- [x] Typed, owned runtime mappings for reanimation, particle, parameter-track,
  and trail definitions with legacy-compatible defaults
- [x] Engine-owned XML loading protocol used by the game definition loader;
  production game sources cannot include engine-core headers
- [x] Runtime definition mapping validated against all 146 reanimations,
  112 particle systems, and the trail definition in the supplied retail PAK
- [x] Source parsing validated against all 261 XML and reanimation definitions
  in the supplied retail PAK: 971,910 nodes
- [x] Portable PNG, JPEG, and GIF decoding to straight `BGRA8Unorm`,
  validated against all 2,492 retail images: 56,362,124 pixels
- [x] Portable `resources.xml` image mapping: extensionless path resolution,
  fixed-width atlas metadata, shared generational handles, automatic companion
  alpha images, explicit alpha images/grids, and alpha-only composition
- [x] Portable bitmap-font resources and text layout: fixed-width generational
  handles and metrics, explicit 32-bit characters, descriptor-relative atlases,
  multi-layer outlines, per-glyph offsets, color multiplication, kerning, and
  conversion to ordinary engine sprite draws
- [x] Bitmap-font parsing and atlas bounds validated against all 20 retail font
  resources: 23 layers, 3,563 glyphs, and 323 kerning pairs
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
- [x] SDL macOS application foundation: resizable high-DPI Metal window,
  event/input translation, monotonic timing, and logical pointer coordinates
- [x] Metal presentation foundation: `CAMetalLayer`, `BGRA8Unorm`, one command
  buffer per frame, drawable-unavailable handling, and clear/present validation
- [x] Metal sprite foundation: fixed 800×600 target, aspect-fit presentation,
  generational private textures, staged updates, a three-frame vertex ring,
  state batching, scissoring, rectangle and portable affine-quad geometry,
  transforms, mirroring, both legacy blend modes, and point/linear
  clamp/repeat sampling
- [x] First retail visual path: the portable game resolves and renders
  `IMAGE_TITLESCREEN` and alpha-composited `IMAGE_PVZ_LOGO` through engine
  protocols when a user-owned PAK is mounted, then lays out title and
  interaction labels through `IFontResources` for the same Metal sprite path
- [x] Portable input-driven game flow: title activation, keyboard/pointer menu
  selection, unavailable-mode feedback, a fixed-duration Adventure transition,
  and an interactive 9-by-5 daytime lawn scaffold use only engine protocols
- [x] First deterministic Level 1 gameplay rules: the portable game owns the
  source-audited 150-sun start, 100-sun Peashooter, 750-tick packet recharge,
  center-row placement rule, non-destructive occupied-cell rejection, and
  keyboard/pointer seed selection behind engine-neutral input
- [x] Deterministic complete Level 1 combat: the portable game
  owns the two-plant tutorial gate, falling 25-sun pickups, 99-tick first-wave
  countdown, all four 1/1/1/2 zombie waves, source-compatible next-wave health
  acceleration, normal-zombie and Peashooter state, fixed-point movement, lane
  targeting, pea collision/damage, four-tick eating cadence, lawn-mower rescue,
  loss, win, and final award; a version 6 source fixture covers the complete
  scenario, while a 15,701-tick Windows runtime trace validates all combat
  observations through the award and remains byte-identical across AppleClang
  and MSVC after portable replay
- [x] Local reference-first parity harness: independent, revisioned legacy
  board fixtures exhaustively cover day, pool, and roof geometry, pointer
  mapping, background cropping, and engine-neutral selection render commands
- [x] Retail menu and lawn rendering through Metal: static selector button
  layers, `IMAGE_BACKGROUND1`, `IMAGE_SEEDBANK`, animated Peashooters, fallback
  placement markers, and selection outlines are expressed solely as ordinary
  engine sprite draws
- [x] Portable retail reanimation playback: source definitions and referenced
  images load through engine protocols; named-layer bounds, 100 Hz looping,
  transform interpolation, disappearing-frame truncation, atlas-cell
  selection, alpha, and independent x/y skew produce backend-neutral affine
  sprite quads consumed directly by Metal
- [x] Version-8 portable game state persists scene, menu, transition, notice,
  grid selection, the 45-cell occupancy bitset, fixed-width reanimation tick,
  logical pointer-hover history, sun, packet recharge, seed selection,
  collection-flight state, micro-pixel zombie movement remainder, and bounded
  combat entities, wave scheduling, mower, outcome, and award state through
  explicit fields; versions 1 through 7 remain readable
- [x] Fixed-width audio firewall: decoded PCM descriptors, sound and voice
  handles, playback parameters, resource diagnostics, decoder/device
  protocols, and the game-facing sound service expose no backend or
  architecture-dependent integer types
- [x] PAK-backed OGG sound effects through the bundled Tremor decoder, confined
  behind a fixed-width adapter and validated against all 167 shipped OGG files:
  88 mono, 79 stereo, and 428,871,145 microseconds of decoded audio
- [x] Portable sound-manifest mapping, extensionless path resolution,
  generational caching, reference counting, and headless behavior; the retail
  manifest declares 168 sounds, of which 166 are loadable, while
  `SOUND_DIAMOND` and `SOUND_TAPGLASS` reference files absent from the PAK
- [x] Native macOS sound-effect playback: SDL float-stereo output, load-time
  resampling, a bounded 32-voice callback mixer, volume, panning, pitch,
  looping, master volume, and stale-handle rejection
- [x] First retail audio path: the portable game loads and plays
  `SOUND_LOADINGBAR_FLOWER` solely through `ISoundResources`
- [x] Fixed-width module-music firewall: generational handles, descriptors,
  playback state, explicit 32-bit order/row positions, resource diagnostics,
  device controls, and the game-facing music service expose no libopenmpt,
  BASS, packed Win32 position, or architecture-dependent integer types
- [x] PAK-backed MO3 playback through BSD-licensed libopenmpt: memory-backed
  loading, order/row seeking, indefinite looping, per-channel muting, module
  volume, tempo factor, pause/resume, and SDL float-stereo callback mixing
- [x] Both retail modules validated in full: `mainmusic.mo3` and
  `mainmusic_hihats.mo3` each expose 30 channels and 236 orders, accept
  interactive channel controls, seek correctly, and render finite non-silent
  48 kHz stereo samples
- [x] First retail music transition: the portable game starts
  `mainmusic.mo3` at the legacy title-theme order `0x98`, stops it during the
  Adventure transition, starts the daytime music at order zero, and restores
  the title music when returning to the menu solely through `IMusicResources`
- [ ] Complete Windows runtime behavior and screenshot baselines for the
  reconstructed legacy target (the complete Level 1 v6 playing/combat baseline
  now matches; a native v7 per-slot sun capture, asynchronous startup timing,
  and screenshot coverage remain)
- [ ] Migration of the remaining gameplay `Board`, `Challenge`, data-array, and
  effect snapshots from raw object blocks to fieldwise fixed-width schemas
- [x] Mapping portable XML tokens into runtime definitions used by effects
- [ ] Existing Windows backend adapters
- [ ] Complete SDL macOS platform backend (fullscreen, cursor, and lifecycle
  parity remain)
- [ ] Complete Metal renderer (untextured geometry, pool paths, Direct3D parity
  tuning, and golden-image validation remain; bitmap text now uses the shared
  sprite path)
- [ ] Complete reanimation parity (attachments, base-pose matrices, track
  overrides and groups, transition blending, text/fullscreen tracks, atlasing,
  and filter overlays remain)
- [x] Native sound-effect and MO3 module-music audio backend
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
                ├── pvz_audio_codecs
                ├── pvz_platform_sdl
                └── pvz_renderer_metal

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

- Retain the open-source SDL/Tremor effect path and add an approved MO3 module
  decoder, or isolate BASS behind the same fixed-width protocols.
- PAK-backed OGG effects and MO3 music.
- Pattern/order jumps, tempo, layered-track muting, focus, pause, volume, and
  panning parity.
- Validation of day, night, pool, fog, roof, minigames, puzzle, survival, Zen
  Garden, credits, saves, achievements, and localization.
- Zombatar tracked independently until the reconstructed game implements it.

Exit gate:

- The content matrix passes on the supported macOS versions.
- Long-running and repeated transition tests show no resource or audio leaks.

Current partial result:

- OGG effects are decoded through a fixed-width Tremor adapter and mixed by
  SDL.
- MO3 music remains a distinct module protocol rather than an ordinary sound
  effect. libopenmpt loads modules directly from PAK memory and supplies the
  order/row, channel-mute, loop, volume, and tempo controls required by the
  legacy music policy.
- The title transition is integrated. Gameplay-specific tune selection,
  synchronized main/drum/hihat instances, burst fades, and the complete content
  matrix remain.

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
| Gameplay | Independent legacy fixtures, differential tests, headless replay, and state hashes |
| Rendering | Engine-neutral command snapshots, golden screenshots, and Metal frame validation |
| Audio | Transition/order fixtures and audible reference checks |
| Resources | Full manifest enumeration and malformed-input tests |
| Saves | Cross-platform load and round-trip fixtures |
| Performance | CPU/GPU frame captures on the baseline Apple-silicon Mac |

## Initial target

The first vertical slice is complete when the portable engine contracts and
state codec build on macOS, the real PAK can be enumerated, XML definitions can
load, a Metal window renders the title screen, one effect and one MO3 transition
play, and a Windows save fixture loads. The title effect and MO3 transition are
now complete; a verified Windows save fixture remains.

## macOS developer entrypoint

The current native slice requires CMake, Ninja, Xcode's macOS SDK, SDL2,
libpng, libjpeg-turbo, giflib, and libopenmpt. Image and effect codecs are
isolated behind `IImageDecoder` and `IAudioDecoder`; OGG decoding uses the
repository's BSD-licensed Tremor source and adds no package dependency. MO3
decoding is isolated behind `IModuleMusicDevice` and uses BSD-licensed
libopenmpt. Configure portable headless-only builds with
`PVZ_BUILD_IMAGE_CODECS=OFF`, `PVZ_BUILD_AUDIO_CODECS=OFF`, and
`PVZ_BUILD_MODULE_MUSIC=OFF` when those decoders are not needed.
Build and launch the app through the project-local entrypoint:

```sh
./script/build_and_run.sh
```

Set `PVZ_PAK_PATH` to make the app mount a user-owned retail archive during
startup:

```sh
PVZ_PAK_PATH=/path/to/main.pak ./script/build_and_run.sh
```

With a retail PAK mounted, the portable game resolves the title background and
logo through `IImageResources`, loads `FONT_BRIANNETOD16` through
`IFontResources`, and submits both images and generated glyph sprites through
the renderer protocol. The same game module advances to a selector menu using
the shipped static button layers and then to the daytime lawn using
`IMAGE_BACKGROUND1`, `IMAGE_SEEDBANK`, and `IMAGE_SEEDPACKET_LARGER`. Occupied
cells render the shipped
`reanim\PeaShooterSingle.reanim` `anim_full_idle` layer. The portable player
turns the legacy transform model, including independent x/y skew, into ordinary
engine affine sprite quads; only the macOS renderer knows that Metal consumes
those draws. Selection and fallback placement overlays use an engine-created
one-pixel texture. Font layout uses `char32_t` text and fixed-width metrics; it
does not expose host `wchar_t` or native font APIs. The default headless build
remains independent of the codec libraries and uses null image-, font-, sound-,
and music-resource implementations.

The current controls are Enter, Space, or primary click on the title; arrow
keys or pointer selection in the menu; and seed-packet selection followed by
pointer or arrow-key plus Enter/Space placement on the lawn. Escape returns
from the lawn to the menu. Adventure is the only enabled mode in this slice.
Level 1 enforces the source-audited initial sun, Peashooter cost and packet
recharge, center-row restriction, occupied-cell rejection, two-plant tutorial,
collectible sky suns, all four 1/1/1/2 zombie waves, randomized wave and firing
schedules, Peashooter targeting, peas, damage, eating, mower/loss behavior, and
the final award. A fixed-width v7 behavior contract keeps legacy random choices,
animation-derived zombie/projectile motion, and per-slot sun trajectories
behind the reference adapter. The 15,701-tick native Windows run reaches the
award at tick 13,796; AppleClang and MSVC consume all 13,029 decisions and
reproduce every combat observation with byte-identical v7 portable outputs.
Normalized playing behavior now matches, including the previously missed
fractional-boundary sun collection; the remaining behavior drift is isolated
to asynchronous startup timing, while native v7 sun-trajectory and screenshot
coverage are still pending.

The same startup path resolves `SOUND_LOADINGBAR_FLOWER` through
`ISoundResources`, decodes it through `IAudioDecoder`, uploads fixed-width
signed 16-bit PCM through `IAudioDevice`, and queues it in the SDL callback
mixer. Neither the game module nor the public engine API includes SDL or
Tremor types.

The title startup path separately loads `sounds/mainmusic.mo3` through
`IMusicResources` and starts it at the legacy title-theme order `0x98`.
`MusicPosition` carries order and row as independent `std::uint32_t` fields;
the packed BASS/Win32 position and its host-dependent `unsigned long` never
cross the portable boundary. The macOS backend streams 48 kHz float stereo
from libopenmpt into the same bounded SDL callback mix used by sound effects.

The PAK inspection tool can validate all shipped bitmap-font descriptors and
their decoded atlases without extracting them:

```sh
./build/macos/engine/pvz_pak_inspect \
  /path/to/main.pak --validate-fonts
```

It can also audit every sound declaration and decode every shipped OGG file:

```sh
./build/macos/engine/pvz_pak_inspect \
  /path/to/main.pak --validate-sounds
```

It can validate the MO3 decoder and the interactive controls needed by legacy
tune transitions:

```sh
./build/macos/engine/pvz_pak_inspect \
  /path/to/main.pak --validate-music
```

The procedural renderer validation scene is selected independently of the
portable game and does not require retail data:

```sh
PVZ_RENDERER_SMOKE=1 ./script/build_and_run.sh --verify
```

The Run action in Codex uses the same script. `--verify`, `--debug`, `--logs`,
and `--telemetry` are available for local runtime validation.
