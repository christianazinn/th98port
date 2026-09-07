# PMDB2/native OPNA register traces

The remaining TH03 audio-authenticity questions are compared at the YM2608
register boundary before synthesis or mixing is changed. Two tools provide the
reproducible sides of that boundary without committing original game data.

## Native trace producer

`th03-pmd-trace` starts each authored effect in a fresh native engine with the
same `OP.M`, `YUME.EFC`, 7,987,200 Hz clock, and minimum-fidelity clock divisor.
By default it traces effects 1 through 22 independently:

```powershell
out/build/gcc/th03-pmd-trace.exe `
  --song path/to/OP.M --efc path/to/YUME.EFC `
  --output out/native-pmd-trace
```

`--effect 8` limits a run to one effect. `manifest.json` (`T3ATR001`) records
only input sizes and SHA-256 identities, engine configuration, terminal sample
and register counts, and artifact names. Each scenario contains:

- `effect-NN-events.tsv`, with exact play completion, effect-call boundary,
  synchronous service-acceptance boundary before the next Timer-A key-on, and
  natural terminal boundary; and
- `effect-NN-registers.tsv`, with contiguous sequence, native chip clock,
  banked 9-bit register address, and value.

Original song/effect bytes and rendered audio are never written by the tool.
All scenarios finish in memory before the output directory is created.

## PC-98 raw-I/O normalization

An instrumented emulator capture has the strict raw schema:

```text
ordinal<TAB>pic_ms<TAB>port<TAB>value
```

`tools/audio/opna_trace.py normalize` pairs `0188/018A` and `018C/018E`
address/data writes into banked YM2608 addresses:

```powershell
python tools/audio/opna_trace.py normalize `
  --input private/opna.tsv --output out/pc98-registers.tsv
```

The normalizer requires contiguous raw ordinals, monotonic finite timestamps,
known ports, a latched address before every data write, and at least one
register write. It publishes atomically and retains the original ordinal/time
for diagnosis. It does not turn PC I/O completion time into a fictitious chip
clock.

Compare exact address/value order after selecting equivalent event windows:

```powershell
python tools/audio/opna_trace.py compare `
  --reference out/pc98-effect-08.tsv `
  --candidate out/native-pmd-trace/effect-08-registers.tsv
```

The comparator reports the first divergent sequence and both register/value
pairs. It deliberately ignores unlike clock domains and never treats equal
final register state as trace parity. Event-window capture/selection must be
documented with each private oracle run; an unsegmented gameplay trace is useful
normalizer evidence but is not an all-effect equality result.

## Controlled all-effect comparison

The independent control probe emits `ordinal/pic_ms/code` markers after each
effect call returns and after its bounded tail. Given that marker stream, the
normalized full PC-98 register stream, and one native output directory,
`compare-effects` selects all 22 equivalent lifecycles:

```powershell
python tools/audio/opna_trace.py compare-effects `
  --markers private/markers.tsv `
  --pc-registers private/registers.tsv `
  --native-directory out/native-pmd-trace
```

PC-98 port writes in one Timer-A update complete over a measurable interval,
whereas native writes in the same update share one chip clock. The comparator
therefore groups PC-98 FM6 writes separated by at most 2 ms, groups native
writes by exact chip clock, and compares the final value of every affected FM6
register per group. This is stricter than comparing an effect's final state:
every intermediate note, tone, volume, LFO, transpose, repeat, and portamento
update must agree. It also compares key-on-to-music-restoration duration within
0.25%. Raw within-group write order remains available to the exact comparator
and is never silently called equal.

The accepted controlled run restarted shipped `PMDB2.COM`, `OP.M`, and
`YUME.EFC` for each ID and repeated the complete capture twice. Both 51,361-row
normalized PC-98 streams were byte-identical. After aligning the external
effect work-area initialization and Timer-A start boundary with PMDB2, all 22
effects have equal group counts and equal canonical state at every group; the
largest duration difference is 0.208%. Original assets and raw traces remain
private and ignored.
