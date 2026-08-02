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
- an explicit 32-bit live plant count, which preserves stacked-plant cases;
- version 2 Level 1 economy fields: spendable sun, fixed-width packet recharge
  state, seed selection, tutorial phase, and the deterministic first falling-
  sun countdown/spawn gate.

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
  -referencefreshprofile=Codex ^
  -recordbehavior="C:\captures\legacy-behavior.pvzb"
```

The two capture switches are mutually exclusive for a process.

The reconstructed x64 target cannot load the 32-bit `bass.dll` distributed
with the retail Steam game. `-nosound` selects the framework's dummy music and
sound backends so the reference can run without incompatible audio middleware.
This affects audio output only; logical input and behavior capture remain
enabled.

`-referenceprofile=<name>` deterministically selects an existing local profile
or creates it before capture starts. `-referencefreshprofile=<name>` does the
same, then resets that named profile and deletes only its saved games before
the run. Use the fresh form for repeatable first-level scenarios so both the
legacy first-run profile dialog and persisted continue-game state stay outside
behavior evidence. Both reference-only switches accept one to twelve ASCII
letters, digits, or spaces and are mutually exclusive.

Behavior capture also maintains the legacy application's logical active-focus
state while recording. This lets scheduled or SSH-driven reference runs update
widgets exactly like a foreground play session instead of freezing the menu
when Windows leaves the capture window inactive. Once a level-intro board has
been instantiated, capture also satisfies the legacy first-draw readiness gate
that would otherwise depend on an available DirectDraw foreground surface.
Focus-loss, pause, and renderer-surface behavior are intentionally outside this
active-gameplay parity stream.

Raw input capture starts at the first actual legacy update. Behavior capture
uses the stable game-layer boundary described below. Both stop when the
application leaves its main loop. The file is serialized and published only
during normal shutdown. The final path and its `.tmp` sibling must not already
exist; the recorder refuses to overwrite evidence.

Generated `.pvzr`, `.pvzc`, and `.pvzb` files are ignored by Git. Retail assets
and captures stay local.

Behavior capture is armed by the command line but begins only after the legacy
runtime's title loading thread completes and the title becomes interactive.
This keeps asynchronous engine loading outside the comparison: tick zero in
both the Windows capture and the portable replay is a game-layer title update.
Raw `-recordreplay` capture still starts immediately when requested.

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

## Latest runtime evidence

On 2026-08-02, the isolated x64 Windows build and the macOS headless replay
both passed all eight tests. A fresh-profile Level 1 version 2 capture then
replayed 8,765 logical ticks with identical inputs and normalized observations:

- title to main menu at tick 2,510;
- Adventure intro at tick 6,162 and playable lawn at tick 7,017;
- Peashooter selection at tick 7,513 and placement at tick 7,564;
- sun changed from 150 to 50, packet recharge advanced from 1 through 750,
  and the deterministic first falling sun spawned at tick 7,963;
- `pvz_behavior_inspect` returned `behavior-captures-match`.

The local Windows evidence artifact was 560,993 bytes with SHA-256
`3c2fb029868d0c220209faf1e9f7f72e89fbc978e055432966c1cd2673bb27c3`.
The portable artifact had the same size; its producer byte differs by design.

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
cross-runtime behavior layer without pretending the object graphs are
equivalent. Version 1 covers lifecycle, focus, and occupancy; version 2 adds
the deterministic Level 1 economy slice. Future gameplay slices must extend
the format and independent exporters rather than adding legacy memory hashes.
Screenshot comparison remains a separate rendering gate because a behavior
match does not prove pixel parity.
