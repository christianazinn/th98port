# Playable-port roadmap

## Purpose and authority

This is the implementation order for a source-faithful native Classic
application, not a proposal to generalize the game. The first validation
milestone is a local, direct-launch P1-versus-P2 VS match using the user's
original game data: character selection is explicit at launch, both players
can play, the match reaches its normal result/next-round paths, and every
displayed frame is formed by the native PC-98-style presentation layer. That
vertical slice is a gate, not the final product target. Work continues through
the original title/selection, Story/MAINL/cutscene/staff flow, persistence, and
audio. Replay files remain an internal validation input; a replay menu,
recording UI, and migration UX were deliberately deferred until the Classic
game was otherwise functional. They are now implemented as the separate New
Classic experience and do not alter Classic's source-facing behavior.

Original game content may be used from the developer's explicit local content
root for authentic builds and tests. It remains ignored and external: no
original asset, font, music, capture, or extracted archive byte is committed or
printed into build logs. Public redistribution policy is independent of this
local fidelity path and does not block implementation.

Repository history predating this roadmap accidentally committed local
originals in `8c32d98`. The current tree now ignores them and retains only the
policy README, but a coordinated history rewrite and remote-ref cleanup is
still required before any public release. That process task does not block the
local original-data validation path requested here.

The donor for scene behaviour is the clean, pinned Anniversary tree at oracle_anniversary_th3r02, commit 3e2381d4275be7287f5525de5c95389d492b0a3c.

## Live implementation status

As of 2026-08-11, the direct-VS and audio-enabled Classic paths have these durable
foundations:

- Deterministic gameplay, V13 parsing/playback, and the frozen 19-file corpus
  are green on GCC and MSVC.
- `th03_assets` has strict span-only RGB, PI, CDG/CD2, BFNT, and MRS
  decoders; supplied bytes remain external.
- `th03_present` has the PC-98 indexed Raster/TRAM substrate, Sprite16,
  GRCG/vector/triangle operations, static MAIN setup, entities,
  players/effects/bullets, palette state, and a reviewed caller-owned final
  source-surface compositor. Entering either 200-line mode or 400-line mode
  now preserves the source access-page-0 side effect.
- The app-owned resource library has the exact direct-VS batch and the full
  148-entry MAIN/MAINL metadata scope, including visible Story splash CD2/PI,
  Continue, and Game Over assets and the hidden transition atlas (`NN16.PI`
  and `NNEX.PI`). It performs exact-case, containment, size, fingerprint,
  immutable ownership, and scene-cache checks without adding filesystem
  behavior to assets or presentation.
- The all-character Bomb presenter is integrated and independently reviewed.
  Its split fixed-storage implementation covers all nine visual/palette paths,
  source-private and process-shared state, interrupted-round rearming, and the
  exact common-effect fanout order under strict GCC/MSVC tests.
- The `MainRoundPresenter` composition foundation is integrated. It validates
  and orders the implemented phase views, inserts common effects between player
  overlays and bullets, applies deferred palettes before page publication,
  preserves terminal no-footer behavior, reproduces the 27-sample warning tone
  loop, resets static repeat-round pages/HUD without rebuilding process state,
  and exposes the shown Raster/TRAM source surface in caller storage.
- Both process-selected MAIN background families are integrated. A neutral
  creation/frame/read seam preserves the source RNG schedule without exposing
  particles to core, while one fixed presenter owns the 48/50 live records,
  exact integer vectors, wrap rules, initial-to-steady transition, and
  repeat-round velocity reset.
- Warning/Gauge presentation is integrated. It preserves the source's blocking
  27-tone interstitial, live warning-level gaiji redraws, shared palette
  oscillator, both selected EX.MRS portraits, all nine Gauge-completion
  restores, the full two-playfield hidden-atlas backdrop, and late
  charged/available/hyper bars without retaining gameplay views.
- The complete MAINL-to-MAIN Sprite16 atlas handoff and all-nine Boss
  presentation are integrated. Direct VS now names all six required PI inputs,
  applies exact even-row line-skip staging, and orders the borrowed pre-mutation
  Boss view between late Bombs and Shots. Renderer-private Ellen/Rikako/Chiyuri
  state remains fixed and process-scoped outside core.
- All-nine Extra Attack and Charge Shot presentation is integrated in the
  literal Extra P1, Charge P1, Extra P2, Charge P2 source order. The fixed
  presenter uses only the committed Sprite16 atlas, exact GRCG primitives, and
  borrowed semantic facts; Kana's one source-stale destination byte remains
  process-scoped presentation state across repeat rounds.
- Dynamic HUD and result presentation is integrated. The process-scoped
  presenter covers HUD-start banners and all 512 compatibility reads, static
  HUD mutations, scores and Story extends, combos, defeat/winner banners,
  winner summaries, and opening/closing transitions. `MainRoundPresenter`
  distinguishes terminal no-footer frames from source-valid route-1 repeats
  and preserves the first repeated-round closing frame.
- `th03_app_runtime` now provides the host-independent outer frame boundary.
  Its explicit rational scheduler returns every overdue deadline separately;
  timestamped aggregate source snapshots preserve P1/P2 holds across device
  removal and focus loss; app quit cannot leak into `CoreInput`. The same target
  converts all graphics/text color domains to caller-owned RGBA with arbitrary
  pitch and computes overflow-safe integer 640x400 letterboxing. SDL bindings,
  a source-rate constant, and first-deadline policy are intentionally absent.
- `th03_app_content` authenticates the complete supported `MAIN.EXE` before
  importing its score/pellet masks and result/combo TRAM cells. It also
  validates and owns a complete prepared GAMEFT gaiji bank and resolves both
  ANK and paired full-width JIS cells through an external PC-98 `FONT.ROM`.
  Original bytes remain external and the hermetic default tests use only
  synthetic content.
- `th03_app_main` now owns one stable, nonmovable direct-VS MAIN process. It
  reauthenticates the selected resource batch, decodes and retains every asset,
  performs the six-PI/palette/gaiji handoff, runs source-early background
  initialization, commits static pages/HUD, constructs all presenters, and
  exposes only guarded core lifecycle and composed-surface operations. The
  hermetic transaction suite passes both compilers, and an opt-in local gate
  has constructed, stepped, and composed all 18x18 paletted P1/P2 selections
  from authenticated supplied data on GCC and MSVC.
- `th03_platform_sdl` now owns the pinned SDL 3.4.10 lifetime, resizable
  integer-scaled upload, monotonic timestamps, and duplicate-safe keyboard and
  gamepad event mapping. Default non-SDL builds remain dependency-free.
- `th03-app` now loads the supported external MAIN/MAINL/OP/FONT/resource
  inputs and launches either the complete Classic owner or the live-only
  direct-VS runner. Both paths simulate every due logical deadline without
  collapsing catch-up and present only the newest completed surface. The
  Classic path preserves the donor's full `input_sp` mask separately from P1/P2
  and applies the stock `input_mode_1p_vs_cpu()` merge before Story MAIN. It
  drains semantic audio commands into one deterministic PMD/EFC interval per
  logical sample and has authenticated dummy/software SDL gates on GCC and MSVC,
  including a 7,000-logical-tick automatic-Demo smoke.
- The source-private Classic Story owner now binds the implemented Stage,
  retained MAIN, Win, Continue, Game Over, both interstage cutscenes, and the
  primary ending, staff/verdict, high-score registration, and conditional
  extra ending into one process-lifetime route. It shares
  the exact MAIN
  warning/repeat/terminal driver with Direct VS, performs next-stage hidden-
  page preload at the winner boundary, retains loss state through Continue,
  and directly owns the high-score registration transaction and retained
  Raster handoff into the final extra-ending script. The staff transaction
  authenticates its palette, twelve
  source-ordered CDGs, music, compiled verdict strings, and flake masks; its
  exact phase floors, semantic audio waits, overrun downgrade, two-page verdict,
  and tone-1 completion are covered by a complete synthetic schedule test and
  an opt-in local original-content construction gate.
- Its Stage title/name content now comes from either the full-file-authenticated
  unpacked Japanese `MAINL.EXE` or the untouched stock DIET-packed profile.
  Packed MAINL/OP inputs are authenticated before bounded in-memory DIET 1.45f
  reconstruction, authenticated again against the supported unpacked identity,
  and then passed to the same owning typed importers. No donor string is
  compiled into the port and no scan/fallback accepts arbitrary executables.
- The dependency-free Classic persistence foundation now decodes and encodes
  the exact original four-rank 824-byte `YUME.NEM`, reconstructs the source's
  populated stock board, and preserves its score comparator and uniform-name
  replacement rules. Its separate filesystem boundary reproduces source all-
  rank stock recovery and normalizes in-place section writes into a flushed,
  read-back-validated atomic whole-file replacement. The integrated MAINL
  registration scene now owns the exact retained-Raster composition, source
  input cadence and codec RNG schedule, and final atomic commit without leaking
  Raster or RNG into persistence. The exact eight-byte `YUME.CFG` codec and
  atomic store are also integrated with the OP score and option views;
  localization and optional QoL remain separate product work.

The complete Classic developer slice and the direct-VS slice are now visible
and interactive. The byte-fed PMD/YM2608 engine, source-shaped TH03
transport/SFX/EFC service, deterministic Classic scene-command session, and
bounded SDL delivery are also integrated. Audio is not yet fidelity-complete:
redistributable rhythm-data packaging, aligned PC-98/T98 audio capture,
captured PC-98 page/palette/pixel
traces, and a human two-player run remain acceptance gates. The application
now defaults to the source-relevant nominal PC-9801 24 kHz timing rational,
`526315/9328` Hz, while retaining an explicit host override. This is exact for
the independently reproduced display model, not a zero-tolerance physical
oscillator claim.

| Native concern | Donor authority |
| --- | --- |
| Outer fresh/repeat routing and OP/MAINL handoff | th03/main/entry.cpp:26-83 |
| MAIN startup, asset loads, page setup, sound start, and HUD setup | th03/main/rstart.cpp:63-245 |
| Per-frame MAIN order and the palette/page footer | th03/main/rndloop.cpp:79-290; especially 259-273 |
| Title/configuration/menu selection | th03/op_01.cpp:33-185, th03/op/m_main.cpp:18-181, and th03/op/m_select.cpp:103-190 |
| MAINL stage splash, win, continuation, game-over, and ending routing | th03/mainl/mlentry.cpp:57-199, th03/mainl/screens.cpp:41-169, and th03/mainl/ending.cpp:31-114 |
| Cutscene input and fast-forward semantics | th03/cutscene/cutscene.cpp:97-193 |
| Staff-roll asset loading, flake work, palette, and timing | th03/mainl/stf_drv.cpp:79-248 |
| PMD/SE-facing source behaviour | th03/snd/snd.h and its MAIN/OP/MAINL callers |

The DOS executables are process and memory partitions, not game-design boundaries to reproduce. The native application may be one executable, but must preserve the observable scene transition, asset lifetime, timing, and frame-order behaviour above. It must not simulate old heaps or split itself into three executables merely to mirror DOS layout.

## Current baseline and target ownership

The existing dependency direction is already appropriate:

- th03_app_main, th03_app_runtime, th03_app_content, and th03_app_resources own
  the current application process, platform-neutral tick/input/host-pixel
  policy, authenticated compiled/font content, and external resource cache.
- th03_platform_sdl owns the host window/device event adapter,
  monotonic clock source, and final upload.
- th03_present owns PC-98 raster/TRAM/compositing and observer consumers, and depends on th03_assets and th03_core.
- th03_formats and th03_replay provide V13 bytes and replay sessions when the app needs them.
- th03_audio_pmd owns byte-fed PMD sequencing, source-style stop/play/fade,
  measure/volume/pause/part-mask state, the exact one-slot TH03 SFX scheduler,
  bounded external EFC parsing and Timer-A/FM6 playback, supplied rhythm-ROM or
  decoded six-voice rhythm access, YM2608 synthesis, register tracing,
  deterministic native PCM, and the
  fixed 166,400-to-48,000 Hz integer resampler. The Classic app session owns
  authenticated file-backed cue binding; the thin SDL executable owns final
  delivery and the optional exact-size `--rhythm-rom` or authenticated private
  `--rhythm-pcm` binding. Gameplay command
  emission and gain policy are integrated; redistributable replacement-rhythm
  packaging remains release work.
- th03_platform_sdl owns the bounded default-device playback stream, canonical
  source-format queue, final-output overrun telemetry, and a subordinate shared
  SDL lifetime lease. It cannot advance or backpressure simulation.

th03_core is the deterministic runner. It owns game state, fixed-point rules, RNG, and neutral borrowed observer calls; it must never gain a window, a renderer, filesystem access, an SDL type, audio-device state, or an asset pathname. th03_replay owns replay input-session semantics; th03_formats owns V13 parsing/serialization and byte spans, not file I/O. The app must use V13 rather than invent a private recording format.

th03_assets remains a collection of pure decoders for supplied byte spans (RGB, PI, CDG/CD2, BFNT, and MRS). It does not know an install directory, DAT filename, cache, or scene. th03_present owns PC-98-flavoured data and drawing state: Raster, palette state, TRAM, planar/GRCG operations, cel/BFNT staging, and presentation-side observer state. It is the right home for a MainRoundPresenter composition object; it is not a host windowing library.

| Target | Owns | Must not own |
| --- | --- | --- |
| th03_platform_sdl | SDL lifetime, event translation, monotonic timestamp acquisition, a 640x400 final-surface upload, and device PCM output later | Gameplay, scene policy, decoded game assets, tick/input semantics, palette/raster rules, or source text |
| th03_app_main / neutral app policy | Command line parsing, external content transaction, resource manifest/cache, selection/profile settings, process construction, and platform-neutral tick/input/runner policy | Decoder internals, PC-98 raster primitives, deterministic state, SDL types, or audio synthesis |
| th03-app executable | Thin composition of th03_app_main with th03_platform_sdl, source-style default bindings, and app-level quit policy | A second gameplay/render model, asset decoding, cadence constants, or audio synthesis |
| th03_present | MainRoundPresenter, source-ordered observer consumers, visible MAIN composition from Raster plus TRAM, and scene-local present state | OS input, elapsed wall-clock policy, filesystem reads, or gameplay mutation |
| th03_audio_pmd | Supplied PMD song/rhythm bytes, PMD/OPNA scheduling and synthesis, register tracing, offline native PCM | SDL devices, UI settings storage, gameplay state, or file paths |

The current libraries contain a tested process coordinator over static setup,
all nine Bomb, Boss, Extra, and Charge paths, entities, players/bullets,
effects, palette/raster, sprites, text, both MAIN backgrounds, and the final Raster-plus-TRAM source
surface. Dynamic HUD/result passes are now source-ordered as well. This closes
the synthetic source-derived MAIN composition inventory for direct VS; it does
not replace the external real-asset startup gate or captured PC-98 frame
comparison. Neutral observer lifecycle events remain facts, never render state
in the core.

## Source/application scene boundary

The native application owns the source's process-to-process handoffs as ordinary scene transitions. Game remains a MAIN match engine, and returns only the same game-semantic outcomes it does today. The app translates those outcomes without placing a menu, campaign, or persistent profile into core.

| App scene | Source counterpart | Required transfer at its boundary |
| --- | --- | --- |
| TitleScene / SelectScene | OP title, option, character and mode selection | App-owned settings and selected mode/characters; load only OP/select assets needed for the current screen. |
| MatchScene | MAIN startup and rndloop | Load and authenticate the selected process inputs, construct one `MainProcess`, and drive only its guarded lifecycle/surface API until the source changes process/scene. |
| StageTransitionScene | MAINL splash, win, retry/continue/game-over routing | Carry only source-semantic campaign/resident values in an app-owned ClassicSessionState; decide the next GameInit or next scene here. |
| CutsceneScene | cutscene executable | Own script lifetime, pacing, and input conversion; retain the donor's first-page/text and fast-forward behaviour. |
| StaffScene | staff-roll driver | Own sequence orchestration; use presentation's staff flake/raster primitives and supplied CDG/palette assets. |

ClassicSessionState is intentionally an app value, not an alias for DeterministicState. It may hold selected characters, round/story progress, and later user-profile values needed at source scene handoffs. It must be constructed and validated explicitly at every MAIN entry. This makes the native boundary inspectable while avoiding a false emulation of resident DOS memory.

## Shortest path: visual direct-launch VS

### 1. Freeze the external-data contract

Add an app-owned resource manager before adding a generic virtual filesystem. It accepts an explicit content-root argument, reads only files under that root, and exposes named immutable byte blobs to the app. A checked-in manifest identifies each required original filename/logical role, decoder kind, expected size or digest, and scene usage. Original files, extracted DAT contents, PI/BFNT/CDG assets, music, and rhythm ROMs remain external user-supplied data; do not convert them to committed PNG/WAV replacements.

The manager performs pathname selection, reading, caching, and authentication;
the MAIN compiled-content importer and external font provider supply the other
authenticated inputs. `MainProcess` transactionally decodes and binds those
inputs into stable process-owned storage. Missing, wrong-sized, or malformed
content produces a named construction failure rather than a substitute asset.
This is slimmer and more faithful than teaching every decoder a filesystem API.

Acceptance gate VS-0: a documented external content root with a manifest can load all assets needed for one selected P1/P2 pair, decode them through the current library APIs, and reject a deliberately wrong file deterministically. The manifest has a source-file/role inventory sufficient to extend the same path to all nine characters.

### 2. Complete one source-ordered MAIN composition object

`MainRoundPresenter` now lives in th03_present, not core or the host.
`MainProcess` owns its MAIN Raster, Tram, palette/static state, decoded backing
assets, and specialised presenters at stable process addresses. It passes the
coordinator as the transient observer for every game call and exposes the
current visible PC-98 surface without exposing mutable core/presentation state.
No observer pointer is retained in core state.

Its implementation must be driven by the donor's actual order, rather than a convenient render pass:

1. Perform each observer visual action in the P1-to-P2/source callback order. Active Bomb work consumes BombFrameStep before its character-specific visual work; existing common Bomb effects retain their later source position.
2. Render Boss, shots, enemies, fireball/hit-circle, Extras/Charge, player overlays, common effects, bullets, and result/defeat/warning/HUD work in their observed source positions.
3. On MainFrameFooterReached, complete the same visible footer boundary as rndloop.cpp:259-273: apply a pending palette before the displayed-page handoff/swap-and-clear operation. A warning-start, replay-pause, or terminal early-return frame has no invented footer.
4. Convert the actual displayed Raster page plus visible TRAM/gaiji state to one 640x400 indexed/palette-resolved source surface. Only the platform adapter expands that source surface to host RGBA pixels.

This is a hard blocker, not an opportunity to add a retained-mode scene graph. Every missing source visual route is an explicit task with a donor location, asset role, and frame-trace test. Character constants and source conditions must be named from established semantics, never left as unexplained character or phase literals.

Acceptance gate VS-1: focused present tests cover startup, every MAIN footer/deferral class, both page handoffs, palette-before-page ordering, and a representative frame from every character/Boss/Extra/Charge/HUD family. They run with synthetic legal assets on GCC and MSVC. The existing core observer null/discard/record invariance and frozen cross-compiler V13 corpus remain green; attaching MainRoundPresenter cannot change a canonical outcome.

### 3. Add the thin host adapter and direct launcher

The integrated th03_platform_sdl boundary pins [SDL 3.4.10](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.10) (release-3.4.10, source archive SHA-256 12b34280415ec8418c864408b93d008a20a6530687ee613d60bfbd20411f2785). It is a narrowly scoped convenience and compatibility layer, not a new game engine. SDL documents creation of a window and default renderer via [SDL_CreateWindowAndRenderer](https://wiki.libsdl.org/SDL3/SDL_CreateWindowAndRenderer).

The initial host policy is deliberately plain:

- Create a resizable 640x400 logical game window. Use SDL's documented [logical-presentation facility](https://wiki.libsdl.org/SDL3/SDL_SetRenderLogicalPresentation) with integer scaling where the selected renderer supports it; retain black borders instead of stretching the 8:5 source raster. Fullscreen is a later host preference, never a new render path.
- Keep one persistent 640x400 streaming RGBA texture. Lock/write/unlock it once per displayed source frame after the CPU-side PC-98 composite. SDL specifically recommends a streaming texture and locking for frequently updated texture data rather than relying on [SDL_UpdateTexture](https://wiki.libsdl.org/SDL3/SDL_UpdateTexture). There is no texture atlas, per-sprite GPU scene, shader palette effect, or intermediate high-resolution framebuffer.
- Pump host events, map physical controls into a small app-owned input value, and latch exactly one CoreInput sample at each scheduled logical tick. SDL events and scancodes never cross into core/present. P1 and P2 keyboard defaults plus controller mapping are app configuration; UI navigation and Escape are separate app-scene commands, not magic gameplay mutation.
- Run gameplay from a fixed source-tick scheduler, never delta_seconds. The
  application default is the nominal PC-9801 24 kHz display rational:
  21,052,600 / (8 * 106 * 440) = `526315/9328` Hz. The first source sample is
  immediately due. Host VSync only paces presentation; it never creates game
  frames. Deterministic smoke/replay callers preserve every overdue deadline;
  interactive source-style `frame_delay(1)` policy rebases after elapsed
  retraces rather than repaying a backlog. Present the newest completed surface
  and add persistent-overload reporting in developer mode.

The first launcher is `th03-app --data <root> --main <MAIN.EXE> --font
<FONT.ROM> --vs --p1 <character> --p2 <character>`, with optional
`--tick-rate <ticks>/<seconds>`. It loads the authenticated
inputs, creates `MainProcess` from `MainDirectVsConfig`, and drives only that
object's guarded step/repeat/terminal/surface API; it has no TitleScene
requirement. The first launch allows selecting each of the nine character
identities and supported VS mode parameters explicitly. Replay
playback/recording may be wired at this input boundary later through
`th03_formats` and `th03_replay`; it must not delay direct VS.
Kotohime is not silently labeled pixel-authentic in her undefined late Bomb
branch. The public launcher uses the documented adjacent-Kana deterministic
normalization `{2*fade, 2*fade, fade}`; it neither reads host-stack residue nor
requires a developer flag. The same binary audit also corrects Kana's generated
green store to reuse the doubled byte.

Acceptance gate VS-2: with external data, all legal direct-launch character pairs enter a match, render a stable 640x400 source surface, accept both players' controls, reach normal round/result routing, and return cleanly to the launcher. For a fixed scripted input, the direct-launch match's final core state/canonical outcome equals headless execution. The same check is performed with a null observer, discard observer, and the full presenter.

Acceptance gate VS-3: create an external PC-98 visual trace for a small set of fixed scenarios (fresh startup, normal play, Bomb, warning, defeat, result, and next round). Store source identity, frame numbers, palette/page metadata, and checksums in the test contract; keep any copyrighted source assets or captures outside the repository. The native composed source surface and timing/page trace match those samples frame for frame. A human two-controller smoke test is useful only after this gate; it cannot replace it.

## Blockers and work that can run now

| Item | Blocks visual VS? | Why / completion condition |
| --- | --- | --- |
| MAIN source-order coverage | Integrated | Every audited direct-VS MAIN family now has a synchronous consumer; real-asset startup and captured PC-98 visual traces remain the fidelity gates. |
| MAIN process construction over the checked external manifest | Integrated | The stable owner reauthenticates and binds the selected 12–17 physical resources plus MAIN/FONT content; hermetic failures and all 324 local paletted selection pairs pass on GCC/MSVC. |
| Fixed tick plus input-latch boundary | Integrated, telemetry open | SDL nanosecond events feed the rational scheduler; nominal PC-9801 24 kHz cadence and immediate first phase are the application default, deterministic callers preserve overdue boundaries, and interactive callers use source-style rebasing. Add persistent-overload developer reporting without changing this API. |
| Final Raster/TRAM-to-source-surface compositor | Integrated | The allocation-free shown-page compositor is complete; the host must consume it rather than redraw approximate HUD/text. |
| SDL window/upload/input adapter | Integrated | Pinned optional builds, dummy/software tests, input mapping, window lifetime, integer upload, and byte-silent public startup are green on GCC/MSVC. |
| V13 conversion, replay browser, and recording UI | Integrated in New Classic | The app records canonical V13, owns bounded slot storage/browser/save/name/checkpoint UI, exposes final Splits/Clears/Timers metadata, and plays through the shared MAIN/MAINL owners. Classic remains free of replay UI. |
| OP menus, Story, MAINL, cutscenes, staff | Integrated | The source-private OP owner binds title, retained Options/Music Room, read-only HiScore, Story/all-VS selection, Story, MAINL/MAIN VS, winner return, both interstages, primary and conditional extra endings, staff/verdict, score persistence, registration, and the exact four-recipe automatic Demo loop. No external OP route or replay UI is required. |
| Audio | Engine, TH03 transport/SFX/EFC service, fixed resampler, Classic session, and SDL endpoint integrated | The launcher applies source-semantic scene commands, advances one deterministic audio interval per logical sample, and treats device loss as delivery-only. MAINL selects and loads the exact character/decisive song and effect bank; fresh/repeat/terminal MAIN publishes source Reset/Play/Stop lifecycle commands. All 61 ordinary live-MAIN donor SFX rows feed the source `snd_se_update()` boundary, including forced warning effect 19, and live pause/resume emits effect 21. Authenticated gates cover all 40 semantic cue identities, 22 effects, all 21 songs, and independent FM1-6/SSG1-2/rhythm stems. Exact-size `--rhythm-rom` and authenticated decoded `--rhythm-pcm` paths reach the engine; replacement-data packaging, aligned reference capture, shipped-TSR traces, and physical-device QA remain release work. |
| Persistent score/configuration UI and localization/QoL | Original UI plus New Classic score/statistics, KeyConfig, and Options integrated; localization pending | Classic retains the exact original score/config codecs and UI. New Classic owns the Replay Patch's versioned 4-rank x 9-character score store, legacy-by-value migration, aggregate/TOTAL and character pages, live play-time/1CC/continue accounting, bounded 26-scancode store, Story movement override, per-player Autofire, independent SFX, and exact warning-flash reduction. English still requires the authenticated translated overlay. |

The following parcels can proceed now, provided they do not claim readiness for VS or change core semantics:

| Parallel parcel | Target / boundary | Initial acceptance gate |
| --- | --- | --- |
| External-data manifest and scene-scoped resource manager | th03-app support code only | Unit tests with a temporary synthetic content root; no I/O added to th03_assets. |
| Asset inventory and exact source role table for MAIN/OP/MAINL | th03-port/docs plus app manifest data | Every role names its donor loader/caller and decoder; unresolved names remain explicit. |
| Final PC-98 source-surface compositor | th03_present | Synthetic Raster/TRAM/palette fixtures prove page selection, palette resolution, gaiji placement, and 640x400 output. |
| Selection/title asset mapping and presenter primitives | th03_present plus app-owned scene code | Source table and synthetic/unit render tests; no dependency on the MAIN compositor being finished. |
| Cutscene Raster/resource executor | `th03_present` plus app-owned scene content | Execute the completed supplied-byte VM's typed operations against the Master/upstream two-page model; retain script/resource ownership outside presentation and keep audio operations semantic. |
| Gameplay SFX emitters | Integrated | Source-ordered neutral facts for all 61 ordinary live-MAIN donor rows feed the existing deterministic session without gameplay or device-clock feedback. |

Avoid creating a placeholder th03-app which owns a second renderer or game state. The useful early app work is resource, input, clock, and construction seams with tests; the only gameplay frame producer remains th03_core plus its borrowed presentation observer.

## After VS: source scene order

Once VS-3 holds, add scenes in donor order rather than building a broad launcher framework.

1. MAINL routing and Story progression are integrated. Stage splash, fresh MAIN,
   winner, Continue, Game Over, interstage/ending cutscenes, and staff/verdict
   are integrated in the owning Classic session. The exact original score
   codec, qualification/name mutation rules, transactional whole-file store,
   authenticated registration Raster transaction, and conditional
   extra-ending cutscene are integrated. The source-traced owner reaches each
   next-stage, continue, game-over, registration, extra-ending, and exit branch
   with the corresponding retained Raster/palette/timing and
   resident/persistence state. The production original-resource gate now
   proves a fresh 1CC, used-credit clear, Continue Yes/No, exhausted-credit
   Game Over, MAIN route 0, atomic registration read-back, and fresh-title Quit
   on both compilers; it substitutes only the terminal after one real MAIN
   process frame and is not a natural-gameplay or replay parity claim.
2. OP title and selection are integrated. The source's Story, VS, Music Room,
   registration view, Options, Quit, and automatic Demo paths all use the
   authenticated BFNT/CDG/PI presentation and typed process handoffs. The SDL
   launcher now composes this owner directly rather than adding a desktop UI or
   rewriting source asset strings.
3. Cutscenes and staff. The owning typed CutsceneScene and complete normal-speed
   staff/verdict Raster transaction are integrated. Replay Patch's optional
   score-file-unlocked held-Z staff pacing remains coupled to persistence. The
   remaining fidelity gate is captured PC-98 first/last-page, skip/fast-forward,
   staff CDG/palette/flake,
   and return-route comparison; conveniences such as rewind, text log, and
   language selection remain later profile work.
4. Profile and replay UX. New Classic now records and plays only V13 at the app
   input boundary, owns its browser/save/pause/Practice routes, and persists a
   versioned bounded SDL-scancode profile without extending a historical save
   block. Format migration remains a tool concern. Remaining Replay Patch work
   stays in the New Classic adapters/presentation policy rather than forking
   deterministic Classic gameplay.

## Audio plan

Audio is a separate fidelity project, not a WAV substitution. The first inventory is the donor's song/SE calls and the externally supplied .M, .EFC, and any rhythm data they require. It must identify source command ordering, load/stop/measure waits, and mute/volume behaviour before choosing a native API. Game timing must never wait on a host audio callback or device state.

The implemented foundation is a byte-fed th03_audio_pmd library adapted from
the permissively licensed 98fmplayer PMD sequencer with ymfm providing YM2608
synthesis. Its port-owned façade implements source-style transport, signed
fades, queries, pause/mask state, the exact one-slot SFX arbitration table, and
validated external EFC playback on the Timer-A-driven FM6 path. It produces
deterministic native PCM and knows no files, windows, or SDL types. The neutral
audio library converts the explicit 166,400 Hz policy stream to canonical 48 kHz
with a fixed integer 15/52 polyphase FIR. `ClassicAudioSession` now authenticates
the external PMD/EFC inventory, maps every existing semantic scene command, and
advances one bounded rational PCM block per logical tick. `th03_platform_sdl`
supplies the bounded playback stream, and the Classic launcher submits each
completed block after applying that sample's commands. Initial or later device
failure stops only submission while deterministic PMD state continues; device
state never changes the source FM-capability decision.
Gameplay song selection, MAIN process lifecycle, every ordinary live MAIN SFX
emitter, and source-blocking live pause/resume with effect 21 are integrated.
Bomb-start effects 17/18 now originate in their exact pre-dispatch callbacks.
Staff and terminal registration preserve the driver's exact volume-255 wait
when BGM is active, while Music Off bypasses the donor helper's unsatisfiable
unguarded driver loop.
SDL's [audio-device stream API](https://wiki.libsdl.org/SDL3/SDL_OpenAudioDeviceStream)
is an appropriate device adapter because it keeps device management outside
the synthesizer. The app owns user gain/mute settings, while the audio engine
continues its source command timeline even if final output is muted unless
donor evidence establishes different semantics.

[ymfm](https://github.com/aaronsgiles/ymfm) has no release tag to treat as a
default pin. The exact 98fmplayer and ymfm commits, vendoring strategy, and
license obligations are now locked. The corpus proves that 20/21 songs use all
six ADPCM-A rhythm voices. The Classic launcher accepts either an exact-size
`--rhythm-rom` input or an authenticated private decoded six-voice
`--rhythm-pcm` profile. With neither, it suppresses ADPCM-A register playback;
zero-filled ROM data is not silence because zero nibbles decode audibly.
The remaining load-bearing unknowns are shipped-TSR register parity for EFC
start/tick/replacement/restoration and the implemented fade/pause/mask
operations, the authentic OPNA clock, and ROM packaging. These are
service/fidelity gates, not blockers for semantic scene command ordering.
Required follow-up gates are offline command/register or PCM traces for known
assets, deterministic muted/unmuted scheduling tests, then physical-device
smoke tests on the supported host matrix.

## Compatibility and size constraints

The initial host support floor should be Windows 10/11 x64 and current Wine, with both MSVC and MinGW builds kept healthy. This is intentionally a small, testable floor; 32-bit or older-Windows support is a later explicit toolchain and QA decision, not an accidental promise. SDL's renderer backend selection remains behind th03_platform_sdl, so the game is not coupled to OpenGL, Vulkan, Direct3D, or a particular GPU.

Preserve the 640x400 indexed/palette-oriented source model until the final host upload. Prefer one CPU composite and one streaming texture over a general-purpose renderer, ECS, scene graph, GUI framework, asset database, image-conversion pipeline, background job system, or duplicate gameplay model. Load assets by scene and discard them at scene transitions. Keep platform-only dependencies out of the deterministic, replay, format, asset, and presentation contracts. These constraints keep the native port small while retaining enough separation to add non-faithful replay/QoL work later without contaminating the faithful match path.

## Readiness checklist

A branch may describe a playable visual VS build only when all of the following are true:

- VS-0 through VS-3 have passed on the pinned external-data/source-trace set.
- All existing core, format, replay, asset, and presentation tests still pass in strict GCC and MSVC configurations; the frozen V13 corpus still agrees across compilers.
- The app has no private gameplay simulation, the core has no platform/render state, and th03_assets has no filesystem API.
- The presentation footer applies palette/page state exactly at the source boundary and deferral/terminal frames remain silent where the donor is silent.
- The build ships no original game art, music, or data, and produces a clear missing-content diagnostic.

At that point the next serial decision is whether to complete source scenes or begin the audio fidelity gate. Both can proceed in parallel after the direct VS compositor and resource contract are stable; neither should be merged into the deterministic gameplay core.
