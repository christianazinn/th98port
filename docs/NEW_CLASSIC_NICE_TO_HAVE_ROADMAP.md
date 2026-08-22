# New Classic content and tooling roadmap

Status: provisional product direction, updated 2026-08-22. This document orders the
next non-critical parcels after the playable port, netplay beta, online replay
service, and updater foundations. It may change as the authoring tools and
Arrange experiment expose better boundaries.

## Non-negotiable boundaries

- Classic remains a sealed port of the original game. It consumes none of the
  localization, Arrange, account-name, or enhanced menu configuration below.
- New Classic may deliberately diverge from Replay Patch presentation and UI,
  but deterministic stock gameplay and V13 replay behavior remain the default.
- Original game assets and extracted original text remain external inputs.
  Tracked files may contain schemas, compilers, synthetic fixtures, and
  translations the project is authorized to distribute.
- Every shipped content package has a deterministic schema and content digest.
  Runtime selection is fail-closed; a partly translated locale is never exposed
  as complete.

## Ordered parcels

### Immediate product regressions

Before the nice-to-have sequence continues:

- `HiScore` must always publish and enter either the local viewer or the
  capability-shaped hub. Selecting it must never leave a non-interactive frame
  that only Escape can unwind.
- The Replay hub must expose the signed-in player's accepted online submissions
  as `My Replays` whenever a validated account-history snapshot is available,
  including an honest empty library.
  This is the user-facing name for the backend/cache dataset previously called
  `My Records`; it remains distinct from the public best-per-profile board and
  from arbitrary downloaded replays.

Both fixes require enter/return/focus, empty-capability, offline, and corrupt-
cache regressions before the next UI parcel.

Status: closed in libth03 `6d13451`. The retained title completion is now
return-only while an active HiScore owner is composed; signed-in history adds a
literal `My Replays` route for both empty and nonempty validated snapshots, and
an already-open Replay hub adopts a completed refresh on its next source tick.
Rebuild distributed standalones before treating an older report as current.

### 1. Source-constrained in-game text

The existing `T3LNG001` catalog owns New Classic menu text rendered with
MNUFONT. In-game text is a distinct localization domain because it uses PC-98
graphics text, fixed-width layouts, gaiji, and scripted timing.

Introduce a companion compiled catalog rather than widening `T3LNG001` until
its runtime guarantees become ambiguous:

```text
UTF-8 author files + schema
          |
          v
deterministic compiler and layout validator
          |
          v
T3GTL001 source-constrained game-text package
          |
          v
New Classic MAIN / MAINL / OP presentation owners
```

The first migration is static semantic text currently embedded in native code
or replacement archives:

1. stage titles and character names;
2. score-registration and Clear Bonus labels;
3. Music Room titles;
4. other graphics-text rows with stable geometry.

The second migration extracts stable semantic text-run identifiers from the
external cutscene/interstitial scripts. A localized catalog replaces glyph
runs while the authenticated source script retains commands, waits, page
changes, and audio timing. Translators never edit binary control streams.

Status: implemented. `th03-cutscene-text`
drives the production VM over all 28 authenticated scripts and emits stable
`T3CTR001` run identity, geometry, CP932, and source-digest metadata;
`make_cutscene_text_author.py` validates it and writes the deterministic UTF-8
`T3CTR-AUTHOR-1` scaffold. `compile_cutscene_text_pack.py` freezes a complete,
source-bound `T3CTP001` catalog, and the game plus replay renderer substitute
only semantic glyph operations after the authenticated VM parses them. The
repository now tracks complete Japanese and English author catalogs and
source-text-free `T3CTR-SCHEMA-1` contracts; both compile without private DAT
extraction. The Japanese corpus produces 1,533 runs and 25,849 glyph cells
with a 39-cell maximum run; the English overlay produces 1,732 runs and 32,847
cells with a 30-cell maximum run. Both complete profiles validate. Commands
and timing are never sourced from translated bytes.

Character-select name bitmaps and other text baked into otherwise reusable
images follow after the text renderer can reproduce their layout. Menu headings
baked into TL02 and related backgrounds are explicitly later work: removing
them requires clean background reconstruction as well as localized rendering.

The source-text part of this parcel is now complete in both shipped languages.
`T3LNG001` contains 402 explicit English and Japanese New Classic messages;
Japanese rows are CP932 and combine authenticated proportional `MNUFONT.PF`
Latin with exact `FONT.ROM` Japanese glyphs. `T3GTL001` contains all 738
graphics-text rows in explicit English and Japanese catalogs, including every
win quote and Music Room comment. The Japanese author is regenerated from
authenticated Japanese OP/MAIN/MAINL executables and DAT assets rather than
from the English translation. `T3CTP001` already contains complete English and
Japanese catalogs for all 28 scripted dialogue/cutscene/ending programs. A
missing translated row can therefore no longer silently fall back in either
shipped locale.

Gates:

- deterministic compilation and exact package hashes;
- locale completeness, fallback, placeholder, encoding, glyph, and maximum
  layout validation;
- synthetic non-ASCII and width-stress fixtures;
- Japanese/English visual regressions for every migrated owner;
- cutscene command/timing identity with only glyph output substituted; and
- no change to Classic or stock-language V13 simulation.

### 2. Replay renderer desktop GUI

Build a small Windows desktop utility in the style of the Data Manager. It
wraps the existing `th03-replay-video` executable; it does not own a second
simulation, audio renderer, or encoder.

The package contains the GUI and the replay-renderer binary. FFmpeg is either
an explicitly licensed bundled artifact or a selected side-by-side/PATH binary;
the builder must not silently redistribute an arbitrary local FFmpeg build.

The initial UI has three restrained regions:

- replay, output MKV, and content directory;
- resolution, encoder, quality preset, language, flash policy, and 5%-step
  master/music/SFX levels; and
- two-pass progress, elapsed/ETA, current phase, cancel, and a collapsible log.

Add a newline-delimited machine progress protocol to the CLI instead of parsing
its carriage-return terminal bar. Drag/drop and remembered non-sensitive
preferences are useful, but reproducibility remains primary: every job also
writes or exposes the exact equivalent command line.

Gates: quoted/non-ASCII paths, cancellation and child cleanup, missing content
and FFmpeg errors, NVIDIA/software encoder selection, progress monotonicity,
output overwrite confirmation, and a PyInstaller package smoke next to the
maintained standalone.

Status: implemented. `T3RVP001` provides strict integral JSON-line progress;
the packaged Windows frontend owns the renderer/FFmpeg process tree, preserves
existing destinations through failure or cancellation, supports Unicode and
space-containing paths, exposes the exact equivalent command, and is included
by the manual Windows beta workflow. Human visual/usefulness testing remains a
release gate, not an implementation dependency.

### 3. Dopamine Arrange prototype

Arrange is a named, versioned ruleset, never a loose New Classic boolean. The
first experimental ruleset is `dopamine-test-v1`:

- spell cut-ins have no gameplay freeze interval;
- every spawned enemy formation uses the upside-down triangle formation; and
- CPU gauge decisions can select only level 2.

The prototype is New Classic Story/VS test functionality and is visibly marked
non-stock. Its rules enter deterministic initialization and any future replay,
verification, online, or netplay identity as one `RulesetId`.

Until a replay format or authenticated extension binds that identity, Arrange
recording, stock V13 playback, online upload, and netplay are disabled rather
than mislabeled as stock. The test selector lives at
`Option -> Gameplay -> Arrange` and persists the complete named ruleset in
`PODD.CFG`; the command-line switch remains for deterministic tooling.

Gates: stock rules byte/state identity, one rules object at construction,
targeted tests for all three deltas, deterministic restart behavior, and clear
rejection at every stock replay/online/netplay boundary.

Status: implemented as the developer-only `dopamine-test-v1` rules contract
and exposed through the retained Gameplay Options page.
The core consumes stock formation RNG before substituting formation 10,
reapplies the lock on repeat rounds, limits CPU gauge release to level 2, and
publishes the cut-in transition without the 27-sample hold. New Classic remains
the owner of input/Pause/configuration/presentation while its recorder is
explicitly memory-disabled; Replay and Online rows remain visible but disabled
and are skipped by navigation, automatic demos and Practice are unavailable,
and score mutation is suppressed. The authenticated title carries a third
`DOPAMINE ARRANGE` credit line. Both the ordinary executable and maintained
standalone also accept `--arrange dopamine-test-v1`. Switching back to Stock
restores the ordinary rules contract; unsupported Arrange replay/online paths
continue to fail closed.

### 4. Asset-decoupling support tools

These can proceed alongside the content migrations:

- a content audit that reports, per scene, which authenticated original assets
  are still required and which text/image regions have native replacements;
- localization extraction, completeness, font/glyph, and pixel-bounds reports;
- a translator preview tool that renders representative rows without launching
  gameplay;
- deterministic scene snapshots for localized layouts; and
- content-package manifests/digests suitable for releases, replays, online
  verification, and future Arrange packages.

These tools should consume the same decoders and schemas as production rather
than inventing shadow interpretations.

Status: in progress. `th03-game-text-preview` now renders all 738 `T3GTL001`
messages into deterministic 640x400 contact sheets plus a machine-readable
`T3GTP001` bounds index through the production PC-98 font and graphics-text
path, including all win quotes and Music Room comments. The definitive
covered-versus-image-baked inventory is
`NEW_CLASSIC_TEXT_LOCALIZATION_COVERAGE.md`. `th03-content-audit` now emits or
validates the shared production
271-resource manifest scene by scene, distinguishes 52 authenticated English
override members from original extraction dependencies, and records every
localized semantic surface without claiming that its source backing vanished.
The remaining work in this parcel is localized scene snapshots, a unified
locale-bundle authoring command, and release-level content manifests. The
unified command should compile and validate `T3LNG001`, `T3GTL001`, and
`T3CTP001` together, emit completeness/font/bounds reports, and generate
translator contact sheets without launching gameplay. General Unicode locales
also need a distributable font and layout contract; the current production
renderer deliberately covers source-era ASCII and CP932 Japanese only.

### 5. Account nametags and VS presentation

The product decisions and first account-backed implementation are complete:

1. one mutable public account nametag, with the opaque UUID as stable identity;
2. printable ASCII and duplicate names for the first version;
3. exact whole-word wrapping across two 28-cell title rows;
4. immutable server snapshots for account rooms and local/unverified identity
   for direct UDP;
5. separate revocable device credentials joined by a short-lived code; and
6. a backward-compatible typed V13 tail for later replay identity rather than
   a forced V14.

The server provides authenticated identity metadata, the match contract
snapshots it, and the MAINL VS intro renders bounded names without changing
deterministic gameplay. Replays will preserve the match-time display rather
than silently substituting a later account rename.

Status: account profile, two-line layout, separately credentialed device
linking, authenticated room snapshot, peer agreement, VS-title rendering, and
the retained first-use/edit UI are complete. The approved design is in
`NEW_CLASSIC_NETPLAY_NAMETAGS.md`. The original 24-byte suggestion was a
conservative single-line placeholder, not a source limit. The real title area
has two 28-cell rows, so validation accepts at most 57 printable-ASCII source
bytes (`28 + space + 28`) only when whole-word wrapping actually fits. The
source character-name row remains unchanged. There are no safe V13 header
bytes to reuse, but a fixed typed identity block can live after the declared
logical input stream and before an optional final `T3RACC1` payload. Existing
V13 readers ignore that tail, so this presentation metadata does not require
V14. That replay-tail/preservation work remains next.

## Current execution order

The original source-text, renderer-GUI, Arrange, content-audit, and live
nametag parcels above are implemented. The remaining product/tooling work is:

1. Add the backward-compatible V13 identity tail and surface match-time names
   in replay details/export without changing deterministic replay authority.
2. Finish localization convenience tooling: one locale-bundle build/check
   command, deterministic Japanese/English scene snapshots, translator contact
   sheets for menu and scripted text, and release manifests.
3. Decouple text baked into images, starting with character-select names/titles
   and reusable menu headings. This requires clean background reconstruction;
   it is not a missing string-catalog problem.
4. Finish production update release inputs: origin, platform packages, signing
   key custody/rotation, and hosted CI artifact validation. The updater engine
   itself is already implemented and default-off without those inputs.
5. Continue online operations and UX: reviewed historical-score import,
   operational verifier/service monitoring, and any remaining public-account
   recovery flows. The replay-backed Story leaderboard/upload/download/watch
   path is already live.
6. Improve PMD/YM2608 audio fidelity where channel balance and simultaneous
   SFX layering still differ audibly from PC-98 output.
7. Expand Arrange only after choosing a shippable ruleset and a replay/content
   identity for it. `dopamine-test-v1` remains an intentionally non-stock test.
8. Design optional `Smooth` display interpolation behind the existing exact
   source scheduler; `Repeat` sample-and-hold remains the authentic default.
9. Resume broad pixel/audio-oracle coverage after these moving functional and
   localized surfaces settle. Stock deterministic/replay gates remain active
   throughout.

Longer-term work remains VS match history, netplay replay evidence on services,
ratings/Elo/seasons/tournament policy, and runtime-localized image-heavy title
art. None is required to call the English/Japanese source-text coverage
complete.

Pixel-oracle expansion is useful after these functional seams settle, not
before every localized or Arrange screen is still moving. Stock routes keep
their existing deterministic and replay gates throughout.
