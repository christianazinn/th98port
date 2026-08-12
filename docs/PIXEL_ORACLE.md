# Classic / New Classic pixel verification

The port has one maintained daily presentation oracle and one deliberately
separate PC-98 raw-capture plan. The first is implemented and automated. The
second is the remaining emulator-instrumentation parcel; this document does
not claim that a guest producer exists yet.

## Host-publication fingerprints (`T3PXH2`)

Set `TH03_PIXEL_ORACLE_LOG` to an ignored output path before launching
`th03-app`. Every complete surface successfully published through SDL is
recorded as one TSV row:

```powershell
$env:TH03_PIXEL_ORACLE_LOG = "$PWD\.scratch\pix\classic-msvc.t3pxh"
.\out\build\msvc-x64-sdl\Release\th03-app.exe --classic <normal arguments>
```

The exact fourteen-column schema is:

```text
stream schema publication logical_tick coalesced_ticks overlay reduced_flash source_fnv64 raster_fnv64 tram_fnv64 shown_page raster_mode scroll_y tone
```

Fields are tab-separated. `stream=T3PXH` and `schema=2` identify this contract.
`publication` is contiguous host output order. Source-driven publications also
carry their rational scheduler `logical_tick`; product-owned or initial host
surfaces use `na`. While capture is enabled, the application publishes each
surface-changing logical tick separately instead of allowing wall-clock
catch-up to hide an intermediate source publication. `coalesced_ticks` records
how many changed logical surfaces the publication represents and is currently
one for those source rows.

`source_fnv64` fingerprints the complete published presentation state, not
only final RGBA pixels. `raster_fnv64` and `tram_fnv64` separately fingerprint
guest Raster and visible TRAM state; host-only surfaces use `na`. `overlay`
marks product or Replay Patch host drawing over a source surface, and
`reduced_flash` states whether New Classic's final reduced-flash policy was
applied. Overlay surfaces retain their base surface's page, raster, scroll,
tone, and guest hashes, so mixed-domain changes remain attributable.

The Raster/TRAM hashes are computed only while an SDL oracle owner is active;
normal shipping runs pay no full-surface hash cost.

Compare captures with:

```powershell
pwsh -NoProfile -File tools\compare_pixel_oracle.ps1 `
  -Base .scratch\pix\classic-gcc.t3pxh `
  -Candidate .scratch\pix\classic-msvc.t3pxh `
  -ReportPath .scratch\pix\classic-compiler-diff.json
```

The comparator requires the exact header and schema, equal-width rows,
contiguous ordinals, symmetric `na` fields, and equal source identity. It
normalizes hexadecimal letter case, reports the first differing field and
logical tick, and rejects unexplained prefixes or suffixes. Deliberately
bounded prefix comparisons must opt into `-AllowCandidateSuffix` or
`-AllowBaseSuffix`. Even malformed inputs produce a BOM-free structured JSON
report with `status=malformed`.

`tests\test_compare_pixel_oracle.ps1` freezes case normalization, asymmetric
`na`, length and suffix policy, structured malformed output, and UTF-8 encoding.
The SDL platform test freezes the producer's exact header and row identity.
An authentic 80-tick no-input Classic gate additionally produced byte-identical
81-row logs across two GCC runs and between GCC and MSVC Release on 2026-08-12.

This layer is useful for:

- repeat-run and GCC-versus-MSVC presentation determinism;
- locating the first Classic/New Classic source-state or host-overlay change;
- separating Raster, TRAM, selector, reduced-flash, and composition faults;
- driving an interactive or replay-locked capture while preserving source tick
  identity.

It is intentionally not called collision-proof pixel evidence. A differing
field decisively proves a published-state difference, but a finite hash match
does not prove equality of every pixel. Use the same content, seed, language,
reduced-flash policy, input/replay, and audio-status timeline on both sides.

## Raw PC-98 oracle (`T3PIX1`, planned)

The authoritative external pixel gate will be a versioned raw capture written
by a devtools-only PC-98 build into an emulator-mounted host directory. It is
not implemented in the current tree. The frozen design records, at matching
source publication acts:

- all four planar VRAM planes for the shown page, plus selected hidden-page
  diagnostics;
- shown/access page, 400/200-line raster mode, vertical scroll, and raw tone;
- applied and pending palettes plus the deferred-palette flag;
- raw TRAM code/attribute words; and
- process, scene, publication, logical-sample, and gameplay-frame identities.

Dense rows will contain fixed scalar state and FNV-1a/32 layer hashes. Selected
ordinals will additionally contain raw planar/TRAM payloads, allowing an
offline comparator to localize the first mismatch to a plane byte and physical
pixel. Indexed and RGBA review images are derived artifacts; raw planar,
palette, TRAM, and selector equality are the actual gate.

The implementation sequence is:

1. freeze the `T3PIX1` reader/writer/comparator and native raw-state adapter;
2. add a disposable `TH03_PIXEL_CAPTURE` guest profile at Replay Patch 0.4.10's
   publication boundaries, then at the equivalent stock/Anniversary boundaries;
3. run locked no-input and V13 cases through `tools\emu` with an ignored mounted
   capture directory; and
4. track only identities, SHA-256 digests, verdicts, and first-divergence
   metadata, never original capture payloads.

Guest instrumentation must remain devtools-only on a disposable branch. It
must not enter the shipping Replay Patch executable or become implementation
authority. `T3PXH2` is the practical daily regression mechanism while this
raw producer remains future work.

## Why not screenshots?

An emulator screenshot is not reliably tied to a source page/palette
publication act, and host RGB conversion mixes several independent domains.
Screenshots remain useful for human review, but cannot identify whether the
first fault was VRAM, TRAM, a palette apply, page selection, scroll, or the host
converter. The layered contract preserves those distinctions.
