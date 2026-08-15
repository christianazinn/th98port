# New Classic product and menu design

Status: **accepted working direction, subject to revision**

Decision date: 2026-08-14

Replay Patch compatibility baseline: **0.4.14** (`33d35c50`)

This document defines the current product direction after the faithful native
Replay Patch port. It replaces the earlier assumption that a host-rendered
Classic/New Classic selector remains the product root. It is a design and
execution contract, not a claim that every described screen or service already
exists.

## Product identity and launch flow

The application boots directly into New Classic:

```text
ZUN Soft opening
future project/development-team credit opening
New Classic title
```

There is no outer mode-selection menu. The New Classic title is the product
root. Its seven rows remain within the existing source-derived box:

```text
Start
VS Start
Music room
HiScore
Replay
Option
Quit
```

This deliberately moves `Replay` above `Option` without enlarging or moving
the title box. `Quit` exits the application.

Classic is reached through `Option -> System -> Switch to Classic`. Selecting
it is an action, not a persistent startup preference. The source-derived
transition must ask for confirmation, finish New Classic's current title/audio
transaction, and launch Classic without replaying the application startup
openings.

Classic remains a sealed, self-contained port of the base game. It receives no
New Classic localization, accessibility, input, audio-percentage, online, or
other native feature settings. Classic's own title, Options, score data,
gameplay, presentation, and audio behavior remain unchanged. When its original
Quit path has completely finished, host ownership returns to a fresh New
Classic title. That destination exists outside the completed Classic
experience and therefore does not add a row or overlay to Classic itself.

Direct command-line Classic launch may remain as a development, test, and
compatibility route. It is not part of the normal player-facing hierarchy.

## Fidelity boundary

Replay Patch 0.4.14 is the final feature baseline for the PC-98 product unless
a serious correctness, replay-compatibility, or data-loss defect requires a
maintenance release. New product features belong in native New Classic rather
than being artificially backported to PC-98.

New Classic uses four fidelity tiers:

| Tier | Policy |
|---|---|
| Deterministic gameplay, RNG, scoring, and rules | Exact against the maintained donor/oracles |
| V13 recording, playback, checkpoints, summaries, and verification | Exact and mutually compatible |
| Ordinary gameplay presentation and audio | Source-faithful at default settings; explicit native presentation options may change the final output only |
| Title submenus, settings, storage tools, and online services | Native extensions in the Replay Patch visual language |

Removing pixel identity as an absolute requirement for extended menus does not
weaken unexplained-difference handling. Every difference must be either a
declared native extension or a bug. Unchanged screens and default gameplay stay
under the existing state, replay, audio, and pixel gates. Extended screens get
native golden/surface tests plus source-style page, palette, input-release, and
transition contracts.

Native speed is not a compatibility defect. Replay checkpoint loading and
other host-owned work should be as fast as reasonably possible rather than
reproducing PC-98 disk, decompression, or CPU delay.

## New Classic Option hierarchy

The first Option screen is a seven-row category menu:

```text
Gameplay
Audio
Input
Video
Language
System
Return
```

All pages use the existing New Classic proportional typography, retained title
background, checkerboard window, palette, release-edge input, and grow/shrink
visual vocabulary. A page should normally contain no more than six choices
plus `Return`. Variable-length language and device lists may page or scroll.

### Gameplay

Initial contents:

```text
Rank
Return
```

Future gameplay-affecting options require explicit replay fields and
leaderboard category identities. They must not be smuggled into an unrecorded
host settings document.

### Audio

Target contents:

```text
Master Volume       100%
Music Volume        100%
Sound FX Volume     100%
Mute When Unfocused Off
Return
```

Implemented now: `Master Volume`, `Music Volume`, and `Sound FX Volume` in 5%
steps, `Mute Unfocused`, and `Return`.

Percentages use 5% increments. Zero percent has the existing disabled
semantics; raising a value above zero re-enables that category. At 100%, the
current source-faithful PMD/OPNA sample stream must remain unchanged.

Master volume remains a final native output gain. Independent Music and SFX
gain is implemented inside the shared PMD driver, before song parts and the
external FM effect part converge on the one YM2608. This preserves source
effect arbitration, FM6 takeover, and source fade volume. The 100% path bypasses
scaling; 0% retains the prior disabled semantics.

Focus mute is final host-delivery policy. Losing focus clears queued SDL PCM
and suppresses new submissions while PMD, gameplay, replay, and retained-menu
clocks continue. Refocus resumes from the current source state. Classic ignores
this New Classic-only setting.

### Input

Target contents:

```text
Key Mode
KeyConfig
JoyConfig
Device Assignment
Input Test
Return
```

The existing New Classic keyboard/Story/Autofire store and exact KeyConfig
screen are retained. JoyConfig adds controller buttons, axes, and dead zones;
the separate assignment owner adds hotplug-safe identity through the existing
logical input boundary.
Device Assignment offers Automatic, Disabled, or one stable connected-device
identity independently for P1 and P2; its child screen owns Restore Defaults.
Input Test shows both aggregate logical words and each connected controller's
current contribution without exposing raw host input to gameplay. None of
these mappings apply to Classic.

### Video

Target contents:

```text
Window Mode
Window Scale
Aspect Ratio
Filtering
Frame Presentation
Flash FX
Return
```

The canonical game surface remains 640x400. Window mode and scale select host
presentation only. Initial scale choices should cover 640x400, 1280x800,
1920x1200, and automatic integer scaling. Aspect and filtering choices must
not change the canonical surface used by pixel and replay verification.

Implemented now: `Window Mode` selects a resizable window or borderless
desktop fullscreen. `Window Scale` selects 1x, 2x, 3x, or the largest of
those presets that fits the current display. `Aspect Ratio` selects either
the exact 640x400 `8:5 Pixels` grid or a `4:3 Display` presentation that
stretches only the final host output to 640x480. `Filtering` selects Nearest
or Linear sampling. Changes apply live through one atomic SDL transaction and
persist in `PODD.CFG`; the renderer continues to upload the same exact
640x400 texture and Classic exposes no such setting. Frame-presentation
policy remains later work.

Simulation remains at the source rational cadence. A display-synchronized mode
may repeat the latest complete source frame at the monitor refresh rate. True
interpolation is a later, explicitly non-authentic experiment and must never
change simulation, input latching, or replay samples.

`Flash FX` is New Classic-only. Classic ignores the setting.

### Language

The existing complete Japanese/English profiles move here first. The long-term
system uses stable message identifiers, UTF-8 translator-facing resources,
locale fallback, placeholder validation, and automated glyph/width/layout
checks. Shell-capable Unicode text and source-constrained in-game text are
separate localization domains even if they share an authoring tool.

A compiled, versioned language pack should be the runtime input. Fluent FTL is
the leading authoring candidate, not yet a frozen implementation choice. A
language cannot be selectable until all required assets and text domains pass
the completeness and layout gates. Classic is never localized by this system.

### System

Target contents:

```text
Switch to Classic
Data & Storage
Online & Privacy
Updates
Credits
Diagnostics
Return
```

Only `Switch to Classic`, `Credits`, and already-working storage-folder actions
need to exist initially. Do not publish disabled placeholder choices merely to
make this target shape visible.

## Configuration ownership

Classic and New Classic must no longer treat one mutable `YUME.CFG` as a shared
product settings document.

- Classic retains the stock-compatible `YUME.CFG` owner and its current exact
  configuration behavior.
- New Classic receives a distinct, versioned, atomically published native
  settings document for its source-compatible options and host extensions.
- Existing `TH3KEY.CFG`, `TH3JOY.CFG`, `TH3DEV.CFG`, replay, and
  expanded-score stores remain distinct typed documents.
- First migration may import current New Classic values from the existing
  `YUME.CFG`, but it must not destroy or reinterpret the Classic file.
- Authentication credentials and secrets, if later required, do not belong in
  the ordinary settings document.

This split precedes the visible menu reorganization. Otherwise changing Rank,
Music, Key Mode, Language, or Flash in New Classic would continue to mutate the
configuration later observed by Classic.

## HiScore, leaderboards, and replays

`HiScore` owns score-oriented views. `Replay` owns replay-oriented views. They
share data and link to each other, but are not the same navigation concept.

The eventual HiScore hub is:

```text
Local HiScores
Online Leaderboards
Personal Bests
Historical Records
Return
```

The eventual Replay hub is:

```text
Local Replays
Online Replay Library
Downloaded Replays
My Uploads
Return
```

Do not add either hub while it would contain only one real destination; the
current direct Local HiScore and Local Replay routes remain less frustrating
until at least one additional view exists.

An ordinary online leaderboard entry is derived by a pinned, sandboxed,
resource-limited server verifier from an uploaded V13 replay. The client never
supplies an authoritative score. The entry links to the replay and exposes
watch, download, and split-detail actions.

Pre-Replay Patch records may be seeded as curated legacy entries with explicit
player, score, date/event, evidence source, and curator note. Their replay
reference is absent and the UI labels them `Historical` or `No replay
available`. They may share a ranking with replay-verified entries only when the
ruleset and evidence are sufficiently comparable; otherwise they remain in the
Historical Records view.

The domain model therefore treats a replay reference as optional and records
verification provenance explicitly:

```text
LeaderboardEntry
  score/category/player/result
  provenance = replay_verified | curated_legacy
  replay_id = optional
  verifier/content/ruleset identity
  evidence/curator fields for legacy rows
```

Online features remain offline-first. Network or service failure cannot affect
gameplay, local score persistence, or local replay recording.

## Implementation order

The order below minimizes throwaway UI and protects Classic before New Classic
begins accumulating more settings.

### 0. Freeze the reference and declare the divergence policy

Status: substantially complete; this document closes the product-design part.

- Keep Replay Patch 0.4.14 and its release-inert capture descendant pinned.
- Preserve current gameplay/replay/audio tests and source-aligned pixel evidence.
- Mark future native menu differences in the pixel coverage manifest rather
  than weakening the oracle globally.

Gate: authority/pin checks remain GREEN and every unchanged route keeps its
existing oracle classification.

### 1. Reorder Replay and Option on the title

Status: complete in session 173.

This is the smallest player-visible parcel and can be implemented immediately.

- Change the New Classic choice order and proportional labels to place Replay
  before Option.
- Update selection, route, return-focus, checker restoration, and title-input
  tests; do not resize or move the box.
- Keep Classic's title array byte-for-behavior unchanged.

Gate: New Classic publishes the seven expected rows and every row enters and
returns from the correct owner; Classic title hashes and route tests are
unchanged.

### 2. Split Classic and New Classic configuration

Status: complete in session 174. New Classic owns versioned `PODD.CFG`;
Classic remains the sole owner of `YUME.CFG`.

- Add the versioned New Classic settings codec/store with atomic
  write/read-back validation.
- Migrate existing New Classic values once without changing `YUME.CFG`.
- Make every New Classic session and playback presentation consume the new
  typed settings; Classic continues to consume only its own configuration.

Gate: migration/corruption/fault-injection tests, independent Classic/New
Classic mutation tests, and unchanged V13/gameplay hashes.

### 3. Introduce the hierarchical Option owner

Status: complete in session 175. New Classic owns the seven-category retained
owner; Classic keeps its original four-row screen.

- Replace New Classic's flat seven-row Option screen with the category screen.
- Initially move only working controls: Rank; Music/SFX On/Off; Key Mode and
  KeyConfig; Flash FX; Japanese/English; Switch to Classic; Return.
- Reuse the retained title page, menu font, checker backing, palette, and input
  release machinery rather than creating another host canvas.
- Return to the exact prior category/cursor after child screens.

Gate: every existing setting still persists and applies live, page ownership
and transition tests cover all category/child returns, and Classic Options are
unchanged.

### 4. Remove the outer experience menu

Status: complete in session 176. Startup boots New Classic directly; the
confirmed System action temporarily enters sealed Classic.

- Run the startup sequence once, then construct New Classic directly.
- Route `System -> Switch to Classic` through a confirmation and a typed
  experience handoff.
- Route completed Classic Quit to a fresh New Classic title; route New Classic
  Quit to application exit.
- Remove the production `ExperienceMenu` loop and, once no tools/tests need it,
  delete the unused menu implementation and stale help/copy.
- Preserve direct `--classic` and `--new-classic` developer/test launch modes.

Gate: startup-to-New-Classic first publication, held-Z seed 34, New
Classic-to-Classic-to-New-Classic lifecycle, app Quit, application close, audio
ownership, input rebasing, and standalone smoke on both compilers.

### 5. Add near-term native settings

These can proceed independently once the settings store and Option hierarchy
exist:

1. Master volume in 5% increments with an exact unity path.
2. Windowed/borderless state and integer scale presets.
3. Nearest/optional linear filtering and aspect policy. Complete in session
   181 with zero-filled v1 settings retaining the prior 8:5/Nearest default.
4. JoyConfig, device assignment, dead-zone calibration, and input test page.
   Host-neutral button/stick/direction/dead-zone persistence, the retained
   source-styled JoyConfig screen, and live SDL mapper replacement are complete
   in session 182. Session 183 completes stable GUID-plus-serial/path device
   identity, atomic `TH3DEV.CFG` assignment persistence, hotplug reassignment,
   and the retained source-styled live Input Test page.
5. The existing Japanese/English selector moved to the Language page with the
   category hierarchy in session 175.
6. Mute When Unfocused is complete in session 185. It applies only at the
   final SDL output boundary and migrates existing settings with default Off.

Each feature becomes visible only when its apply, persistence, rollback, and
return-to-menu paths are complete.

### 6. Independent Music and SFX percentages

Status: complete in session 184. New Classic persists and applies independent
5%-step Music and Sound FX levels; Classic remains on the exact 100% path and
its original On/Off UI.

The implementation places category gain at the shared PMD ownership seam
without altering PMD fades, effect priority, channel takeover, pause/resume,
or source command timing.

Gate: 100% output is byte-identical across the complete 21-song/22-effect
matrix; 0%, intermediate gain, clipping, fade, simultaneous music/effect, and
live Option changes are independently tested on GCC and MSVC.

### 7. Localization-pack pipeline

Status: complete across title/Option in session 186, authenticated Replay
browser/save in session 187, and Practice/input screens in session 188. Stable IDs,
UTF-8 authoring files, the deterministic `T3LNG001` compiler/loader, exact
fallback/placeholder/glyph/width validation, built-in Japanese/English
catalogs, and a width-stressing pseudo-locale now own the New Classic title,
hierarchical Option UI, Replay browser and save/name flow, Practice setup,
KeyConfig, JoyConfig, controller assignment, and Input Test. Named dynamic
templates are load-time width-checked using declared worst-case values; host
key/controller names remain bounded platform data inside localized rows. The
donor page/palette/input machines and Classic are unchanged. A future Unicode
product-shell renderer remains separate from the authenticated ASCII MNUFONT
contract.

- Freeze message identifiers and text domains.
- Build the translator-facing authoring, compilation, and validation tools.
- Convert Japanese/English into the new pipeline without visual regressions.
- Add one deliberately width-stressing test locale before calling the format
  extensible.

Gate: completeness, fallback, placeholders, glyph coverage, maximum widths,
deterministic package hashes, and representative screen snapshots.

### 8. HiScore/Replay hubs and online domain seam

Status: domain/cache boundary complete in session 189; visible hubs remain
deliberately deferred until a second real destination exists.

- Define leaderboard/replay identifiers, category keys, provenance, and local
  cache documents without selecting a service vendor.
- Add the HiScore hub when Personal Bests or a real online view exists.
- Add the Replay hub when an online/downloaded view exists.
- Preserve direct Local views until then.

Gate: offline behavior, replay cross-links, legacy entries without fake replay
IDs, cache corruption/recovery, and no network dependency below the client
service boundary.

The frozen client-side contract is documented in
`NEW_CLASSIC_ONLINE_CACHE.md`. It contains typed content-digest identities,
explicit replay-verified versus curated-legacy provenance, and a bounded
atomic offline cache. It contains no endpoint, credential, provider name, or
network transport, and nothing currently makes either future hub visible.

### 9. Server and sharing

- Deploy a queued, sandboxed `th03rv` verifier keyed by replay, content,
  ruleset, and verifier version.
- Derive score/result/category server-side; enforce size, time, decompression,
  and rate limits.
- Add upload consent, authentication, moderation, and privacy only after the
  trust model and operational owner are settled.

Gate: hostile corpus, reproducible verification, idempotent submission,
versioned categories, replay download integrity, and service-offline client
behavior.

### 10. Display-refresh decoupling and optional interpolation

Display-synchronized repeated presentation can precede interpolation. The
fixed source scheduler and replay input boundary remain untouched. True
interpolation requires a separate presentation specification and is not a
prerequisite for any prior phase.

## Release discipline

Every test-worthy phase ends with:

- focused tests for the changed owner;
- full GCC and MSVC native/SDL matrices;
- the complete V13 corpus and pause/slowdown gates;
- Classic non-regression routes;
- relevant pixel/audio gates with intentional-divergence classification;
- `git diff --check`; and
- a reproducible maintained standalone rebuild for human testing.

Do not add inert placeholder rows, weaken source or replay gates, or let a
New Classic setting silently flow into Classic merely to shorten a parcel.
