# New Classic content and tooling roadmap

Status: provisional product direction, 2026-08-20. This document orders the
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

Character-select name bitmaps and other text baked into otherwise reusable
images follow after the text renderer can reproduce their layout. Menu headings
baked into TL02 and related backgrounds are explicitly later work: removing
them requires clean background reconstruction as well as localized rendering.

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
than mislabeled as stock. The initial access path may be a developer command
line switch; a polished Arrange selector belongs later in New Classic.

Gates: stock rules byte/state identity, one rules object at construction,
targeted tests for all three deltas, deterministic restart behavior, and clear
rejection at every stock replay/online/netplay boundary.

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

### 5. Account nametags and VS presentation

This is deliberately last. Before implementation, decide:

1. whether the public account display name or a per-match nametag is shown;
2. Unicode normalization, uniqueness/case-folding, moderation, and edit policy;
3. maximum graphemes and UTF-8 bytes, plus source-font fallback/truncation;
4. whether both players' immutable account IDs and display-name snapshots are
   stored in replays;
5. whether a backward-compatible authenticated V13 extension is acceptable or
   the change waits for V14; and
6. offline/direct-IP behavior and privacy expectations.

Once decided, the server provides authenticated identity metadata, the match
contract snapshots it, and the MAINL VS intro renders bounded names without
changing deterministic gameplay. Replays should preserve the match-time
display, not silently substitute a later account rename.

## Suggested immediate execution order

1. Freeze `T3GTL001` and migrate static in-game English text out of C++ tables.
2. Add external pack loading and the cutscene text-run extractor/compiler.
3. Build the replay renderer's machine progress protocol and GUI package.
4. Add the explicit ruleset seam and Dopamine Arrange developer prototype.
5. Continue the remaining text/image decoupling and preview/audit tools.
6. Stop for the nametag product decisions above.

Pixel-oracle expansion is useful after these functional seams settle, not
before every localized or Arrange screen is still moving. Stock routes keep
their existing deterministic and replay gates throughout.
