# Deterministic input replay

## Purpose

The replay stream records the complete portable `IInputFrame` observed at each
100 Hz simulation tick. It is independent of SDL, Win32, Cocoa, Metal, and
Direct3D, so the same stream can drive the headless game or either platform
composition target.

Replays establish cross-platform determinism. They do not by themselves prove
parity with the legacy game; a reference replay must first be recorded from the
Windows adapter and then run against the portable game.

The reconstructed Windows adapter can record `PVZR` at the legacy
`WidgetManager`/logical-update boundary. Its `PVZB` mode nests the same input
and adds a deliberately narrow behavior observation after every update. The
legacy `LawnApp`/`Board` object graph is never serialized or hashed as though
it were the portable `GameModule` state.

## Version 1 binary format

All integers are fixed-width and little-endian. No native structure layout,
pointer, padding, `long`, or `size_t` value is persisted.

### Header

| Field | Type | Required value |
| --- | --- | --- |
| Magic | `uint32` | `0x525A5650` (`PVZR`) |
| Version | `uint16` | `1` |
| Simulation frequency | `uint32` | `100` |
| Frame count | `uint32` | At most 1,000,000 |

### Frame

| Field | Type | Meaning |
| --- | --- | --- |
| Tick | `uint64` | Zero-based and strictly sequential |
| Keys down | `uint16` | Bit per portable `KeyCode` |
| Keys pressed | `uint16` | Transient bit per portable `KeyCode` |
| Pointer buttons down | `uint8` | Bit per portable `PointerButton` |
| Pointer buttons pressed | `uint8` | Transient pointer bits |
| Pointer X | `int32` | 800 by 600 logical coordinate |
| Pointer Y | `int32` | 800 by 600 logical coordinate |
| Wheel delta | `int32` | Portable wheel delta |
| Text length | `uint16` | At most 4,096 code points |
| Text | repeated `uint32` | Valid Unicode scalar values |

Unknown mask bits, non-sequential ticks, invalid Unicode, unsupported versions,
wrong tick frequencies, oversized counts, truncation, and trailing data are
rejected. Loading is transactional: a failed stream does not replace the
previous replay.

`InputReplay::AppendFrame(tick, input, error)` captures any engine-facing input
implementation directly. Platform-native event objects never enter the stream.

## State and transcript hashes

After each replay frame, the verifier serializes `GameModule` through its
fixed-width state contract:

- The state hash is 64-bit FNV-1a over the final serialized state.
- The transcript hash starts at the standard FNV-1a offset and incrementally
  hashes every tick state in order.

The transcript detects a temporary divergence even if two runs later converge
to the same final state. Hashes are comparison evidence, not save data or
security primitives.

## Current representative replay

The headless executable runs a 1,310-frame fixture:

1. Activate the title screen.
2. Start Adventure from the main menu.
3. Advance the 450-tick selector transition and 855-tick Level 1 intro.
4. Select the Peashooter packet using logical pointer input.
5. Plant in column 3, row 2, spending 100 of the initial 150 sun.
6. Move the grid focus left to column 2 while packet recharge advances.

The executable validates the final scene, tick count, selection, occupied-cell
bits, sun, packet recharge state, final state hash, and full transcript hash.
Any mismatch returns a
non-zero status, making the existing headless CTest entry a deterministic local
gate.

Run it directly:

```sh
./out/portable/game/pvz_game_headless
```

Run a Windows-recorded input stream through the portable game:

```sh
./out/portable/game/pvz_game_headless \
  --replay /absolute/path/windows-input.pvzr \
  --write-session /absolute/path/portable-result.pvzc
```

The headless runner also accepts a `PVZB` behavior capture as replay input and
uses its nested `PVZR` stream:

```sh
./out/portable/game/pvz_game_headless \
  --replay /absolute/path/windows-behavior.pvzb \
  --write-behavior /absolute/path/portable-behavior.pvzb
```

Write the verified headless session to a local capture:

```sh
./out/portable/game/pvz_game_headless \
  --write-session /absolute/path/headless.pvzc
```

## Version 1 session capture

A `.pvzc` session combines the input replay with the state hash produced after
every update:

| Field | Type | Meaning |
| --- | --- | --- |
| Magic | `uint32` | `0x435A5650` (`PVZC`) |
| Version | `uint16` | `1` |
| Simulation frequency | `uint32` | `100` |
| Replay byte count | `uint32` | At most 256 MiB |
| Replay bytes | byte array | Complete versioned `PVZR` stream |
| State hash count | `uint32` | Must equal the replay frame count |
| Transcript hash | `uint64` | Rolling hash over serialized tick states |
| State hashes | repeated `uint64` | Final serialized-state hash per tick |

The macOS composition target enables capture only when explicitly requested.
It wraps the selected portable game at the `IGame` boundary, records the exact
logical input passed by `ApplicationRunner`, and saves state immediately after
each update. SDL events and Metal objects never enter the capture.

Record a macOS session:

```sh
PVZ_RECORD_SESSION_PATH=/absolute/path/mac-session.pvzc \
  ./script/build_and_run.sh
```

The capture is written atomically when the application exits normally.
Generated `.pvzr`, `.pvzc`, and `.pvzb` files are ignored by Git.

Inspect one capture:

```sh
./build/macos/engine/pvz_replay_inspect mac-session.pvzc
```

The inspector also accepts input-only `PVZR` captures:

```sh
./out/portable/engine/pvz_replay_inspect windows-input.pvzr
```

Compare two captures:

```sh
./build/macos/engine/pvz_replay_inspect \
  mac-session.pvzc windows-session.pvzc
```

The comparator reports the first differing input tick and first differing
state-hash tick. A comparison returns success only when input frames, all state
hashes, and the transcript hash match when both inputs are complete sessions.
For a raw-to-raw or raw-to-session comparison, it compares the input frames and
reports `inputs-match`; state comparison is unavailable until both operands
contain state hashes.

## Versioned behavior capture

A `.pvzb` capture combines the complete input replay with one normalized
post-update observation per frame:

| Field | Type | Meaning |
| --- | --- | --- |
| Magic | `uint32` | `0x425A5650` (`PVZB`) |
| Version | `uint16` | `1`, `2`, or `3`; new captures write `3` |
| Simulation frequency | `uint32` | `100` |
| Producer | `uint8` | Unknown, portable game, or legacy Windows |
| Replay byte count | `uint32` | At most 256 MiB |
| Replay bytes | byte array | Complete versioned `PVZR` stream |
| Observation count | `uint32` | Must equal replay frame count |

Version 1 uses a 24-byte observation:

| Field | Type | Meaning |
| --- | --- | --- |
| Tick | `uint64` | Zero-based and strictly sequential |
| Scene | `uint8` | Normalized lifecycle/game scene |
| Board stage | `uint8` | None, day, night, pool, fog, roof, boss, or other |
| Grid column | `uint8` | `0` through `8`, or `0xFF` with no focus |
| Grid row | `uint8` | `0` through `5`, or `0xFF` with no focus |
| Occupied cells | `uint64` | Low 54 bits represent the 9 by 6 board |
| Plant count | `uint32` | Live board plants; may exceed occupied bits for stacked plants |

Version 2 appends twelve fixed-width bytes, for a 36-byte observation:

| Field | Type | Meaning |
| --- | --- | --- |
| Sun | `uint16` | Current Level 1 spendable sun |
| Seed refresh counter | `uint16` | Current Peashooter packet recharge progress, or zero while idle |
| Seed refresh time | `uint16` | Peashooter packet recharge duration, or zero while idle |
| Seed refreshing | `uint8` boolean | Whether the Peashooter packet is recharging |
| Seed selection | `uint8` enum | None, Peashooter, or another legacy seed |
| Tutorial phase | `uint8` enum | Normalized Level 1 pick-up, placement, refresh, or completion phase |
| First-sun countdown | `uint16` | Deterministic tutorial countdown before the first falling sun; zero afterward |
| First sun spawned | `uint8` boolean | Whether the deterministic first falling-sun gate has fired |

Version 3 appends a game-semantic random-decision tape after all observations:

| Field | Type | Meaning |
| --- | --- | --- |
| Decision count | `uint32` | At most 100,000 semantic gameplay decisions |
| Decisions | repeated 15-byte records | Ordered falling-sun and normal-zombie choices |

Each decision record is fieldwise encoded:

| Field | Type | Meaning |
| --- | --- | --- |
| Kind | `uint8` enum | Falling sun or normal zombie |
| Next countdown | `uint16` | Next sky-sun delay; zero for a zombie |
| X | `int32` | Spawn position in milli-pixels |
| Ground Y | `int32` | Falling-sun destination in milli-pixels; zero for a zombie |
| Speed | `uint32` | Zombie speed in micro-pixels per tick; zero for a sun |

The Windows exporter records values after the legacy game has made each
choice. It does not record the global PRNG state: rendering, loading, or other
legacy systems may consume that stream independently. The portable game reads
the semantic tape strictly in event order and fails on exhaustion, a kind
mismatch, an invalid range, or an unused decision. Zombie movement carries the
captured micro-pixel remainder in fixed-width state so sub-milli speed is not
rounded away over long runs.

Version 1 and 2 captures remain readable and run with the portable deterministic
fallback choices because they contain no decision tape.

The producer is provenance only and is not compared. When versions differ,
the inspector compares the common schema prefix. When both captures are
version 3, it also compares every semantic decision. The Windows exporter
converts legacy enums and live `DataArray<Plant>` objects field by field; it
does not persist array metadata, pointers, padding, or native object memory.
The portable exporter derives the same schema from `GameModule`.

Compare captures:

```sh
./out/portable/parity/pvz_behavior_inspect \
  windows-behavior.pvzb portable-behavior.pvzb
```

The inspector first verifies the nested inputs, then reports the first
different observation field, for example
`behavior-mismatch-tick=63 field=grid-column left=3 right=2`.
