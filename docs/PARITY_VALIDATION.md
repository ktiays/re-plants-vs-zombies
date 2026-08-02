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
- fixed-width, transactional version-8 persistence with versions 1 through 7
  retained as readable inputs;
- a 1,310-tick replay whose final sun, recharge, occupancy, state hash, and
  rolling transcript hash are fixed.

The version 3 Windows runtime replay now confirms this economy slice through
two placements and three collected suns. It also verifies pointer-hover grid
focus, the delayed collection flight, and the tutorial transition that counts
sun already moving toward the counter.

## Current complete Level 1 combat gate

The combat fixture is independent of `LevelOneCombat` and records values
audited from `Board::SetTutorialState`, `Board::PickZombieWaves`,
`Board::UpdateZombieSpawning`, `Plant::UpdateShooter`,
`Plant::UpdateShooting`, `Plant::Fire`, `Projectile::UpdateNormalMotion`,
`Projectile::FindCollisionTarget`, `Zombie::CheckIfPreyCaught`, and
`Zombie::EatPlant`. Portable differential tests cover:

- the two-plant tutorial transition, 400-tick tutorial sun boundary, 25-sun
  value, and 99-tick first-wave countdown;
- the complete Level 1 normal-zombie composition of 1, 1, 1, and 2 across all
  four executable waves;
- the source `2500 + Rand(600)` next-wave range, the 400-tick minimum age and
  200-tick accelerated countdown, and the 50-to-65-percent wave-health gate;
- 300 plant health, 270 normal-zombie health, 20 pea damage, 3.33-pixel pea
  movement, 150-tick launch rate, 33-tick firing sequence, and four damage
  every four zombie-age ticks;
- a deterministic complete-level simulation reaching the award after 4,995
  combat ticks, an engine-input integration path from sun collection through
  zombie spawn, and transactional round-trip coverage for the fixed 782-byte
  combat record;
- lawn-mower collision before the `x = -100` loss check, source-derived mower
  acceleration and one-use spent state, plus explicit won/lost/award state;
- independently collectible overlapping sky suns, proving that the spawn
  countdown continues while an earlier pickup remains active.

The version 3 semantic tape records post-choice sun countdown/X/ground-Y and
normal-zombie X/speed values rather than copying the legacy global PRNG. The
portable game consumes those decisions through an engine-neutral interface,
rejects ordering or range drift, and carries micro-pixel zombie speed through
fixed-width movement state. A 9,169-tick Windows capture with five sun choices
and one zombie choice matches the portable behavior stream exactly.

Version 4 adds semantic wave-schedule decisions and per-tick current-wave,
zombie-countdown, zombie-count, outcome, mower, and award observations. The
complete-level expected values are source-audited at revision
`79f7b4cc4d09eae842e0bb57ad798ffef8e25007`. Version 5 adds semantic initial
and recurring Peashooter launch schedules after the first complete native run
proved that fixed 150-tick reloads accelerated later waves. Native full-level
v5 observations also carry current-wave health and projectile count, exposing
the originating shot/collision drift before it shifts a later wave. A semantic
projectile-spawn record carries the native pre-update pea origin produced by
the live head animation, without exposing reanimation state to portable game
logic. A per-live-slot zombie-motion record applies the same boundary to the
legacy `_ground` animation track and records the post-update synchronized
integer position used by legacy collision, plus the one-bit result of the
headless zombie's random health-decay choice. Strict full-level replay is the
runtime evidence gate. Version 6 adds per-live-slot synchronized projectile
positions so legacy float accumulation cannot cross a later collision pixel.

The 2026-08-02 v6 Windows run crossed that gate for combat. Its 15,701-frame
trace reaches the native award at tick 13,796 and contains 13,029 ordered
semantic decisions. AppleClang and MSVC both consume the entire tape, reproduce
every version-4-through-6 combat observation through the award, and emit
byte-identical 1,409,462-byte portable captures. Cross-testing exposed and
fixed stable projectile-slot ordering, legacy float-to-integer projectile
motion, and the final-zombie one-third-health award rule.

The complete normalized behavior stream is not yet identical. Asynchronous
startup first differs at tick 5,030 (`main-menu` versus `adventure-intro`), and
the first playing-field difference is one missed 25-sun collection at tick
9,503 (native 50, portable 25). No combat field differs. Sun trajectory/pickup
geometry and rendered screenshot parity remain explicit follow-up gates.

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

The board gates cover deterministic geometry, render-command alignment, the
Level 1 economy, and complete Level 1 combat. The remaining cross-runtime
infrastructure is:

- a trajectory/collection diagnostic for the remaining 25-sun pickup drift;
- a local image normalizer and difference reporter for user-owned golden
  screenshots;
- a parity manifest that records coverage and approved deviations per scene.

The fresh Windows version 3 runtime capture remains the backward-compatibility
gate through the first normal-zombie spawn. The same 9,169-frame artifact now
replays through the version 6 reader with byte-identical AppleClang/MSVC
results. See
`WINDOWS_REFERENCE_CAPTURE.md` for the evidence record.

Until the relevant evidence exists, a migrated visual or gameplay slice should
be reported as implemented but not parity-validated.

The Windows input hook is deliberately below the Win32 message boundary: it
observes logical `WidgetManager` input, including legacy demo playback, and
captures immediately before each actual game update. It does not hash native
legacy objects. See `WINDOWS_REFERENCE_CAPTURE.md`.
