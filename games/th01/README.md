# TH01 Native

A partial native port of TH01 (*Highly Responsive to Prayers*).

The asset-free `th01_core` now implements REIIDEN input sensing, player
horizontal clamping, integer Orb helpers, shot spawn/pellet/boss collision
helpers, and the CShots-to-Orb update composition with an ordered render trace.
It uses shared master.lib IRAND. See
`docs/CORE_FOUNDATIONS.md` for exact donor boundaries and missing gameplay.

**Read `docs/COPY_DIFF.md` before adding anything.** The initial scaffolding
was copied from `th03-port` under W5a step 3. That document records the copied
regions and unresolved dependency assumptions. The later TH01 VRAM and core
implementations have their own source contracts.

## What builds today

The default configuration builds the partial core and VRAM libraries plus six
tests: skeleton, core, Orb scalars, shot collisions, shot-Orb composition, and
VRAM. Both compilers use warnings as errors:

```
cmake --preset gcc
cmake --build --preset gcc
ctest --preset gcc

cmake --preset msvc-x64
cmake --build --preset msvc-x64
ctest --preset msvc-x64
```

## What does not build, and why that is recorded rather than fixed

`th01_platform_sdl` — the copied SDL 3 host layer — is behind `TH01_ENABLE_SDL`,
which is `OFF`. Enabling it needs two things this tree does not have: an
authenticated SDL 3.4.10 source tree (see `tools/bootstrap_sdl3.ps1`, which is
offline by construction and refuses to run without an explicitly acquired
archive), and a 14-header closure spanning five `th03-port` library targets that
W5a step 3 did not copy. `docs/COPY_DIFF.md` §6 has the measurement, the control,
and the reason "just copy those too" is not the answer.

## Where the decisions live

`docs/CORE_FOUNDATIONS.md`, `docs/COPY_DIFF.md`, and `docs/RENDERER_MODEL.md`
ship in this native repository. The following planning/coverage records belong
to the separate private harness; they are not relative links in this checkout.

| question | answer lives in |
|---|---|
| what this tree is a copy of, and the diff | `docs/COPY_DIFF.md` |
| the port plan and its steps | `state/port/W5A_TH01_PLAN.md` |
| TH01's sources mapped onto the original TH03 module boundaries | `state/port/TH01_PORT_SCOPE.md` |
| the per-source coverage ledger | `state/port/TH01_SOURCE_COVERAGE.md` |
