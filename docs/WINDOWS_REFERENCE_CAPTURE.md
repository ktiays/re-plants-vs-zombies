# Windows reference capture

## Scope

The reconstructed Win32 game can emit either the versioned `PVZR`
logical-input format or a `PVZB` capture containing the same input plus
normalized behavior observations. This is an evidence bridge, not a claim that
the legacy and portable object models share a native state layout.

The recorder observes the legacy `WidgetManager` after window coordinates have
been remapped into the game's 800 by 600 logical space. Recording at that
boundary also covers input delivered by the legacy demo player rather than only
live `WM_*` messages.

At the start of every actual legacy game update, the recorder captures:

- held and newly pressed portable keys;
- held and newly pressed primary, secondary, and middle pointer buttons;
- the latest logical pointer position;
- accumulated wheel delta;
- text delivered since the previous logical update.

Transient input is consumed after one recorded tick. Held state remains until a
matching release. Slow-motion iterations that perform no game update do not
produce a replay frame; fast-forward iterations produce one frame per actual
update.

In behavior mode, a normalized observation is recorded after that same update:

- scene and board stage;
- current valid grid focus, or the fixed `0xFF` no-focus sentinel;
- a 54-bit 9 by 6 occupancy mask built from live on-board plants;
- an explicit 32-bit live plant count, which preserves stacked-plant cases.

These values are converted field by field. Legacy pointers, `DataArray`
metadata, object padding, and native integer widths never enter the file.

## Windows build and capture

Configure the reconstructed target with both the portable engine and legacy
Windows target enabled:

```bat
cmake -S . -B build\windows ^
  -DPVZ_BUILD_PORTABLE_ENGINE=ON ^
  -DPVZ_BUILD_LEGACY_WINDOWS=ON ^
  -DPVZ_BUILD_MACOS_APP=OFF ^
  -DBUILD_TESTING=ON
cmake --build build\windows --config Release
```

Run the resulting reconstructed executable with an unused output path:

```bat
path\to\LawnProject.exe ^
  -nosound ^
  -recordreplay="C:\captures\legacy-input.pvzr"
```

Or record the preferred behavior-comparison artifact:

```bat
path\to\LawnProject.exe ^
  -nosound ^
  -recordbehavior="C:\captures\legacy-behavior.pvzb"
```

The two capture switches are mutually exclusive for a process.

The reconstructed x64 target cannot load the 32-bit `bass.dll` distributed
with the retail Steam game. `-nosound` selects the framework's dummy music and
sound backends so the reference can run without incompatible audio middleware.
This affects audio output only; logical input and behavior capture remain
enabled.

Raw input capture starts at the first actual legacy update. Behavior capture
uses the stable game-layer boundary described below. Both stop when the
application leaves its main loop. The file is serialized and published only
during normal shutdown. The final path and its `.tmp` sibling must not already
exist; the recorder refuses to overwrite evidence.

Generated `.pvzr`, `.pvzc`, and `.pvzb` files are ignored by Git. Retail assets
and captures stay local.

Behavior capture is armed by the command line but begins only after the legacy
runtime reaches its first stable title frame. This keeps asynchronous engine
loading outside the comparison: tick zero in both the Windows capture and the
portable replay is a game-layer title update. Raw `-recordreplay` capture still
starts immediately when requested.

## Validate and replay on macOS

Inspect the Windows stream:

```sh
./out/portable/engine/pvz_replay_inspect \
  /absolute/path/legacy-input.pvzr
```

Drive the portable game with those exact logical frames and write a state-hash
session:

```sh
./out/portable/game/pvz_game_headless \
  --replay /absolute/path/legacy-input.pvzr \
  --write-session /absolute/path/portable-result.pvzc
```

For a direct behavior comparison, use the nested input from the Windows
capture to produce the portable observation stream:

```sh
./out/portable/game/pvz_game_headless \
  --replay /absolute/path/legacy-behavior.pvzb \
  --write-behavior /absolute/path/portable-behavior.pvzb

./out/portable/parity/pvz_behavior_inspect \
  /absolute/path/legacy-behavior.pvzb \
  /absolute/path/portable-behavior.pvzb
```

The behavior inspector returns success only when the nested input frames and
all normalized observations match. On failure it reports the first input tick
or the first behavior tick and field.

The headless runner rejects malformed, oversized, non-100-Hz, or
non-sequential streams before running the game. `pvz_replay_inspect` can compare
two raw input streams, a raw stream with the input nested in a session, or two
complete sessions:

```sh
./out/portable/engine/pvz_replay_inspect \
  legacy-input.pvzr portable-result.pvzc
```

A raw-to-session comparison proves that the portable run consumed the recorded
input exactly. Two complete sessions additionally compare every portable state
hash and the rolling transcript hash.

## Evidence boundary

`PVZR` proves what logical input reached each legacy update. `PVZB` adds the
first cross-runtime behavior layer without pretending the object graphs are
equivalent. The initial schema is intentionally small; future gameplay slices
must extend it through a new format version and independent exporters rather
than adding legacy memory hashes. Screenshot comparison remains a separate
rendering gate because a behavior match does not prove pixel parity.
