# Legacy parity validation

## Purpose

Portable code must be checked against evidence from the reconstructed Windows
game, not only against tests written from the portable implementation. A test
that obtains both its input and expected result from the same migrated helper
can confirm internal consistency while preserving a migration error.

The parity system therefore keeps the reference and implementation independent.
It is a local development gate; this repository does not install or require a
hosted CI workflow.

## Evidence layers

Each migrated gameplay or presentation slice should provide the applicable
layers below before it is marked complete:

1. **Reference fixture** — fixed-width values captured from, or audited against,
   a recorded Windows-reference revision.
2. **Differential behavior test** — portable output is compared with the
   independent fixture for representative values, boundaries, and every member
   of small finite domains.
3. **Render-command snapshot** — a deterministic engine-neutral frame records
   resource selections, source rectangles, destinations, colors, blend state,
   and ordering before a rendering backend is involved.
4. **Golden image** — Windows-reference and native output are captured at the
   same logical tick and input state, normalized to 800 by 600, and compared
   with a small documented tolerance.
5. **Runtime validation** — backend diagnostics and performance captures prove
   that visual agreement was not obtained through an invalid or inefficient
   platform path.

Pure game behavior normally needs layers 1 and 2. Rendering migrations need all
five. Golden images containing retail assets remain local and must not be
committed.

## Reference-first fixture policy

A fixture is reference evidence, not a convenient expected value:

- Record the exact legacy source or executable revision.
- Record the source function, capture tool, replay, and logical tick used.
- Generate or audit the fixture before changing the portable implementation.
- Never generate expected values by calling portable production code.
- Never update a fixture merely to make a portable test pass.
- Represent an intentional difference as a narrow, documented exception rather
  than silently replacing the reference.

When a Windows runtime capture is not yet available, a source-audited fixture
may be used temporarily if it names the exact legacy functions and revision.
It must be replaced or confirmed by a runtime capture when that slice becomes
playable on the reference target.

## Current board-geometry gate

`pvz_game_legacy_parity_tests` establishes the first reference-first gameplay
gate. Its fixture is independent of portable `BoardGeometry` and records:

- all nine column origins;
- day, pool, and roof row counts and cell heights;
- day and pool plant origins;
- roof plant origins and pointer hit regions for every column;
- the board background crop;
- the legacy source revision and function provenance.

The test exhaustively compares every grid coordinate, exercises pointer mapping
from independently supplied positions, and drives `GameModule` through title,
menu, transition, grid focus, and rendering. It then validates the background
crop, seed-bank and packet destinations, and four selection-outline commands
against the legacy fixture.

## Current Level 1 gameplay gate

The first seed-bank and placement slice is checked independently from rendering.
The fixture values were audited from `Board::InitLevel`,
`Plant.cpp::gPlantDefs`, `SeedPacket::Update`, and the Level 1 background-row
selection in the reconstructed legacy source. Portable tests cover:

- 150 initial sun and the 100-sun Peashooter cost;
- selection requirements and non-destructive occupied-cell rejection;
- center-row-only planting for the first level;
- the exact refresh boundary, which becomes ready only after counter 750;
- fixed-width, transactional version-5 persistence with versions 1 through 4
  retained as readable inputs;
- a 1,310-tick replay whose final sun, recharge, occupancy, state hash, and
  rolling transcript hash are fixed.

This is source-audited and locally differential-tested, but not yet confirmed
by a new Windows runtime capture because the reference device is temporarily
unavailable. It remains explicitly pending runtime verification rather than
being reported as full parity.

Run this gate locally:

```sh
./script/validate_parity.sh
```

Run the complete portable suite:

```sh
cmake -S . -B out/portable -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DPVZ_BUILD_PORTABLE_ENGINE=ON \
  -DPVZ_BUILD_LEGACY_WINDOWS=OFF \
  -DBUILD_TESTING=ON
cmake --build out/portable
ctest --test-dir out/portable --output-on-failure
```

## Next infrastructure milestones

The board gates cover deterministic geometry, render-command alignment, and the
first Level 1 economy rules. The remaining cross-runtime infrastructure is:

- extension of the Windows behavior observation schema to cover seed-bank
  economy, followed by a fresh runtime capture of the Level 1 slice;
- the logical-tick input hook is complete: the Windows reference emits `PVZR`,
  while the macOS application and headless runner emit portable-state `PVZC`
  sessions; the headless runner can consume the Windows stream;
- a local image normalizer and difference reporter for user-owned golden
  screenshots;
- a parity manifest that records coverage and approved deviations per scene.

Until the relevant evidence exists, a migrated visual or gameplay slice should
be reported as implemented but not parity-validated.

The Windows input hook is deliberately below the Win32 message boundary: it
observes logical `WidgetManager` input, including legacy demo playback, and
captures immediately before each actual game update. It does not hash native
legacy objects. See `WINDOWS_REFERENCE_CAPTURE.md`.
