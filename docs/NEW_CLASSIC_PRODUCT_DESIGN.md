# New Classic product and menu design

Status: **accepted working direction, subject to revision**

Decision date: 2026-08-14

Replay Patch compatibility baseline: **0.4.15** (`43028f09`)

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

Replay Patch 0.4.15 is the final feature baseline for the PC-98 product unless
a serious correctness, replay-compatibility, or data-loss defect requires a
maintenance release. New product features belong in native New Classic rather
than being artificially backported to PC-98.

New Classic uses four fidelity tiers:

| Tier | Policy |
|---|---|
| Deterministic gameplay, RNG, scoring, and rules | Exact against the maintained donor/oracles |
| V14 recording, playback, checkpoints, summaries, and verification | Exact and mutually compatible |
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
Input
Video
Audio
Language
System
Return
```

All pages use the existing New Classic proportional typography, retained title
background, checkerboard window, palette, release-edge input, and grow/shrink
visual vocabulary. A page should normally contain no more than six choices
plus `Return`. Variable-length language and device lists may page or scroll.

Every Option label receives one reciprocal shadow pixel at (+1,+1): selected
white text uses a black shadow and unselected black text uses a white shadow.
This preserves the source glyph colors and retained-title transparency while
keeping the detailed upper background readable. The shadow remains inside the
existing row restoration domain so transitions erase it with the label.

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

Implemented now: `Master Vol.`, `Music`, and `SFX` in 5% steps, `Mute Away`,
and `Return`. These compact on-screen labels preserve the concepts named in
the target layout while fitting the existing proportional-font columns.

Percentages use 5% increments. Zero percent has the existing disabled
semantics; raising a value above zero re-enables that category. At 100%, the
current source-faithful PMD/OPNA sample stream must remain unchanged.

The displayed value is a linear audible-amplitude percentage. The final PCM
Master gain and ADPCM-B lane can apply that value directly. FM and rhythm
total-level codes and fixed SSG levels are logarithmic, so the PMD adapter
converts each 5% step to the nearest native attenuation instead of multiplying
the register code. In particular, 75% FM uses three 0.75 dB steps (about 77%
amplitude), rather than the roughly 48 dB cut produced by the original
incorrect register multiplication.

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

Implemented now: `Switch to Classic`, `Data & Storage`, `Online & Privacy`,
capability-gated `Updates`, `Credits`, `Diagnostics`, and `Return`.
`Data & Storage` opens the persistent user-data directory or the active replay
directory through a typed host action after the deterministic menu tick. The
retained owner performs no filesystem or shell work, and Classic exposes no
equivalent extension. `Credits` uses the authenticated proportional font and
the same row-clipped reciprocal shadow as the rest of New Classic Options. It
contains only factual, replaceable project/source acknowledgements; the
separately reserved project-team startup animation remains untouched.
`Online & Privacy` reports whether the room identity belongs to the platform
store or the explicit environment fallback. It provides a confirmation-gated
`Forget Room Identity` action on Windows, Linux, and macOS. The
retained menu never receives or renders credentials; it emits a typed host
request that removes only `PODD.AUTH` after the deterministic menu tick.
Windows protects the opaque credential with current-user DPAPI; POSIX publishes
the compatible `T3AUTH01` record atomically with owner-only `0600` permissions.
`Diagnostics` opens the authoritative user-data directory containing
`last-run.log` and crash evidence through the same typed post-tick host action
as Data & Storage. The strict `T3UPD001` signed-manifest parser, monotonic
release selection rule, offline authoring tool, provider-backed pinned-key
P-256 authenticator, and inert read-back-verified package staging now form the
first Updates infrastructure boundary. The platform-neutral acquisition
transaction plus strict WinHTTP/libcurl adapters now implement manifest-first
authentication, exact signed-artifact response bounds, no redirects, and
failure-preserving inert staging on every beta platform. `T3PKG001` adds a
portable traversal-proof package and complete inert generation materializer.
`T3ACT001` now freezes the active/pending/previous identity transaction,
epoch-bound trial acknowledgement, interrupted-trial rollback, and alternating
durable state store. The packaged bootstrap, conservative generation cleanup,
compiled trust record, background next-launch install task, retained explicit
`Check for Updates` flow, and persisted automatic-check policy are complete.
The row exists only when the
running New Classic build has all three real capabilities: a compiled trust
root, bootstrap-provided storage roots, and its platform HTTPS adapter. The
source owner emits one typed request, remains responsive while the bounded host
worker runs, and renders only fixed localized `Checking`, `Up to date`,
`Restart to apply`, or `Check failed` states. It never renders remote text or
replaces/relaunches the running process. Default, direct, Classic,
replay-capture, and pixel-oracle builds expose no row or disabled placeholder.
Capable packages default to one automatic background attempt per process; the
Updates child can disable future automatic attempts while retaining the manual
check. Existing `PODD.CFG` documents migrate atomically with that default
enabled.
Production origin/pin values and signing-key custody/rotation remain release
inputs.

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

The HiScore hub is:

```text
Local HiScores
Online Leaderboards
Personal Bests
Return
```

The Replay hub is capability-shaped. With cached replay rows it is:

```text
Local Replays
Upload Replay              (official desktop service or configured override)
My Replays                 (when signed-in history is available; may be empty)
Online Replay Library
Downloaded Replays
Return
```

`My Replays` is the accepted-submission history (the service/cache may retain
the internal `My Records` name). It may show an honest empty library once the
signed-in history capability is known; it is not shown as an inert
placeholder. It remains distinct from the public best-per-profile board.

The HiScore hub appears only when at least one validated cached online board
exists for the active rank. With no such board, HiScore still opens the local
viewer directly, so an offline or unconfigured installation gains no empty
navigation layer. The Replay hub appears only when the active-rank cache
contains at least one replay record or upload is available.
Unsupported rows are omitted rather than disabled. Otherwise Replay still
opens the local viewer directly, so an offline, curated-history-only, or
unconfigured installation gains no empty navigation layer.

An ordinary online leaderboard entry is derived by a pinned, sandboxed,
resource-limited server verifier from an uploaded V14 replay. The client never
supplies an authoritative score. The entry links to the replay and exposes
watch, download, and split-detail actions.

Pre-Replay Patch records may be seeded as curated legacy entries after their
video evidence has been reviewed. They appear directly on the same leaderboard
and use the same score ordering, but have no replay watch/download action and
need no public evidence link.

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

- Keep Replay Patch 0.4.15 and its release-inert capture descendant pinned.
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
Classic mutation tests, and unchanged V14/gameplay hashes.

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
7. Online Match connection, relay policy, and Direct UDP host port persistence
   is complete in session 260. `PODD.CFG` v6 introduced these fields; the
   preferences and automatic-update policy remain local and never enter
   deterministic match state.
8. The provisional Arrange ruleset selector is complete in session 335.
   `PODD.CFG` v7 upgrades v1-v6 documents with Stock as the exact default and
   persists either the complete Stock or `dopamine-test-v1` contract.

Each feature becomes visible only when its apply, persistence, rollback, and
return-to-menu paths are complete.

### 6. Independent Music and SFX percentages

Status: complete in session 184. New Classic persists and applies independent
5%-step Music and Sound FX levels; Classic remains on the exact 100% path and
its original On/Off UI.

Session 204 corrected the intermediate gain curve: percentages now express
linear amplitude and are translated to each YM2608 lane's logarithmic level
domain. Unity remains an exact control-flow bypass.

The implementation places category gain at the shared PMD ownership seam
without altering PMD fades, effect priority, channel takeover, pause/resume,
or source command timing.

Gate: 100% output is byte-identical across the complete 21-song/22-effect
matrix; 0%, intermediate gain, clipping, fade, simultaneous music/effect, and
live Option changes are independently tested on GCC and MSVC.

### 7. Localization-pack pipeline

Status: complete across title/Option in session 186, authenticated Replay
browser/save in session 187, Practice/input screens in session 188, and the
complete Japanese menu domain in session 342. Stable IDs, UTF-8 authoring
files, the deterministic `T3LNG001` compiler/loader, exact fallback,
placeholder, glyph, and width validation, explicit Japanese/English catalogs,
and a width-stressing pseudo-locale now own the New Classic title,
hierarchical Option UI, Replay browser and save/name flow, Practice setup,
KeyConfig, JoyConfig, controller assignment, and Input Test. Named dynamic
templates are load-time width-checked using declared worst-case values; host
key/controller names remain bounded platform data inside localized rows.
English and Latin fragments use authenticated proportional `MNUFONT.PF`;
Japanese is compiled as CP932 and uses the exact authenticated PC-98
`FONT.ROM`. The separate `T3GTL001` and `T3CTP001` domains now also contain
complete explicit English and Japanese source text for all 761 graphics-text
rows and all 28 scripted dialogue/cutscene/ending programs. The donor
page/palette/input machines and Classic are unchanged. A future general-Unicode
product-shell renderer remains separate from this exact ASCII/CP932 contract.

- Freeze message identifiers and text domains.
- Build the translator-facing authoring, compilation, and validation tools.
- Convert Japanese/English into the new pipeline without visual regressions.
- Add one deliberately width-stressing test locale before calling the format
  extensible.

Gate: completeness, fallback, placeholders, glyph coverage, maximum widths,
deterministic package hashes, and representative screen snapshots.

### 8. HiScore/Replay hubs and online domain seam

Status: production Story leaderboard/replay path complete on Windows, Linux,
and macOS. The
domain/cache boundary was completed in session 189. Session 201 adds the
read-only HiScore hub and source-styled cached online board/detail view.
Session 202 adds asynchronous refresh of the active rank's
nine Story boards. Session 212 adds the first detail action: a replay-backed
entry emits one bounded background download and reports success only after
digest, V14, metadata, and atomic-publication validation. Session 213 adds
Watch: the host revalidates local content-addressed bytes, hands the immutable
document back to OP, crosses a black/silent boundary, rebases the scheduler,
and adopts the existing New Classic playback owner. Session 214 adds the
source-styled Replay hub plus replay-only online and downloaded libraries;
without an active-rank replay record, the direct local route remains. Session
215
completes the four-row HiScore hub: Personal Bests summarizes active-rank local
best score/stage, playtime, and 1CC totals for every unlocked character from a
read-only expanded score snapshot.

Session 258 adds explicit local replay upload. The official Windows origin adds
one Upload Replay row and a retained numbered-slot owner. The selected parsed
document crosses a typed boundary once; DPAPI identity acquisition/enrollment,
canonical V14 encoding, and HTTPS all run on a bounded background worker. The
encoder writes request memory only, and completion is presented on the next
retained UI boundary. Offline and unsupported platforms expose no upload row.

Session 259 adds authenticated `My Records`. The service returns the same
bounded replay-backed category document as the public library, but with every
accepted row owned by the hidden account. Native refresh is memory-only and
starts only for an existing identity or after an accepted explicit upload;
empty or unavailable history exposes no row. Download and Watch reuse the
existing validated handoffs, and public ranking remains best-per-profile.

Session 302 deploys the private pinned verifier plus the public D1/R2 Worker
and proves the complete production transaction with genuine WR `TH3R06.RPY`:
native verification, accepted publication, `T3ONL002` public and My Records
projection, exact R2 download, and the existing client Download/Watch input.
The official desktop builds expose upload against `https://azinn.dev` without
requiring an environment variable. An explicitly empty override remains the
offline switch. Session 303 adds the equivalent bounded libcurl transport and
owner-only atomic identity to Linux and macOS, enabling the same refresh,
upload, My Records, download, Watch, and Forget flows. Reviewed historical-row
data remains a separate follow-up.

- Define leaderboard/replay identifiers, category keys, provenance, and local
  cache documents without selecting a service vendor.
- Add the HiScore hub when Personal Bests or a real online view exists.
  Complete for cached online boards in session 201 and Personal Bests in
  session 215.
- Add the Replay hub when an online/downloaded view exists. Complete in
  session 214; session 258 also admits the real upload capability, and session
  259 adds authenticated accepted-submission history.
- Preserve direct Local views until then.

Gate: offline behavior, replay cross-links, legacy entries without fake replay
IDs, cache corruption/recovery, and no network dependency below the client
service boundary.

The frozen client-side contract is documented in
`NEW_CLASSIC_ONLINE_CACHE.md`. It contains typed content-digest identities,
explicit replay-verified versus curated-legacy provenance, and a bounded
atomic offline cache. It contains no endpoint, credential, provider name, or
network transport. The HiScore owner consumes only already-validated snapshots;
it performs no filesystem or network work on source-time ticks.

The official application uses `https://azinn.dev`. `TH03_ONLINE_ORIGIN` may
override that HTTPS origin for staging or disable online work when explicitly
empty. One bounded worker refreshes complete board snapshots outside source
time and the runner adopts them only between
host iterations. The currently visible HiScore owner remains immutable; fresh
data appears after returning to and re-entering HiScore. Captures and Classic
never enable this path.

### 9. Server and sharing

The deployed trust model, Cloudflare mapping, structured verifier boundary,
and product decisions are in `NEW_CLASSIC_ONLINE_SERVICE_DRAFT.md`.

- Operate the synchronous bounded private `th03rv` verifier keyed by replay,
  content, ruleset, and verifier version; add a queue only if measured load
  requires it.
- Derive score/result/category server-side; enforce size, time, decompression,
  and rate limits.
- Seed the reviewed historical records without weakening offline behavior.

Gate: hostile corpus, reproducible verification, idempotent submission,
versioned categories, replay download integrity, and service-offline client
behavior.

### 10. Display-refresh decoupling and optional interpolation

Display-synchronized repeated presentation can precede interpolation. The
fixed source scheduler and replay input boundary remain untouched. True
interpolation requires a separate presentation specification and is not a
prerequisite for any prior phase.

`DISPLAY_REFRESH_DECOUPLING_DRAFT.md` proposes sample-and-hold output now,
explicit blocked-presentation isolation, no inert FPS/refresh setting, and a
future `Repeat / Smooth` boundary once interpolation creates real additional
display frames.

### 11. Packaging, updates, and netplay follow-ons

The maintained standalone/package flow needs an authenticated, rollback-safe
autoupdater before broad distribution. `T3UPD001` now freezes the bounded,
platform/channel-specific signed metadata and sole monotonic release sequence;
`tools/release/write_update_manifest.ps1` deterministically authors and
self-verifies it from an offline P-256 key. The runtime authentication owner
now reparses the exact bytes and verifies a unique caller-pinned P-256 SPKI
through authenticated Mbed TLS 3.6.7. Production pin material/rotation,
user-data preservation, and opt-out UI remain release infrastructure. Windows
now has a fail-closed
WinHTTP absolute-HTTPS adapter, Linux and macOS have the equivalent bounded
libcurl adapter, and the shared manifest-first transaction authenticates and
selects before requesting the exact signed artifact. Authenticated inert
materialization now binds the platform/channel/sequence and exact artifact
bytes into an atomic, member-verified immutable generation. `T3ACT001` retains
active/pending/previous identities and proves trial health or rollback through
a crash-recoverable alternating store. The packaged bootstrap now owns the
child lifetime, acknowledges health inside `th03-app`, and conservatively
prunes only atomically marked generations outside the activation keep set.
The host install transaction now joins bounded acquisition directly to marked
generation materialization and durable pending nomination for the next
bootstrap launch; it neither replaces nor relaunches the running process.
The release trust seam is default-off and accepts only an all-or-nothing set of
compiled origin, exact platform/channel, installed-sequence floor, and P-256
SPKI pin; runtime settings and environment variables cannot replace it.
The bootstrap now passes its update storage context to every child independently
from a trial-health token, and the background install task deep-copies the
compiled trust record before doing bounded host-time work. A capability-shaped
retained Updates child now starts that worker explicitly or once automatically
per capable process and nominates a valid generation for the next launch. Its
persisted opt-out affects future processes without disabling manual checks.
Production values and signing-key
custody/rotation remain release-owned gates. These owners must not run
inside source-time gameplay or silently alter Classic/New Classic content
identities. See `UPDATE_MANIFEST.md`, `UPDATE_PACKAGE.md`, and
`UPDATE_ACTIVATION.md`.

Completed netplay matches already produce an in-memory, source-compatible V14
document and enter the ordinary Save Replay transaction. Product-level netplay
replay browsing, metadata, upload, and watch flows are the next relatively
high-priority netplay parcel after private-beta lifecycle closure. VS
leaderboards, ratings/Elo, match history, seasons, and tournament policy remain
long-term features after replay-backed evidence and operational moderation are
established.

### 12. Localized game content, replay-export UI, and Arrange experiments

The next nice-to-have sequence is frozen in
`NEW_CLASSIC_NICE_TO_HAVE_ROADMAP.md`. In-game graphics text and scripted
dialogue receive a source-constrained companion to the existing MNUFONT menu
catalog; a desktop replay-export GUI wraps the existing deterministic renderer;
and the first Dopamine Arrange experiment enters through one explicit versioned
rules contract, selected and persisted from the Gameplay Options page. Account
nametags and replay identity are intentionally last and remain decision-gated.

Background-integrated menu headings may eventually become runtime-localized,
but only after clean background reconstruction is available. Classic consumes
none of these extensions.

## Release discipline

Every test-worthy phase ends with:

- focused tests for the changed owner;
- full GCC and MSVC native/SDL matrices;
- the complete V14 corpus and pause/slowdown gates;
- Classic non-regression routes;
- relevant pixel/audio gates with intentional-divergence classification;
- `git diff --check`; and
- a reproducible maintained standalone rebuild for human testing.

Do not add inert placeholder rows, weaken source or replay gates, or let a
New Classic setting silently flow into Classic merely to shorten a parcel.
