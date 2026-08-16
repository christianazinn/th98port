# TH03 native audio: PMD, YM2608, and validation plan

Status: accepted engine-foundation decision, updated 2026-08-11. The original
TH03 files were inspected in place; no original game, driver, music, effect,
rhythm-ROM, or PCM bytes are copied into this repository.

## Decision

Use a narrow PMD-compatible audio service with four explicit layers:

1. presentation-owned semantic audio events, outside `libth03` simulation state;
2. a PMD sequencer/service layer that owns song, `.EFC`, timer, fade, measure,
   pause, part-mask, and effect-arbitration semantics and emits YM2608 writes;
3. `ymfm` as the YM2608 synthesizer, behind a small pinned adapter; and
4. a host resampler/mixer that applies independent user BGM and SFX gains after
   PMD's own volume/fade processing.

The accepted implementation is the pinned BSD-2-Clause PMD sequencer from
`Myon98/98fmplayer` at
`4fa914e4b2b994cb3ccf92d571a20a7cdf1fe36a`, behind an opaque C shim, with
the pinned BSD-3-Clause `ymfm` YM2608 core at
`81aec25ccbb98f4873a255f7551ac4dadac59b4a`. Exact locks, retained notices, and
the two bounded extraction/service deltas are tracked in `docs/SOURCES.lock` and
`third_party/{98fmplayer,ymfm}/`.

This choice does not outsource TH03's service semantics. 98fmplayer's external
FM-effect path is stubbed, so the port owns `.EFC`, stop/start/fade/measure,
pause/mask, and SFX arbitration behavior against donor documentation and shipped-
TSR traces. The imported foundation supplies byte-fed `.M` sequencing and chip
writes only. The port-owned service now supplies stop/play, signed fade,
measure/volume, pause/resume, PMD part masks, TH03's exact one-slot SFX
scheduler, and bounded external `.EFC` playback on Timer-A-driven FM6. Do not wrap
foobar2000, ship a PMDWin DLL, or import GPL `pmdmini` code merely to avoid that
boundary work.

This fits the post-core roadmap: deterministic gameplay remains the already
validated authority, while audio becomes a presentation consumer rather than a
new input to core state or replay hashes. See [POST_CORE_ROADMAP.md](../POST_CORE_ROADMAP.md)
and [SOURCES.lock](../SOURCES.lock).

## Evidence and confidence

The statements below distinguish three evidence classes:

- **Observed asset/source fact:** direct inspection of the local TH03 extraction
  or the ReC98 Anniversary source pinned at
  `3e2381d4275be7287f5525de5c95389d492b0a3c`.
- **Upstream fact:** a statement made by, or visible in, a cited upstream
  repository at the exact commit linked.
- **Inference / validation target:** an architectural conclusion or behavior
  still requiring a trace from the shipped TSR.

Primary local source references are ReC98's
[`kaja.h`](https://github.com/nmlgc/ReC98/blob/3e2381d4275be7287f5525de5c95389d492b0a3c/libs/kaja/kaja.h),
[`pmddata.doc`](https://github.com/nmlgc/ReC98/blob/3e2381d4275be7287f5525de5c95389d492b0a3c/libs/kaja/pmddata.doc),
[`snd.h`](https://github.com/nmlgc/ReC98/blob/3e2381d4275be7287f5525de5c95389d492b0a3c/th02/snd/snd.h),
[`load.cpp`](https://github.com/nmlgc/ReC98/blob/3e2381d4275be7287f5525de5c95389d492b0a3c/th02/snd/load.cpp), and
[`se.cpp`](https://github.com/nmlgc/ReC98/blob/3e2381d4275be7287f5525de5c95389d492b0a3c/th02/snd/se.cpp).
The current replay-patch evidence for pause/resume and hidden-preroll part masks
is recorded in `state/notes/tool-emu-demo-oracle.md`; it is useful corroboration,
but is not a substitute for an audio trace of the shipped TH03 configuration.

## Shipped TH03 audio inventory

The extracted installation contains the following relevant assets:

| Kind | Actually present | Count / role |
|---|---|---|
| PMD song | `.M` | 21 songs, 67,111 bytes total |
| PMD FM-effect bank | `.EFC` | `YUME.EFC`, 1,304 bytes |
| alternate PMD song | `.M2` | none |
| MIDI/MMD song | `.MMD` or equivalent | none |
| OPNA ADPCM/PCM bank | `.PPC`, `.PVI`, `.P86`, `.PPS`, `.PZI`, `.PVI2` | none |
| resident drivers | `.COM` | `PMD.COM`, `PMDB2.COM`, `PMD86.COM` |

The 21 song names are:

```text
00MM.M  01MM.M  02MM.M  03MM.M  04MM.M  05MM.M  06MM.M
07MM.M  08MM.M  DEC.M   DEMO1.M DEMO2.M DEMO3.M DEMO4.M
DEMO5.M ED.M    OP.M    OVER.M  SCORE.M SELECT.M WIN.M
```

The music room exposes 19 of these. `WIN.M` and `OVER.M` are transition/game-over
tracks rather than music-room selections. `00MM.M` through `08MM.M` are the
character themes; the selected character changes the digit in the loaded name.

The pinned donor's `th02/pc98/game_bat.cpp` replica selects `PMD.COM`,
`PMDB2.COM`, or `PMD86.COM` after the game's sound hardware probe, starts the
selected driver with `/M8 /V0 /E2 /K`, runs the game, and releases that same
driver afterward. A byte-silent check of the shipped batch file remains the
authority gate for that command line. Thus the native target is not an
abstract “PMD file player”: it is the behavior common to, or deliberately chosen
from, these shipped resident variants. In particular, the port still has to
decide which variant is the reference when they differ.

Absence of a separate PCM bank is a strong scope signal: TH03 does not ship data
for PMD's optional `.PPC`/`.PVI`/`.P86`/`.PPS`/PPZ paths. It does **not** prove
that no song issues rhythm commands. YM2608's six rhythm voices use the chip's
internal ADPCM-A ROM, which is a separate data-provenance problem discussed
below.

The inherited loader has a MIDI branch that can form a MIDI/MMD filename and use
interrupt `61h`, while SFX stays on PMD interrupt `60h`. The installed TH03 set
has neither MMD song assets nor an MMD driver launch path, so MIDI/MMD support is
out of scope for the first native audio milestone. `.M2` and all optional PCM
bank loaders can likewise remain unsupported until an in-scope asset requires
them.

## TH03's PMD call contract

### Resident-driver services used or already relevant

| PMD call | Meaning | Required native behavior |
|---|---|---|
| `AH=00h` | start song | Starts the already loaded song on the next applicable timer service; reinitializes most per-part state, overall volume, and some masks. |
| `AH=01h` | stop song | Stops/mutes song playback. |
| `AH=02h`, `AL=speed` | fade | Signed byte: `1..127` fades out; `128..255` represents `-128..-1` and fades in. Greater magnitude is faster. |
| `AH=05h` | get measure | Returns the driver's measure counter. TH03 uses it for visual timing, so it is an API value, not a value reconstructed from output PCM. |
| `AH=06h` | song-buffer address | TH03 reads a `.M` into this buffer. Native code should replace it with an atomic byte-span load. |
| `AH=08h` | get overall volume | `0` is loudest and `255` is quietest; TH03 waits for exact `255` after some fades. |
| `AH=09h` | get driver type/version | Used during sound-mode detection. A native façade should return a fixed documented capability set. |
| `AH=0Bh` | FM-effect-buffer address | TH03 loads `YUME.EFC` into this separate buffer. |
| `AH=0Ch`, `AL=id` | play FM effect | Starts an effect from the loaded `.EFC` bank. This is not PMD's embedded SSG-effect command. |
| `AH=10h` | public work-area address | Original/debug code can inspect part and driver state. The native equivalent should expose typed trace state, not packed DOS pointers. |
| `AH=19h`, `AL=volume` | set overall volume | Mutates PMD's internal music-volume/fade state. It must not implement the user's BGM slider. |
| `AH=1Ah` / `1Bh` | pause / resume | Pauses and resumes the song sequencer. SFX remains independently serviceable. |
| `AH=1Eh`, `AL=part` | mask/unmask part | `0..15` covers the parts relevant to current replay preroll; adding `80h` unmasks. Sequencing continues while output is masked. Starting a song resets masks. |
| `AH=22h` | query buffer sizes | Returns song, tone, and effect buffer capacities in KiB. Native span validation replaces this safely. |

The PMD documentation says a start request does not necessarily produce sound
inside the call: normal playback begins on the next Timer-B service, or after
the current Timer-A/Timer-B work completes. Preserve that boundary in the
register trace. It also warns that start resets overall volume and substantial
part state. A native `Play` must therefore be a PMD operation, not merely an
unpause bit.

The original loader asks PMD for a destination and performs a DOS read with a
hard-coded `0x5000` maximum instead of using `AH=22h`. It does not itself stop
playback before overwriting the buffer. ReC98 correctly classifies the latter as
a landmine because a slow read can expose a partially replaced song to the timer
interrupt. The native API should preserve observable successful call ordering,
but make `LoadSong(bytes)` and `LoadEffectBank(bytes)` atomic. Reproducing a
partial asynchronous DOS read is neither necessary nor safe.

### Music call sites

The source-level sequence is consistent across screens:

- title: stop, load `OP.M`, then play after the title's visual setup;
- character select: stop, load `SELECT.M`, play;
- round setup: load the selected character theme (or `DEC.M` at the relevant
  transition), then `round_startup` starts the already-loaded song;
- win: load and play `WIN.M`, then stop at the transition;
- game over: stop, load and play `OVER.M`, wait for driver measure 3 (or 64
  frames if BGM is unavailable), then stop after the palette transition;
- staff roll: fade the prior song at speed 16, wait until reported volume is
  exactly 255, stop, load `ED.M`, and later play it; ending input fades at speed
  8 while the visual transition runs;
- terminal score registration uses the same exact-volume wait before its final
  stop. The donor helper does not check `snd_active`, so Music Off has no driver
  state that can satisfy either wait. The native functional path bypasses only
  those unavailable-driver waits and retains exact 255 gating for active BGM;
- continue and other exits use nonblocking fades, including speed 3; and
- script commands can stop, start, load/start a named song, and request a PMD
  fade without blocking the script interpreter; stock scripts add speed 12.

This yields four stock fade speeds that must be fixtures: `3`, `8`, `12`, and
`16`. Do not replace them with wall-clock durations. The PMD contract updates
fade state from
Timer-A work. The current PMDWin-derived implementation applies one fade step
per eight Timer-A services; that is useful implementation evidence, also visible
in [`foo_input_pmd`'s timer and fade code](https://github.com/stuerp/foo_input_pmd/blob/632622b07ad930d70c430bdbca9e71e96e1e9306/PMD/PMD.cpp),
but the precise cadence remains a shipped-TSR trace target.

The measure value must also come from the sequencer. PMD's own documentation
explicitly warns that the reported measure is intended for game/visual timing
and is not guaranteed to correspond to a musically exact bar in every file.

### SFX scheduling and `.EFC`

TH03 loads `YUME.EFC` on entry to its main executable and reloads it around
transitions. A gameplay `snd_se_play(id)` does not call PMD immediately. It owns
one pending slot and replaces that slot only when the new request's priority is
at least the pending request's priority. Once per gameplay frame,
`snd_se_update()` invokes `AH=0Ch` on pending-frame zero, advances the hold
counter, and clears the slot at its configured frame count. `snd_se_play_force()`
resets, queues, and performs that first update immediately.

These hold counts are scheduling/arbitration windows, not decoded effect
durations. The exact table in the TH03 source is:

| ID | Priority | Scheduler frames | ID | Priority | Scheduler frames |
|---:|---:|---:|---:|---:|---:|
| 1 | 0 | 0 | 12 | 17 | 11 |
| 2 | 32 | 36 | 13 | 32 | 4 |
| 3 | 16 | 16 | 14 | 18 | 80 |
| 4 | 2 | 4 | 15 | 18 | 16 |
| 5 | 18 | 16 | 16 | 18 | 48 |
| 6 | 18 | 8 | 17 | 18 | 74 |
| 7 | 18 | 10 | 18 | 32 | 50 |
| 8 | 64 | 48 | 19 | 32 | 32 |
| 9 | 16 | 80 | 20 | 18 | 24 |
| 10 | 17 | 24 | 21 | 18 | 68 |
| 11 | 2 | 17 | 22 | 14 | 0 |

The complete source census finds 63 sites using 18 IDs: IDs 4, 11, 12, and 22
are unused, and ID 8 is the assembly-only score-registration cue. The updater runs
after player updates and before the pause branch in the round loop, which fixes
same-frame ordering. The native presentation layer should preserve this
scheduler as an event policy in front of PMD, rather than asking the audio
callback to arbitrate nondeterministically.

`snd_bgm_active()` is based on the BGM-mode flag, while `snd_se_active()` is
based on FM capability. However, OP configuration starts the driver only when
Music is on, so Classic Music=off also makes effects unavailable. Independent
user BGM/SFX gain is a later explicit QoL policy, not original behavior. The
accepted replay-patch pause/resume path is separate: it pauses song sequencing
while the resident driver and SFX service remain available.

## Candidate audit

Pins below are the upstream `HEAD` values observed on 2026-08-09. Any import
must add an exact source lock and retain all required notices.

### `98fmplayer` PMD sequencer — accepted donor

Audited and imported commit:
[`4fa914e4b2b994cb3ccf92d571a20a7cdf1fe36a`](https://github.com/Myon98/98fmplayer/tree/4fa914e4b2b994cb3ccf92d571a20a7cdf1fe36a),
BSD-2-Clause. The driver has no filesystem or mutable file-scope state; it takes
a mutable byte image and reaches the chip through three callbacks plus a timer
interrupt. Four of six retained driver files remain byte-identical to the pin.
The marked `fmdriver.h` extraction patch replaces the unused PPZ8 include with
the same guarded callback layout, avoiding an otherwise transitive C11-atomic
and visualizer dependency. The service parcel adds one bounded
`fmdriver_pmd.c` correction: the upstream fade tick computed intermediate
volume but stored only the saturated endpoints, making every ordinary fade
stall. The port stores that already-computed byte; generated gates lock speeds
3, 8, and 16 plus negative fade-in without changing the no-fade PCM trace.

`pmd_load()` is chip-silent, but playback writes PMD repeat counters into the
song image. The port therefore validates an immutable input span, retains an
immutable prepared copy, and creates a new mutable image for every Play. Its C
header is not valid C++, so public port headers expose no vendor layout.

The generated hermetic gate locks the first 199,680 maximum-fidelity samples:
97 register writes, register FNV-1a `dd11f6ed`, promoted Q31 stereo PCM FNV-1a
`4bad3dc5`, and 12 Timer-B ticks, identical on GCC and MSVC. ymfm's signed-16-
scale shared SSG lane is weighted by the T98-Next profile described below,
summed with each stereo FM/ADPCM lane under signed-16 saturation, and then
multiplied exactly by 65,536. This supplies the amplitude domain declared by
the signed-32 canonical/SDL boundary. A separate
byte-silent external gate loads and advances all 21 shipped songs on both and
independently confirms that 20 issue ADPCM-A rhythm key-ons.

### T98-Next reference mix

The private testing build uses T98-Next 13.1th's configured Sound86/FMGEN mix
as a practical listening reference. This is deliberately narrower than a claim
about the physical PC-9801-86 analog output. The supplied `MAIN.INI` has FM,
rhythm, and PCM at 20 and SSG at 14. Read-only disassembly of the supplied
`Sound86.nhw` shows that initialization subtracts 20 from each value. Its SSG
setter uses `pow(10, dB/40)`, so -6 half-dB units produce 3866 per PSG voice,
or 11598 for all three. ymfm's shared lane has a 32764 full-scale sum, giving
the nearest bounded ratio `725/2048`. The upstream zero-biased SSG waveform is
made bipolar to match FMGEN and remove its large DC offset. After ordinary key
edges are consumed immediately, an exact Sound86-versus-ymfm corpus drive finds
a stable FM amplitude ratio of 2.40..2.61 across 19 sounding songs; the
canonical T98 profile uses the center ratio `5/2` before saturation and Q31
promotion.

This matters for `OP.M`: YMFM formerly deferred PMD's adjacent direct key-off
and key-on until the next generated sample, collapsing both writes into an
unchanged live-on state. FM3 became exactly silent after roughly three seconds
despite continuing authored key-ons. T98/FMGEN applies each edge immediately;
the native compatibility patch now does the same after refreshing operator
cache state. An authentic twelve-second FM3 gate fails if the notes decay away.
Read-only disassembly also recovers six contiguous signed-16 44.1 kHz rhythm
sample arrays from `Sound86.nhw`, with source voice lengths `0x2590`, `0x3b10`,
`0x6eed`, `0x3e00`, `0x2f5e`, and `0x2018`. The private standalone builder
authenticates the complete device module before extracting those voices; the
native decoded-rhythm adapter reproduces Sound86's key-on/dump, total level,
per-instrument level, panning, Q10 cursor, and attenuation-table behavior. The
one-second authentic opening with that decoded profile is unclipped, peaks at
2,143,813,632 in Q31, and has FNV-1a `304CF5B3`. FM1-6, SSG1-2, and rhythm are
also isolated after a ten-second post-mask settling interval and frozen as nine
distinct ten-second PCM hashes. Register/timer behavior is unchanged.

The supplied 16 KiB `SOUND.ROM` remains board firmware, not the exact 8 KiB
YM2608 ADPCM-A rhythm image, and is never sliced or mislabeled. Without either
an 8 KiB image or the decoded profile, the adapter suppresses ADPCM-A writes;
returning zero bytes is not silence because the decoder turns zero nibbles into
a real, loud waveform.

The external FM-effect path is a marked TH03 extraction delta, not an upstream
capability claim. A port-owned validator accepts the 128-word pointer table,
127 exact terminated programs, the shipped ten-command subset, program-local
repeat targets, and 26-byte uniquely keyed FM tone records. It copies the bank,
relocates all internal words into each fresh mutable song image, advances the
effect from Timer A on FM6, and masks/releases the ordinary FM6 part. Generated
tests cover malformed-bank atomicity, replacement, pause/Stop independence,
repeat relocation, and caller lifetime. The byte-silent external gate starts
and naturally terminates all 22 authored `YUME.EFC` entries on GCC and MSVC.
Shipped-TSR register parity remains a fidelity gate for exact start/tick/end
ordering; it is not needed to make the native player functionally bounded.

### `ymfm` — recommended synthesis backend

Audited commit:
[`81aec25ccbb98f4873a255f7551ac4dadac59b4a`](https://github.com/aaronsgiles/ymfm/tree/81aec25ccbb98f4873a255f7551ac4dadac59b4a).
Upstream describes a C++14 Yamaha FM-family library, tested with GCC, Clang, and
Visual Studio 2019, and includes YM2608. It originated in MAME but is distributed
as a standalone BSD-3-Clause library; retain the
[`LICENSE`](https://github.com/aaronsgiles/ymfm/blob/81aec25ccbb98f4873a255f7551ac4dadac59b4a/LICENSE).
Its stated goal is audible faithfulness, not guaranteed cycle- or digitally
bit-exact reproduction. See the upstream
[`README`](https://github.com/aaronsgiles/ymfm/blob/81aec25ccbb98f4873a255f7551ac4dadac59b4a/README.md).

Strengths:

- clean permissive license and compact, source-embeddable C++14 API;
- explicit caller-owned clocks, timers, register reads/writes, and save/restore;
- YM2608 FM, SSG, ADPCM-A rhythm, and ADPCM-B engines;
- no x86-only assembly or pointer-size ABI in the public chip interface; and
- register-write tracing can live entirely in the adapter, before writes enter
  the chip.

Limits and maintenance costs:

- it is a chip, not a PMD sequencer, service API, file loader, SFX scheduler, or
  device mixer;
- the caller must provide external data reads for `ACCESS_ADPCM_A` and
  `ACCESS_ADPCM_B`. The YM2608 class fixes the six rhythm address ranges at
  `0x0000..0x1fff`; zero-filled reads are valid ADPCM rather than silence, so
  the adapter must suppress the ADPCM-A domain when no rhythm data is present;
- default maximum-fidelity YM2608 output is input clock divided by 8. At the
  provisional 7,987,200 Hz configuration that is 998,400 samples/s, requiring
  an explicit host-rate converter. The fidelity/rate behavior is documented in
  [`ymfm_opn.h`](https://github.com/aaronsgiles/ymfm/blob/81aec25ccbb98f4873a255f7551ac4dadac59b4a/src/ymfm_opn.h);
- its normal YM2608 output is already summed by component/channel. Independent
  post-PMD BGM/SFX stems require a small maintained extension or masked-channel
  output adapter; and
- “mostly integer” is not a determinism proof. Pin the version and test PCM
  hashes across supported compilers/architectures before promising byte-exact
  cross-platform PCM.

Verdict: use it. Validate register semantics separately from acoustic accuracy,
and keep the adapter small enough to rebase when upstream changes.

### `foo_input_pmd` and its `PMD/` subtree — audited alternative, not imported

Audited commit:
[`632622b07ad930d70c430bdbca9e71e96e1e9306`](https://github.com/stuerp/foo_input_pmd/tree/632622b07ad930d70c430bdbca9e71e96e1e9306).
The repository's current release is 0.5.0 and supports `.M`/`.M2`; its README
says the driver is heavily modified from C60's PMDWin 0.52 and now uses `ymfm`.
See the upstream
[`README`](https://github.com/stuerp/foo_input_pmd/blob/632622b07ad930d70c430bdbca9e71e96e1e9306/README.md),
root [`LICENSE`](https://github.com/stuerp/foo_input_pmd/blob/632622b07ad930d70c430bdbca9e71e96e1e9306/LICENSE), and
[`PMD/` tree](https://github.com/stuerp/foo_input_pmd/tree/632622b07ad930d70c430bdbca9e71e96e1e9306/PMD).

What can be extracted:

- The foobar adapter is outside `PMD/`; `PMDDecoder.cpp` contains the foobar SDK
  and `pfc` dependencies. The sequencing, YM2608 wrapper, P86/PPS/PPZ, and file
  support are grouped under `PMD/`. Therefore extraction is feasible without
  linking foobar2000.
- It is not cleanly drop-in portable. PMD sources include the project PCH;
  `File.*` uses `windows.h`, `WCHAR`, PathCch, fixed Windows path buffers, MSVC
  pragmas, and secure-CRT helpers. The public PMD API exposes wide paths and
  filesystem search policy. An extraction should replace those with byte spans
  and an injected asset resolver, then build the isolated library on MSVC and
  GCC/Clang x64. The existing project does demonstrate both x86 and x64 foobar
  packages, but that does not establish non-Windows portability.

Behavioral fit:

- The driver processes three SSG parts, FM 4–6, FM 1–3, extended FM, rhythm,
  ADPCM/P86, and PPZ8, and implements PMD-compatible Timer-A/Timer-B fade and
  sequencing. This is substantially closer than writing the `.M` parser first.
- Its high-quality fade is a player feature separate from the compatible PMD
  fade. Do not expose the former as TH03's `AH=02h` behavior.
- The OPNA wrapper advances `ymfm` with a fixed-point time accumulator and takes
  the latest native chip sample at each requested host output time; this is a
  zero-order/sample-hold style rate conversion, not a separately specified
  high-quality resampler. It also mixes optional 44.1 kHz, mono, 16-bit drum
  WAVs when no rhythm ROM is supplied. Replace both choices with a port-owned,
  deterministic resampling/mixing contract.
- Gain-table construction uses floating `std::pow`. Either prove resulting
  tables identical on supported targets or replace them with pinned generated
  integer tables before making cross-platform PCM-hash claims.
- The changelog contains recent fixes for PVI loading, tempo sign extension,
  WAV frequency, two PPZ banks, rests, panning, PPS settings, and endless loops.
  This demonstrates active care but also shows that PMD edge compatibility is a
  living maintenance surface. TH03-scoped traces are mandatory.

Critical gap:

The public `pmd_driver_t` API loads music and optional PCM-family assets, starts,
stops, renders, seeks, and fades. It has no external `.EFC` buffer/load/play API.
`PMDSSGEffect.cpp` consumes a static/embedded SSG effect table and is not the
PMD interrupt `AH=0Bh/0Ch` FM-effect service used by TH03. A related current
PMDWin-derived implementation even leaves its music-data “FM effect” command as
a one-byte skip; see the GPL source's
[`fm_efct_set`](https://github.com/gzaffin/pmdmini/blob/1b959a1081bbd9f386d0c8481637157669513ce1/src/pmdwin/pmdwincore.cpp#L7243-L7248).
That does not prove the external TSR format is impossible to add, but it rules
out treating current PMDWin playback as evidence of `.EFC` compatibility.

License/provenance gate:

The repository root declares MIT copyright by its current maintainer, while the
PMD files explicitly credit PMDWin/C60 and Masahiro Kajihara and the README calls
the code heavily modified PMDWin 0.52. There is no separate PMD-subtree COPYING,
NOTICE, or author/license file besides the nested BSD `ymfm` license. This is not
a conclusion that the root license is invalid. It is a requirement to record
the provenance chain and confirm—through an upstream notice or maintainer/rightsholder
confirmation—that the MIT grant covers the inherited PMDWin-derived code and
the required attributions. Do this before import, not at release freeze.

Verdict: not selected. The narrower 98fmplayer extraction proves the same
byte-fed separation without Win32/filesystem coupling or an unresolved
PMDWin-subtree license chain. Retain this audit as rejected-alternative context.

### PMDWin / `pmdmini` — reference only unless GPL is accepted

The actively updated
[`gzaffin/pmdmini`](https://github.com/gzaffin/pmdmini/tree/1b959a1081bbd9f386d0c8481637157669513ce1)
contains PMDWin-derived source and distributes under
[`GPL-2.0`](https://github.com/gzaffin/pmdmini/blob/1b959a1081bbd9f386d0c8481637157669513ce1/LICENSE.md).
It is valuable as a behavior/provenance cross-check. Do not copy its source into
a permissively distributed native port unless the project deliberately accepts
GPL obligations. A binary/DLL wrapper would also retain opaque filesystem,
platform, tracing, and service-control problems, so it is not a useful escape
hatch.

## YM2608 rhythm and ADPCM data policy

YM2608 rhythm is code **plus data**. `ymfm` models the six fixed ADPCM-A address
ranges but retrieves every byte through its host interface; it does not grant or
embed Yamaha's rhythm ROM. The relevant access path is visible in
[`ymfm_adpcm.cpp`](https://github.com/aaronsgiles/ymfm/blob/81aec25ccbb98f4873a255f7551ac4dadac59b4a/src/ymfm_adpcm.cpp),
and the fixed ranges in
[`ymfm_opn.cpp`](https://github.com/aaronsgiles/ymfm/blob/81aec25ccbb98f4873a255f7551ac4dadac59b4a/src/ymfm_opn.cpp#L973-L1007).
Other players likewise ask the user for either `ym2608_adpcm_rom.bin` or six
specifically named 44.1 kHz mono 16-bit WAVs; that loading convention does not
confer redistribution rights on the data.

Policy:

- Never vendor, download automatically, hash-lock into a public fixture bundle,
  or redistribute a rhythm ROM/sample set without an explicit compatible grant.
- Accept an external user-supplied 8 KiB ROM dump through the Classic launcher's
  `--rhythm-rom` option. The launcher validates an ordinary exact-size file and
  the engine copies it before SDL construction. The user is responsible for
  obtaining it lawfully for their jurisdiction.
- For the private T98-Next conformance build, authenticate the known
  `Sound86.nhw` profile and extract its six decoded voices into the bounded
  `T98RHYTHM.PCM` package. Accept that package through `--rhythm-pcm`; never
  execute the device module or commit/package either input publicly.
- Optionally support six explicitly user-selected replacement WAVs, but label
  that a non-reference acoustic mode. Only ship replacements whose license and
  authorship are recorded.
- Missing rhythm data is a deterministic, visible mode: suppress ADPCM-A
  register publication and issue one diagnostic. Never treat zero bytes as
  silence or fall back to mystery bytes fetched
  from an unofficial ROM site.
- Repository tests use generated impulses/ramps or purpose-created freely
  licensed samples, never original game or Yamaha ROM bytes.

TH03 ships no ADPCM-B/PPC/PVI/P86 bank. The byte-silent corpus gate now proves
that 20 of 21 songs use ADPCM-A rhythm while the ordinary ADPCM/PCM part is
musically unused across all 21. Authentic playback therefore requires the
caller-supplied 8 KiB rhythm image for hardware-style playback. The private
T98-reference build may use its authenticated decoded profile; empty input is
an explicit rhythm-suppressed development mode.

## Recommended architecture

```text
presentation/lifecycle code
    -> frame-stamped AudioEvent stream + TH03 SFX priority scheduler
    -> PMD service/sequencer (song + EFC + Timer A/B + fade/measure/pause)
    -> source-tagged YM2608 register writes
    -> one shared YM2608 state, component/channel stem output
    -> deterministic host-rate resampler
    -> BGM bus * user_bgm_gain + SFX bus * user_sfx_gain
    -> device adapter / ring buffer
```

### Semantic events stay outside the core

Use a presentation-owned event sink with frame number and within-frame ordinal:

```text
LoadSong(asset_id)       PlaySong             StopSong
FadeSong(signed_speed)   PauseSong            ResumeSong
LoadEffectBank(asset_id) PlayEffect(effect_id)
MaskPart(part)           UnmaskPart(part)
```

Asset IDs resolve to caller-supplied byte spans; paths never enter the
sequencer. Loading is atomic and size checked. The event stream can be logged
and replayed in an audio test, but it is not serialized in `libth03::State`, does
not affect gameplay hashes, and cannot feed timing back into the simulation.
The audio callback consumes a buffered clocked stream; callback underruns must
not stall or perturb gameplay.

The presentation/lifecycle layer owns when title, select, round, win, game-over,
and ending events occur. The one-slot SFX scheduler also lives there because its
priority and once-per-frame update are TH03 call semantics, not chip behavior.

### Sequencer and tracing

The sequencer exposes PMD operations rather than a generic player API. It owns:

- an explicit caller-supplied OPNA clock and integer/rational timebase; the
  conventional 7,987,200 Hz value remains provisional until primary PC-98
  hardware evidence is tracked;
- Timer-A and Timer-B ordering, including deferred start;
- PMD `.M` cursor/loop/part state and the measure counter;
- the internal `0..255` fade volume and signed fade speed;
- pause/resume and part masks without stopping timer/chip time;
- the external `.EFC` bank and current effect state; and
- a monotonic sequence number for every service call and chip write.

Trace hooks belong before the synthesizer. They must be available in release
builds behind a disabled sink, not reconstructed from logs or PCM.

### Shared chip, separate user gains

The shipped New Classic implementation applies category ownership at PMD's
existing song/effect boundary. Its public percentages are linear audible
amplitude, not a percentage of the numeric chip register. FM and OPNA rhythm
therefore translate each 5% step to the nearest 0.75 dB total-level
attenuation, SSG uses its coarser approximately 3 dB steps, and genuinely
linear ADPCM-B/PPZ paths retain integer multiplication. The 100% path bypasses
all translation and remains register- and PCM-identical to the source path.

The earlier implementation multiplied PMD's pre-inversion FM level byte. At a
displayed 75%, a full-scale effect lost 64 total-level steps (about 48 dB),
which explains why it sounded nearly muted. That operation is specifically
forbidden by the current contract and is covered by isolated FM/SSG energy
regressions.

Do not implement BGM/SFX sliders by calling PMD `AH=19h`, changing fade speed,
skipping sequencer ticks, suppressing effect events, or running two independent
PMD drivers. All of those change PMD state or erase authentic channel stealing.

Instead, the PMD adapter tags each active output component/channel as music or
FM effect. Extend the synthesis adapter to expose sums by FM channel and by
SSG/rhythm/ADPCM component without clocking state more than once. Route a channel
owned by the current `.EFC` effect to the SFX bus; when PMD restores/releases it,
route it back to BGM. Then:

```text
device_sample = saturate(
    q31_mul(pmd_faded_bgm_sample, user_bgm_gain) +
    q31_mul(sfx_sample,             user_sfx_gain))
```

When an effect steals a music channel and SFX gain is zero, that channel is
silent until PMD restores it; do not invent counterfactual music. With both gains
at unity, the two stems must sum bit-for-bit to the ordinary one-chip mix. PMD's
internal fade remains visible only on the BGM stem and continues to advance even
when user BGM gain is zero. Pause likewise freezes song sequencing according to
PMD while effects continue.

This stem routing is the only planned `ymfm` extension and should be isolated in
the adapter. The `.EFC` trace must first establish which channels/components the
shipped driver owns and how it restores them. If effect ownership cannot be
identified without invasive synthesis changes, default/reference mode remains
the exact combined mix; independent-gain behavior then needs an explicit,
documented non-reference shadow-render policy rather than a guessed split.

### Resampling and device output

The canonical renderer output is fixed as signed 32-bit stereo PCM at 48,000
Hz. The current minimum-fidelity engine produces exactly 166,400 Hz
(`7,987,200 / 48`), reducing the conversion ratio to 15 output samples for
every 52 input samples. `PmdFixedResampler` implements that one ratio only; it
is not a general host-device rate converter.

The one-chip mixer applies the documented T98-Next SSG weight, adds either the
YM2608 ADPCM-A result or decoded T98 rhythm voices under the same signed-16
saturation policy, and promotes the result into Q31 before this resampler.
Without that
promotion, SDL's S32 conversion interprets a nominal 32767 sample against 2^31
full scale and makes the result effectively silent. The digital weighting and
promotion are locked; captured PC-98 analog gain remains a separate hardware
comparison.

The checked-in port-owned generator produces 15 signed-Q30 phases of a 128-tap
Blackman-windowed sinc with a 20 kHz cutoff. Every quantized phase is corrected
to an exact Q30 sum of one. The runtime uses a 128-sample fixed ring and signed
64-bit accumulation, rounds nearest with half away from zero, and saturates to
signed 32-bit output. It performs no allocation or floating-point work.

Output sample `m` is centered at `floor(m * 52 / 15)` with phase
`(m * 52) % 15`. The symmetric support retains 63 earlier samples and requires
64 samples of lookahead; missing startup and tail samples are zero. A nonempty
finite stream emits exactly
`ceil((input_sample_count + 63) * 15 / 52)` samples after bounded flush, while
an empty stream emits none. Finish is explicit and idempotent, reset returns to
the all-zero phase, and arbitrary streaming chunk boundaries are sample-exact.
The frozen impulse hash is `2748126492` on strict GCC and MSVC.

Apply fixed user gains only after this canonical boundary and after any future
stem formation. If the physical device cannot accept 48 kHz signed stereo, a
final device-only conversion occurs afterward and is excluded from canonical
hashes. Neither host device time nor its conversion state may feed back into
PMD, presentation, gameplay, or replay timing.

The SDL 3.4.10 endpoint now opens the default playback device with this exact
S32/stereo/48 kHz app-side format. Its source queue is capped at 4,800 frames
(100 ms): an overrun clears stale unconverted input and keeps the newest block,
while counting discarded frames and the discontinuity. Queue failure, underrun,
device conversion, pause, and resume remain final-output facts only. The audio
owner shares the process-exclusive SDL runtime through a subordinate lease and
balances `SDL_INIT_AUDIO` independently from video/gamepad ownership. Dummy
driver tests lock format, queue accounting, failure unwind, moves, and both
destruction orders. Classic opens the endpoint paused after its initial
VSync-bound presentation, queues two silent plus one rendered source interval,
and only then resumes it. Its scheduler preserves every nominal PC-98 deadline,
so a slower host presentation can skip intermediate display frames without
slowing PMD/game time. Physical WASAPI and hotplug remain release QA.

This makes three validation levels explicit:

- PMD service/register traces: exact behavioral oracle;
- canonical PCM before user gain/device conversion: byte-exact native
  regression artifact where the backend/ROM/configuration are identical; and
- captured PC-98 analog PCM: acoustic/timing comparison with documented
  tolerance, never expected to hash-identically to a synthesized digital stream.

## Ranked build-versus-adapt options

| Rank | Option | Decision |
|---:|---|---|
| 1 | Adapt the narrow BSD-2 98fmplayer PMD sequencer behind an opaque C shim and drive pinned `ymfm` | **Accepted and implemented for the engine and TH03 transport/SFX scheduler.** Add `.EFC` in port-owned code. |
| 2 | Clean TH03-scoped PMD sequencer from documented services plus independently captured traces; use stock/pinned `ymfm` | Fallback if shipped-TSR comparison exposes unbounded donor incompatibility. |
| 3 | Adapt GPL PMDWin/`pmdmini` as a source dependency | Only if the whole distribution deliberately accepts GPL-2.0 and its scope. Otherwise reference behavior only. |
| 4 | Embed foobar/plugin/DLL, emulate the entire PC-98 audio stack, or pre-render songs/SFX | Reject. Opaque dependencies, weak tracing/control, poor portability, and no faithful dynamic fade/SFX interaction. |

The engine-foundation choice is now accepted. The platform-neutral Classic
session also maps all currently emitted scene cues through authenticated song
and EFC resources into exact rational native blocks and canonical 48 kHz PCM.
It reports `snd_active`-style driver availability independently of PMD Play/Stop
state and never observes SDL queue/device state. Launcher binding and missing
gameplay-stage/SFX emitters remain outside that completed boundary.

Option 2 remains the bounded
fallback only if shipped-TSR comparison exposes a material incompatibility that
cannot be corrected at the narrow service boundary.

## Implemented foundation/EFC service and next trace probe

The engine foundation already proves byte-fed transactional loading, fresh-song
and fresh-chip replay, deterministic register/PCM output, supplied rhythm-ROM
ownership, and all-21-song bounded advancement without SDL, settings UI, an
audio device, or gameplay-core integration. The service layer additionally
locks source stop/restart, signed fade steps, measure/volume queries,
pause-frozen sequencer state, PMD part-mask/reset state, all 22 TH03
priority/hold entries, and bounded EFC/FM6 execution. Extend that offline
boundary into the shipped-TSR trace probe:

1. Give the probe `--song`, `--efc`, optional `--rhythm-rom`, `--script`,
   `--ticks`/`--samples`, `--pcm-out`, and `--trace-out`. Original assets remain
   external ignored inputs.
2. Drive `OP.M` with the implemented stop/load/play, fade, query, pause/resume,
   and mask surface and compare it to normalized shipped-TSR traces. Separately
   drive `OVER.M` until measure 3.
3. Load `YUME.EFC`, invoke every authored ID `1..22`, and record the complete
   channel/register ownership lifecycle. Also script at least one lower-priority
   replacement rejection, equal-priority replacement, forced effect, and an
   effect while music is paused/fading.
4. Render the same finite scripts with MSVC x64 and GCC/Clang x64. Add x86 only
   if it is a supported product target; never make x86 an accidental condition
   of correctness.
5. The service probe passes when semantic and register traces are stable, bounded renders
   of all `.M` files return without decoder-side hangs or out-of-range reads,
   `.EFC` behavior matches the reference trace, and canonical PCM is reproducible
   for the same backend/data. Intentional song loops are not decoder hangs.

No original song, effect, or rhythm bytes or derived PCM goldens belong in the
repository. CI can run the same probe on synthetic permissive fixtures; the
private/oracle job supplies the original external assets.

## Exact validation artifacts

Each scenario produces a directory named by scenario ID and a manifest with
tool version, source SHAs, asset hashes (hashes only), clock, fidelity, rhythm
mode, resampler ID, channel-stem policy, compiler, architecture, and endianness.

Required files:

| Artifact | Exact contents / comparison |
|---|---|
| `events.tsv` | `event_seq`, gameplay frame, within-frame ordinal, event kind, numeric argument, opaque asset ID. Exact diff. |
| `pmd_ticks.tsv` | PMD tick, chip clock, Timer-A count, Timer-B count, play/pause flags, measure, fade speed, fade volume, loop count, active effect ID, effect scheduler state, and part-mask bits. Exact diff. |
| `registers.tsv` | monotonically ordered write number, chip clock, PMD tick, source tag (`bgm`/`sfx`/`system`), port, register, value. Exact diff against normalized shipped-driver trace. |
| `native_synth.pcm` | canonical little-endian signed stereo PCM before user gain/device conversion; exact hash for the pinned native renderer, resampler, and legally supplied rhythm data. |
| `bgm_stem.pcm`, `sfx_stem.pcm` | exact canonical stems; their unity-gain saturated sum must equal `native_synth.pcm` sample for sample. |
| `reference_capture.wav` | private PC-98 line-level/digital capture with leading sync marker retained; compare onset/timer landmarks, duration, loop point, envelope/rhythm behavior, and bounded spectral/RMS error, not byte hash. |
| `comparison.json` | first divergent event/tick/register, first divergent PCM sample, max/mean sample delta for native regressions, and the declared acoustic metrics for hardware captures. |

Minimum scenario set:

- first fixed interval of `OP.M`, every character theme, and one complete loop of
  the shortest looping song;
- `OVER.M` measure-3 boundary and no-BGM 64-frame fallback at the presentation
  level;
- fade-out speeds 3/8/16 through exact reported volume 255, plus one fade-in;
- pause/resume with an SFX during pause;
- mask and unmask each supported part while sequencing continues, followed by a
  start that verifies mask reset;
- every observed `.EFC` ID and the scheduler collision cases described above;
- user BGM gain 0 with SFX audible, user SFX gain 0 with BGM/fade state advancing,
  and both gains 1 proving stem recombination; and
- rhythm-data present, absent/silent, and generated replacement modes.

For sequencer validation, the authoritative equality is the normalized service
and YM2608 register stream. PCM cannot diagnose whether a mismatch came from
PMD timing, synthesis, resampling, or mixing, so always report the first
register divergence before a waveform metric.

## Unresolved decisions and release gates

1. **Reference TSR:** choose `PMD.COM`, `PMDB2.COM`, or `PMD86.COM` for each
   supported sound-board mode and trace their material differences.
2. **PMD dependency provenance:** resolved for the imported BSD-2 98fmplayer
   files; retain the exact pin, source delta, and binary notices.
3. **`.EFC` trace parity:** the bounded format, shipped opcode subset, FM6
   ownership, replacement, and natural termination are implemented. Normalize
   a shipped-TSR register trace to prove exact start/tick/end/restoration order,
   including the unused empty effect-ID-zero behavior.
4. **Actual song feature subset:** corpus structure and the native byte-silent
   gate resolve rhythm (20/21) and ordinary ADPCM/PCM (unused). Shipped-TSR
   register comparison remains necessary for exact sequencing fidelity.
5. **Rhythm provenance:** exact-size ROM, authenticated private decoded-T98,
   and true no-data suppression policies are implemented. Decide whether a
   redistributable replacement exists and how public packaging communicates
   that choice. Do not use unofficial download links.
6. **Stem implementation:** the source/channel ownership probe now proves
   independent post-settling FM1-6, SSG1-2, and rhythm output without changing
   the production chip state. Keep those exact hashes as regression evidence;
   aligned T98/physical capture remains the waveform-fidelity gate.
7. **Canonical rate/resampler:** resolved as explicit host policy for the current
   minimum-fidelity 166,400 Hz stream: fixed 48 kHz, 15 phases, 128 taps, signed
   Q30, 64-sample lookahead, round-half-away, and signed-32 saturation. The
   authentic clock/profile question remains separate.
8. **Portability floor:** x64 Windows is mandatory; decide whether 32-bit Windows
   and non-Windows hosts are release targets before freezing ABI/tests.
9. **Determinism promise:** generated engine and resampler PCM/register hashes
   are now an exact GCC/MSVC contract. External shipped-TSR traces remain the
   authenticity contract rather than a waveform claim.
10. **Presentation timing coverage:** capture menu, continue, game-over, ending,
    pause, and stage-transition event ordering; the gameplay replay corpus alone
    does not freeze these audiovisual boundaries.

Audio remains outside `libth03`. The no-UI engine, source-shaped
transport/SFX/EFC service, and fixed 15/52 integer resampler plus their generated
and external byte-silent gates are implemented. The resampler's checked-in
15-phase, 128-tap signed-Q30 table has exact per-phase DC gain, a 20 kHz cutoff,
at most 0.025 dB droop through 17 kHz, and at least 75 dB rejection from 24 kHz;
its 64-sample lookahead and zero-extended tail are explicit host policy. The
next safe move is SDL device delivery, followed by semantic scene-command
integration.
