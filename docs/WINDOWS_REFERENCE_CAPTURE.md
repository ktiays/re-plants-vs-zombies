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

Version 3 additionally records ordered, game-semantic Level 1 random choices:

- falling-sun next countdown, spawn X, and ground Y;
- normal-zombie spawn X and speed in micro-pixels per tick.

The adapter observes the chosen gameplay values; it does not expose or copy
the legacy global PRNG state.

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

For a version 3 input, the headless runner injects the captured decisions
through the portable game interface and rejects exhausted, wrong-kind, invalid,
or unused tape entries. The behavior inspector returns success only when the
nested input frames, all normalized observations, and both version 3 decision
tapes match. On failure it reports the first input tick, behavior tick and
field, or decision index.

## Latest runtime evidence

On 2026-08-02, an isolated x64 Windows build produced a fresh-profile Level 1
version 3 capture. The portable headless game replayed all 9,169 logical ticks
with identical inputs, normalized observations, and six semantic decisions:

- playable lawn at tick 5,778, first packet selection at 6,275, and first
  Peashooter placement at 6,325;
- five falling-sun decisions with their actual countdown, X, and ground-Y
  choices, plus one normal-zombie spawn at 793 pixels with a captured speed of
  293,494 micro-pixels per tick;
- sun credit at ticks 6,836, 7,541, and 8,523, including the legacy collection
  flight and the tutorial transition caused by sun already in flight;
- second packet selection at tick 8,716 and second placement at 8,767;
- `pvz_behavior_inspect` returned `behavior-captures-match`.

This replay exposed and fixed two portable defects before the gate passed:
cursor-preview focus was not following pointer movement without a click, and
sun was credited immediately instead of after its flight to the counter.

The Windows evidence artifact is 586,943 bytes with SHA-256
`a202846a60db27bc39b7ea2755aea8ba8a493c2af5a86cd1e7db28a5e105e1ae`.
The portable artifact has the same size and SHA-256
`08860a6be91b9ad8212dabc8bd4049f2f82406405b20dec023a4c72fff3b6098`;
the producer byte differs by design.

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
the Level 1 economy slice; version 3 adds ordered gameplay decisions without
coupling to global PRNG consumption. Future gameplay slices must extend the
format and independent exporters rather than adding legacy memory hashes.
Screenshot comparison remains a separate rendering gate because a behavior
match does not prove pixel parity.
