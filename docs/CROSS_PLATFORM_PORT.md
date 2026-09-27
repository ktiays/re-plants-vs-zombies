# Cross-platform port of the reconstructed game

The reconstructed game (`Lawn/`, `Sexy.TodLib/`, `LawnApp.*`) and its
SexyAppFramework now build without Win32, DirectDraw, Direct3D, DirectSound or
BASS. Everything the game needs from the operating system goes through a small
set of platform-neutral interfaces, each with one implementation per platform.
macOS is the first supported platform.

## Layers

```text
Lawn/, Sexy.TodLib/, LawnApp        game code (unchanged apart from portability fixes)
            │
SexyAppFramework                     widgets, images, fonts, resources, app loop
  ├─ graphics/HardwareRenderer       accelerated drawing (was D3DInterface)
  ├─ graphics/DisplayInterface       screen image + presentation (was DDInterface)
  ├─ graphics/DeviceImage            images that can be drawn by either renderer (was DDImage)
  ├─ sound/AudioMixer                software mixer (sound effects + music sources)
  ├─ sound/MixerSoundManager         SoundManager on the mixer (was DSoundManager)
  ├─ sound/ModuleMusicInterface      MusicInterface: libopenmpt modules + Ogg streams (was BASS)
  ├─ misc/RegistryStore              per-user settings file (was the Windows registry)
  └─ paklib/PakInterface             main.pak reader (portable file I/O)
            │
Platform interfaces
  ├─ graphics/RenderDevice.h         GPU backend: textures, draw batches, present
  ├─ sound/AudioMixer.h (AudioDevice) audio output
  └─ platform/Platform.h             paths, message boxes, URLs, clipboard, time
            │
Backends
  ├─ graphics/metal/MetalRenderDevice.mm   macOS / Metal
  ├─ platform/sdl/SDLAudioDevice.cpp        SDL3 audio stream
  └─ platform/sdl/SDLPlatform.cpp           SDL3 platform services
```

SDL3 also provides the window, input events, and timing used by
`SexyAppBase.cpp`. Game code never sees SDL or Metal types.

## Rendering

The framework keeps both of its original renderers:

- **Software** (`3D acceleration` off): the screen is a `MemoryImage` drawn by
  the framework's own rasterizers. Each presented frame uploads it as a
  texture.
- **Hardware** (default): draws to the screen image are forwarded to
  `HardwareRenderer`, which reproduces the vertex generation of the original
  Direct3D 7 renderer (including its half-pixel offsets) and hands batches of
  triangles to the `RenderDevice`.

Either way the backend renders into a persistent 800×600 scene target (the
widget system only repaints dirty regions) and `Present` scales it into the
window with aspect-correct letterboxing: nearest filtering at integer scales,
linear otherwise. Coordinates in `RenderVertex` use the Direct3D 7 pixel-center
convention; backends apply the half-pixel shift.

Differences from the Windows renderer:

- Textures are always 32-bit and never split into power-of-two tiles. The
  Direct3D renderer stored opaque images as R5G6B5, so its gradients banded
  slightly.
- Clipping of transformed blits uses scissor rectangles instead of CPU polygon
  clipping; the covered pixels are the same.

### Adding a backend

Implement `Sexy::RenderDevice` (see the comments in `RenderDevice.h`), create
it in `SexyAppBase::MakeWindow` for the new platform, and select its sources
in `SexyAppFramework/CMakeLists.txt`. `HardwareRenderer` batches by render
state, so a backend only needs one pipeline per blend mode, point/linear and
clamp/wrap samplers, scissor support, triangle and line lists, and a
blit-to-drawable pass.

## Audio

`AudioMixer` mixes 16-bit PCM voices and music sources into float stereo at
44.1 kHz; the platform `AudioDevice` pulls from it on its own thread. Sound
effects keep DirectSound's volume curve and pan law. Music uses libopenmpt for
the MO3 modules and exposes the per-channel volume, tempo, speed, and order
controls the game's layered soundtrack previously obtained by calling BASS
directly (`MusicInterface::SetModuleChannelVolume` and friends).

## Files, settings, and saves

- Game data is found at startup in `$PVZ_GAME_DIR`, the working directory, the
  executable's directory (`Contents/Resources` in a bundle), or a directory
  above it. Paths inside the game still use backslashes; `Sexy::FixPath`
  converts them for the host file system.
- Profiles and saves live in the per-user data directory
  (`~/Library/Application Support/PopCap/PlantsVsZombies/` on macOS).
- Registry values are stored in `registry.ini` in the same directory.
- Packaged `compiled/*.compiled` definition caches contain raw 32-bit
  structures and are only used by 32-bit builds; other builds parse the XML
  definitions.

## 64-bit correctness

The original target was 32-bit Windows, so several widths were pinned:

- `ulong` (pixels, colors) is `uint32_t`; `DWORD`, `WORD`, `BYTE`, `LONG` keep
  their Windows widths (`platform/PlatformTypes.h`).
- The Mersenne Twister state, `Buffer` CRC, `Buffer::ReadLong`, and software
  rasterizer colors use 32-bit arithmetic.
- `__time64_t` values stored in profiles stay 64-bit.

## Developer options

Command-line switches handled by `SexyAppBase`:

| Switch | Effect |
| --- | --- |
| `-changedir=<dir>` | Game data directory |
| `-windowed`, `-fullscreen` | Override the saved screen mode |
| `-software` | Use the software renderer |
| `-nosound` | Disable audio |
| `-inputscript=<file>` | Drive the game from a script (see `SexyAppBase::LoadInputScript`) |
| `-capturedir=<dir>` `-captureinterval=<ms>` | Periodically write the screen to PNG files |
| `-tod` | Debug builds: enable the developers' cheat keys |

Environment variables: `PVZ_GAME_DIR` (data directory), `SEXY_NONINTERACTIVE`
(message boxes print to stderr and return the default answer; for automated
runs), `PVZ_STRICT_ASSERTS` (debug builds: abort on framework assertions
instead of logging them).

## Known limitations

- Only the macOS (Metal) rendering backend exists. Windows and Linux need a
  `RenderDevice` implementation; the rest of the code base is portable C++ on
  SDL3.
- The app bundle links Homebrew's SDL3, libpng, libjpeg-turbo, and libopenmpt
  dynamically, so it runs on machines with those libraries installed.
- Online features of the original (update checks, registration, crash
  reporting, the DRM wrapper) are not implemented.
- Demo recording and playback (`-record`, `-play`) were removed.
