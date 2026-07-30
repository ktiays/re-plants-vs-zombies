# Deterministic input replay

## Purpose

The replay stream records the complete portable `IInputFrame` observed at each
100 Hz simulation tick. It is independent of SDL, Win32, Cocoa, Metal, and
Direct3D, so the same stream can drive the headless game or either platform
composition target.

Replays establish cross-platform determinism. They do not by themselves prove
parity with the legacy game; a reference replay must first be recorded from the
Windows adapter and then run against the portable game.

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

The headless executable runs a 65-frame fixture:

1. Activate the title screen.
2. Start Adventure from the main menu.
3. Advance the fixed 60-tick transition.
4. Place in column 3, row 4 using logical pointer input.
5. Move left to column 2.
6. Place again using the keyboard.

The executable validates the final scene, tick count, selection, occupied-cell
bits, final state hash, and full transcript hash. Any mismatch returns a
non-zero status, making the existing headless CTest entry a deterministic local
gate.

Run it directly:

```sh
./out/portable/game/pvz_game_headless
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
Generated `.pvzc` files are ignored by Git.

Inspect one capture:

```sh
./build/macos/engine/pvz_replay_inspect mac-session.pvzc
```

Compare two captures:

```sh
./build/macos/engine/pvz_replay_inspect \
  mac-session.pvzc windows-session.pvzc
```

The comparator reports the first differing input tick and first differing
state-hash tick. A comparison returns success only when input frames, all state
hashes, and the transcript hash match.
