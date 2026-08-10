# Playable-port roadmap

## Purpose and authority

This is the implementation order for the smallest source-faithful native application, not a proposal to generalize the game. The first milestone is a local, direct-launch P1-versus-P2 VS match using the user's original game data: character selection is explicit at launch, both players can play, the match reaches its normal result/next-round paths, and every displayed frame is formed by the native PC-98-style presentation layer. Menus, Story, cutscenes, staff roll, persistent scores, replay UI, and audio are later milestones. A silent visual VS vertical slice is useful; a fully faithful playable release is not complete until audio is added.

The donor for scene behaviour is the clean, pinned Anniversary tree at oracle_anniversary_th3r02, commit 3e2381d4275be7287f5525de5c95389d492b0a3c.

## Live implementation status

As of 2026-08-10, the direct-VS path has these durable foundations:

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
  96-entry selected-character metadata scope required by the adjacent MAINL
  transition (`NN16.PI` and `NNEX.PI` included). It performs exact-case,
  containment, size, fingerprint, immutable ownership, and scene-cache checks
  without adding filesystem behavior to assets or presentation.
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

The shortest remaining chain to a visible interactive build is: fill Boss,
Extra/Charge, and HUD/result gaps in the existing
composition owner; add the fixed 56.423 Hz input scheduler; then add the thin
SDL upload and input shell. PC-98 visual traces remain the release authority
after the synthetic cross-compiler gates.

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

- th03-app (proposed) owns the application.
- th03_platform_sdl (proposed) owns the host window, input, clock, and final upload.
- th03_present owns PC-98 raster/TRAM/compositing and observer consumers, and depends on th03_assets and th03_core.
- th03_formats and th03_replay provide V13 bytes and replay sessions when the app needs them.
- th03_audio_pmd (proposed, later) owns supplied-data synthesis and sends only PCM to the platform adapter.

th03_core is the deterministic runner. It owns game state, fixed-point rules, RNG, and neutral borrowed observer calls; it must never gain a window, a renderer, filesystem access, an SDL type, audio-device state, or an asset pathname. th03_replay owns replay input-session semantics; th03_formats owns V13 parsing/serialization and byte spans, not file I/O. The app must use V13 rather than invent a private recording format.

th03_assets remains a collection of pure decoders for supplied byte spans (RGB, PI, CDG/CD2, BFNT, and MRS). It does not know an install directory, DAT filename, cache, or scene. th03_present owns PC-98-flavoured data and drawing state: Raster, palette state, TRAM, planar/GRCG operations, cel/BFNT staging, and presentation-side observer state. It is the right home for a MainRoundPresenter composition object; it is not a host windowing library.

| Proposed target | Owns | Must not own |
| --- | --- | --- |
| th03_platform_sdl | SDL lifetime, event pump, physical input state, logical-tick scheduler, a 640x400 final-surface upload, and device PCM output later | Gameplay, scene policy, decoded game assets, palette/raster rules, or source text |
| th03-app | Command line, external content root, resource manifest/cache, scene routing, selection/profile settings, construction of GameInit, and app-level quit/pause policy | Decoder internals, PC-98 raster primitives, deterministic state, or audio synthesis |
| th03_present | MainRoundPresenter, source-ordered observer consumers, visible MAIN composition from Raster plus TRAM, and scene-local present state | OS input, elapsed wall-clock policy, filesystem reads, or gameplay mutation |
| th03_audio_pmd (later) | Supplied music/SE bytes, PMD/OPNA scheduling and synthesis, mix gain, offline PCM | SDL devices, UI settings storage, gameplay state, or file paths |

The current libraries contain a tested process coordinator over static setup,
all nine Bomb paths, entities, players/bullets, effects, palette/raster,
sprites, text, both MAIN backgrounds, and the final Raster-plus-TRAM source
surface. It deliberately names every unimplemented pass, so it is not yet
proof that every real MAIN
frame is complete. Source-order coverage is still required for Boss and
Extra/Charge paths and dynamic HUD/result/warning overlays. The neutral
observer lifecycle events are a seam for that work; they are not render state
in the core.

## Source/application scene boundary

The native application owns the source's process-to-process handoffs as ordinary scene transitions. Game remains a MAIN match engine, and returns only the same game-semantic outcomes it does today. The app translates those outcomes without placing a menu, campaign, or persistent profile into core.

| App scene | Source counterpart | Required transfer at its boundary |
| --- | --- | --- |
| TitleScene / SelectScene | OP title, option, character and mode selection | App-owned settings and selected mode/characters; load only OP/select assets needed for the current screen. |
| MatchScene | MAIN startup and rndloop | Build a fresh GameInit and GameContent; attach the per-call MainRoundPresenter observer; release match assets when the source would change process/scene. |
| StageTransitionScene | MAINL splash, win, retry/continue/game-over routing | Carry only source-semantic campaign/resident values in an app-owned ClassicSessionState; decide the next GameInit or next scene here. |
| CutsceneScene | cutscene executable | Own script lifetime, pacing, and input conversion; retain the donor's first-page/text and fast-forward behaviour. |
| StaffScene | staff-roll driver | Own sequence orchestration; use presentation's staff flake/raster primitives and supplied CDG/palette assets. |

ClassicSessionState is intentionally an app value, not an alias for DeterministicState. It may hold selected characters, round/story progress, and later user-profile values needed at source scene handoffs. It must be constructed and validated explicitly at every MAIN entry. This makes the native boundary inspectable while avoiding a false emulation of resident DOS memory.

## Shortest path: visual direct-launch VS

### 1. Freeze the external-data contract

Add an app-owned resource manager before adding a generic virtual filesystem. It accepts an explicit content-root argument, reads only files under that root, and exposes named immutable byte blobs to the app. A checked-in manifest identifies each required original filename/logical role, decoder kind, expected size or digest, and scene usage. Original files, extracted DAT contents, PI/BFNT/CDG assets, music, and rhythm ROMs remain external user-supplied data; do not convert them to committed PNG/WAV replacements.

The manager performs pathname selection, reading, and caching; it then hands byte spans to th03_assets and the existing ENEDAT parser. Decoded objects are immutable and cached by scene scope. MainRoundAssets in presentation receives already-decoded character sheets, palettes, sprites, and source-specific tables. Missing, wrong-sized, or malformed content must produce a named load failure rather than a substitute asset. This is slimmer and more faithful than teaching every decoder a filesystem API.

Acceptance gate VS-0: a documented external content root with a manifest can load all assets needed for one selected P1/P2 pair, decode them through the current library APIs, and reject a deliberately wrong file deterministically. The manifest has a source-file/role inventory sufficient to extend the same path to all nine characters.

### 2. Complete one source-ordered MAIN composition object

`MainRoundPresenter` now lives in th03_present, not core or the host. The current
foundation coordinates caller-owned MAIN Raster, Tram, palette/static state,
and specialised presenters at stable process addresses. The future app creates
that process bundle for a match, passes the coordinator as the transient
observer for each game call, and asks it for the current visible PC-98 surface.
No observer pointer is retained in core state.

Its implementation must be driven by the donor's actual order, rather than a convenient render pass:

1. Perform each observer visual action in the P1-to-P2/source callback order. Active Bomb work consumes BombFrameStep before its character-specific visual work; existing common Bomb effects retain their later source position.
2. Render Boss, shots, enemies, fireball/hit-circle, Extras/Charge, player overlays, common effects, bullets, and result/defeat/warning/HUD work in their observed source positions.
3. On MainFrameFooterReached, complete the same visible footer boundary as rndloop.cpp:259-273: apply a pending palette before the displayed-page handoff/swap-and-clear operation. A warning-start, replay-pause, or terminal early-return frame has no invented footer.
4. Convert the actual displayed Raster page plus visible TRAM/gaiji state to one 640x400 indexed/palette-resolved source surface. Only the platform adapter expands that source surface to host RGBA pixels.

This is a hard blocker, not an opportunity to add a retained-mode scene graph. Every missing source visual route is an explicit task with a donor location, asset role, and frame-trace test. Character constants and source conditions must be named from established semantics, never left as unexplained character or phase literals.

Acceptance gate VS-1: focused present tests cover startup, every MAIN footer/deferral class, both page handoffs, palette-before-page ordering, and a representative frame from every character/Boss/Extra/Charge/HUD family. They run with synthetic legal assets on GCC and MSVC. The existing core observer null/discard/record invariance and frozen cross-compiler V13 corpus remain green; attaching MainRoundPresenter cannot change a canonical outcome.

### 3. Add the thin host adapter and direct launcher

Use a new th03_platform_sdl boundary with one pinned SDL version, initially [SDL 3.4.10](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.10) (release-3.4.10, source archive SHA-256 12b34280415ec8418c864408b93d008a20a6530687ee613d60bfbd20411f2785). It is a narrowly scoped convenience and compatibility layer, not a new game engine. SDL documents creation of a window and default renderer via [SDL_CreateWindowAndRenderer](https://wiki.libsdl.org/SDL3/SDL_CreateWindowAndRenderer).

The initial host policy is deliberately plain:

- Create a resizable 640x400 logical game window. Use SDL's documented [logical-presentation facility](https://wiki.libsdl.org/SDL3/SDL_SetRenderLogicalPresentation) with integer scaling where the selected renderer supports it; retain black borders instead of stretching the 8:5 source raster. Fullscreen is a later host preference, never a new render path.
- Keep one persistent 640x400 streaming RGBA texture. Lock/write/unlock it once per displayed source frame after the CPU-side PC-98 composite. SDL specifically recommends a streaming texture and locking for frequently updated texture data rather than relying on [SDL_UpdateTexture](https://wiki.libsdl.org/SDL3/SDL_UpdateTexture). There is no texture atlas, per-sprite GPU scene, shader palette effect, or intermediate high-resolution framebuffer.
- Pump host events, map physical controls into a small app-owned input value, and latch exactly one CoreInput sample at each scheduled logical tick. SDL events and scancodes never cross into core/present. P1 and P2 keyboard defaults plus controller mapping are app configuration; UI navigation and Escape are separate app-scene commands, not magic gameplay mutation.
- Run gameplay from a fixed source-tick scheduler, never delta_seconds. Establish the exact cadence from source/instrumented PC-98 evidence before declaring VS timing complete. Host VSync only paces presentation; it never creates, skips, or combines game frames. If a host is late, process each overdue logical boundary with a separately latched input sample, present the newest completed surface, and report persistent overload in developer mode. Do not silently drop core ticks or tie simulation count to monitor refresh.

The first launcher shape is th03-app --data <root> --vs --p1 <character> --p2 <character>. It constructs GameContent, fresh GameInit, the resource manager, and MainRoundPresenter; it has no TitleScene requirement. The first launch allows selecting each of the nine character identities and supported VS mode parameters explicitly. Replay playback/recording may be wired at this input boundary later through th03_formats and th03_replay; it must not delay direct VS.

Acceptance gate VS-2: with external data, all legal direct-launch character pairs enter a match, render a stable 640x400 source surface, accept both players' controls, reach normal round/result routing, and return cleanly to the launcher. For a fixed scripted input, the direct-launch match's final core state/canonical outcome equals headless execution. The same check is performed with a null observer, discard observer, and the full presenter.

Acceptance gate VS-3: create an external PC-98 visual trace for a small set of fixed scenarios (fresh startup, normal play, Bomb, warning, defeat, result, and next round). Store source identity, frame numbers, palette/page metadata, and checksums in the test contract; keep any copyrighted source assets or captures outside the repository. The native composed source surface and timing/page trace match those samples frame for frame. A human two-controller smoke test is useful only after this gate; it cannot replace it.

## Blockers and work that can run now

| Item | Blocks visual VS? | Why / completion condition |
| --- | --- | --- |
| Remaining MAIN source-order coverage | Yes | The coordinator, footer, repeat reset, and existing passes are integrated; finish every named missing family before calling the image complete. |
| MainRoundAssets construction over the checked external manifest | Yes | The 96-role manifest/cache is frozen, but the app still must decode and bind the selected process bundle without substitute data. |
| Fixed tick plus input-latch boundary | Yes | A window whose simulation follows host refresh is not a source-faithful match. Finish fake-clock scheduler tests and establish source cadence. |
| Final Raster/TRAM-to-source-surface compositor | Integrated | The allocation-free shown-page compositor is complete; the host must consume it rather than redraw approximate HUD/text. |
| SDL window/upload/input adapter | Yes | Required to make the completed composition playable, but its first test can use a synthetic surface. |
| V13 conversion, replay browser, and recording UI | No | Core and formats already provide the canonical format path. Add application recording only after the host latches input correctly. |
| OP menus, Story, MAINL, cutscenes, staff | No | They are scene work after direct VS; do not make them a prerequisite for validating MAIN. |
| Audio | No for the explicitly marked silent visual vertical slice; yes for a fully faithful release | Start source/data investigation now, but do not block visible MAIN on an invented WAV backend. |
| Persistent score/configuration UI and localization/QoL | No | Keep it outside the first faithful path and behind an app-owned profile/catalog seam. |

The following parcels are safe to begin before complete presentation, provided they do not claim readiness for VS or change core semantics:

| Parallel parcel | Target / boundary | Initial acceptance gate |
| --- | --- | --- |
| External-data manifest and scene-scoped resource manager | th03-app support code only | Unit tests with a temporary synthetic content root; no I/O added to th03_assets. |
| Asset inventory and exact source role table for MAIN/OP/MAINL | th03-port/docs plus app manifest data | Every role names its donor loader/caller and decoder; unresolved names remain explicit. |
| SDL host shell | th03_platform_sdl | A fake 640x400 surface is letterboxed/scaled without changing its pixels; fake-clock and input-latch tests do not include core. |
| Final PC-98 source-surface compositor | th03_present | Synthetic Raster/TRAM/palette fixtures prove page selection, palette resolution, gaiji placement, and 640x400 output. |
| Selection/title asset mapping and presenter primitives | th03_present plus app-owned scene code | Source table and synthetic/unit render tests; no dependency on the MAIN compositor being finished. |
| Cutscene script format inventory | An app-facing th03_cutscene library only if the parser becomes independently testable | Supplied-byte parser has no filesystem and captures source input/fast-forward cases. |
| PMD/OPNA feasibility and legal provenance audit | Future th03_audio_pmd | Exact .M/.EFC/rhythm-data requirements and a reproducible upstream selection decision. |

Avoid creating a placeholder th03-app which owns a second renderer or game state. The useful early app work is resource, input, clock, and construction seams with tests; the only gameplay frame producer remains th03_core plus its borrowed presentation observer.

## After VS: source scene order

Once VS-3 holds, add scenes in donor order rather than building a broad launcher framework.

1. MAINL routing and Story progression. Implement stage splash, round win, continuation, game-over, and the fresh/repeat handoffs in StageTransitionScene, following mlentry.cpp, screens.cpp, and ending.cpp. It constructs the next MAIN session and loads/unloads assets at the same semantic boundary. Acceptance: a source-traced Story route reaches each next-stage, continue, game-over, and ending branch with the corresponding screen/palette/timing sequence.
2. OP title and selection. Implement only the source's actual choices (Story, VS, Music Room, registration view, option, quit) and its character selection/configuration steps. Reuse BFNT/CDG/PI primitives; do not import a desktop UI toolkit. Game text remains source bytes/BFNT/gaiji first. If a later launcher needs translated labels, keep a separate app-owned catalog rather than rewriting gameplay or source asset strings. Acceptance: source-traced title-to-MAINL handoff, all selected modes/characters, and source palette/page transitions match.
3. Cutscenes and staff. Keep sequence policy in CutsceneScene and StaffScene, with image/flakes/text operations in presentation. Preserve the donor's script/input details before adding conveniences such as rewind, text log, or language selection. Acceptance: first/last pages, skip and fast-forward boundaries, staff CDG/palette/flake sequence, and return route match source traces.
4. Profile and replay UX. Only after source scenes work, add a versioned app profile for controls/window/audio volume and an optional replay browser. It is a user-interface layer, not an extension of a historical save block. Record and play only V13 at the app input boundary; format migration remains a tool concern.

## Audio plan

Audio is a separate fidelity project, not a WAV substitution. The first inventory is the donor's song/SE calls and the externally supplied .M, .EFC, and any rhythm data they require. It must identify source command ordering, load/stop/measure waits, and mute/volume behaviour before choosing a native API. Game timing must never wait on a host audio callback or device state.

The recommended later shape is a byte-fed th03_audio_pmd library that produces deterministic PCM and knows no files, windows, or SDL types. A small th03_platform_sdl endpoint feeds it to an SDL audio stream; SDL's [audio-device stream API](https://wiki.libsdl.org/SDL3/SDL_OpenAudioDeviceStream) is an appropriate device adapter because it keeps device management outside the synthesizer. The app owns user gain/mute settings, while the audio engine continues its source command timeline even if final output is muted unless donor evidence establishes different semantics.

[ymfm](https://github.com/aaronsgiles/ymfm) is a technically plausible OPNA (YM2608) synthesis candidate, but it has no release tag to treat as a default pin. Do not add it until the exact commit, vendoring strategy, license/credit obligations, required clock/rate conversion, and source trace are recorded. Likewise, [foo_input_pmd](https://github.com/stuerp/foo_input_pmd) is useful for upstream-format study, not an automatic dependency or a license decision. Required audio gates are: offline command/register or PCM traces for known assets, deterministic muted/unmuted scheduling tests, then device smoke tests on the supported host matrix.

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
