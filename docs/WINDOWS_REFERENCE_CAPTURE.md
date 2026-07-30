# Windows reference input capture

## Scope

The reconstructed Win32 game can emit the same versioned `PVZR` logical-input
format consumed by the portable game. This is an input-evidence bridge, not a
claim that the legacy and portable object models already share a state schema.

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
  -recordreplay="C:\captures\legacy-input.pvzr"
```

Capture starts at the first actual legacy update and stops when the application
leaves its main loop. The file is serialized and published only during normal
shutdown. The final path and its `.tmp` sibling must not already exist; the
recorder refuses to overwrite evidence.

Generated `.pvzr` and `.pvzc` files are ignored by Git. Retail assets and
captures stay local.

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

`PVZR` proves what logical input reached each legacy update. Replaying it
through `GameModule` establishes deterministic portable behavior for that
input. It does not compare the legacy game's internal `LawnApp`/`Board` state
with the portable state because those representations are not yet equivalent.

The next behavior-parity layer must define a narrow fixed-width observation
schema—scene, board geometry, selected grid cell, entity summaries, and other
slice-specific values—and implement independent legacy and portable exporters.
Raw legacy object memory and native structure hashes are explicitly unsuitable
for that comparison.
